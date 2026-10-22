// Where the input simulation gets its movement context from (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "GSTN_InputContextProvider.generated.h"

class AActor;
class UGSTN_MoverSimStateTracker;
struct FGSTN_BufferFrameEvents;

/**
 * Tells the input simulation about the owner's movement: which way it faces, which context tags the command bindings can
 * require, and which frame events the movement produced (a landing). The simulation runs before Mover in the fixed tick,
 * so everything here describes the end of the previous frame, and it is read inside the simulation: implementations must
 * be deterministic and read only rolled back state (never a Mover component query, which returns the newest state during a
 * re-simulation; gas-npp.md). Held by the host component as an instanced object, so a game with another movement system
 * swaps the provider and nothing else.
 *
 * The virtuals are plain C++ on purpose: they run every simulated tick, and a Blueprint override would pay the Blueprint VM
 * for every call of every pawn. A game that needs Blueprint for this derives from UGSTN_BlueprintInputContextProvider, which
 * routes them to BlueprintNativeEvents; everyone else subclasses this class in C++.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class GSTNINPUTNP_API UGSTN_InputContextProvider : public UObject
{
	GENERATED_BODY()

public:
	/** Binds the provider to the owner at BeginPlay (finds the tracker, caches components) */
	virtual void Initialize(AActor* Owner) {}

	/** Yaw in degrees the owner faces at the end of the previous frame */
	virtual float GetFacingYaw() const { return 0.f; }

	/** Adds the movement context tags (Input.Context.* and the movement system's own) to Out */
	virtual void AppendContextTags(FGameplayTagContainer& Out) const {}

	/**
	 * A small summary of the movement context (0 = unknown). The simulation stores it in the sync state each frame and
	 * hands the previous one back to AppendFrameEvents, which is how transitions are found from a tracker that only knows
	 * the latest frame
	 */
	virtual uint8 GetContextFingerprint() const { return 0; }

	/** Adds the events the change from the previous fingerprint to the current one produced (Input.BufferEvent.Landed...) */
	virtual void AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const {}
};

/**
 * The provider written in Blueprint, for a game whose movement is not Mover or whose context needs a rule C++ does not have.
 * The simulation calls these every fixed tick (and again for every re-simulated tick), so they are the slower path: keep the
 * Blueprint short, and prefer a C++ subclass of UGSTN_InputContextProvider for a movement system that every character shares.
 *
 * Each function is const and returns its result, so the Blueprint answers from rolled back state only: no world queries, no
 * Mover component queries (those return the newest game-frame state during a re-simulation), no random numbers, no time.
 * Reading a UGSTN_MoverSimStateTracker is fine (it follows the rollback), so is state the Blueprint keeps only when it is
 * rebuilt from such reads in the same call. Initialize is the one non-simulation hook: it runs at BeginPlay and is where a
 * Blueprint caches the components it reads.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class GSTNINPUTNP_API UGSTN_BlueprintInputContextProvider : public UGSTN_InputContextProvider
{
	GENERATED_BODY()

public:
	virtual void Initialize(AActor* Owner) override;
	virtual float GetFacingYaw() const override;
	virtual void AppendContextTags(FGameplayTagContainer& Out) const override;
	virtual uint8 GetContextFingerprint() const override;
	virtual void AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const override;

	/** BeginPlay of the host, with its owner. Not part of the simulation: cache components here */
	UFUNCTION(BlueprintImplementableEvent, Category = "Input|Context", meta = (DisplayName = "Initialize"))
	void ReceiveInitialize(AActor* Owner);

	/** Yaw in degrees the owner faces at the end of the previous frame. Default: 0 */
	UFUNCTION(BlueprintNativeEvent, Category = "Input|Context")
	float GetBlueprintFacingYaw() const;
	virtual float GetBlueprintFacingYaw_Implementation() const { return 0.f; }

	/** The movement context tags (Input.Context.* and the movement system's own) at the end of the previous frame. Default: none */
	UFUNCTION(BlueprintNativeEvent, Category = "Input|Context")
	FGameplayTagContainer GetBlueprintContextTags() const;
	virtual FGameplayTagContainer GetBlueprintContextTags_Implementation() const { return FGameplayTagContainer(); }

	/** A small summary of the movement context (0 = unknown), see UGSTN_InputContextProvider::GetContextFingerprint. Default: 0 */
	UFUNCTION(BlueprintNativeEvent, Category = "Input|Context")
	uint8 GetBlueprintContextFingerprint() const;
	virtual uint8 GetBlueprintContextFingerprint_Implementation() const { return 0; }

	/** The Input.BufferEvent.* tags the change from the previous fingerprint to the current one produced. Default: none */
	UFUNCTION(BlueprintNativeEvent, Category = "Input|Context")
	FGameplayTagContainer GetBlueprintFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint) const;
	virtual FGameplayTagContainer GetBlueprintFrameEvents_Implementation(uint8 PreviousFingerprint, uint8 CurrentFingerprint) const { return FGameplayTagContainer(); }
};

/**
 * Context from Mover, through the sim-state tracker. The movement mode name picks the context class: grounded, airborne or
 * on a wall, each a list of mode names because the modes are game content (the plugin only knows Mover's defaults).
 *
 * Tags: Input.Context.Grounded / .Airborne / .OnWall from the class, plus the Mover tags listed in MoverTags that the
 * tracked state has (Mover's tag queries are by tag, not enumerable). OnWall comes with Airborne, so a wall binding
 * requires both and beats a plain airborne one by having more required tags. Event: Input.BufferEvent.Landed when the
 * class went from airborne to grounded.
 */
UCLASS(BlueprintType)
class GSTNINPUTNP_API UGSTN_MoverInputContextProvider : public UGSTN_InputContextProvider
{
	GENERATED_BODY()

public:
	UGSTN_MoverInputContextProvider();

	virtual void Initialize(AActor* Owner) override;
	virtual float GetFacingYaw() const override;
	virtual void AppendContextTags(FGameplayTagContainer& Out) const override;
	virtual uint8 GetContextFingerprint() const override;
	virtual void AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const override;

	/** Mover modes on the ground. Default: Walking */
	UPROPERTY(EditAnywhere, Category = "Context|Modes")
	TArray<FName> GroundedModes;

	/** Mover modes in the air, not on a wall. Default: Falling, Flying */
	UPROPERTY(EditAnywhere, Category = "Context|Modes")
	TArray<FName> AirborneModes;

	/** Wall movement modes (the project's own, e.g. WallSliding). A wall mode counts as airborne and on a wall */
	UPROPERTY(EditAnywhere, Category = "Context|Modes")
	TArray<FName> WallModes;

	/** Mover tags copied into the context when the tracked state has them. Default: the ground, air, falling, flying and crouching tags */
	UPROPERTY(EditAnywhere, Category = "Context|Tags")
	FGameplayTagContainer MoverTags;
	// No Categories filter on MoverTags: they are Mover's own mode tags, which have no GSTN root

	/** The context classes, also the values of the fingerprint */
	enum class EContext : uint8
	{
		Unknown = 0,
		Grounded = 1,
		Airborne = 2,
		OnWall = 3,
	};

private:
	EContext Classify() const;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_MoverSimStateTracker> Tracker;
};
