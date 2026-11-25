#include "GSTN_CameraComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "GSTN_CameraMath.h"
#include "GSTN_CameraRuntimeTypes.h"
#include "GSTN_CameraSection.h"
#include "GSTN_CameraBiasVolume.h"
#include "GSTN_CameraProfile.h"
#include "GSTN_CameraTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace GSTNCameraComponentPrivate
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("gstn.Camera.Enable"), 1, TEXT("0 turns the GSTN camera off on this machine (the camera component keeps its own default view)."));
	static TAutoConsoleVariable<int32> CVarDebug(TEXT("gstn.Camera.Debug"), 0, TEXT("1 draws the pivot and prints the active mode, section and stack of every GSTN camera."));
	// Playtest switches (listed in the GSTN Debug Options window). They set the player option when the value changes (a debug aid).
	static TAutoConsoleVariable<int32> CVarAutoFollow(TEXT("gstn.Camera.AutoFollow"), 0, TEXT("1 turns the gentle auto follow (camera turns behind the runner) on, 0 off. Default off (owner 2026-11-25: it ruins aiming precision)."));
	static TAutoConsoleVariable<int32> CVarAssist(TEXT("gstn.Camera.Assist"), 0, TEXT("1 turns Camera Assist on: hold recenter and the move stick turns the camera."));
	static TAutoConsoleVariable<int32> CVarDistance(TEXT("gstn.Camera.Distance"), 1, TEXT("Camera distance preset: 0 = near, 1 = medium, 2 = far."));
	static TAutoConsoleVariable<float> CVarZoom(TEXT("gstn.Camera.Zoom"), 1.f, TEXT("Camera zoom (debug panel), a scale of the base distance: 1 = 580 cm, the PSO2 default; 0.8 closer, 1.3 farther (range 0.6-1.6). Sets the player's Custom distance when changed."));
	static TAutoConsoleVariable<int32> CVarLookLog(TEXT("gstn.Camera.LookLog"), 0, TEXT("1 = one CSV row per camera frame in Saved/Logs/gstn_camera_look_<pawn>.csv: control rotation before the camera, look delta, orbit pitch and its clamp, view pitch, written control rotation. Diagnostics for the P1a pitch lock (2026-11-21)."));
	static TAutoConsoleVariable<int32> CVarOcclusion(TEXT("gstn.Camera.Occlusion"), 1, TEXT("0 turns the occlusion probe (fade and pull-in) off."));

	/** True when the entry of this definition belongs to a section */
	static bool IsSectionDefinition(const UGSTN_CameraModeDefinition* Definition)
	{
		return Definition && Definition->ModeTag.MatchesTagExact(GSTNCameraTags::Mode_Section.GetTag());
	}
}

using namespace GSTNCameraComponentPrivate;

UGSTN_CameraComponent::UGSTN_CameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After movement: the camera reads the PRESENTED state (camera.md §2.2), finalized and smoothed by Network Prediction before the actor ticks.
	// TG_PostPhysics, not TG_PostUpdateWork: UWorld::Tick updates the camera managers (GetCameraView) between the two, so a later tick would
	// hand every frame the previous frame's view (2026-11-20, the camera anchor's jitter). A subject that needs its own update first is a
	// tick prerequisite of this component
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	// Ticks only once activated (Activate enables the tick), so an Auto Activate off component costs nothing
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Nothing about the camera replicates, ever (camera.md §1)
	SetIsReplicatedByDefault(false);
	bUsePawnControlRotation = false;
}

bool UGSTN_CameraComponent::ShouldRunFor(ENetMode NetMode, bool bGameWorld, bool bEnabled)
{
	return bEnabled && bGameWorld && NetMode != NM_DedicatedServer;
}

void UGSTN_CameraComponent::BeginPlay()
{
	Super::BeginPlay();
	const UWorld* World = GetWorld();
	if (!World || !ShouldRunFor(World->GetNetMode(), World->IsGameWorld(), true))
	{
		SetComponentTickEnabled(false);
	}
}

void UGSTN_CameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseFades();
	WriteLookLog(FGSTN_CameraSubjectState(), nullptr, true);
	Super::EndPlay(EndPlayReason);
}

void UGSTN_CameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// An inactive camera (Auto Activate off) must never drive the view or write the control rotation: it would fight the
	// pawn's own camera every frame
	if (!IsActive())
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && !Pawn->IsLocallyControlled())
	{
		return;
	}
	EvaluateCamera(DeltaTime);
}

FGSTN_CameraPlayerState& UGSTN_CameraComponent::State()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		if (UGSTN_CameraLocalPlayerSubsystem* Subsystem = UGSTN_CameraLocalPlayerSubsystem::Get(Controller))
		{
			return Subsystem->GetState();
		}
	}
	return FallbackState;
}

FGSTN_CameraOrbitState UGSTN_CameraComponent::GetOrbit()
{
	return State().Orbit;
}

FGSTN_CameraFrame UGSTN_CameraComponent::GetFrame()
{
	return State().Frame;
}

void UGSTN_CameraComponent::AddLookInput(FVector2D Delta)
{
	PendingLook += Delta;
}

void UGSTN_CameraComponent::RequestRecenter()
{
	bRecentering = true;
	RecenterRate = -1.f;
}

void UGSTN_CameraComponent::SetOpponent(AActor* Opponent)
{
	OpponentActors.Reset();
	if (Opponent)
	{
		OpponentActors.Add(Opponent);
	}
}

void UGSTN_CameraComponent::SetOpponents(const TArray<AActor*>& Opponents)
{
	OpponentActors.Reset();
	for (AActor* Opponent : Opponents)
	{
		if (Opponent && Opponent != GetOwner())
		{
			OpponentActors.AddUnique(Opponent);
		}
	}
}

EGSTN_ShoulderSide UGSTN_CameraComponent::GetShoulderSide() const
{
	return bHasShoulderSideOverride ? ShoulderSideOverride : GetProfile().Shoulder.ShoulderSide;
}

void UGSTN_CameraComponent::SetShoulderSide(EGSTN_ShoulderSide Side)
{
	bHasShoulderSideOverride = true;
	ShoulderSideOverride = Side;
}

void UGSTN_CameraComponent::ToggleShoulderSide()
{
	SetShoulderSide(GetShoulderSide() == EGSTN_ShoulderSide::Right ? EGSTN_ShoulderSide::Left : EGSTN_ShoulderSide::Right);
}

void UGSTN_CameraComponent::SetSubjectOverride(const FGSTN_CameraSubjectState& InState)
{
	SubjectOverride = InState;
	SubjectOverride.bValid = true;
	bHasSubjectOverride = true;
}

void UGSTN_CameraComponent::ClearSubjectOverride()
{
	bHasSubjectOverride = false;
}

void UGSTN_CameraComponent::Cut()
{
	bPendingCut = true;
}

bool UGSTN_CameraComponent::ReadSubject(FGSTN_CameraSubjectState& Out) const
{
	if (bHasSubjectOverride)
	{
		Out = SubjectOverride;
		return true;
	}
	return FGSTN_CameraAdapterRegistry::Read(GetOwner(), Out);
}

UGSTN_CameraMode* UGSTN_CameraComponent::GetTopModeInstance() const
{
	const TObjectPtr<UGSTN_CameraMode>* Found = SlotInstances.Find(Stack.TopKey());
	return Found ? Found->Get() : nullptr;
}

UGSTN_CameraModeDefinition* UGSTN_CameraComponent::GetTopDefinition() const
{
	const TObjectPtr<UGSTN_CameraModeDefinition>* Found = SlotDefinitions.Find(Stack.TopKey());
	return Found ? Found->Get() : nullptr;
}

bool UGSTN_CameraComponent::IsFadeable(const UPrimitiveComponent* Component)
{
	if (!Component)
	{
		return false;
	}
	return IsFadeableWithTag(Component, UGSTN_CameraProfile::GetDefaultProfile().FadeTag);
}

bool UGSTN_CameraComponent::IsFadeableWithTag(const UPrimitiveComponent* Component, FName Tag)
{
	if (!Component)
	{
		return false;
	}
	return Component->ComponentHasTag(Tag) || (Component->GetOwner() && Component->GetOwner()->ActorHasTag(Tag));
}

// ---- Definitions and rules ----

UGSTN_CameraModeDefinition* UGSTN_CameraComponent::GetBuiltinDefinition(const FGameplayTag& Tag)
{
	if (TObjectPtr<UGSTN_CameraModeDefinition>* Existing = BuiltinDefinitions.Find(Tag))
	{
		return Existing->Get();
	}
	const UGSTN_CameraProfile& Settings = GetProfile();
	UGSTN_CameraModeDefinition* Definition = NewObject<UGSTN_CameraModeDefinition>(this);
	Definition->ModeTag = Tag;
	Definition->Occlusion = Settings.Occlusion;
	Definition->BlendIn = Settings.FollowBlend;

	if (Tag.MatchesTagExact(GSTNCameraTags::Mode_Follow.GetTag()))
	{
		UGSTN_CameraMode_Follow* Mode = NewObject<UGSTN_CameraMode_Follow>(Definition);
		Mode->Tuning = Settings.Follow;
		Definition->Mode = Mode;
	}
	else if (Tag.MatchesTagExact(GSTNCameraTags::Mode_Free.GetTag()))
	{
		UGSTN_CameraMode_Free* Mode = NewObject<UGSTN_CameraMode_Free>(Definition);
		Mode->Tuning = Settings.FreeView;
		Mode->FreeTuning = Settings.Free;
		Mode->PitchCurves = Settings.FreePitchCurves;
		Definition->Mode = Mode;
		Definition->bAllowAutoFollow = true;
		Definition->bAllowFramingVolumes = true;
	}
	else if (Tag.MatchesTagExact(GSTNCameraTags::Mode_Shoulder.GetTag()))
	{
		UGSTN_CameraMode_Shoulder* Mode = NewObject<UGSTN_CameraMode_Shoulder>(Definition);
		Mode->Tuning = Settings.Shoulder;
		Definition->Mode = Mode;
		Definition->BlendIn = Settings.ShoulderBlend;
		Definition->bLookIsAim = true;
		Definition->LookSensitivityScale = Settings.Shoulder.AimSensitivityScale;
		Definition->bShowCrosshair = true;
		Definition->bConvergeAim = Settings.Shoulder.bConvergeAim;
		Definition->Occlusion = Settings.ShoulderOcclusion;
	}
	else if (Tag.MatchesTagExact(GSTNCameraTags::Mode_Section.GetTag()))
	{
		Definition->Mode = NewObject<UGSTN_CameraMode_Section>(Definition);
	}
	else if (Tag.MatchesTagExact(GSTNCameraTags::Mode_Duel.GetTag()))
	{
		UGSTN_CameraMode_Duel* Mode = NewObject<UGSTN_CameraMode_Duel>(Definition);
		Mode->Tuning = Settings.Duel;
		Definition->Mode = Mode;
		Definition->BlendIn = Settings.DuelBlend;
		Definition->bAllowManualControl = false;
	}
	else if (Tag.MatchesTagExact(GSTNCameraTags::Mode_Arena.GetTag()))
	{
		UGSTN_CameraMode_Arena* Mode = NewObject<UGSTN_CameraMode_Arena>(Definition);
		Mode->Tuning = Settings.Arena;
		Definition->Mode = Mode;
		Definition->BlendIn = Settings.ArenaBlend;
		Definition->bAllowManualControl = false;
	}
	else
	{
		// Not a built-in: the caller falls back to Follow
		return nullptr;
	}
	BuiltinDefinitions.Add(Tag, Definition);
	return Definition;
}

UGSTN_CameraModeDefinition* UGSTN_CameraComponent::ResolveDefinition(const FGSTN_CameraModeRule& Rule)
{
	UGSTN_CameraModeDefinition* Definition = nullptr;
	if (Rule.bModeFromSection)
	{
		const AGSTN_CameraSection* Section = ActiveSection.Get();
		const FGameplayTag Wanted = (Section && Section->Spec.ModeTag.IsValid()) ? Section->Spec.ModeTag : GSTNCameraTags::Mode_Section.GetTag();
		Definition = GetBuiltinDefinition(Wanted);
		if (!Definition)
		{
			Definition = GetBuiltinDefinition(GSTNCameraTags::Mode_Section.GetTag());
		}
	}
	else if (!Rule.Mode.IsNull())
	{
		UGSTN_CameraModeDefinition* Loaded = Rule.Mode.LoadSynchronous();
		if (Loaded && Loaded->IsValidDefinition())
		{
			Definition = Loaded;
		}
	}
	else if (Rule.ModeTag.IsValid())
	{
		Definition = GetBuiltinDefinition(Rule.ModeTag);
	}
	return Definition ? Definition : GetBuiltinDefinition(GSTNCameraTags::Mode_Free.GetTag());
}

int32 UGSTN_CameraComponent::KeyFor(UGSTN_CameraModeDefinition* Definition, AGSTN_CameraSection* Section)
{
	const FEntryKey Lookup(Definition, Section);
	if (const int32* Found = Keys.Find(Lookup))
	{
		return *Found;
	}
	const int32 Key = NextKey++;
	Keys.Add(Lookup, Key);
	return Key;
}

UGSTN_CameraRuleSet* UGSTN_CameraComponent::ChooseRuleSet()
{
	const UGSTN_CameraProfile& Settings = GetProfile();
	if (const AGSTN_CameraSection* Section = ActiveSection.Get())
	{
		if (!Section->Spec.RuleSetOverride.IsNull())
		{
			if (UGSTN_CameraRuleSet* Override = Section->Spec.RuleSetOverride.LoadSynchronous())
			{
				return Override;
			}
		}
	}
	if (!RuleSetOverride.IsNull())
	{
		if (UGSTN_CameraRuleSet* Pawn = RuleSetOverride.LoadSynchronous())
		{
			return Pawn;
		}
	}
	if (!Settings.DefaultRuleSet.IsNull())
	{
		if (UGSTN_CameraRuleSet* Project = Settings.DefaultRuleSet.LoadSynchronous())
		{
			return Project;
		}
	}
	if (!BuiltinRuleSet)
	{
		BuiltinRuleSet = UGSTN_CameraRuleSet::MakeDefault(this, Settings.DefaultModeTag, Settings.Stability.ExitDelaySeconds, Settings.Stability.MinDwellSeconds);
	}
	return BuiltinRuleSet;
}

void UGSTN_CameraComponent::DropSlots(const TArray<int32>& DroppedKeys)
{
	for (const int32 Key : DroppedKeys)
	{
		SlotInstances.Remove(Key);
		SlotDefinitions.Remove(Key);
		LastViews.Remove(Key);
		SlotSections.Remove(Key);
		for (auto It = Keys.CreateIterator(); It; ++It)
		{
			if (It.Value() == Key)
			{
				It.RemoveCurrent();
			}
		}
	}
}

// ---- Sections ----

void UGSTN_CameraComponent::UpdateSection(const FGSTN_CameraSubjectState& Subject)
{
	TArray<AGSTN_CameraSection*> Inside;
	if (const UGSTN_CameraSectionSubsystem* Sections = UGSTN_CameraSectionSubsystem::Get(GetWorld()))
	{
		Sections->GatherContaining(Subject.Pivot, Inside);
	}

	// When the target entered each section it is in, for the equal-priority tie
	for (AGSTN_CameraSection* Section : Inside)
	{
		if (!EnterTimes.Contains(Section))
		{
			EnterTimes.Add(Section, Clock);
		}
	}
	for (auto It = EnterTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !Inside.Contains(It.Key().Get()))
		{
			It.RemoveCurrent();
		}
	}

	TArray<FGSTN_CameraSectionCandidate> Candidates;
	for (int32 Index = 0; Index < Inside.Num(); ++Index)
	{
		FGSTN_CameraSectionCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Id = Index;
		Candidate.Priority = Inside[Index]->Spec.Priority;
		Candidate.EnterTime = EnterTimes.FindRef(Inside[Index]);
		Candidate.VolumeSize = Inside[Index]->GetVolumeSize();
	}
	const int32 Chosen = FGSTN_CameraMath::ChooseSection(Candidates);
	AGSTN_CameraSection* NewSection = Chosen != INDEX_NONE ? Inside[Chosen] : nullptr;
	AGSTN_CameraSection* OldSection = ActiveSection.Get();
	if (NewSection == OldSection)
	{
		return;
	}

	FGSTN_CameraSectionEvent Event;
	Event.ViewTarget = GetOwner();
	Event.Camera = this;
	if (OldSection)
	{
		Event.Section = OldSection;
		LastSectionBlendOut = OldSection->Spec.BlendOutSeconds;
		LastSectionBlendFunction = OldSection->Spec.BlendFunction;
		for (UGSTN_CameraSectionAction* Action : OldSection->Spec.Actions)
		{
			if (Action)
			{
				Action->OnSectionExited(Event);
			}
		}
	}
	ActiveSection = NewSection;
	if (NewSection)
	{
		Event.Section = NewSection;
		for (UGSTN_CameraSectionAction* Action : NewSection->Spec.Actions)
		{
			if (Action)
			{
				Action->OnSectionEntered(Event);
			}
		}
	}
}

void UGSTN_CameraComponent::RaiseFrameChange(const FVector& From, const FVector& To, EGSTN_CameraFrameChangeReason Reason, bool bCut, float MinDeltaDeg)
{
	FGSTN_CameraPlayerState& Player = State();
	if (!FGSTN_CameraMath::ShouldRaiseFrameChange(Player.Frame.Up, From, To, MinDeltaDeg, false))
	{
		return;
	}
	// The camera turned the reference: the move stick keeps the old one until it returns to the centre
	FGSTN_CameraMath::LatchStickReference(Player.StickLatch, GetProfile().StickLatch, FGSTN_CameraMath::DirectionToHeading(Player.Frame, From));
	FGSTN_CameraFrameChange Change;
	Change.FromForward = From;
	Change.ToForward = To;
	Change.Reason = Reason;
	Change.bCut = bCut;
	OnReferenceFrameChanged.Broadcast(Change);
	OnReferenceFrameChangedNative.Broadcast(Change);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		if (UGSTN_CameraLocalPlayerSubsystem* Subsystem = UGSTN_CameraLocalPlayerSubsystem::Get(Controller))
		{
			Subsystem->OnReferenceFrameChanged.Broadcast(Change);
		}
	}
}

// ---- Manual control ----

void UGSTN_CameraComponent::ApplyLook(const FGSTN_CameraSubjectState& Subject, FGSTN_CameraPlayerState& Player, float DeltaSeconds)
{
	const UGSTN_CameraProfile& Settings = GetProfile();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	FGSTN_CameraUserOptions& UserOptions = Options();
	// A cvar acts when it CHANGES (the player's own menu choice is not overwritten every frame)
	static int32 LastAutoFollow = 0, LastAssist = 0, LastDistance = 1;
	static float LastZoom = 1.f;
	const int32 NowAutoFollow = CVarAutoFollow.GetValueOnGameThread(), NowAssist = CVarAssist.GetValueOnGameThread(), NowDistance = FMath::Clamp(CVarDistance.GetValueOnGameThread(), 0, 2);
	const float NowZoom = CVarZoom.GetValueOnGameThread();
	if (NowAutoFollow != LastAutoFollow) { LastAutoFollow = NowAutoFollow; UserOptions.bAutoFollow = NowAutoFollow != 0; }
	if (NowAssist != LastAssist) { LastAssist = NowAssist; UserOptions.bCameraAssist = NowAssist != 0; }
	if (NowDistance != LastDistance) { LastDistance = NowDistance; UserOptions.DistancePreset = static_cast<EGSTN_CameraDistancePreset>(NowDistance); }
	if (NowZoom != LastZoom)
	{
		LastZoom = NowZoom;
		if (NowZoom > 0.f)
		{
			UserOptions.DistancePreset = EGSTN_CameraDistancePreset::Custom;
			UserOptions.CustomDistanceScale = FGSTN_CameraMath::StepZoomScale(Settings.DistancePresets, NowZoom, 0.f);
		}
	}

	UGSTN_CameraLocalPlayerSubsystem* Subsystem = UGSTN_CameraLocalPlayerSubsystem::Get(Controller);
	if (Subsystem)
	{
		Subsystem->EnsureInputContext(Controller, Settings);
	}

	// The Enhanced Input actions are polled, so the plugin needs no input component of its own
	bool bRecenterDown = false;
	FVector2D AssistMove = FVector2D::ZeroVector;
	// Only the look action depends on the look source: with ControlRotation (the playtest pawn, GASP's look turns the controller) the buttons
	// below (recenter, distance, zoom...) are still the plugin's own. Before 2026-11-25 they were polled only with the Plugin source, so on the
	// playtest pawn no distance input ever reached the camera
	if (Controller && Controller->IsLocalController())
	{
		if (const UEnhancedPlayerInput* Input = Cast<UEnhancedPlayerInput>(Controller->PlayerInput))
		{
			if (Settings.LookInputSource == EGSTN_CameraLookInputSource::Plugin && !Settings.LookAction.IsNull())
			{
				if (const UInputAction* Look = Settings.LookAction.LoadSynchronous())
				{
					PendingLook += Input->GetActionValue(Look).Get<FVector2D>();
				}
			}
			if (!Settings.SwapShoulderAction.IsNull())
			{
				if (const UInputAction* Swap = Settings.SwapShoulderAction.LoadSynchronous())
				{
					const bool bDown = Input->GetActionValue(Swap).Get<bool>();
					if (bDown && !bSwapShoulderWasDown)
					{
						ToggleShoulderSide();
					}
					bSwapShoulderWasDown = bDown;
				}
			}
			if (!Settings.RecenterAction.IsNull())
			{
				if (const UInputAction* Recenter = Settings.RecenterAction.LoadSynchronous())
				{
					bRecenterDown = Input->GetActionValue(Recenter).Get<bool>();
				}
			}
			if (!Settings.DistanceAction.IsNull())
			{
				if (const UInputAction* Distance = Settings.DistanceAction.LoadSynchronous())
				{
					const float Value = Input->GetActionValue(Distance).Get<float>();
					const int32 Direction = Value > 0.5f ? 1 : (Value < -0.5f ? -1 : 0);
					if (Direction != 0 && Direction != LastDistanceDirection)
					{
						CycleDistancePreset(Direction);
					}
					LastDistanceDirection = Direction;
				}
			}
			if (!Settings.ZoomAction.IsNull())
			{
				if (const UInputAction* Zoom = Settings.ZoomAction.LoadSynchronous())
				{
					PendingZoom += Input->GetActionValue(Zoom).Get<float>();
				}
			}
			if (!Settings.AssistMoveAction.IsNull())
			{
				if (const UInputAction* Move = Settings.AssistMoveAction.LoadSynchronous())
				{
					AssistMove = Input->GetActionValue(Move).Get<FVector2D>();
				}
			}
		}
	}

	// The project feeds the controller (GASP's look adds to the control rotation): only the change since this camera's own last write is input
	LookRow = FLookLogRow();
	if (Controller)
	{
		LookRow.ControlBefore = Controller->GetControlRotation();
		LookRow.ControlLeft = LeftControlRotation;
	}
	if (Settings.LookInputSource == EGSTN_CameraLookInputSource::ControlRotation && Controller && Controller->IsLocalController())
	{
		const FRotator Now = Controller->GetControlRotation();
		const bool bContinuous = bHasLeftControlRotation && GFrameCounter == LeftControlRotationFrame + 1;
		const float Unit = FMath::Max(UE_SMALL_NUMBER, Settings.LookDegreesPerUnit);
		const FVector2D Units = FGSTN_CameraMath::ControlRotationLookDelta(Now, LeftControlRotation, bContinuous, Unit);
		PendingLook += Units;
		LookRow.LookDeltaDeg = Units * Unit;
		LookRow.bContinuous = bContinuous;
	}

	// Recenter button: a tap recenters; with Camera Assist on, holding it lets the move stick turn the camera (Astro Bot accessibility)
	if (bRecenterDown)
	{
		RecenterHeldSeconds += DeltaSeconds;
		if (!UserOptions.bCameraAssist && !bRecenterWasDown)
		{
			RequestRecenter();
		}
	}
	else
	{
		if (UserOptions.bCameraAssist && bRecenterWasDown && RecenterHeldSeconds < Settings.CameraAssist.HoldSeconds)
		{
			RequestRecenter();
		}
		RecenterHeldSeconds = 0.f;
	}
	bRecenterWasDown = bRecenterDown;
	bAssistActive = UserOptions.bCameraAssist && bRecenterDown && RecenterHeldSeconds >= Settings.CameraAssist.HoldSeconds;

	FVector2D Look = PendingLook;
	PendingLook = FVector2D::ZeroVector;
	const float LookUnit = FMath::Max(UE_SMALL_NUMBER, Settings.LookDegreesPerUnit);
	if (bAssistActive && !AssistMove.IsNearlyZero())
	{
		// In look units, so the sensitivity options apply the same way
		Look += AssistMove * (Settings.CameraAssist.DegPerSecond * DeltaSeconds / LookUnit);
	}
	const bool bHasInput = !Look.IsNearlyZero();

	const UGSTN_CameraModeDefinition* Definition = GetTopDefinition();
	const UGSTN_CameraMode* Mode = GetTopModeInstance();
	const TWeakObjectPtr<AGSTN_CameraSection>* SectionPtr = SlotSections.Find(Stack.TopKey());
	const AGSTN_CameraSection* Section = SectionPtr ? SectionPtr->Get() : nullptr;
	const bool bAim = Definition && Definition->bLookIsAim;

	// Manual control: the mode allows it and, in a section, so does the section (Shoulder aims regardless)
	const bool bAllowed = Definition && Definition->bAllowManualControl && (!Section || Section->Spec.bAllowManualControl || bAim);

	FGSTN_CameraOrbitState& Orbit = Player.Orbit;
	if (!bAllowed)
	{
		// Consumed: the plugin takes the input and the view stays where the mode or the designer puts it
		Player.SecondsSinceLookInput += DeltaSeconds;
	}
	else
	{
		if (bHasInput)
		{
			bRecentering = false;
			Player.SecondsSinceLookInput = 0.f;
			const float LookScale = Settings.LookDegreesPerUnit * UserOptions.LookSensitivityMultiplier * Definition->LookSensitivityScale;
			Orbit.YawDeg = FMath::UnwindDegrees(Orbit.YawDeg + Look.X * LookScale * (UserOptions.bInvertLookX ? -1.f : 1.f));
			Orbit.PitchDeg += Look.Y * LookScale * (UserOptions.bInvertLookY ? -1.f : 1.f);
		}
		else
		{
			Player.SecondsSinceLookInput += DeltaSeconds;
		}
	}

	// Soft framing volumes and the gentle follow behind: free areas only (a section is the designer's hard framing and wins), never in aim
	const bool bFreeArea = !Section && !bAim && Definition && bAllowed;
	UpdateFraming(Subject, Player, Definition && Definition->bAllowFramingVolumes && bFreeArea, DeltaSeconds);
	bAutoFollowing = false;
	if (bFreeArea && !bRecentering)
	{
		FGSTN_CameraMath::StepBiasOrbit(Orbit, LastBias, Player.SecondsSinceLookInput, DeltaSeconds);

		bool bBlocked = !Definition->bAllowAutoFollow || !UserOptions.bAutoFollow || bAssistActive
			|| FrameTags.HasTag(GSTNCameraTags::Request_Aim) || FrameTags.HasTag(GSTNCameraTags::State_OneOpponent) || FrameTags.HasTag(GSTNCameraTags::State_ManyOpponents)
			|| FrameTags.HasAny(Settings.AutoFollowBlockTags);
		const FVector Flat = FGSTN_CameraMath::ProjectOnUpPlane(Subject.Velocity, Player.Frame.Up);
		const float Speed = static_cast<float>((Subject.Velocity - Player.Frame.Up * FVector::DotProduct(Subject.Velocity, Player.Frame.Up)).Size());
		const float MoveHeading = Flat.IsNearlyZero() ? Orbit.YawDeg : FGSTN_CameraMath::DirectionToHeading(Player.Frame, Flat);
		const float Scale = bBlocked ? 0.f : FMath::Clamp(1.f - LastBias.YawWeight, 0.f, 1.f);
		const float Before = Orbit.YawDeg;
		Orbit.YawDeg = FGSTN_CameraMath::StepAutoFollow(Player.AutoFollow, Settings.AutoFollow, Orbit.YawDeg, MoveHeading, Speed, Subject.bGrounded,
			Player.SecondsSinceLookInput, Scale, DeltaSeconds);
		bAutoFollowing = !FMath::IsNearlyEqual(Before, Orbit.YawDeg, 1.0e-4f);
	}
	else
	{
		Player.AutoFollow.RateDegPerSecond = 0.f;
	}

	if (bAllowed)
	{
		float MinPitch = -80.f;
		float MaxPitch = 80.f;
		if (Mode)
		{
			Mode->GetPitchRange(MinPitch, MaxPitch);
		}
		Orbit.PitchDeg = FMath::Clamp(Orbit.PitchDeg, MinPitch, MaxPitch);
		LookRow.MinPitchDeg = MinPitch;
		LookRow.MaxPitchDeg = MaxPitch;
	}
	LookRow.OrbitPitchDeg = Orbit.PitchDeg;
	LookRow.OrbitYawDeg = Orbit.YawDeg;

	// Free zoom (2026-11-25): the input moves the Custom distance from wherever the player is now (a preset included). Not while aiming: the
	// shoulder camera keeps its own arm and the input is dropped
	const float ZoomInput = PendingZoom;
	PendingZoom = 0.f;
	const bool bZooming = !FMath::IsNearlyZero(ZoomInput) && !bAim;
	if (bZooming)
	{
		const float From = FGSTN_CameraMath::DistancePresetScale(Settings.DistancePresets, UserOptions.DistancePreset, UserOptions.CustomDistanceScale);
		UserOptions.CustomDistanceScale = FGSTN_CameraMath::StepZoomScale(Settings.DistancePresets, From, ZoomInput);
		UserOptions.DistancePreset = EGSTN_CameraDistancePreset::Custom;
	}

	// The player distance: the preset of the options, limited by the section, eased (never a snap) unless the zoom input moved it this frame
	float DistanceTarget = FGSTN_CameraMath::DistancePresetScale(Settings.DistancePresets, UserOptions.DistancePreset, UserOptions.CustomDistanceScale);

	if (Section)
	{
		const FGSTN_CameraSectionSpec& Spec = Section->Spec;
		const float DesiredYaw = Section->GetBaseHeading(Player.Frame) + Spec.DesiredYawDeg;
		if (bAim)
		{
			// Aiming inside a section: the shoulder limits apply, not the section ones
		}
		else if (Spec.bAllowManualControl)
		{
			const float Idle = (Spec.bOnlyWhileMoving && Subject.Velocity.SizeSquared() < 100.0) ? 0.f : Player.SecondsSinceLookInput;
			FGSTN_CameraMath::StepRecenter(Orbit, DesiredYaw, Spec.DesiredPitchDeg, Idle, Spec.RecenterDelaySeconds, Spec.RecenterDegPerSecond, DeltaSeconds);
			Orbit = FGSTN_CameraMath::ClampOrbitAroundDesired(Orbit, DesiredYaw, Spec.DesiredPitchDeg, Spec.Limits);
			const float Limit = FMath::Max(1.f, Spec.Limits.DistanceScaleLimit);
			DistanceTarget = FMath::Clamp(DistanceTarget, 1.f / Limit, Limit);
		}
		else
		{
			Orbit.YawDeg = DesiredYaw;
			Orbit.PitchDeg = Spec.DesiredPitchDeg;
			DistanceTarget = 1.f;
		}
	}
	else if (bRecentering && Definition)
	{
		// The recenter action: ease the heading behind the facing of the subject (camera.md §2.4)
		if (RecenterRate < 0.f)
		{
			RecenterTargetYaw = FGSTN_CameraMath::DirectionToHeading(Player.Frame, Subject.Facing);
			const float Duration = FMath::Max(0.01f, Settings.Free.RecenterSeconds);
			RecenterRate = FMath::Abs(FGSTN_CameraMath::ShortestAngleDelta(Orbit.YawDeg, RecenterTargetYaw)) / Duration;
		}
		Orbit.YawDeg = FGSTN_CameraMath::StepAngleToward(Orbit.YawDeg, RecenterTargetYaw, RecenterRate * DeltaSeconds);
		if (FMath::IsNearlyZero(FGSTN_CameraMath::ShortestAngleDelta(Orbit.YawDeg, RecenterTargetYaw), 0.01f))
		{
			bRecentering = false;
		}
	}
	if (bAim)
	{
		// The shoulder camera has its own arm: keep the player distance where it is
		DistanceTarget = Player.Distance.bInit ? Player.Distance.To : DistanceTarget;
	}
	if (bZooming && Player.Distance.bInit)
	{
		// The zoom input is already a smooth motion (a wheel notch is a small multiplicative step): follow it directly. Restarting the
		// ease-in-out every frame of a held zoom would keep it at the slow start of the curve and the zoom would barely move
		Player.Distance.Value = Player.Distance.From = Player.Distance.To = DistanceTarget;
		Player.Distance.Elapsed = 0.f;
	}
	Orbit.DistanceScale = FGSTN_CameraMath::StepEasedFloat(Player.Distance, DistanceTarget, Settings.DistancePresets.ChangeSeconds, DeltaSeconds);
}

void UGSTN_CameraComponent::UpdateFraming(const FGSTN_CameraSubjectState& Subject, const FGSTN_CameraPlayerState& Player, bool bActive, float DeltaSeconds)
{
	TArray<AGSTN_CameraBiasVolume*> Volumes;
	if (const UGSTN_CameraSectionSubsystem* Registry = UGSTN_CameraSectionSubsystem::Get(GetWorld()))
	{
		Registry->GetBiasVolumes(Volumes);
	}
	const FGSTN_CameraFrame& Frame = Player.Frame;
	const float Speed = static_cast<float>((Subject.Velocity - Frame.Up * FVector::DotProduct(Subject.Velocity, Frame.Up)).Size());
	TArray<FGSTN_CameraBiasInput> Inputs;
	for (AGSTN_CameraBiasVolume* Volume : Volumes)
	{
		const FGSTN_CameraBiasSpec& Spec = Volume->Spec;
		const float Raw = (bActive && Spec.bEnabled) ? Spec.Weight * Volume->GetSpatialWeight(Subject.Pivot) : 0.f;
		float& Weight = BiasWeights.FindOrAdd(Volume);
		const float Seconds = Raw > Weight ? Spec.BlendInSeconds : Spec.BlendOutSeconds;
		Weight = Seconds <= UE_SMALL_NUMBER ? Raw : Weight + (Raw - Weight) * static_cast<float>(1.0 - FMath::Exp(-DeltaSeconds / Seconds));
		if (Weight < 1.0e-3f && Raw <= 0.f)
		{
			Weight = 0.f;
			BiasFocus.Remove(Volume);
			continue;
		}

		// The focus, smoothed when it moves (an enemy, a platform)
		const FVector FocusTarget = Volume->GetFocusLocation();
		FVector& Focus = BiasFocus.FindOrAdd(Volume, FocusTarget);
		Focus = Spec.FocusDampingSeconds <= UE_SMALL_NUMBER ? FocusTarget : Focus + (FocusTarget - Focus) * (1.0 - FMath::Exp(-DeltaSeconds / Spec.FocusDampingSeconds));

		FGSTN_CameraBiasInput& In = Inputs.AddDefaulted_GetRef();
		In.Priority = Spec.Priority;
		In.Weight = Weight;
		In.bBiasYaw = Spec.bBiasYaw;
		if (Spec.YawSource == EGSTN_CameraBiasYawSource::TowardFocus)
		{
			const FVector ToFocus = FGSTN_CameraMath::ProjectOnUpPlane(Focus - Subject.Pivot, Frame.Up);
			In.HeadingDeg = (ToFocus.IsNearlyZero() ? Player.Orbit.YawDeg : FGSTN_CameraMath::DirectionToHeading(Frame, ToFocus)) + Spec.YawOffsetDeg;
		}
		else
		{
			In.HeadingDeg = Volume->GetBaseHeading(Frame) + Spec.YawOffsetDeg;
		}
		In.bBiasPitch = Spec.bBiasPitch;
		In.PitchDeg = Spec.PitchDeg;
		In.DistanceScale = Spec.DistanceScale;
		In.FovOffsetDeg = Spec.FovOffsetDeg;
		if (Spec.FramePivotFraction > 0.f)
		{
			In.PivotOffset = ((Focus - Subject.Pivot) * Spec.FramePivotFraction).GetClampedToMaxSize(Spec.MaxPivotShiftCm);
		}
		In.Strength = Spec.Strength;
		In.FirmRangeDeg = Spec.FirmRangeDeg;
		In.ResumeDelaySeconds = Spec.ResumeDelaySeconds;
		In.PullSeconds = Spec.PullSeconds;
		In.MaxPullDegPerSecond = Spec.MaxPullDegPerSecond;
		const bool bEngaged = !Spec.bBiasYaw || FMath::Abs(FGSTN_CameraMath::ShortestAngleDelta(Player.Orbit.YawDeg, In.HeadingDeg)) <= Spec.MaxEngageAngleDeg;
		In.bPullSuppressed = (Spec.bSuppressInAir && !Subject.bGrounded) || (Spec.bOnlyWhileMoving && Speed < 50.f) || !bEngaged;
	}
	for (auto It = BiasWeights.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	LastBias = FGSTN_CameraMath::CombineBias(Inputs);
	// The view terms come from the smoothed weights already, so they blend in and out with them
	FramingDistanceScale = LastBias.DistanceScale;
	FramingFovOffsetDeg = LastBias.FovOffsetDeg;
	FramingPivotOffset = LastBias.PivotOffset;
}

// ---- Occlusion ----

void UGSTN_CameraComponent::ReleaseFades()
{
	for (FFade& Fade : Fades)
	{
		if (UPrimitiveComponent* Component = Fade.Component.Get())
		{
			Component->SetCustomPrimitiveDataFloat(GetProfile().FadeCustomDataIndex, 1.f);
		}
	}
	Fades.Reset();
}

void UGSTN_CameraComponent::ResolveOcclusion(const FGSTN_CameraView& View, const FGSTN_CameraOcclusionTuning& Tuning, float DeltaSeconds, FGSTN_CameraPose& InOutPose)
{
	const UGSTN_CameraProfile& Settings = GetProfile();
	UWorld* World = GetWorld();
	for (FFade& Fade : Fades)
	{
		Fade.bBlockingNow = false;
	}

	float TargetFraction = 1.f;
	if (World && CVarOcclusion.GetValueOnGameThread() != 0 && View.ArmCm > UE_SMALL_NUMBER)
	{
		// The pivot the camera orbits, with the shoulder offsets: the probe runs from it to the camera
		const FVector EffectivePivot = InOutPose.Location + InOutPose.Forward * View.ArmCm;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNCameraOcclusion), false, GetOwner());
		for (const TWeakObjectPtr<AActor>& Opponent : OpponentActors)
		{
			if (const AActor* OpponentPtr = Opponent.Get())
			{
				Params.AddIgnoredActor(OpponentPtr);
			}
		}
		for (int32 Iteration = 0; Iteration < 4; ++Iteration)
		{
			FHitResult Hit;
			if (!World->SweepSingleByChannel(Hit, EffectivePivot, InOutPose.Location, FQuat::Identity, Settings.TraceChannel, FCollisionShape::MakeSphere(Tuning.ProbeRadiusCm), Params))
			{
				break;
			}
			UPrimitiveComponent* Blocker = Hit.GetComponent();
			const float FromCamera = static_cast<float>(FVector::Dist(InOutPose.Location, Hit.Location));
			const EGSTN_CameraOcclusionAction Action = FGSTN_CameraMath::DecideOcclusion(Tuning.Policy, IsFadeableWithTag(Blocker, Settings.FadeTag), FromCamera, Tuning.NearClipSafeCm);
			if (Action == EGSTN_CameraOcclusionAction::PullIn)
			{
				TargetFraction = FMath::Clamp(Hit.Time, 0.1f, 1.f);
				break;
			}
			if (Action == EGSTN_CameraOcclusionAction::Fade && Blocker)
			{
				FFade* Existing = Fades.FindByPredicate([Blocker](const FFade& Fade) { return Fade.Component == Blocker; });
				if (!Existing)
				{
					Existing = &Fades.AddDefaulted_GetRef();
					Existing->Component = Blocker;
				}
				Existing->bBlockingNow = true;
			}
			if (!Blocker)
			{
				break;
			}
			// Faded or left alone: look beyond it
			Params.AddIgnoredComponent(Blocker);
		}
	}

	// Fast in, slow out
	const float Seconds = TargetFraction < PullInFraction ? Tuning.PullInSecondsIn : Tuning.PullInSecondsOut;
	const float Alpha = Seconds <= UE_SMALL_NUMBER ? 1.f : 1.f - FMath::Exp(-DeltaSeconds / Seconds);
	PullInFraction = FMath::Lerp(PullInFraction, TargetFraction, Alpha);
	if (PullInFraction < 0.9999f)
	{
		const FVector EffectivePivot = InOutPose.Location + InOutPose.Forward * View.ArmCm;
		InOutPose.Location = EffectivePivot - InOutPose.Forward * (View.ArmCm * PullInFraction);
	}

	const float Step = Tuning.FadeSeconds <= UE_SMALL_NUMBER ? 1.f : (1.f - Tuning.FadedOpacity) * DeltaSeconds / Tuning.FadeSeconds;
	for (int32 Index = Fades.Num() - 1; Index >= 0; --Index)
	{
		FFade& Fade = Fades[Index];
		UPrimitiveComponent* Component = Fade.Component.Get();
		if (!Component)
		{
			Fades.RemoveAtSwap(Index);
			continue;
		}
		const float Target = Fade.bBlockingNow ? Tuning.FadedOpacity : 1.f;
		Fade.Opacity = FMath::Clamp(Fade.Opacity + FMath::Clamp(Target - Fade.Opacity, -Step, Step), 0.f, 1.f);
		Component->SetCustomPrimitiveDataFloat(Settings.FadeCustomDataIndex, Fade.Opacity);
		if (!Fade.bBlockingNow && Fade.Opacity >= 1.f)
		{
			Fades.RemoveAtSwap(Index);
		}
	}
}

// ---- Aim ----

void UGSTN_CameraComponent::UpdateAim(const FGSTN_CameraSubjectState& Subject, const FGSTN_CameraPose& Pose, UGSTN_CameraModeDefinition* Top)
{
	AimDirection = Pose.Forward;
	Crosshair = FGSTN_CrosshairView();
	const UGSTN_CameraMode_Shoulder* Shoulder = Top && Top->bConvergeAim ? Cast<UGSTN_CameraMode_Shoulder>(GetTopModeInstance()) : nullptr;
	UWorld* World = GetWorld();
	if (!Shoulder || !World)
	{
		return;
	}
	const float Min = Shoulder->Tuning.ConvergeMinCm;
	const float Max = FMath::Max(Min, Shoulder->Tuning.ConvergeMaxCm);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNCameraAim), false, GetOwner());
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Pose.Location + Pose.Forward * Min, Pose.Location + Pose.Forward * Max, GetProfile().TraceChannel, Params);

	FVector AimOrigin = Subject.Pivot;
	if (GetOwner() && GetOwner()->Implements<UGSTN_CameraAimOrigin>())
	{
		AimOrigin = IGSTN_CameraAimOrigin::Execute_GetCameraAimOrigin(GetOwner());
	}
	AimDirection = FGSTN_CameraMath::ConvergedAimDirection(Pose.Location, Pose.Forward, AimOrigin, bHit, Hit.ImpactPoint, Min, Max);
	Crosshair.bConverged = bHit;
	Crosshair.HitPoint = bHit ? Hit.ImpactPoint : Pose.Location + Pose.Forward * Max;
	Crosshair.DistanceCm = static_cast<float>(FVector::Dist(Pose.Location, Crosshair.HitPoint));
	if (Top->bShowCrosshair)
	{
		OnCrosshairUpdated.Broadcast(Crosshair);
	}
}

// ---- The frame ----

void UGSTN_CameraComponent::EvaluateCamera(float DeltaSeconds)
{
	const UGSTN_CameraProfile& Settings = GetProfile();
	if (CVarEnable.GetValueOnGameThread() == 0)
	{
		bHasView = false;
		return;
	}
	const float Dt = FMath::Clamp(DeltaSeconds, 0.f, 0.25f);
	Clock += Dt;
	FGSTN_CameraPlayerState& Player = State();

	// 1. The presented subject. A lost or suppressed one freezes the view; its return is a cut (camera.md §7.3)
	FGSTN_CameraSubjectState Subject;
	if (!ReadSubject(Subject) || Subject.bSuppressed)
	{
		bHolding = true;
		HoldSeconds += Dt;
		return;
	}
	if (bHolding)
	{
		bHolding = false;
		HoldSeconds = 0.f;
		bPendingCut = true;
	}
	const bool bCutThisFrame = bPendingCut;

	// 2. The gravity frame: Up is rate limited and the reference forward is parallel transported (camera.md §7.2)
	FVector TargetUp = Subject.Up.GetSafeNormal();
	if (TargetUp.IsNearlyZero())
	{
		TargetUp = FVector::UpVector;
	}
	if (const AGSTN_CameraSection* Section = ActiveSection.Get())
	{
		if (Section->Spec.UpSource == EGSTN_CameraUpSource::SectionActor)
		{
			TargetUp = Section->GetActorUpVector();
		}
	}
	if (!Player.bFrameInit)
	{
		Player.Frame = FGSTN_CameraMath::MakeFrame(TargetUp, FVector::ZeroVector);
		Player.bFrameInit = true;
	}
	else if (bCutThisFrame)
	{
		Player.Frame.RefForward = FGSTN_CameraMath::TransportForward(Player.Frame.Up, TargetUp, Player.Frame.RefForward);
		Player.Frame.Up = TargetUp;
	}
	else
	{
		const FVector NewUp = FGSTN_CameraMath::StepUp(Player.Frame.Up, TargetUp, Settings.Stability.UpRateLimitDegPerSecond, Dt);
		if (!NewUp.Equals(Player.Frame.Up, 1.0e-9))
		{
			Player.Frame.RefForward = FGSTN_CameraMath::TransportForward(Player.Frame.Up, NewUp, Player.Frame.RefForward);
			Player.Frame.Up = NewUp;
		}
	}
	const FGSTN_CameraFrame Frame = Player.Frame;

	// 3. Sections (point in volume of the presented pivot, never an overlap event)
	UpdateSection(Subject);
	AGSTN_CameraSection* Section = ActiveSection.Get();

	// 4. The tag container the rules read: pawn tags plus the synthetic local ones (camera.md §3.2)
	FGameplayTagContainer Tags = RequestTags;
	if (AActor* Owner = GetOwner())
	{
		// The pawn or one of its components may be the tag source (a project rig component needs no pawn change)
		UObject* TagSource = Owner->Implements<UGSTN_CameraTagSource>() ? static_cast<UObject*>(Owner) : Owner->FindComponentByInterface(UGSTN_CameraTagSource::StaticClass());
		if (TagSource)
		{
			FGameplayTagContainer FromSource;
			IGSTN_CameraTagSource::Execute_GetCameraTags(TagSource, FromSource);
			Tags.AppendTags(FromSource);
		}
	}
	if (Section)
	{
		Tags.AddTag(GSTNCameraTags::State_InSection);
		if (Section->Spec.SectionTag.IsValid())
		{
			Tags.AddTag(Section->Spec.SectionTag);
		}
		if (Section->Spec.ShoulderPolicy == EGSTN_CameraShoulderPolicy::Forbid)
		{
			Tags.AddTag(GSTNCameraTags::State_ShoulderBlocked);
		}
	}

	// Group framing of the opponents the project registered (CAM.7): local tags, like InSection
	{
		int32 OpponentCount = 0;
		for (const TWeakObjectPtr<AActor>& Opponent : OpponentActors)
		{
			OpponentCount += Opponent.IsValid() ? 1 : 0;
		}
		const EGSTN_CameraGroupFraming Group = FGSTN_CameraMath::ChooseGroupFraming(OpponentCount);
		if (Group == EGSTN_CameraGroupFraming::Duel)
		{
			Tags.AddTag(GSTNCameraTags::State_OneOpponent);
		}
		else if (Group == EGSTN_CameraGroupFraming::Arena)
		{
			Tags.AddTag(GSTNCameraTags::State_ManyOpponents);
		}
	}

	FrameTags = Tags;

	// 5. The rule table: highest priority match, debounced
	UGSTN_CameraRuleSet* RuleSet = ChooseRuleSet();
	if (RuleSet != CurrentRuleSet)
	{
		CurrentRuleSet = RuleSet;
		Player.RuleState = FGSTN_CameraRuleChoiceState();
	}
	if (bCutThisFrame)
	{
		Player.RuleState = FGSTN_CameraRuleChoiceState();
	}
	TArray<FGSTN_CameraRuleEval> Evals;
	for (const FGSTN_CameraModeRule& Rule : RuleSet->Rules)
	{
		FGSTN_CameraRuleEval& Eval = Evals.AddDefaulted_GetRef();
		Eval.Priority = Rule.Priority;
		Eval.bMatches = Rule.Query.IsEmpty() || Rule.Query.Matches(Tags);
		Eval.EnterDelaySeconds = Rule.EnterDelaySeconds;
		Eval.ExitDelaySeconds = Rule.ExitDelaySeconds;
	}
	const int32 Chosen = FGSTN_CameraMath::StepRuleChoice(Player.RuleState, Evals, Dt, RuleSet->MinDwellSeconds);
	UGSTN_CameraModeDefinition* Definition = RuleSet->Rules.IsValidIndex(Chosen) ? ResolveDefinition(RuleSet->Rules[Chosen]) : GetBuiltinDefinition(GSTNCameraTags::Mode_Free.GetTag());
	const bool bSectionEntry = IsSectionDefinition(Definition) || (Definition && RuleSet->Rules.IsValidIndex(Chosen) && RuleSet->Rules[Chosen].bModeFromSection);
	AGSTN_CameraSection* EntrySection = bSectionEntry ? Section : nullptr;
	if (bSectionEntry && !EntrySection)
	{
		// The section ended during the exit delay: keep the entry (and its angle) of the section the top already has
		if (const TWeakObjectPtr<AGSTN_CameraSection>* Previous = SlotSections.Find(Stack.TopKey()))
		{
			EntrySection = Previous->Get();
		}
	}

	// 6. The stack: push or promote the chosen mode
	if (!Player.bOrbitInit)
	{
		Player.Orbit.YawDeg = Settings.DefaultYawDeg;
		Player.Orbit.PitchDeg = (Definition && Definition->Mode) ? Definition->Mode->GetDefaultPitchDeg() : Settings.FreeView.PitchDeg;
		Player.Orbit.DistanceScale = 1.f;
		Player.bOrbitInit = true;
	}
	const int32 Key = KeyFor(Definition, EntrySection);
	const int32 PreviousTopKey = Stack.TopKey();
	const bool bModeChanged = Key != PreviousTopKey;
	EGSTN_CameraFrameChangeReason ChangeReason = EGSTN_CameraFrameChangeReason::ModeChanged;
	EGSTN_CameraStickHold StickHold = EGSTN_CameraStickHold::OnAnyChange;
	float HoldMinDeg = Settings.HoldMinYawDeltaDeg;
	bool bCut = bCutThisFrame;
	if (bModeChanged)
	{
		const TWeakObjectPtr<AGSTN_CameraSection>* PreviousSection = SlotSections.Find(PreviousTopKey);
		FGSTN_CameraBlendTuning Blend = Definition->BlendIn;
		if (EntrySection)
		{
			const FGSTN_CameraSectionSpec& Spec = EntrySection->Spec;
			Blend.BlendSeconds = Spec.BlendInSeconds;
			Blend.Function = Spec.BlendFunction;
			bCut = bCut || Spec.bCutOnEnter;
			ChangeReason = EGSTN_CameraFrameChangeReason::SectionEntered;
			StickHold = Spec.StickHold;
			if (Spec.HoldMinYawDeltaDeg >= 0.f)
			{
				HoldMinDeg = Spec.HoldMinYawDeltaDeg;
			}
		}
		else if (PreviousSection && PreviousSection->IsValid())
		{
			// Leaving a section: its own blend out
			Blend.BlendSeconds = (*PreviousSection)->Spec.BlendOutSeconds;
			Blend.Function = (*PreviousSection)->Spec.BlendFunction;
			ChangeReason = EGSTN_CameraFrameChangeReason::SectionExited;
			StickHold = (*PreviousSection)->Spec.StickHold;
			if ((*PreviousSection)->Spec.HoldMinYawDeltaDeg >= 0.f)
			{
				HoldMinDeg = (*PreviousSection)->Spec.HoldMinYawDeltaDeg;
			}
		}

		TArray<int32> Removed;
		Stack.Request(Key, Blend, Settings.Stability.MaxStackDepth, &Removed);
		DropSlots(Removed);
		if (!SlotInstances.Contains(Key))
		{
			UGSTN_CameraMode* Source = Definition->Mode ? Definition->Mode.Get() : nullptr;
			UGSTN_CameraMode* Instance = Source ? DuplicateObject<UGSTN_CameraMode>(Source, this) : NewObject<UGSTN_CameraMode_Follow>(this);
			Instance->ResetState();
			SlotInstances.Add(Key, Instance);
			SlotDefinitions.Add(Key, Definition);
			SlotSections.Add(Key, EntrySection);
		}
		if (EntrySection)
		{
			// Entering a section discards the manual offset (camera.md §4.3)
			Player.Orbit.YawDeg = EntrySection->GetBaseHeading(Frame) + EntrySection->Spec.DesiredYawDeg;
			Player.Orbit.PitchDeg = EntrySection->Spec.DesiredPitchDeg;
			Player.Orbit.DistanceScale = 1.f;
			Player.SecondsSinceLookInput = 0.f;
		}
	}
	if (bCut)
	{
		TArray<int32> Removed;
		Stack.Cut(&Removed);
		DropSlots(Removed);
		PullInFraction = 1.f;
		if (UGSTN_CameraMode* Top = GetTopModeInstance())
		{
			Top->ResetState();
		}
	}
	bPendingCut = false;
	ActiveModeTag = Definition ? Definition->ModeTag : FGameplayTag();

	// 7. Manual control: the orbit moves, is clamped to the section, eases back
	ApplyLook(Subject, Player, Dt);

	TArray<int32> Removed;
	Stack.Advance(Dt, &Removed);
	DropSlots(Removed);

	// 8. Views: the top entry live, covered ones from the view they had when they stopped being on top
	FGSTN_CameraModeContext Context;
	Context.Frame = Frame;
	Context.Subject = Subject;
	Context.Orbit = Player.Orbit;
	Context.DeltaSeconds = Dt;
	Context.AspectRatio = FMath::Max(0.1f, AspectRatio);
	Context.FramingDistanceScale = FramingDistanceScale;
	Context.FramingFovOffsetDeg = FramingFovOffsetDeg;
	Context.FramingPivotOffset = FramingPivotOffset;
	if (const TWeakObjectPtr<AGSTN_CameraSection>* TopSection = SlotSections.Find(Stack.TopKey()))
	{
		Context.Section = TopSection->Get();
	}
	{
		Context.bHasShoulderSideOverride = bHasShoulderSideOverride;
		Context.ShoulderSideOverride = ShoulderSideOverride;
		// Opponents: the readable ones in registration order; the first is the duel opponent
		for (const TWeakObjectPtr<AActor>& Opponent : OpponentActors)
		{
			FGSTN_CameraSubjectState Read;
			if (const AActor* Actor = Opponent.Get(); Actor && FGSTN_CameraAdapterRegistry::Read(Actor, Read))
			{
				Context.Opponents.Add(Read);
			}
		}
		// The Arena mode takes its yaw base and its focus limit from the arena section, when it comes from one
		if (Context.Section)
		{
			Context.ArenaBaseHeadingDeg = Context.Section->GetBaseHeading(Frame);
			Context.bHasArenaBounds = Context.Section->GetWorldBounds(Context.ArenaBounds);
		}
		Context.bBothGrounded = Subject.bGrounded;
		if (!Context.Opponents.IsEmpty())
		{
			Context.bHasOpponent = true;
			Context.Opponent = Context.Opponents[0];
			Context.bBothGrounded = Subject.bGrounded && Context.Opponent.bGrounded;
		}
	}
	UGSTN_CameraMode* TopMode = GetTopModeInstance();
	FGSTN_CameraView TopView;
	if (TopMode)
	{
		TopView = TopMode->ComputeView(Context);
		LastViews.Add(Stack.TopKey(), TopView);
	}

	FGSTN_CameraView Blended = TopView;
	for (int32 Index = 0; Index < Stack.Num(); ++Index)
	{
		const bool bTop = Index == Stack.Num() - 1;
		const FGSTN_CameraView* Entry = bTop ? &TopView : LastViews.Find(Stack.Entries[Index].Key);
		if (!Entry)
		{
			continue;
		}
		Blended = Index == 0 ? *Entry : FGSTN_CameraMath::BlendView(Blended, *Entry, Stack.WeightOf(Index));
	}

	// 9. The reference frame event: raised when the camera decides to turn the stick reference. Manual orbit never reaches this point
	if ((bModeChanged || bCutThisFrame) && bHasPublishedForward && TopMode)
	{
		const bool bHoldWanted = bCutThisFrame || StickHold == EGSTN_CameraStickHold::OnAnyChange || (StickHold == EGSTN_CameraStickHold::OnCut && bCut);
		if (bHoldWanted && StickHold != EGSTN_CameraStickHold::None)
		{
			RaiseFrameChange(LastPublishedForward, FGSTN_CameraMath::HeadingToDirection(Frame, TopView.HeadingDeg), bCutThisFrame ? EGSTN_CameraFrameChangeReason::Respawn : ChangeReason, bCut, HoldMinDeg);
		}
	}

	// 10. Pose, occlusion, outputs
	FGSTN_CameraPose Pose = FGSTN_CameraMath::ComputePose(Frame, Blended);
	UGSTN_CameraModeDefinition* TopDefinition = GetTopDefinition();
	ResolveOcclusion(Blended, TopDefinition ? TopDefinition->Occlusion : Settings.Occlusion, Dt, Pose);
	LastView = Blended;
	LastPose = Pose;
	bHasView = true;
	if (TopDefinition)
	{
		ActiveModeTag = TopDefinition->ModeTag;
	}
	LastPublishedForward = FGSTN_CameraMath::HeadingToDirection(Frame, Blended.HeadingDeg);
	bHasPublishedForward = true;

	// The camera is the only writer of the control rotation: the input source samples CameraYaw and aim from it (camera.md §5)
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		if (Controller->IsLocalController() && Settings.ControlRotationPolicy == EGSTN_CameraControlRotationPolicy::WriteBack)
		{
			Controller->SetControlRotation(FRotationMatrix::MakeFromXZ(Pose.Forward, Pose.Up).Rotator());
		}
		if (Controller->IsLocalController())
		{
			// After the write back: next frame's look input is the change from here (the camera's own motion is never read as input)
			LeftControlRotation = Controller->GetControlRotation();
			LeftControlRotationFrame = GFrameCounter;
			bHasLeftControlRotation = true;
		}
		WriteLookLog(Subject, Controller, false);
	}

	UpdateAim(Subject, Pose, TopDefinition);

	if (CVarDebug.GetValueOnGameThread() != 0 && GetWorld())
	{
		DrawDebugSphere(GetWorld(), Blended.Pivot, 20.f, 8, FColor::Yellow);
		DrawDebugLine(GetWorld(), Pose.Location, Blended.Pivot, FColor::Cyan);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) + 0x47535443ull, 0.f, FColor::White,
				FString::Printf(TEXT("Camera %s mode=%s section=%s stack=%d dist=%.2f framing yaw=%.2f pitch=%.2f pull=%.2f autofollow=%d assist=%d latch=%d"), *GetNameSafe(GetOwner()), *ActiveModeTag.ToString(), *GetNameSafe(ActiveSection.Get()), Stack.Num(),
					Player.Orbit.DistanceScale, LastBias.YawWeight, LastBias.PitchWeight, LastBias.PullWeight, bAutoFollowing ? 1 : 0, bAssistActive ? 1 : 0, Player.StickLatch.bLatched ? 1 : 0));
		}
	}
}

void UGSTN_CameraComponent::WriteLookLog(const FGSTN_CameraSubjectState& Subject, const APlayerController* Controller, bool bFlushOnly)
{
	const bool bOn = CVarLookLog.GetValueOnGameThread() != 0;
	if (bOn && !bFlushOnly && Controller)
	{
		const FRotator Written = Controller->GetControlRotation();
		const UWorld* World = GetWorld();
		LookLogLines.Add(FString::Printf(TEXT("%llu,%.4f,%d,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%d,%.3f,%.3f,%.1f,%.1f,%.3f,%.3f,%.3f,%d,%s"),
			(unsigned long long)GFrameCounter, World ? World->GetTimeSeconds() : 0.0, static_cast<int32>(GetProfile().LookInputSource),
			FRotator::NormalizeAxis(LookRow.ControlBefore.Pitch), FRotator::NormalizeAxis(LookRow.ControlBefore.Yaw),
			FRotator::NormalizeAxis(LookRow.ControlLeft.Pitch), FRotator::NormalizeAxis(LookRow.ControlLeft.Yaw),
			LookRow.LookDeltaDeg.Y, LookRow.LookDeltaDeg.X, LookRow.bContinuous ? 1 : 0, LookRow.OrbitPitchDeg, LookRow.OrbitYawDeg,
			LookRow.MinPitchDeg, LookRow.MaxPitchDeg, LastView.PitchDeg, FRotator::NormalizeAxis(Written.Pitch), FRotator::NormalizeAxis(Written.Yaw),
			Subject.bGrounded ? 1 : 0, *ActiveModeTag.ToString()));
	}
	if (LookLogLines.Num() >= 60 || ((!bOn || bFlushOnly) && LookLogLines.Num() > 0))
	{
		// One file per pawn (several PIE players in one process would interleave their rows)
		const FString Path = FPaths::ProjectLogDir() / FString::Printf(TEXT("gstn_camera_look_%s.csv"), *GetNameSafe(GetOwner()));
		FString Text;
		if (!IFileManager::Get().FileExists(*Path))
		{
			Text = TEXT("frame,time,source,ctrl_before_pitch,ctrl_before_yaw,ctrl_left_pitch,ctrl_left_yaw,look_pitch_deg,look_yaw_deg,continuous,orbit_pitch,orbit_yaw,pitch_min,pitch_max,view_pitch,written_pitch,written_yaw,grounded,mode\n");
		}
		Text += FString::Join(LookLogLines, TEXT("\n")) + TEXT("\n");
		FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
		LookLogLines.Reset();
	}
}

void UGSTN_CameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	Super::GetCameraView(DeltaTime, DesiredView);
	const UGSTN_CameraProfile& Settings = GetProfile();
	const bool bHoldExpired = bHolding && Settings.KOHoldSeconds > 0.f && HoldSeconds > Settings.KOHoldSeconds;
	if (!bHasView || bHoldExpired || CVarEnable.GetValueOnGameThread() == 0)
	{
		return;
	}
	DesiredView.Location = LastPose.Location;
	DesiredView.Rotation = FRotationMatrix::MakeFromXZ(LastPose.Forward, LastPose.Up).Rotator();
	DesiredView.FOV = LastView.FovDeg;
}

// ---- Profile, options, distance, stick reference (P1a) ----

const UGSTN_CameraProfile& UGSTN_CameraComponent::GetProfile() const
{
	return Profile ? *Profile : UGSTN_CameraProfile::GetDefaultProfile();
}

FGSTN_CameraUserOptions& UGSTN_CameraComponent::Options()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (UGSTN_CameraLocalPlayerSubsystem* Subsystem = UGSTN_CameraLocalPlayerSubsystem::Get(Controller))
	{
		return Subsystem->Options;
	}
	return FallbackOptions;
}

FGSTN_CameraUserOptions UGSTN_CameraComponent::GetUserOptions()
{
	return Options();
}

void UGSTN_CameraComponent::SetUserOptions(const FGSTN_CameraUserOptions& NewOptions)
{
	Options() = NewOptions;
}

void UGSTN_CameraComponent::SetDistancePreset(EGSTN_CameraDistancePreset Preset)
{
	Options().DistancePreset = Preset;
}

void UGSTN_CameraComponent::AddZoomInput(float Input)
{
	PendingZoom += Input;
}

EGSTN_CameraDistancePreset UGSTN_CameraComponent::CycleDistancePreset(int32 Direction)
{
	FGSTN_CameraUserOptions& UserOptions = Options();
	UserOptions.DistancePreset = FGSTN_CameraMath::CycleDistancePreset(UserOptions.DistancePreset, Direction);
	return UserOptions.DistancePreset;
}

float UGSTN_CameraComponent::GetDistanceScale()
{
	return State().Orbit.DistanceScale;
}

FRotator UGSTN_CameraComponent::ResolveMoveReferenceRotation(FVector2D MoveStick)
{
	FGSTN_CameraPlayerState& Player = State();
	const float Dt = Player.LastStickResolveClock < 0.0 ? 0.f : static_cast<float>(FMath::Max(0.0, Clock - Player.LastStickResolveClock));
	Player.LastStickResolveClock = Clock;
	const float CameraYaw = FGSTN_CameraMath::DirectionToHeading(Player.Frame, LastPose.Forward);
	const float Yaw = FGSTN_CameraMath::ResolveStickReference(Player.StickLatch, GetProfile().StickLatch, static_cast<float>(MoveStick.Size()), CameraYaw, Dt);
	const FVector Forward = FGSTN_CameraMath::HeadingToDirection(Player.Frame, Yaw);
	return FRotationMatrix::MakeFromXZ(Forward, Player.Frame.Up).Rotator();
}

bool UGSTN_CameraComponent::IsStickLatched()
{
	return State().StickLatch.bLatched;
}
