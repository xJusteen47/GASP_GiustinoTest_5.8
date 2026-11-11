// Held objects, world side.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "GSTN_HeldTypes.h"

#include "GSTN_HeldObjectComponent.generated.h"

class AGSTN_Projectile;

/**
 * The replicated stamps of an object's pickup record in GSTNInteract's registry (M10 G2): the handle, the whole-centimetre anchor and the
 * server frames it is live in, plus the end of the record before it (a client that misses a state still closes the old record by stamp).
 * Handle 0 = none. A record is immutable: every rest is a new record, every pickup or throw closes it
 */
USTRUCT()
struct FGSTN_HeldRecordStamp
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Handle = 0;

	UPROPERTY()
	FIntVector PositionCm = FIntVector::ZeroValue;

	UPROPERTY()
	int32 FromFrame = 0;

	/** Exclusive; MAX_int32 while the object rests */
	UPROPERTY()
	int32 ToFrame = MAX_int32;

	UPROPERTY()
	int32 PreviousHandle = 0;

	UPROPERTY()
	int32 PreviousToFrame = MAX_int32;
};

/** One kind of object (crate, barrel...): shared tuning, a data asset so many placed objects use one row. Slice 1 subset of the plan's UGSTN_ObjectDefinition */
UCLASS(BlueprintType)
class GSTNHELDRUNTIME_API UGSTN_HeldObjectDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held", meta = (ShowOnlyInnerProperties))
	FGSTN_HeldObjectTuning Tuning;

	/**
	 * HOOK, not read yet (owner's design 2026-11-10: "the other attack buttons do something depending on the object"): the ability a command
	 * starts while this object is in hand, by command tag (Command.Attack.*, Command.Special.*). Empty = the kit's own move. A later slice gives
	 * the holder these abilities for the hold and routes the commands to them (the plan's held-weapon overlay, section 6)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held|Actions", meta = (Categories = "Command"))
	TMap<FGameplayTag, TSubclassOf<class UGameplayAbility>> CommandActions;

	/** HOOK, not read yet: the object's own ranged action (a gun's shot, a wand's bolt), bound by a later slice. Null = none */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Held|Actions")
	TSubclassOf<class UGameplayAbility> RangedAction;

#if WITH_EDITOR
	/**
	 * M10 G12: errors for tuning that cannot work (no pickup reach, a throw with no speed), warnings for tuning that is probably a slip (a size
	 * tag no size class names, a wind-up without a montage or that releases after its montage ended)
	 */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

/**
 * Makes its actor a held object (M10). Put it on any movable actor (or use AGSTN_HeldObjectActor); without a Definition the built-in
 * test tuning applies, so a placed object works with no setup.
 *
 * Authority: the object is the host's (combat-m10 §9 C, server-authoritative by design, not simulation state). Only the host changes
 * State, Holder, Flight and RestLocation, through UGSTN_HeldSubsystem; they replicate as properties, so an object at rest costs no bytes.
 * Every machine places the actor itself from them each frame: at the holder (hold offset in the holder's space) while Held, at its
 * projectile's drawn position while Flying (on a client that is its own predicted frame, as for any projectile), at RestLocation when Idle.
 * Movement replication stays off.
 */
UCLASS(ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent))
class GSTNHELDRUNTIME_API UGSTN_HeldObjectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_HeldObjectComponent();

	/** Optional shared tuning; null = the built-in test tuning */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Held")
	TObjectPtr<UGSTN_HeldObjectDefinition> Definition;

	const FGSTN_HeldObjectTuning& GetTuning() const;

	/** When its holder drops it: the tuning's own rules (bOverrideBreakRules) or the project's (UGSTN_HeldRules) */
	const FGSTN_HeldBreakRules& GetBreakRules() const;

	/** HOOK: the ability Command starts with this object in hand (UGSTN_HeldObjectDefinition::CommandActions); null = the kit's own. Not read yet */
	TSubclassOf<class UGameplayAbility> FindCommandAction(const FGameplayTag& Command) const;

	/** Host: the serial of the current (or last) hold of this object (the Held link's partner serial) */
	uint8 GetHoldSerial() const { return HoldSerial; }
	/** Host: the serial of the holder's Hand link of the current (or last) hold (M10 G1), 0 before the first */
	uint8 GetHandLinkSerial() const { return HandLinkSerial; }
	/** The id the holder's fragment names (host object id + 1; 0 = none) */
	uint16 GetObjectRef() const { return ObjectId == INDEX_NONE ? 0 : static_cast<uint16>(ObjectId + 1); }

	EGSTN_HeldObjectState GetState() const { return State; }
	AActor* GetHolder() const { return Holder; }
	AGSTN_Projectile* GetFlight() const { return Flight; }
	/** Host-assigned id (registration order), the race tie-break; INDEX_NONE on clients */
	int32 GetObjectId() const { return ObjectId; }

	/** Host only, through UGSTN_HeldSubsystem */
	void SetHeld(AActor* InHolder);
	void SetFlying(AGSTN_Projectile& InFlight);
	void SetIdleAt(const FVector& Location);
	/** Host (M10 G7): broken by its impact; back at its home at RespawnFrame (INDEX_NONE = never) */
	void SetBroken(int32 InRespawnFrame);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend class UGSTN_HeldSubsystem;

	UFUNCTION()
	void OnRep_RestLocation();

	UFUNCTION()
	void OnRep_RecordStamp();

	/**
	 * Host (M10 G2): a new record at RestLocation, live from the frame after the last fixed tick plus the interact rules' MinAgeFrames (so a
	 * client holds it before it simulates that frame; 0 before the first tick: a placed object is there from the start). Closes the open one
	 */
	void OpenRecord();

	/** Host: ends the open record after the last fixed tick's frame (the frame a pickup resolved in still sees it) */
	void CloseRecord();

	/** Every machine: makes the world's GSTNInteract registry hold RecordStamp (adds, or stamps the end of, its two records) */
	void ApplyRecord();

	/** Places the owner from the replicated state (every machine) */
	void UpdatePlacement();

	/**
	 * Presentation (slice 3, placeholder look): a text above this object while it is the local player's pickup target, i.e. the pawn has
	 * Interact.Target.Pickup and UGSTN_HeldSubsystem::FindPickupTarget names this object. Never on a dedicated server; reads, never writes
	 */
	void UpdatePrompt();

	UPROPERTY(Transient)
	TObjectPtr<class UTextRenderComponent> Prompt;

	UPROPERTY(ReplicatedUsing = OnRep_RestLocation, VisibleInstanceOnly, BlueprintReadOnly, Category = "Held", meta = (AllowPrivateAccess = "true"))
	EGSTN_HeldObjectState State = EGSTN_HeldObjectState::Idle;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Held", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> Holder;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Held", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AGSTN_Projectile> Flight;

	/** The pickup record (M10 G2) */
	UPROPERTY(ReplicatedUsing = OnRep_RecordStamp)
	FGSTN_HeldRecordStamp RecordStamp;

	/** Host only: the Hand link serial the subsystem formed the current hold with, so its release breaks that very link */
	uint8 HandLinkSerial = 0;

	/** Host only: the carry load payload of the current (or last) hold (M10 G6, FGSTN_HeldLoad), for the log and the report */
	uint8 HoldLoadPayload = 0;

	/** Where it rests while Idle (1/10 cm on the wire) */
	UPROPERTY(ReplicatedUsing = OnRep_RestLocation)
	FVector_NetQuantize10 RestLocation;

	int32 ObjectId = INDEX_NONE;

	/** Host: bumped by every SetHeld */
	uint8 HoldSerial = 0;

	/** Host: the flight's motion, kept after the projectile actor is gone (hit or expiry) to land where it ended */
	FGSTN_BallisticSegment FlightSegment;
	int32 FlightEndFrame = INDEX_NONE;

	/** Host (M10 G7): where it first came to rest (a broken object respawns there), whether that is set, rebounds of the current throw */
	FVector HomeLocation = FVector::ZeroVector;
	bool bHasHome = false;
	int32 Bounces = 0;
	int32 RespawnFrame = INDEX_NONE;
	/** Host: who threw the current flight (a rebound keeps the thrower's credit) */
	TWeakObjectPtr<AActor> Thrower;

	/**
	 * Throwing client (M10 G8): the throw's ghost, then the host's projectile that took it over; the object is drawn at it until the replicated
	 * state catches up (Flying with that projectile, or at rest). Presentation only, never replicated
	 */
	TWeakObjectPtr<AGSTN_Projectile> PredictedFlight;
	bool bPredicting = false;
	bool bPredictionHanded = false;
};
