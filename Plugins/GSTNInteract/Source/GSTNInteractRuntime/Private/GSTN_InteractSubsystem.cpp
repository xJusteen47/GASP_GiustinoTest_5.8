// Interaction world subsystem (interaction.md §3.2, §2.4, §9).

#include "GSTN_InteractSubsystem.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GSTN_InteractableComponent.h"
#include "GSTN_InteractRuntimeTypes.h"
#include "GSTN_InteractSettings.h"

bool UGSTN_InteractSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Every net mode, dedicated servers included: a server validates presses against the same registry
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && (World->IsGameWorld());
}

void UGSTN_InteractSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UGSTN_InteractSettings& Settings = UGSTN_InteractSettings::Get();
	DefaultRules = Settings.GetSanitisedDefaultRules();

	TArray<FString> Errors;
	Meanings.Build(Settings.Meanings, &Errors);
	for (const FString& Error : Errors)
	{
		UE_LOG(LogGSTNInteract, Warning, TEXT("Interact meaning table: %s"), *Error);
	}

	// One ring of 27 cells must cover the relaxed reach plus a radius: the cell is at least that big
	const float Cell = DefaultRules.HardMaxReachCm * DefaultRules.StickyReachScale + Settings.GridMarginCm;
	Registry.Configure(Cell, DefaultRules.MaxRecords);
}

void UGSTN_InteractSubsystem::Deinitialize()
{
	Registry.Empty();
	Components.Empty();
	Providers.Empty();
	Super::Deinitialize();
}

int32 UGSTN_InteractSubsystem::GetFrame() const
{
	if (FrameOverride.IsSet())
	{
		return FrameOverride.GetValue();
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	double Seconds = World->GetTimeSeconds();
	if (const AGameStateBase* GameState = World->GetGameState())
	{
		Seconds = GameState->GetServerWorldTimeSeconds();
	}
	return FMath::FloorToInt32(Seconds * UGSTN_InteractSettings::Get().FrameRateHz);
}

int32 UGSTN_InteractSubsystem::AddRequirement(const FGameplayTagQuery& Query)
{
	if (Query.IsEmpty())
	{
		return -1;
	}
	return Requirements.Add(Query);
}

bool UGSTN_InteractSubsystem::RegisterRecord(const FGSTN_InteractCandidate& Record, UGSTN_InteractableComponent* Component)
{
	if (!Registry.Add(Record))
	{
		UE_LOG(LogGSTNInteract, Warning, TEXT("Interact record %s was refused (duplicate stable key, invalid id or full registry)"), *Record.Id.ToString());
		return false;
	}
	if (Component)
	{
		Components.Add(Record.Id, Component);
	}
	return true;
}

void UGSTN_InteractSubsystem::UnregisterRecord(const FGSTN_InteractCandidateId& Id)
{
	const int32 Frame = GetFrame();
	Registry.Remove(Id, Frame);
	Components.Remove(Id);
	Registry.Purge(Frame, DefaultRules.TombstoneFrames);
}

UGSTN_InteractableComponent* UGSTN_InteractSubsystem::ResolveComponent(const FGSTN_InteractCandidateId& Id) const
{
	const TWeakObjectPtr<UGSTN_InteractableComponent>* Found = Components.Find(Id);
	return Found ? Found->Get() : nullptr;
}

int32 UGSTN_InteractSubsystem::AllocateSpawnedHandle()
{
	return static_cast<int32>(0x80000000u | ++SpawnedSerial);
}

int32 UGSTN_InteractSubsystem::MakeStableHandle(const FString& StableKey)
{
	// High bit clear: never meets a spawned serial. Zero is "none", so it is moved off
	const int32 Hash = UGSTN_InteractMath::HashStableKey(StableKey) & 0x7FFFFFFF;
	return Hash == 0 ? 1 : Hash;
}

void UGSTN_InteractSubsystem::RegisterProvider(const IGSTN_InteractCandidateProvider* Provider)
{
	if (Provider)
	{
		Providers.AddUnique(Provider);
	}
}

void UGSTN_InteractSubsystem::UnregisterProvider(const IGSTN_InteractCandidateProvider* Provider)
{
	Providers.Remove(Provider);
}

bool UGSTN_InteractSubsystem::IsSegmentBlocked(const FVector& From, const FVector& To) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// Object type query for WorldStatic only, simple collision: anything dynamic differs between machines or between a frame and its resimulation
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNInteractLos), /*bTraceComplex*/ false);
	return World->LineTraceTestByObjectType(From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void UGSTN_InteractSubsystem::NoteProbeFocus(int32 EntityId, int32 Frame, const FGSTN_InteractCandidateId& Id)
{
	TArray<FProbeFocus, TInlineAllocator<4>>& Ring = ProbeFoci.FindOrAdd(EntityId);
	// A re-simulated frame replaces what it wrote before, and drops the frames after it
	Ring.RemoveAll([Frame](const FProbeFocus& Each) { return Each.Frame >= Frame; });
	if (Ring.Num() >= 4)
	{
		Ring.RemoveAt(0);
	}
	Ring.Add({ Frame, Id });
}

bool UGSTN_InteractSubsystem::FindProbeFocus(int32 EntityId, int32 Frame, FGSTN_InteractCandidateId& OutId, int32 MaxAgeFrames) const
{
	OutId.Reset();
	const TArray<FProbeFocus, TInlineAllocator<4>>* Ring = ProbeFoci.Find(EntityId);
	if (!Ring)
	{
		return false;
	}
	for (int32 Index = Ring->Num() - 1; Index >= 0; --Index)
	{
		const FProbeFocus& Each = (*Ring)[Index];
		if (Each.Frame <= Frame)
		{
			if (Frame - Each.Frame > MaxAgeFrames)
			{
				return false;
			}
			OutId = Each.Id;
			return true;
		}
	}
	return false;
}
