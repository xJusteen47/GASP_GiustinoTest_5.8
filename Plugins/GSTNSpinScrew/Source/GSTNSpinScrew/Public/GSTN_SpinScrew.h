// A screw on the floor that the spin and the drill dive unscrew (spin-drill.md 13c, owner 2026-11-24).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/GSTN_SpinTurnable.h"

#include "GSTN_SpinScrew.generated.h"

class UStaticMeshComponent;
class USceneComponent;

/** What an unscrewed screw does (owner 2026-11-24: the first kind; the others are listed in spin-drill.md 13c "Future screw kinds") */
UENUM(BlueprintType)
enum class EGSTN_ScrewRelease : uint8
{
	/** It goes into the turner's hand as a held object when nothing occupies the hands, else it rests on the floor where it was */
	ToHandOrFloor,
	/** It pops out and vanishes */
	PopAndVanish,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGSTN_OnScrewTurned, AGSTN_SpinScrew*, Screw, int32, Progress, AActor*, Turner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_OnScrewReleased, AGSTN_SpinScrew*, Screw, AActor*, Turner);

/**
 * A greybox screw: a hex-ish head flush with the floor, a shaft below it and a slot on top that shows the turn. Every spin near it (GSTN Spin
 * Turn Subsystem) turns it SpinSteps, every drill dive that lands on it DrillSteps; the sense equal to LoosenSense unscrews (the head rises and
 * turns), the other one screws it back when bOppositeTightens. At StepsToRelease it is out: OnReleased fires (wire doors and platforms to it in
 * the level Blueprint or a child class) and Release decides where it goes.
 *
 * Authority only: OnSpinTurned comes from authoritative events; Progress and bReleased replicate and every machine draws them. The look turns
 * smoothly toward the replicated angle (presentation only). Extend by a Blueprint child: mesh, numbers, OnTurned / OnReleased events.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GSTN Spin Screw"))
class GSTNSPINSCREW_API AGSTN_SpinScrew : public AActor, public IGSTN_SpinTurnable
{
	GENERATED_BODY()

public:
	AGSTN_SpinScrew();

	// IGSTN_SpinTurnable
	virtual void OnSpinTurned_Implementation(AActor* Turner, int32 Sense, EGSTN_SpinTurnSource Source) override;
	virtual bool CanBeSpinTurned_Implementation(AActor* Turner) const override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Turns it by Steps in Sense (+1 clockwise, -1 counter-clockwise) as a spin would. Authority only; for level scripts and tests */
	UFUNCTION(BlueprintCallable, Category = "SpinScrew")
	void TurnBy(AActor* Turner, int32 Sense, int32 Steps);

	UFUNCTION(BlueprintPure, Category = "SpinScrew")
	int32 GetProgress() const { return Progress; }

	UFUNCTION(BlueprintPure, Category = "SpinScrew")
	bool IsReleased() const { return bReleased; }

	/** Steps from screwed in to out (owner 3A: 6, tune per screw) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Turning", meta = (ClampMin = "1"))
	int32 StepsToRelease = 6;

	/** Steps a spin near it is worth */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Turning", meta = (ClampMin = "0"))
	int32 SpinSteps = 1;

	/** Steps a drill dive landing on it is worth (the drill does not latch on a level object: one dive = one bite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Turning", meta = (ClampMin = "0"))
	int32 DrillSteps = 3;

	/** The sense that unscrews: -1 counter-clockwise (an ordinary right-hand screw), +1 clockwise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Turning", meta = (ClampMin = "-1", ClampMax = "1"))
	int32 LoosenSense = -1;

	/** The other sense screws it back in (never below fully in) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Turning")
	bool bOppositeTightens = true;

	/** Where it goes once out */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Release")
	EGSTN_ScrewRelease Release = EGSTN_ScrewRelease::ToHandOrFloor;

	/** The held object spawned for ToHandOrFloor (a GSTN Held Object Actor or any actor with the held object component) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Release")
	TSubclassOf<AActor> PickupClass;

	/** Look: degrees the head turns per step */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Look", meta = (ForceUnits = "deg"))
	float DegreesPerStep = 90.f;

	/** Look: how much the head rises per step of unscrewing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Look", meta = (ClampMin = "0", ForceUnits = "cm"))
	float RisePerStepCm = 5.f;

	/** Look: how fast the drawn head follows the replicated progress */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpinScrew|Look", meta = (ClampMin = "0.1"))
	float LookFollowSpeed = 12.f;

	/** Every change of progress (authority, and on clients through the replicated value) */
	UPROPERTY(BlueprintAssignable, Category = "SpinScrew")
	FGSTN_OnScrewTurned OnTurned;

	/** Fully out (authority, and on clients when bReleased arrives) */
	UPROPERTY(BlueprintAssignable, Category = "SpinScrew")
	FGSTN_OnScrewReleased OnReleased;

protected:
	/** Blueprint hook on every progress change */
	UFUNCTION(BlueprintImplementableEvent, Category = "SpinScrew", meta = (DisplayName = "On Screw Turned"))
	void BP_OnTurned(int32 NewProgress, AActor* Turner);

	/** Blueprint hook when it comes out, before it goes to the hand, the floor or away */
	UFUNCTION(BlueprintImplementableEvent, Category = "SpinScrew", meta = (DisplayName = "On Screw Released"))
	void BP_OnReleased(AActor* Turner);

	/** Authority: the screw is out. Default: spawn the pickup (to hand or floor) or pop away, then hide this actor */
	UFUNCTION(BlueprintNativeEvent, Category = "SpinScrew")
	void ReleaseScrew(AActor* Turner);

	UFUNCTION()
	void OnRep_Progress();

	UFUNCTION()
	void OnRep_Released();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpinScrew")
	TObjectPtr<USceneComponent> Root;

	/** Turns and rises with the progress: head, shaft and slot hang under it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpinScrew")
	TObjectPtr<USceneComponent> Turning;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpinScrew")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpinScrew")
	TObjectPtr<UStaticMeshComponent> Shaft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpinScrew")
	TObjectPtr<UStaticMeshComponent> Slot;

	UPROPERTY(ReplicatedUsing = OnRep_Progress, VisibleInstanceOnly, BlueprintReadOnly, Category = "SpinScrew")
	int32 Progress = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Released, VisibleInstanceOnly, BlueprintReadOnly, Category = "SpinScrew")
	bool bReleased = false;

	/** Who turned it last (authority), handed to OnTurned on the authority; clients get null */
	TWeakObjectPtr<AActor> LastTurner;

private:
	void ApplyLook();
	void HideScrew();

	float DrawnProgress = 0.f;
};
