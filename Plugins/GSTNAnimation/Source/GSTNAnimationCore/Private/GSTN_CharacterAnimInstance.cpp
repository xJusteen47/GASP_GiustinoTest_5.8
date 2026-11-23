// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_CharacterAnimInstance.h"

#include "GSTN_OverrideLayerSource.h"
#include "GSTN_PoseDriverSource.h"
#include "GSTN_StanceTypes.h"
#include "HAL/IConsoleManager.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_CharacterAnimInstance)

FGSTN_OverridePick FGSTN_OverridePick::Pick(TConstArrayView<FGSTN_OverrideCandidate> Candidates)
{
	FGSTN_OverridePick Out;
	int32 BestPriority = 0;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FGSTN_OverrideCandidate& Candidate = Candidates[Index];
		if (Candidate.bActive && Candidate.Slot > GSTNOverrideSlot::None && (Out.Index == INDEX_NONE || Candidate.Priority > BestPriority))
		{
			Out.Slot = Candidate.Slot;
			Out.BlendTime = Candidate.BlendTime;
			Out.Index = Index;
			BestPriority = Candidate.Priority;
		}
	}
	return Out;
}

void UGSTN_CharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Linked instances are updated before the main one, so their answers are this frame's
	TArray<FGSTN_OverrideCandidate, TInlineAllocator<4>> Candidates;
	TArray<const UAnimInstance*, TInlineAllocator<4>> Instances;
	if (const USkeletalMeshComponent* Mesh = GetOwningComponent())
	{
		for (const UAnimInstance* Linked : Mesh->GetLinkedAnimInstances())
		{
			if (const IGSTN_OverrideLayerSource* Source = Cast<IGSTN_OverrideLayerSource>(Linked))
			{
				FGSTN_OverrideCandidate& Candidate = Candidates.AddDefaulted_GetRef();
				Candidate.bActive = Source->IsOverrideLayerActive();
				Candidate.Slot = Source->GetOverrideSlot();
				Candidate.Priority = Source->GetOverridePriority();
				Candidate.BlendTime = Source->GetOverrideBlendTime();
				Instances.Add(Linked);
			}
		}
	}

	const FGSTN_OverridePick Pick = FGSTN_OverridePick::Pick(Candidates);
	bOverrideLayerActive = Pick.Index != INDEX_NONE;
	ActiveOverrideSlot = Pick.Slot;
	ActiveOverrideLayerClass = bOverrideLayerActive ? Instances[Pick.Index]->GetClass() : nullptr;
	if (bOverrideLayerActive)
	{
		OverrideBlendTime = Pick.BlendTime;  // kept after the layer stops, so the blend out uses the same time
	}

	UpdateFootLockRequest(DeltaSeconds);
	UpdateStance(DeltaSeconds);
	UpdateCoveredLanding(DeltaSeconds);
}

void UGSTN_CharacterAnimInstance::UpdateCoveredLanding(float DeltaSeconds)
{
	// Up = the actor's own up (gravity's opposite on a planetoid too): this module knows no movement component
	const AActor* Owner = GetOwningActor();
	const FVector Up = Owner ? Owner->GetActorUpVector() : FVector::UpVector;
	const float UpSpeed = static_cast<float>(CapsuleVelocity.Dot(Up));
	const float FloorSpeed = static_cast<float>((CapsuleVelocity - Up * UpSpeed).Size());
	const FGSTN_CoveredLandingOutput Out = CoveredLandingLogic.Step(CoveredLanding, DeltaSeconds, UpSpeed, FloorSpeed, bMontageDrivingPose,
		FootLockMontageWeight);
	bCoveredLandingCapture = Out.bCapture;
	CoveredLandingAlpha = Out.HoldAlpha;
}

void UGSTN_CharacterAnimInstance::UpdateFootLockRequest(float DeltaSeconds)
{
	// A montage "drives" the pose while it is not stopping (playing or blending in); a stopping one only adds its fading weight
	bool bMontageDriving = false;
	float MaxWeight = 0.f;
	for (const FAnimMontageInstance* Instance : MontageInstances)
	{
		if (Instance && Instance->IsValid())
		{
			MaxWeight = FMath::Max(MaxWeight, Instance->GetWeight());
			bMontageDriving |= !Instance->IsStopped();
		}
	}
	FootLockMontageWeight = MaxWeight;
	bMontageDrivingPose = bMontageDriving;

	// A gameplay clip in the main graph (IGSTN_PoseDriverSource) arms the same window as a montage; after it lets go the hand-off weight
	// fades over its blend so the feet are held until the next pose has fully blended in, then step to it
	bool bSourceDriving = false;
	if (const AActor* Owner = GetOwningActor())
	{
		for (UActorComponent* Component : Owner->GetComponentsByInterface(UGSTN_PoseDriverSource::StaticClass()))
		{
			float HandOff = 0.f;
			const IGSTN_PoseDriverSource* Source = Cast<IGSTN_PoseDriverSource>(Component);
			if (Source && Source->IsDrivingPose(HandOff))
			{
				bSourceDriving = true;
				PoseDriverHandOff = FMath::Max(HandOff, 0.f);
			}
		}
	}
	if (bSourceDriving)
	{
		FootLockHandOffWeight = 1.f;
	}
	else if (bPoseDriverActive || FootLockHandOffWeight > 0.f)
	{
		FootLockHandOffWeight = PoseDriverHandOff > KINDA_SMALL_NUMBER ? FMath::Max(FootLockHandOffWeight - DeltaSeconds / PoseDriverHandOff, 0.f) : 0.f;
	}
	bPoseDriverActive = bSourceDriving;
	bMontageDriving |= bSourceDriving;

	// Capsule speed from the actor's own motion (no movement component knowledge: this module depends on no Mover)
	float Speed = 0.f;
	if (const AActor* Owner = GetOwningActor())
	{
		const FVector Location = Owner->GetActorLocation();
		CapsuleVelocity = FVector::ZeroVector;
		if (bFootLockHasLastLocation && DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			CapsuleVelocity = (Location - FootLockLastActorLocation) / DeltaSeconds;
			Speed = static_cast<float>(CapsuleVelocity.Size());
		}
		FootLockLastActorLocation = Location;
		bFootLockHasLastLocation = true;
	}
	CapsuleSpeed = Speed;
	bFootLockMoving = Speed > FootLockStillSpeed;

	if (bMontageDriving)
	{
		// Arms the window for when this montage stops; the montage's own frames are never touched
		bFootLockWindowOpen = true;
		FootLockWindowTime = 0.f;
		bFootLockInstantRelease = true;
		bFootLockRequest = false;
		return;
	}
	bFootLockInstantRelease = false;
	if (bFootLockWindowOpen)
	{
		FootLockWindowTime += DeltaSeconds;
		// Moving (walk away, knockback, falling) or the time cap closes the window for good: no re-lock when the capsule stops again
		if (bFootLockMoving || FootLockWindowTime > FootLockMaxHold)
		{
			bFootLockWindowOpen = false;
		}
	}
	bFootLockRequest = bFootLockWindowOpen;
}


namespace GSTNStance
{
#if !UE_BUILD_SHIPPING
	static TAutoConsoleVariable<FString> CVarStanceTest(TEXT("a.GSTN.StanceTest"), TEXT(""),
		TEXT("Test only: object path of a UGSTN_StanceDefinition every UGSTN_CharacterAnimInstance uses (SetStance); 'none' clears it; empty = no effect."));
#endif
}

void UGSTN_CharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	if (!ActiveStance)
	{
		ActiveStance = DefaultStance;
	}
	// A character that spawns at rest shows its stance at once
	StanceReturnTime = ActiveStance ? 1000.f : -1.f;
	StanceRestAlpha = 1.f;
	bStanceWaitingTrigger = false;
	bStanceWasMontage = false;
	StanceDelayLeft = -1.f;
}

void UGSTN_CharacterAnimInstance::SetStance(UGSTN_StanceDefinition* NewStance)
{
	if (NewStance == ActiveStance)
	{
		return;
	}
	ActiveStance = NewStance;
	++StanceSerial;
	if (NewStance && StanceReturnTime >= 0.f)
	{
		// Already shown: the node blends from its last output to the new stance (SwitchBlendTime) instead of fading in from the game idle
		StanceReturnTime = 1000.f;
	}
}

void UGSTN_CharacterAnimInstance::UpdateStance(float DeltaSeconds)
{
#if !UE_BUILD_SHIPPING
	const FString TestPath = GSTNStance::CVarStanceTest.GetValueOnGameThread();
	if (TestPath != StanceTestApplied)
	{
		StanceTestApplied = TestPath;
		if (TestPath.Equals(TEXT("none"), ESearchCase::IgnoreCase))
		{
			SetStance(nullptr);
		}
		else if (!TestPath.IsEmpty())
		{
			SetStance(LoadObject<UGSTN_StanceDefinition>(nullptr, *TestPath));
		}
	}
#endif

	StanceMontageScale = 1.f - FootLockMontageWeight;
	const UGSTN_StanceDefinition* Stance = ActiveStance;
	if (!Stance)
	{
		StanceDebugState = 0;
		StanceReturnTime = -1.f;
		return;
	}

	if (bMontageDrivingPose)
	{
		// The montage owns the pose: the stance yields through StanceMontageScale as the montage blends in, then waits for the return trigger.
		// Its own first frame is never touched beyond that ("first frame changes pose" rule)
		bStanceWasMontage = true;
		StanceDebugState = 1;
		if (StanceReturnTime >= 0.f)
		{
			StanceReturnTime += DeltaSeconds;
		}
		return;
	}
	if (bStanceWasMontage)
	{
		// The montage stopped driving (blend-out start or cut): its weight is still ~1 here, so hiding the stance now shows nothing
		bStanceWasMontage = false;
		bStanceWaitingTrigger = true;
		StanceReturnTime = -1.f;
		StanceDelayLeft = -1.f;
	}

	const bool bMoving = CapsuleSpeed > StanceStillSpeed;
	if (bMoving)
	{
		// Only at rest: GASP locomotion never carries the stance. Once fully out, the stance waits for the capsule to stop again
		StanceRestAlpha = FMath::Max(0.f, StanceRestAlpha - DeltaSeconds / FMath::Max(Stance->FadeOutTime, KINDA_SMALL_NUMBER));
		if (StanceRestAlpha <= 0.f)
		{
			bStanceWaitingTrigger = true;
			StanceReturnTime = -1.f;
			StanceDelayLeft = -1.f;
		}
		StanceDebugState = 4;
		return;
	}

	if (bStanceWaitingTrigger)
	{
		// Montage fully out, and (WithFeet) no foot still planted by the blend-out foot lock: its steps start now
		const int32 LeftPhase = FootLockDebugPhase / 10;
		const int32 RightPhase = FootLockDebugPhase % 10;
		const bool bFootPlanted = LeftPhase == 1 || RightPhase == 1;
		bool bTrigger = FootLockMontageWeight <= 0.f;
		if (Stance->ReturnTiming == EGSTN_StanceReturnTiming::WithFeet)
		{
			bTrigger &= !bFootPlanted;
		}
		if (!bTrigger)
		{
			StanceDebugState = 1;
			return;
		}
		bStanceWaitingTrigger = false;
		StanceDelayLeft = Stance->ReturnDelay;
		StanceRestAlpha = 1.f;
	}

	if (StanceDelayLeft > 0.f)
	{
		StanceDelayLeft -= DeltaSeconds;
		StanceDebugState = 2;
		return;
	}
	if (StanceReturnTime < 0.f)
	{
		StanceReturnTime = 0.f;
	}
	else
	{
		StanceReturnTime = FMath::Min(StanceReturnTime + DeltaSeconds, 1000.f);
	}
	// Stopped before the stance was fully out: it comes back at the fade-out rate
	StanceRestAlpha = FMath::Min(1.f, StanceRestAlpha + DeltaSeconds / FMath::Max(Stance->FadeOutTime, KINDA_SMALL_NUMBER));
	StanceDebugState = 3;
}