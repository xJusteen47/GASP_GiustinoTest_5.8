// Automation tests of the interaction runtime (interaction.md §10, GSTNInteract.Runtime.*). Headless: no render, no net driver.

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GSTN_InteractableComponent.h"
#include "GSTN_InteractorComponent.h"
#include "GSTN_InteractPresenter.h"
#include "GSTN_InteractSettings.h"
#include "GSTN_InteractSubsystem.h"
#include "GSTN_InteractTags.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNInteractRuntimeTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A throwaway game world; the interact subsystems are created with it */
	struct FRig
	{
		UWorld* World = nullptr;
		UGSTN_InteractSubsystem* Subsystem = nullptr;

		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNInteractTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Subsystem = UGSTN_InteractSubsystem::Get(World);
		}

		~FRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		AActor* SpawnActor(const FVector& Location, UClass* Class = AActor::StaticClass())
		{
			FActorSpawnParameters Params;
			Params.bNoFail = true;
			AActor* Actor = World->SpawnActor<AActor>(Class, FTransform(Location), Params);
			if (!Actor->GetRootComponent())
			{
				USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
				Actor->SetRootComponent(Root);
				Root->RegisterComponent();
				Actor->SetActorLocation(Location);
			}
			return Actor;
		}

		UGSTN_InteractableComponent* AddInteractable(AActor* Actor)
		{
			UGSTN_InteractableComponent* Component = NewObject<UGSTN_InteractableComponent>(Actor);
			Component->Meaning = GSTNInteractTags::Root_Target.GetTag();
			Component->RegisterComponent();
			return Component;
		}
	};

	static void SetMeaning()
	{
		// The settings default object carries the meaning table; tests add the only native leaf they can name
		UGSTN_InteractSettings* Settings = GetMutableDefault<UGSTN_InteractSettings>();
		FGSTN_InteractMeaning Meaning;
		Meaning.Tag = GSTNInteractTags::Root_Target.GetTag();
		Meaning.DefaultPriority = 100;
		Meaning.HoldFrames = 30;
		Settings->Meanings = { Meaning };
	}
}

using namespace GSTNInteractRuntimeTest;

#define GSTN_INTERACT_RT_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNInteractRuntimeTest_##Name, "GSTNInteract.Runtime." #Name, GSTNInteractRuntimeTest::TestFlags) \
	bool FGSTNInteractRuntimeTest_##Name::RunTest(const FString& Parameters)

GSTN_INTERACT_RT_TEST(RecordReplicatesWithStamps)
{
	SetMeaning();
	FRig Rig;
	if (!TestNotNull(TEXT("the world creates the interact subsystem"), Rig.Subsystem))
	{
		return false;
	}

	// The property that carries a spawned record is replicated, and the component replicates
	const FProperty* Stamp = UGSTN_InteractableComponent::StaticClass()->FindPropertyByName(TEXT("RecordStamp"));
	TestTrue(TEXT("the stamp is a replicated property"), Stamp && Stamp->HasAnyPropertyFlags(CPF_Net));
	TestTrue(TEXT("and it notifies on arrival"), Stamp && Stamp->HasAnyPropertyFlags(CPF_RepNotify));

	Rig.Subsystem->SetFrameOverride(5000);
	AActor* ServerActor = Rig.SpawnActor(FVector(500.f, 20.f, 0.f));
	UGSTN_InteractableComponent* Server = Rig.AddInteractable(ServerActor);
	Server->RegisterRecord();
	const FGSTN_InteractRecordStamp ServerStamp = Server->GetRecordStamp();
	TestTrue(TEXT("authority assigns a handle"), ServerStamp.IsSet());
	TestTrue(TEXT("spawned handles have the high bit set"), static_cast<uint32>(ServerStamp.Handle) & 0x80000000u);
	const int32 Now = Rig.Subsystem->GetFrame();
	TestEqual(TEXT("FromFrame is now + MinAgeFrames"), ServerStamp.FromFrame, Now + Rig.Subsystem->GetDefaultRules().MinAgeFrames);
	TestEqual(TEXT("and it is actionable from then"), ServerStamp.ActionableFrom, ServerStamp.FromFrame);

	const FGSTN_InteractCandidate* Record = Rig.Subsystem->GetRegistry().Find(Server->GetCandidateId());
	if (!TestNotNull(TEXT("the server registered its record"), Record))
	{
		return false;
	}

	// What a client builds on receiving the stamp: the same record, field for field
	AActor* ClientActor = Rig.SpawnActor(FVector(500.f, 20.f, 0.f));
	UGSTN_InteractableComponent* Client = Rig.AddInteractable(ClientActor);
	const FGSTN_InteractCandidate Built = Client->BuildCandidate(ServerStamp, *Rig.Subsystem, -1);
	TestTrue(TEXT("same id"), Built.Id == Record->Id);
	TestTrue(TEXT("same position"), Built.PositionCm == Record->PositionCm);
	TestEqual(TEXT("same FromFrame"), Built.FromFrame, Record->FromFrame);
	TestEqual(TEXT("same ActionableFrom"), Built.ActionableFrom, Record->ActionableFrom);
	TestEqual(TEXT("same meaning index"), Built.MeaningIndex, Record->MeaningIndex);

	// Not focusable before the stamp, focusable at it
	TestFalse(TEXT("not live before FromFrame"), Record->IsLiveAt(ServerStamp.FromFrame - 1));
	TestTrue(TEXT("live at FromFrame"), Record->IsLiveAt(ServerStamp.FromFrame));

	// Destroying the actor stamps the end and leaves a tombstone
	const FGSTN_InteractCandidateId Id = Server->GetCandidateId();
	// The world has not begun play in this rig, so end the component the way its destruction would
	Rig.Subsystem->SetFrameOverride(5100);
	Server->UnregisterRecord();
	const FGSTN_InteractCandidate* Ended = Rig.Subsystem->GetRegistry().Find(Id);
	TestTrue(TEXT("the record ends at the removal frame and stays as a tombstone"), Ended && Ended->ToFrame != MAX_int32);
	return true;
}

GSTN_INTERACT_RT_TEST(DedicatedServerNoPresentation)
{
	TestFalse(TEXT("a dedicated server presents nothing"), UGSTN_InteractPresentationSubsystem::ShouldPresentInNetMode(NM_DedicatedServer));
	TestTrue(TEXT("a listen server renders"), UGSTN_InteractPresentationSubsystem::ShouldPresentInNetMode(NM_ListenServer));
	TestTrue(TEXT("a client renders"), UGSTN_InteractPresentationSubsystem::ShouldPresentInNetMode(NM_Client));
	TestTrue(TEXT("standalone renders"), UGSTN_InteractPresentationSubsystem::ShouldPresentInNetMode(NM_Standalone));
	TestFalse(TEXT("a local player's interactor on a dedicated server is still not presented"), UGSTN_InteractPresentationSubsystem::ShouldPresentInteractor(true, NM_DedicatedServer));

	// The interaction subsystem itself exists in every net mode: a server validates presses against its registry
	FRig Rig;
	TestNotNull(TEXT("the registry subsystem exists"), Rig.Subsystem);
	TestNotNull(TEXT("and so does the presentation subsystem in a rendering world"), UGSTN_InteractPresentationSubsystem::Get(Rig.World));
	return true;
}

GSTN_INTERACT_RT_TEST(OnlyLocalPlayerPresented)
{
	TestTrue(TEXT("local player, rendering machine"), UGSTN_InteractPresentationSubsystem::ShouldPresentInteractor(true, NM_Client));
	TestFalse(TEXT("another player's interactor is not drawn (INT.4)"), UGSTN_InteractPresentationSubsystem::ShouldPresentInteractor(false, NM_ListenServer));

	FRig Rig;
	UGSTN_InteractPresentationSubsystem* Presentation = UGSTN_InteractPresentationSubsystem::Get(Rig.World);
	if (!TestNotNull(TEXT("presentation subsystem"), Presentation))
	{
		return false;
	}
	// A pawn with no controller is nobody's local pawn; a plain actor counts as local
	APawn* Pawn = Rig.World->SpawnActor<APawn>(FVector::ZeroVector, FRotator::ZeroRotator);
	UGSTN_InteractorComponent* Remote = NewObject<UGSTN_InteractorComponent>(Pawn);
	Remote->RegisterComponent();
	Presentation->RegisterInteractor(Remote);
	TestFalse(TEXT("an uncontrolled pawn's interactor is not local"), Remote->IsLocallyPresented());
	TestEqual(TEXT("nothing is presented for it"), Presentation->GetPresentedInteractorCount(), 0);

	AActor* Prop = Rig.SpawnActor(FVector::ZeroVector);
	UGSTN_InteractorComponent* Local = NewObject<UGSTN_InteractorComponent>(Prop);
	Local->RegisterComponent();
	Presentation->RegisterInteractor(Local);
	TestEqual(TEXT("a non-pawn owner is presented"), Presentation->GetPresentedInteractorCount(), 1);
	return true;
}

GSTN_INTERACT_RT_TEST(GameThreadFocusMatchesCore)
{
	SetMeaning();
	FRig Rig;
	if (!TestNotNull(TEXT("subsystem"), Rig.Subsystem))
	{
		return false;
	}
	AActor* Near = Rig.SpawnActor(FVector(150.f, 0.f, 0.f));
	AActor* Side = Rig.SpawnActor(FVector(150.f, 100.f, 0.f));
	UGSTN_InteractableComponent* NearComponent = Rig.AddInteractable(Near);
	UGSTN_InteractableComponent* SideComponent = Rig.AddInteractable(Side);
	Rig.Subsystem->SetFrameOverride(0);
	NearComponent->RegisterRecord();
	SideComponent->RegisterRecord();

	AActor* Interactor = Rig.SpawnActor(FVector::ZeroVector);
	UGSTN_InteractorComponent* Component = NewObject<UGSTN_InteractorComponent>(Interactor);
	Component->RegisterComponent();

	const int32 Frame = 5000;
	Rig.Subsystem->SetFrameOverride(Frame);
	const FGSTN_InteractFocusResult Probed = Component->ProbeAtFrame(Frame);

	// The same inputs through the core, with a state of its own
	FGSTN_InteractQuery Query;
	Query.Frame = Frame;
	Query.Origin = Interactor->GetActorLocation();
	Query.Up = Interactor->GetActorUpVector();
	Query.Facing = Interactor->GetActorForwardVector();
	Query.Rules = &Rig.Subsystem->GetDefaultRules();
	Query.Registry = &Rig.Subsystem->GetRegistry();
	Query.Meanings = &Rig.Subsystem->GetMeanings();
	Query.Requirements = Rig.Subsystem->GetRequirements();
	FGSTN_InteractFocusState State;
	const FGSTN_InteractFocusResult Direct = UGSTN_InteractMath::UpdateFocus(Query, State);

	TestTrue(TEXT("the interactor found a focus"), Probed.HasFocus());
	TestTrue(TEXT("the same focus as the core"), Probed.FocusId == Direct.FocusId && Probed.MeaningIndex == Direct.MeaningIndex);
	TestTrue(TEXT("the item straight ahead is it"), Probed.FocusId == NearComponent->GetCandidateId());
	TestTrue(TEXT("it resolves to an actor"), Component->GetFocusActor() == Near);
	TestEqual(TEXT("the meaning is the one the table compiled"), Probed.MeaningIndex, 1);

	// Server validation: in reach, then out of reach
	TestTrue(TEXT("a press on the focus is accepted on authority"), Component->TryInteractServer(Probed.FocusId, EGSTN_InteractVerb::Tap));
	Interactor->SetActorLocation(FVector(-1000.f, 0.f, 0.f));
	TestFalse(TEXT("and refused far away"), Component->TryInteractServer(Probed.FocusId, EGSTN_InteractVerb::Tap));
	return true;
}

GSTN_INTERACT_RT_TEST(LosIgnoresDynamicActors)
{
	FRig Rig;
	if (!TestNotNull(TEXT("subsystem"), Rig.Subsystem))
	{
		return false;
	}
	auto MakeBox = [&Rig](const FVector& Location, ECollisionChannel ObjectType)
	{
		AActor* Actor = Rig.SpawnActor(Location);
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Box->SetBoxExtent(FVector(10.f, 100.f, 100.f));
		Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ObjectType);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetupAttachment(Actor->GetRootComponent());
		Box->RegisterComponent();
		return Actor;
	};

	const FVector From(0.f, 0.f, 0.f);
	const FVector To(400.f, 0.f, 0.f);
	TestFalse(TEXT("an empty world blocks nothing"), Rig.Subsystem->IsSegmentBlocked(From, To));

	AActor* Dynamic = MakeBox(FVector(200.f, 0.f, 0.f), ECC_WorldDynamic);
	TestFalse(TEXT("a WorldDynamic actor (a door) never blocks"), Rig.Subsystem->IsSegmentBlocked(From, To));
	Dynamic->Destroy();

	AActor* PawnBox = MakeBox(FVector(200.f, 0.f, 0.f), ECC_Pawn);
	TestFalse(TEXT("a pawn never blocks"), Rig.Subsystem->IsSegmentBlocked(From, To));
	PawnBox->Destroy();

	MakeBox(FVector(200.f, 0.f, 0.f), ECC_WorldStatic);
	TestTrue(TEXT("static geometry blocks"), Rig.Subsystem->IsSegmentBlocked(From, To));
	return true;
}

GSTN_INTERACT_RT_TEST(SettingsAndRulesAssetAreSane)
{
	const UGSTN_InteractSettings& Settings = UGSTN_InteractSettings::Get();
	FString Error;
	TestTrue(TEXT("the default rules validate after sanitising"), Settings.GetSanitisedDefaultRules().IsValid(&Error));
	TestTrue(TEXT("the settings name a built-in presenter"), Settings.PresenterClasses.Num() > 0);
	TestFalse(TEXT("the interactable component is not replicated by the interactor"), GetDefault<UGSTN_InteractorComponent>()->GetIsReplicated());
	TestTrue(TEXT("the interactable component replicates its stamp"), GetDefault<UGSTN_InteractableComponent>()->GetIsReplicated());
	return true;
}

// ---- M12 G5: the prompt ----

GSTN_INTERACT_RT_TEST(PromptTextIsAPureFunctionOfTheView)
{
	FGSTN_InteractPresentationView View;
	View.Display.Name = FText::FromString(TEXT("Chest"));
	View.Display.TapVerb = FText::FromString(TEXT("Look"));
	View.Display.HoldVerb = FText::FromString(TEXT("Open"));

	FString Text = UGSTN_InteractPresenter_Prompt::FormatPrompt(View).ToString();
	TestTrue(TEXT("the name leads"), Text.StartsWith(TEXT("Chest")));
	TestTrue(TEXT("the tap verb is shown"), Text.Contains(TEXT("[Tap] Look")));
	TestTrue(TEXT("the hold verb is shown"), Text.Contains(TEXT("[Hold] Open")));
	TestFalse(TEXT("no bar while nothing is held"), Text.Contains(TEXT("%")));
	TestFalse(TEXT("an actionable target is not greyed"), Text.Contains(TEXT("unavailable")));

	View.HoldProgress = 0.5f;
	Text = UGSTN_InteractPresenter_Prompt::FormatPrompt(View, 10).ToString();
	TestTrue(TEXT("half a hold fills half the bar"), Text.Contains(TEXT("[#####.....]")));
	TestTrue(TEXT("and shows the percent"), Text.Contains(TEXT("50%")));

	View.Display.TapVerb = FText::GetEmpty();
	View.Display.HoldVerb = FText::GetEmpty();
	View.bUnactionable = true;
	Text = UGSTN_InteractPresenter_Prompt::FormatPrompt(View).ToString();
	TestFalse(TEXT("empty verbs are not drawn"), Text.Contains(TEXT("[Tap]")) || Text.Contains(TEXT("[Hold]")));
	TestTrue(TEXT("a greyed target says so"), Text.Contains(TEXT("(unavailable)")));

	TestEqual(TEXT("bar below zero"), UGSTN_InteractPresenter_Prompt::MakeHoldBar(-1.f, 4), FString(TEXT("[....]")));
	TestEqual(TEXT("bar above one"), UGSTN_InteractPresenter_Prompt::MakeHoldBar(2.f, 4), FString(TEXT("[####]")));
	return true;
}

GSTN_INTERACT_RT_TEST(PawnFocusFeedsThePrompt)
{
	SetMeaning();
	FRig Rig;
	UGSTN_InteractPresentationSubsystem* Presentation = UGSTN_InteractPresentationSubsystem::Get(Rig.World);
	if (!TestNotNull(TEXT("interact and presentation subsystems"), Rig.Subsystem) || !TestNotNull(TEXT("presentation"), Presentation))
	{
		return false;
	}
	AActor* Item = Rig.SpawnActor(FVector(150.f, 0.f, 0.f));
	UGSTN_InteractableComponent* ItemComponent = Rig.AddInteractable(Item);
	ItemComponent->DisplayName = FText::FromString(TEXT("Chest"));
	ItemComponent->HoldVerbOverride = FText::FromString(TEXT("Open"));
	Rig.Subsystem->SetFrameOverride(0);
	ItemComponent->RegisterRecord();

	// A real pawn (not a plain actor): the interactor probes for it and the view is built from its focus
	APawn* Pawn = Rig.World->SpawnActor<APawn>(FVector::ZeroVector, FRotator::ZeroRotator);
	UGSTN_InteractorComponent* Interactor = NewObject<UGSTN_InteractorComponent>(Pawn);
	Interactor->RegisterComponent();
	Presentation->RegisterInteractor(Interactor);

	const int32 Frame = 5000;
	Rig.Subsystem->SetFrameOverride(Frame);
	const FGSTN_InteractFocusResult Focus = Interactor->ProbeAtFrame(Frame);
	TestTrue(TEXT("the pawn's interactor focuses the item in front"), Focus.FocusId == ItemComponent->GetCandidateId());

	FGSTN_InteractPresentationView View;
	if (!TestTrue(TEXT("a view is built for the focus"), Presentation->BuildView(*Interactor, View)))
	{
		return false;
	}
	TestTrue(TEXT("the view targets the item"), View.Target.Get() == Item);
	TestEqual(TEXT("the view carries the item name"), View.Display.Name.ToString(), FString(TEXT("Chest")));
	TestEqual(TEXT("nothing is held yet"), View.HoldProgress, 0.f);

	UGSTN_InteractPresenter_Prompt* Prompt = NewObject<UGSTN_InteractPresenter_Prompt>(GetTransientPackage());
	Prompt->bDrawOnScreen = false;
	Prompt->OnFocusEntered(View);
	TestTrue(TEXT("the prompt shows"), Prompt->IsPromptVisible());
	TestTrue(TEXT("with the name"), Prompt->GetPromptText().ToString().Contains(TEXT("Chest")));
	TestTrue(TEXT("and the hold verb"), Prompt->GetPromptText().ToString().Contains(TEXT("Open")));

	// Hold half the meaning's 30 frames: the view and the prompt carry the progress
	Interactor->SetInteractHeld(true);
	Rig.Subsystem->SetFrameOverride(Frame + 15);
	Interactor->ProbeAtFrame(Frame + 15);
	TestTrue(TEXT("a view is still built"), Presentation->BuildView(*Interactor, View));
	TestTrue(TEXT("half the hold is in the view"), FMath::IsNearlyEqual(View.HoldProgress, 0.5f, 0.05f));
	Prompt->OnFocusUpdated(View, 0.016f);
	TestTrue(TEXT("and in the prompt"), Prompt->GetPromptText().ToString().Contains(TEXT("%")) && Prompt->GetPromptHoldProgress() > 0.4f);

	Prompt->OnFocusLeft(View);
	TestFalse(TEXT("the prompt hides when the focus ends"), Prompt->IsPromptVisible());
	TestTrue(TEXT("and clears its text"), Prompt->GetPromptText().IsEmpty());

	// The target goes away: an id with no object draws nothing
	Item->Destroy();
	Interactor->ProbeAtFrame(Frame + 16);
	TestFalse(TEXT("a destroyed target has no view"), Presentation->BuildView(*Interactor, View));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
