// Platform parts bridge: part state -> platform shape mask (traversal-surfaces.md 4.4, plan T6).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSTN_PlatformPartGates.h"
#include "GSTN_PlatformPartsComponent.generated.h"

class UGSTN_CombatSimComponent;
class UGSTN_EnemyPlatformComponent;
class UGSTN_PartSetDefinition;
struct FGSTN_PlatformPoseContext;
struct FGSTN_PlatformSpec;

/** The broken mask the authority fed its platform and the pose frame it took effect at, replicated to the clients */
USTRUCT()
struct FGSTN_PlatformPartsNetState
{
	GENERATED_BODY()

	UPROPERTY()
	int32 BrokenMask = 0;

	/** Pose frame of the change (server simulation frame numbers, the same on every machine); INDEX_NONE before any simulated frame */
	UPROPERTY()
	int32 Frame = INDEX_NONE;

	bool operator==(const FGSTN_PlatformPartsNetState& Other) const { return BrokenMask == Other.BrokenMask && Frame == Other.Frame; }
	bool operator!=(const FGSTN_PlatformPartsNetState& Other) const { return !(*this == Other); }
};

/**
 * Makes the platform shapes of a fighter follow its body parts (traversal-surfaces.md 4.4, TS.5): a shape whose PartName names a part
 * goes away when that part, or an ancestor of it, is broken, with the shape's Breaking telegraph (DropGraceFrames) so a player on it
 * has time to leave; it comes back, Active at once, when a restore clears the bit (Combat.Event.Part.Restored is the same change seen
 * as an event: following the mask covers it). Put it next to a GSTN Enemy Platform component on a pawn whose part set is attached by
 * a GSTN Parts Attach component (or by hand through FGSTN_PartsAttach).
 *
 * Source of the mask: the confirmed BrokenMask, never a predicted one (combat-parts.md 6.5: breaking a surface under a player is
 * irreversible). The authority reads the parts fragment of the owner's combat sync state at its pose point and stamps the change on
 * the frame it is posing; it replicates the mask with that frame (NetState), and a client stamps the same frame
 * (SetShapeMaskAtFrame), though it is already past it. Why not each machine reading its own copy of the fragment: a client sees the
 * mask a round trip after the authority and would stamp a later frame, so its pawn slides on for frames where the authority's had
 * fallen and every one of them reconciles (scenario run 1 of T6: 4-7 corrections). With the authority's frame and the Breaking
 * telegraph (still solid) the client's past frames were right as long as the mask arrives within the grace: no correction.
 * The one authority branch is about where a fact comes from, outside the simulation; the simulation only sees the colliders.
 *
 * When: at the platform's pose point (UGSTN_EnemyPlatformComponent::OnPrePose), before the pose of every frame simulated for the
 * first time. Nothing is read or written on a replayed frame and nothing here is simulation state: the shape timelines of the
 * platform are.
 *
 * The part names are resolved against the set attached to the owner (FGSTN_PartsAttach::FindSet) whenever that set changes (a kit
 * swap); before a set is attached, or for a name the set lacks, the shape behaves as body. Validation makes an unknown name an error.
 */
UCLASS(ClassGroup = (GSTN), meta = (BlueprintSpawnableComponent, DisplayName = "GSTN Platform Parts"))
class GSTNPLATFORMPARTS_API UGSTN_PlatformPartsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSTN_PlatformPartsComponent();

	/**
	 * The part set validation checks the shapes' part names against when the owner has no GSTN Parts Attach component with a
	 * DefaultPartSet (a set that comes only from a kit). Never read at runtime: the attached set is the truth there
	 */
	UPROPERTY(EditAnywhere, Category = "Platform|Parts")
	TSoftObjectPtr<UGSTN_PartSetDefinition> ValidationPartSet;

	/** Broken parts make their shapes go Breaking for their DropGraceFrames first (TS.5). Off: they go Inactive at once */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Platform|Parts")
	bool bTelegraphBreak = true;

	/** The shape mask last given to the platform (0xFF before the first pose) */
	UFUNCTION(BlueprintPure, Category = "Platform|Parts")
	uint8 GetAppliedShapeMask() const { return AppliedMask; }

	/** The broken mask last read */
	UFUNCTION(BlueprintPure, Category = "Platform|Parts")
	int32 GetLastBrokenMask() const { return LastBrokenMask; }

	/** Part bits gating each shape, for the set resolved last */
	const FGSTN_PlatformPartGates& GetGates() const { return Gates; }

	/** Replaces the combat state as the source of the broken mask (tests, debug commands); unset = back to the combat state */
	void SetBrokenMaskOverride(TOptional<uint16> Mask) { BrokenMaskOverride = Mask; }

	/**
	 * Resolves the shapes' part names against a set: the gate of each named shape is its part and that part's ancestors. Names the set
	 * lacks leave the shape as body and are reported in OutUnknown (shape index, part name). A null set resolves nothing
	 */
	static FGSTN_PlatformPartGates ResolveGates(const FGSTN_PlatformSpec& Spec, const UGSTN_PartSetDefinition* Set, TArray<TPair<int32, FName>>* OutUnknown = nullptr);

	/** Errors (a part name the set lacks) and warnings (no platform next to it, no set to check against) of this owner. True if no error */
	bool CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	/** The authority's mask and frame, as this machine has it */
	const FGSTN_PlatformPartsNetState& GetNetState() const { return NetState; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	void BindPlatform();
	void UnbindPlatform();
	void HandlePrePose(UGSTN_EnemyPlatformComponent& InPlatform, const FGSTN_PlatformPoseContext& Context);

	/** The confirmed broken mask of the owner (0 without combat state or parts) */
	uint16 ReadBrokenMask();

	/** The set validation checks against: ValidationPartSet, else the owner's attach component's DefaultPartSet */
	const UGSTN_PartSetDefinition* FindValidationSet() const;

	TWeakObjectPtr<UGSTN_EnemyPlatformComponent> Platform;
	TWeakObjectPtr<UGSTN_CombatSimComponent> CombatSim;
	FDelegateHandle PrePoseHandle;

	/** The set the gates were resolved for; a different attached set resolves again */
	TWeakObjectPtr<const UGSTN_PartSetDefinition> ResolvedSet;
	bool bResolved = false;
	FGSTN_PlatformPartGates Gates;

	/** Written by the authority, replicated (read at the clients' pose point) */
	UPROPERTY(Replicated)
	FGSTN_PlatformPartsNetState NetState;

	TOptional<uint16> BrokenMaskOverride;
	uint16 LastBrokenMask = 0;
	uint8 AppliedMask = 0xFF;
};
