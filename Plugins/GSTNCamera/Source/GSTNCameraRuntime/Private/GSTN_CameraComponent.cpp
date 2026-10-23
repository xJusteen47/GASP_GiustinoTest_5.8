#include "GSTN_CameraComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "GSTN_CameraMath.h"
#include "GSTN_CameraRuntimeTypes.h"
#include "GSTN_CameraSection.h"
#include "GSTN_CameraSettings.h"
#include "GSTN_CameraTags.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"

namespace GSTNCameraComponentPrivate
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("gstn.Camera.Enable"), 1, TEXT("0 turns the GSTN camera off on this machine (the camera component keeps its own default view)."));
	static TAutoConsoleVariable<int32> CVarDebug(TEXT("gstn.Camera.Debug"), 0, TEXT("1 draws the pivot and prints the active mode, section and stack of every GSTN camera."));
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
	// After movement and animation: the camera reads the PRESENTED state (camera.md §2.2)
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
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
	if (!World || !ShouldRunFor(World->GetNetMode(), World->IsGameWorld(), UGSTN_CameraSettings::Get().bEnabled))
	{
		SetComponentTickEnabled(false);
	}
}

void UGSTN_CameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseFades();
	Super::EndPlay(EndPlayReason);
}

void UGSTN_CameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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
	OpponentActor = Opponent;
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
	const FName Tag = UGSTN_CameraSettings::Get().FadeTag;
	return Component->ComponentHasTag(Tag) || (Component->GetOwner() && Component->GetOwner()->ActorHasTag(Tag));
}

// ---- Definitions and rules ----

UGSTN_CameraModeDefinition* UGSTN_CameraComponent::GetBuiltinDefinition(const FGameplayTag& Tag)
{
	if (TObjectPtr<UGSTN_CameraModeDefinition>* Existing = BuiltinDefinitions.Find(Tag))
	{
		return Existing->Get();
	}
	const UGSTN_CameraSettings& Settings = UGSTN_CameraSettings::Get();
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
		Mode->Tuning = Settings.Follow;
		Mode->FreeTuning = Settings.Free;
		Definition->Mode = Mode;
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
	return Definition ? Definition : GetBuiltinDefinition(GSTNCameraTags::Mode_Follow.GetTag());
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
	const UGSTN_CameraSettings& Settings = UGSTN_CameraSettings::Get();
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
	const UGSTN_CameraSettings& Settings = UGSTN_CameraSettings::Get();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;

	UGSTN_CameraLocalPlayerSubsystem* Subsystem = UGSTN_CameraLocalPlayerSubsystem::Get(Controller);
	if (Subsystem)
	{
		Subsystem->EnsureInputContext(Controller);
	}

	// The Enhanced Input actions are polled, so the plugin needs no input component of its own
	if (Settings.LookInputSource == EGSTN_CameraLookInputSource::Plugin && Controller && Controller->IsLocalController())
	{
		if (const UEnhancedPlayerInput* Input = Cast<UEnhancedPlayerInput>(Controller->PlayerInput))
		{
			if (!Settings.LookAction.IsNull())
			{
				if (const UInputAction* Look = Settings.LookAction.LoadSynchronous())
				{
					PendingLook += Input->GetActionValue(Look).Get<FVector2D>();
				}
			}
			if (!Settings.RecenterAction.IsNull())
			{
				if (const UInputAction* Recenter = Settings.RecenterAction.LoadSynchronous())
				{
					const bool bDown = Input->GetActionValue(Recenter).Get<bool>();
					if (bDown && !bRecenterWasDown)
					{
						RequestRecenter();
					}
					bRecenterWasDown = bDown;
				}
			}
		}
	}

	const FVector2D Look = PendingLook;
	PendingLook = FVector2D::ZeroVector;
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
			const float LookScale = Settings.LookDegreesPerUnit * (Subsystem ? Subsystem->LookSensitivityMultiplier : 1.f) * Definition->LookSensitivityScale;
			const bool bInvert = Subsystem ? Subsystem->bInvertLookY : Settings.bInvertLookY;
			Orbit.YawDeg = FMath::UnwindDegrees(Orbit.YawDeg + Look.X * LookScale);
			Orbit.PitchDeg += Look.Y * LookScale * (bInvert ? -1.f : 1.f);
		}
		else
		{
			Player.SecondsSinceLookInput += DeltaSeconds;
		}
		float MinPitch = -80.f;
		float MaxPitch = 80.f;
		if (Mode)
		{
			Mode->GetPitchRange(MinPitch, MaxPitch);
		}
		Orbit.PitchDeg = FMath::Clamp(Orbit.PitchDeg, MinPitch, MaxPitch);
	}

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
		}
		else
		{
			Orbit.YawDeg = DesiredYaw;
			Orbit.PitchDeg = Spec.DesiredPitchDeg;
			Orbit.DistanceScale = 1.f;
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
}

// ---- Occlusion ----

void UGSTN_CameraComponent::ReleaseFades()
{
	for (FFade& Fade : Fades)
	{
		if (UPrimitiveComponent* Component = Fade.Component.Get())
		{
			Component->SetCustomPrimitiveDataFloat(UGSTN_CameraSettings::Get().FadeCustomDataIndex, 1.f);
		}
	}
	Fades.Reset();
}

void UGSTN_CameraComponent::ResolveOcclusion(const FGSTN_CameraView& View, const FGSTN_CameraOcclusionTuning& Tuning, float DeltaSeconds, FGSTN_CameraPose& InOutPose)
{
	const UGSTN_CameraSettings& Settings = UGSTN_CameraSettings::Get();
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
		if (const AActor* Opponent = OpponentActor.Get())
		{
			Params.AddIgnoredActor(Opponent);
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
			const EGSTN_CameraOcclusionAction Action = FGSTN_CameraMath::DecideOcclusion(Tuning.Policy, IsFadeable(Blocker), FromCamera, Tuning.NearClipSafeCm);
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
	const bool bHit = World->LineTraceSingleByChannel(Hit, Pose.Location + Pose.Forward * Min, Pose.Location + Pose.Forward * Max, UGSTN_CameraSettings::Get().TraceChannel, Params);

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
	const UGSTN_CameraSettings& Settings = UGSTN_CameraSettings::Get();
	if (!Settings.bEnabled || CVarEnable.GetValueOnGameThread() == 0)
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
	if (AActor* Owner = GetOwner(); Owner && Owner->Implements<UGSTN_CameraTagSource>())
	{
		FGameplayTagContainer FromSource;
		IGSTN_CameraTagSource::Execute_GetCameraTags(Owner, FromSource);
		Tags.AppendTags(FromSource);
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
	UGSTN_CameraModeDefinition* Definition = RuleSet->Rules.IsValidIndex(Chosen) ? ResolveDefinition(RuleSet->Rules[Chosen]) : GetBuiltinDefinition(GSTNCameraTags::Mode_Follow.GetTag());
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
		Player.Orbit.PitchDeg = (Definition && Definition->Mode) ? Definition->Mode->GetDefaultPitchDeg() : Settings.Follow.PitchDeg;
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
	if (const TWeakObjectPtr<AGSTN_CameraSection>* TopSection = SlotSections.Find(Stack.TopKey()))
	{
		Context.Section = TopSection->Get();
	}
	if (const AActor* Opponent = OpponentActor.Get())
	{
		Context.bHasOpponent = FGSTN_CameraAdapterRegistry::Read(Opponent, Context.Opponent);
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
	}

	UpdateAim(Subject, Pose, TopDefinition);

	if (CVarDebug.GetValueOnGameThread() != 0 && GetWorld())
	{
		DrawDebugSphere(GetWorld(), Blended.Pivot, 20.f, 8, FColor::Yellow);
		DrawDebugLine(GetWorld(), Pose.Location, Blended.Pivot, FColor::Cyan);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) + 0x47535443ull, 0.f, FColor::White,
				FString::Printf(TEXT("Camera %s mode=%s section=%s stack=%d"), *GetNameSafe(GetOwner()), *ActiveModeTag.ToString(), *GetNameSafe(ActiveSection.Get()), Stack.Num()));
		}
	}
}

void UGSTN_CameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	Super::GetCameraView(DeltaTime, DesiredView);
	const UGSTN_CameraSettings& Settings = UGSTN_CameraSettings::Get();
	const bool bHoldExpired = bHolding && Settings.KOHoldSeconds > 0.f && HoldSeconds > Settings.KOHoldSeconds;
	if (!bHasView || bHoldExpired || !Settings.bEnabled || CVarEnable.GetValueOnGameThread() == 0)
	{
		return;
	}
	DesiredView.Location = LastPose.Location;
	DesiredView.Rotation = FRotationMatrix::MakeFromXZ(LastPose.Forward, LastPose.Up).Rotator();
	DesiredView.FOV = LastView.FovDeg;
}
