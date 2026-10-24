// Interaction presentation (interaction.md §5). Local only, never simulated.

#include "GSTN_InteractPresenter.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GSTN_InteractableComponent.h"
#include "GSTN_InteractorComponent.h"
#include "GSTN_InteractSettings.h"
#include "GSTN_InteractSubsystem.h"
#include "GSTN_InteractTags.h"

void UGSTN_InteractPresenter_Debug::OnFocusUpdated_Implementation(const FGSTN_InteractPresentationView& View, float DeltaTime)
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FColor Color = View.bUnactionable ? UnactionableTint : Tint;
	const float Radius = FMath::Max(View.RadiusCm, 20.f);
	DrawDebugCircle(World, View.Anchor, Radius, 24, Color, false, -1.f, SDPG_World, 2.f, FVector::ForwardVector, FVector::RightVector, false);

	FString Text = View.Display.Name.ToString();
	if (!View.Display.TapVerb.IsEmpty())
	{
		Text += FString::Printf(TEXT("\n[Tap] %s"), *View.Display.TapVerb.ToString());
	}
	if (!View.Display.HoldVerb.IsEmpty())
	{
		Text += FString::Printf(TEXT("\n[Hold] %s"), *View.Display.HoldVerb.ToString());
		if (View.HoldProgress > 0.f)
		{
			Text += FString::Printf(TEXT(" %d%%"), FMath::RoundToInt32(View.HoldProgress * 100.f));
		}
	}
	if (View.bUnactionable)
	{
		Text += TEXT("\n(unavailable)");
	}
	DrawDebugString(World, View.Anchor + FVector(0.f, 0.f, Radius + 20.f), Text, nullptr, Color, 0.f, true);
#endif
}

bool UGSTN_InteractPresentationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	// The host renders (listen server); a dedicated server draws nothing
	return Super::ShouldCreateSubsystem(Outer) && World && World->IsGameWorld() && ShouldPresentInNetMode(World->GetNetMode());
}

void UGSTN_InteractPresentationSubsystem::Deinitialize()
{
	Entries.Empty();
	PresenterStore.Empty();
	Super::Deinitialize();
}

void UGSTN_InteractPresentationSubsystem::CreatePresenters(FEntry& Entry)
{
	for (const TSubclassOf<UGSTN_InteractPresenter>& Class : UGSTN_InteractSettings::Get().PresenterClasses)
	{
		if (Class && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			UGSTN_InteractPresenter* Presenter = NewObject<UGSTN_InteractPresenter>(this, Class);
			Entry.Presenters.Add(Presenter);
			PresenterStore.Add(Presenter);
		}
	}
}

void UGSTN_InteractPresentationSubsystem::RegisterInteractor(UGSTN_InteractorComponent* Interactor)
{
	if (!Interactor || Entries.ContainsByPredicate([Interactor](const FEntry& Entry) { return Entry.Interactor == Interactor; }))
	{
		return;
	}
	FEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Interactor = Interactor;
	CreatePresenters(Entry);
}

void UGSTN_InteractPresentationSubsystem::UnregisterInteractor(UGSTN_InteractorComponent* Interactor)
{
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		if (Entries[Index].Interactor == Interactor || !Entries[Index].Interactor.IsValid())
		{
			for (UGSTN_InteractPresenter* Presenter : Entries[Index].Presenters)
			{
				PresenterStore.Remove(Presenter);
			}
			Entries.RemoveAtSwap(Index);
		}
	}
}

int32 UGSTN_InteractPresentationSubsystem::GetPresentedInteractorCount() const
{
	int32 Count = 0;
	const ENetMode NetMode = GetWorld() ? GetWorld()->GetNetMode() : NM_Standalone;
	for (const FEntry& Entry : Entries)
	{
		const UGSTN_InteractorComponent* Interactor = Entry.Interactor.Get();
		if (Interactor && ShouldPresentInteractor(Interactor->IsLocallyPresented(), NetMode))
		{
			++Count;
		}
	}
	return Count;
}

bool UGSTN_InteractPresentationSubsystem::BuildView(const UGSTN_InteractorComponent& Interactor, FGSTN_InteractPresentationView& OutView) const
{
	const FGSTN_InteractFocusResult Focus = Interactor.GetFocus();
	if (!Focus.HasFocus() || Focus.IsGhost())
	{
		return false;
	}
	const UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	const UGSTN_InteractableComponent* Component = Subsystem ? Subsystem->ResolveComponent(Focus.FocusId) : nullptr;
	if (!Component || !Component->GetOwner())
	{
		// An id with no object (a ghost, a destroyed actor, a provider's candidate with no component) draws nothing
		return false;
	}
	OutView = FGSTN_InteractPresentationView();
	OutView.FocusId = Focus.FocusId;
	OutView.Target = Component->GetOwner();
	OutView.Display = Component->ResolveDisplayInfo(*Subsystem);
	OutView.Anchor = Component->ResolveAnchor();
	if (const FGSTN_InteractCandidate* Record = Subsystem->GetRegistry().Find(Focus.FocusId))
	{
		OutView.RadiusCm = static_cast<float>(Record->RadiusCm);
	}
	OutView.HoldProgress = Interactor.GetHoldProgress();
	OutView.bUnactionable = Focus.IsUnactionable();
	return true;
}

void UGSTN_InteractPresentationSubsystem::DrawDebugRecords(const UGSTN_InteractorComponent& Interactor) const
{
#if ENABLE_DRAW_DEBUG
	const UGSTN_InteractSubsystem* Subsystem = UGSTN_InteractSubsystem::Get(GetWorld());
	if (!Subsystem || !Interactor.GetOwner())
	{
		return;
	}
	const int32 Frame = Subsystem->GetFrame();
	Subsystem->GetRegistry().QueryAround(Interactor.GetOwner()->GetActorLocation(), Frame, [this](const FGSTN_InteractCandidate& Record)
	{
		DrawDebugSphere(GetWorld(), Record.GetPosition(), FMath::Max<float>(Record.RadiusCm, 8.f), 8, FColor::Cyan, false, -1.f);
	});
#endif
}

void UGSTN_InteractPresentationSubsystem::Tick(float DeltaTime)
{
	const ENetMode NetMode = GetWorld() ? GetWorld()->GetNetMode() : NM_Standalone;
	const bool bPresentation = GSTNInteractCVars::IsPresentationEnabled();
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		FEntry& Entry = Entries[Index];
		const UGSTN_InteractorComponent* Interactor = Entry.Interactor.Get();
		if (!Interactor)
		{
			for (UGSTN_InteractPresenter* Presenter : Entry.Presenters)
			{
				PresenterStore.Remove(Presenter);
			}
			Entries.RemoveAtSwap(Index);
			continue;
		}
		if (!ShouldPresentInteractor(Interactor->IsLocallyPresented(), NetMode))
		{
			continue;
		}

		FGSTN_InteractPresentationView View;
		const bool bHasView = bPresentation && BuildView(*Interactor, View);
		if (GSTNInteractCVars::IsDebugDrawEnabled())
		{
			DrawDebugRecords(*Interactor);
		}

		if (!bHasView)
		{
			if (Entry.Shown.IsValid())
			{
				for (UGSTN_InteractPresenter* Presenter : Entry.Presenters)
				{
					Presenter->OnFocusLeft(Entry.LastView);
				}
				Entry.Shown.Reset();
				OnPresentedFocusChanged.Broadcast(GSTNInteractTags::Message_FocusChanged.GetTag(), Interactor, FGSTN_InteractPresentationView());
			}
			continue;
		}

		if (Entry.Shown != View.FocusId)
		{
			if (Entry.Shown.IsValid())
			{
				for (UGSTN_InteractPresenter* Presenter : Entry.Presenters)
				{
					Presenter->OnFocusLeft(Entry.LastView);
				}
			}
			for (UGSTN_InteractPresenter* Presenter : Entry.Presenters)
			{
				Presenter->OnFocusEntered(View);
			}
			Entry.Shown = View.FocusId;
			// The highlight must agree with the simulated focus, so nothing here is delayed to smooth visuals; the event is for audio and UI
			OnPresentedFocusChanged.Broadcast(GSTNInteractTags::Message_FocusChanged.GetTag(), Interactor, View);
		}
		for (UGSTN_InteractPresenter* Presenter : Entry.Presenters)
		{
			Presenter->OnFocusUpdated(View, DeltaTime);
		}
		Entry.LastView = View;
	}
}
