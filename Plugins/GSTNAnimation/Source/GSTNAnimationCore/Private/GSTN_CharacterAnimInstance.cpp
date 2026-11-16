// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_CharacterAnimInstance.h"

#include "GSTN_OverrideLayerSource.h"
#include "GSTN_StanceTypes.h"
#include "HAL/IConsoleManager.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_CharacterAnimInstance)

void UGSTN_CharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const IGSTN_OverrideLayerSource* Best = nullptr;
	const UAnimInstance* BestInstance = nullptr;
	if (const USkeletalMeshComponent* Mesh = GetOwningComponent())
	{
		// Linked instances are updated before the main one, so their answers are this frame's
		for (const UAnimInstance* Linked : Mesh->GetLinkedAnimInstances())
		{
			const IGSTN_OverrideLayerSource* Source = Cast<IGSTN_OverrideLayerSource>(Linked);
			if (Source && Source->IsOverrideLayerActive() && (!Best || Source->GetOverridePriority() > Best->GetOverridePriority()))
			{
				Best = Source;
				BestInstance = Linked;
			}
		}
	}

	bOverrideLayerActive = Best != nullptr;
	ActiveOverrideLayerClass = BestInstance ? BestInstance->GetClass() : nullptr;
	if (Best)
	{
		OverrideBlendTime = Best->GetOverrideBlendTime();
	}

	UpdateFootLockRequest(DeltaSeconds);
	UpdateStance(DeltaSeconds);
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

	// Capsule speed from the actor's own motion (no movement component knowledge: this module depends on no Mover)
	float Speed = 0.f;
	if (const AActor* Owner = GetOwningActor())
	{
		const FVector Location = Owner->GetActorLocation();
		if (bFootLockHasLastLocation && DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			Speed = FVector::Dist(Location, FootLockLastActorLocation) / DeltaSeconds;
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