#include "GSTN_FastCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Containers/Queue.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/ThreadSafeCounter.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "IImageWrapperModule.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "RHIGPUReadback.h"
#include "ShowFlags.h"
#include "Tasks/Task.h"
#include "TextureResource.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCapture, Log, All);

/**
 * State shared with the render thread and the write tasks. Pending is touched only on the render thread (filled by the copy command,
 * emptied by the poll command); Ready is the render -> game thread hand-over; the counters are read from the game thread.
 */
struct FGSTN_FastCaptureShared
{
	struct FPending
	{
		int32 Frame = 0;
		int32 View = 0;
		TUniquePtr<FRHIGPUTextureReadback> Readback;
	};

	struct FReady
	{
		int32 Frame = 0;
		int32 View = 0;
		TArray<FColor> Pixels;
	};

	TArray<FPending> Pending;
	TQueue<FReady, EQueueMode::Spsc> Ready;
	FThreadSafeCounter InFlight;
	FThreadSafeCounter Writes;
	FThreadSafeCounter Written;
	FThreadSafeCounter WriteFailures;
	int32 Size = 0;

	/** Render thread: moves every finished readback (all of them when bAll, after the GPU went idle) to Ready. */
	void Poll(FRHICommandListImmediate& RHICmdList, bool bAll)
	{
		if (bAll && Pending.Num() > 0)
		{
			RHICmdList.SubmitCommandsAndFlushGPU();
			RHICmdList.BlockUntilGPUIdle();
		}
		for (int32 Index = 0; Index < Pending.Num();)
		{
			FPending& P = Pending[Index];
			if (!bAll && !P.Readback->IsReady())
			{
				++Index;
				continue;
			}
			FReady Out;
			Out.Frame = P.Frame;
			Out.View = P.View;
			int32 RowPitch = 0;
			int32 Height = 0;
			const FColor* Data = static_cast<const FColor*>(P.Readback->Lock(RowPitch, &Height));
			if (Data && RowPitch >= Size)
			{
				Out.Pixels.SetNumUninitialized(Size * Size);
				for (int32 Row = 0; Row < Size; ++Row)
				{
					FMemory::Memcpy(&Out.Pixels[Row * Size], Data + Row * RowPitch, Size * sizeof(FColor));
				}
				// Final color LDR leaves scene alpha in A (0 on the backdrop): the review pictures are opaque, as the Python path's PNGs
				for (FColor& C : Out.Pixels)
				{
					C.A = 255;
				}
			}
			P.Readback->Unlock();
			Ready.Enqueue(MoveTemp(Out));
			InFlight.Decrement();
			Pending.RemoveAtSwap(Index, EAllowShrinking::No);
		}
	}
};

UGSTN_FastCapture* UGSTN_FastCapture::CreateFastCapture(UObject* WorldContext, const FGSTN_FastCaptureSettings& InSettings)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogGSTNCapture, Error, TEXT("CreateFastCapture: no world"));
		return nullptr;
	}
	// The PNG writer resolves the image wrapper module from worker tasks: load it here, on the game thread, once
	FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	UGSTN_FastCapture* Capture = NewObject<UGSTN_FastCapture>(World);
	Capture->AddToRoot();
	Capture->Settings = InSettings;
	Capture->Shared = MakeShared<FGSTN_FastCaptureShared, ESPMode::ThreadSafe>();
	Capture->Shared->Size = InSettings.Size;
	return Capture;
}

int32 UGSTN_FastCapture::AddView(const FString& Name, USceneCaptureComponent2D* Capture, bool bReviewLighting)
{
	if (!Capture || !Shared)
	{
		return INDEX_NONE;
	}
	UTextureRenderTarget2D* Target = UKismetRenderingLibrary::CreateRenderTarget2D(Capture, Settings.Size, Settings.Size, RTF_RGBA8);
	Targets.Add(Target);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	// The temporal AA history of a capture that shoots every frame of the window stays valid between Shoot calls
	Capture->bAlwaysPersistRenderingState = true;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->TextureTarget = Target;
	if (bReviewLighting)
	{
		TArray<FString> Names;
		Settings.ReviewFlagsOff.ParseIntoArray(Names, TEXT(","), true);
		for (FString& FlagName : Names)
		{
			FlagName.TrimStartAndEndInline();
			const int32 Index = FEngineShowFlags::FindIndexByName(*FlagName);
			if (Index == INDEX_NONE)
			{
				UE_LOG(LogGSTNCapture, Warning, TEXT("AddView %s: unknown show flag '%s', skipped"), *Name, *FlagName);
				continue;
			}
			Capture->ShowFlags.SetSingleFlag(Index, false);
		}
		// Lumen is chosen by the post process, not only by show flags: the review views use no dynamic GI and no reflections
		Capture->PostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
		Capture->PostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		Capture->PostProcessSettings.bOverride_ReflectionMethod = true;
		Capture->PostProcessSettings.ReflectionMethod = EReflectionMethod::None;
		Capture->PostProcessBlendWeight = 1.f;
	}
	Views.Add({Name, Capture});
	return Views.Num() - 1;
}

void UGSTN_FastCapture::BeginSequence(const FString& InSequenceDir)
{
	if (bInSequence)
	{
		EndSequence(20.f);
	}
	SequenceDir = InSequenceDir;
	for (const FView& View : Views)
	{
		IFileManager::Get().MakeDirectory(*FPaths::Combine(SequenceDir, View.Name), true);
	}
	Ring.Reset();
	bInSequence = true;
	bCommitted = false;
	CommitFrom = 0;
	LastShotFrame = INDEX_NONE;
	Shot = Dropped = PeakInFlight = Stalls = 0;
	ShootSeconds = 0.0;
	SequenceStart = FPlatformTime::Seconds();
	Shared->Written.Reset();
	Shared->WriteFailures.Reset();
}

void UGSTN_FastCapture::Shoot(int32 Frame)
{
	if (!bInSequence || !Shared)
	{
		return;
	}
	const double T0 = FPlatformTime::Seconds();
	// Back-pressure: only when the GPU is far behind (MaxInFlightPerView frames per view) the game thread waits for the oldest copies
	const int32 Limit = FMath::Max(1, Settings.MaxInFlightPerView) * FMath::Max(1, Views.Num());
	if (Shared->InFlight.GetValue() >= Limit)
	{
		++Stalls;
		TSharedPtr<FGSTN_FastCaptureShared, ESPMode::ThreadSafe> S = Shared;
		ENQUEUE_RENDER_COMMAND(GSTNFastCaptureDrain)([S](FRHICommandListImmediate& RHICmdList) { S->Poll(RHICmdList, true); });
		FlushRenderingCommands();
	}
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		USceneCaptureComponent2D* Capture = Views[ViewIndex].Capture.Get();
		UTextureRenderTarget2D* Target = Targets.IsValidIndex(ViewIndex) ? Targets[ViewIndex].Get() : nullptr;
		if (!Capture || !Target)
		{
			continue;
		}
		Capture->CaptureScene();   // enqueues the scene render of this view on the render thread, does not wait for it
		FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
		if (!Resource)
		{
			continue;
		}
		TSharedPtr<FGSTN_FastCaptureShared, ESPMode::ThreadSafe> S = Shared;
		Shared->InFlight.Increment();
		ENQUEUE_RENDER_COMMAND(GSTNFastCaptureCopy)([S, Resource, Frame, ViewIndex](FRHICommandListImmediate& RHICmdList)
		{
			FGSTN_FastCaptureShared::FPending P;
			P.Frame = Frame;
			P.View = ViewIndex;
			P.Readback = MakeUnique<FRHIGPUTextureReadback>(TEXT("GSTNFastCapture"));
			P.Readback->EnqueueCopy(RHICmdList, Resource->GetRenderTargetTexture());
			S->Pending.Add(MoveTemp(P));
		});
	}
	PeakInFlight = FMath::Max(PeakInFlight, Shared->InFlight.GetValue());
	++Shot;
	LastShotFrame = Frame;
	Pump();
	ShootSeconds += FPlatformTime::Seconds() - T0;
}

void UGSTN_FastCapture::Commit(int32 PressFrame)
{
	if (!bInSequence || bCommitted)
	{
		return;
	}
	bCommitted = true;
	CommitFrom = PressFrame - Settings.PreRollFrames;
	for (TPair<int32, TArray<TPair<int32, TArray<FColor>>>>& Entry : Ring)
	{
		for (TPair<int32, TArray<FColor>>& ViewPixels : Entry.Value)
		{
			if (Entry.Key >= CommitFrom)
			{
				StartWrite(Entry.Key, ViewPixels.Key, MoveTemp(ViewPixels.Value));
			}
			else
			{
				++Dropped;
			}
		}
	}
	Ring.Reset();
}

void UGSTN_FastCapture::Pump()
{
	if (!Shared)
	{
		return;
	}
	if (Shared->InFlight.GetValue() > 0)
	{
		TSharedPtr<FGSTN_FastCaptureShared, ESPMode::ThreadSafe> S = Shared;
		ENQUEUE_RENDER_COMMAND(GSTNFastCapturePoll)([S](FRHICommandListImmediate& RHICmdList) { S->Poll(RHICmdList, false); });
	}
	FGSTN_FastCaptureShared::FReady Item;
	while (Shared->Ready.Dequeue(Item))
	{
		if (Item.Pixels.Num() == 0)
		{
			++Dropped;   // the copy could not be locked: counted, the frame is missing on disk (review_pack reports missing frames)
			continue;
		}
		if (bCommitted && Item.Frame >= CommitFrom)
		{
			StartWrite(Item.Frame, Item.View, MoveTemp(Item.Pixels));
		}
		else if (!bCommitted)
		{
			Ring.FindOrAdd(Item.Frame).Emplace(Item.View, MoveTemp(Item.Pixels));
		}
		else
		{
			++Dropped;
		}
	}
	TrimRing();
}

void UGSTN_FastCapture::TrimRing()
{
	if (bCommitted || LastShotFrame == INDEX_NONE)
	{
		return;
	}
	const int32 Oldest = LastShotFrame - Settings.PreRollFrames;
	for (auto It = Ring.CreateIterator(); It; ++It)
	{
		if (It.Key() < Oldest)
		{
			Dropped += It.Value().Num();
			It.RemoveCurrent();
		}
	}
}

void UGSTN_FastCapture::StartWrite(int32 Frame, int32 ViewIndex, TArray<FColor>&& Pixels)
{
	if (!Views.IsValidIndex(ViewIndex))
	{
		return;
	}
	const FString Path = FPaths::Combine(SequenceDir, Views[ViewIndex].Name, FString::Printf(TEXT("f%03d.png"), Frame));
	TSharedPtr<FGSTN_FastCaptureShared, ESPMode::ThreadSafe> S = Shared;
	const int32 Size = Settings.Size;
	const int32 Quality = Settings.PngQuality;
	S->Writes.Increment();
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [S, Path, Size, Quality, Pixels = MoveTemp(Pixels)]() mutable
	{
		const FImageView Image(Pixels.GetData(), Size, Size, EGammaSpace::sRGB);
		if (FImageUtils::SaveImageByExtension(*Path, Image, Quality))
		{
			S->Written.Increment();
		}
		else
		{
			S->WriteFailures.Increment();
		}
		S->Writes.Decrement();
	});
}

bool UGSTN_FastCapture::EndSequence(float TimeoutSeconds)
{
	if (!bInSequence || !Shared)
	{
		return true;
	}
	const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
	TSharedPtr<FGSTN_FastCaptureShared, ESPMode::ThreadSafe> S = Shared;
	ENQUEUE_RENDER_COMMAND(GSTNFastCaptureFinal)([S](FRHICommandListImmediate& RHICmdList) { S->Poll(RHICmdList, true); });
	FlushRenderingCommands();
	Pump();
	// A sequence that never committed (no press, nothing kept) drops its ring: nothing of it was wanted on disk
	if (!bCommitted)
	{
		for (const TPair<int32, TArray<TPair<int32, TArray<FColor>>>>& Entry : Ring)
		{
			Dropped += Entry.Value.Num();
		}
		Ring.Reset();
	}
	bool bOk = true;
	while (Shared->Writes.GetValue() > 0 || Shared->InFlight.GetValue() > 0)
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			bOk = false;
			break;
		}
		FPlatformProcess::Sleep(0.002f);
		Pump();
	}
	bInSequence = false;
	const FString Stats = GetStats();
	FFileHelper::SaveStringToFile(Stats + TEXT("\n"), *FPaths::Combine(SequenceDir, TEXT("fastcap.json")));
	UE_LOG(LogGSTNCapture, Log, TEXT("EndSequence %s: %s%s"), *SequenceDir, *Stats, bOk ? TEXT("") : TEXT(" TIMEOUT"));
	return bOk && Shared->WriteFailures.GetValue() == 0;
}

FString UGSTN_FastCapture::GetStats() const
{
	if (!Shared)
	{
		return TEXT("{}");
	}
	return FString::Printf(
		TEXT("{\"views\": %d, \"shot\": %d, \"written\": %d, \"write_failures\": %d, \"dropped\": %d, \"commit_from\": %d, \"shoot_ms\": %.1f, \"peak_in_flight\": %d, \"stalls\": %d, \"wall_s\": %.2f}"),
		Views.Num(), Shot, Shared->Written.GetValue(), Shared->WriteFailures.GetValue(), Dropped, bCommitted ? CommitFrom : -1,
		ShootSeconds * 1000.0, PeakInFlight, Stalls, FPlatformTime::Seconds() - SequenceStart);
}

void UGSTN_FastCapture::Release()
{
	EndSequence(20.f);
	Views.Reset();
	Targets.Reset();
	Ring.Reset();
	if (IsRooted())
	{
		RemoveFromRoot();
	}
}

void UGSTN_FastCapture::BeginDestroy()
{
	// The copy commands hold raw pointers to the targets' resources: let them run before the targets can go
	if (Shared && Shared->InFlight.GetValue() > 0)
	{
		FlushRenderingCommands();
	}
	Super::BeginDestroy();
}
