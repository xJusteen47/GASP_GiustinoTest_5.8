// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Math/GSTN_DefenceViews.h"
#include "Simulation/GSTN_CombatTickExtension.h"

class UAbilitySystemComponent;
class UGSTN_DefenceRules;
struct FGSTN_DefenceEntityData;
struct FGSTN_DefenceFragment;

/**
 * The defence model's per-frame rules over the defence fragment (m6 spec 5.2 steps 3 and 7, 5.3), run inside the combat tick for every
 * entity that has defence data attached (FGSTN_DefenceAttach); one map lookup for every other entity. Priority 110: after the parts
 * extension and the defaults, so it sees what they wrote.
 *
 *  - AfterClocks (the defence clocks): a break that reached BreakEndFrame recovers the shield (RecoverFractionAfterBreak of the maximum,
 *    stamped at the frame); the Empty flag follows the analytic value (zero while raised ends the hold window through the hook); a shield
 *    restore request on the attribute mirror (UGSTN_GE_ShieldRestore::RequestedDelta) is applied to the value as a re-stamp at the frame;
 *    the mirror attribute is written at every re-stamp and every MirrorIntervalFrames while the value changes (raised, or regenerating);
 *    the pushback a blocked hit queued for this frame is given to Mover as a push back move (UGSTN_GE_PushBack, the slide of the victim
 *    side: no Falling, no tumble, ground snap on).
 *  - AfterExecute (the stamps of the frame): the hold window stamps of the shield's hook become the raise (the first stamp of a timeline:
 *    RaiseShield at the press frame) and the release (an ended stamp: ReleaseShield at that frame); a shield still raised with no stamp
 *    (its timeline was cancelled or ended) is released at the frame. The parry window stamps become MoveParryStartFrame/EndFrame/Profile
 *    (the track resolved again from the running timeline: profile by name against the entity's rules, hurtbox selector against its set).
 *  - BeforeTagProjection: Combat.State.Shielding (the bubble blocks), ShieldStun, ShieldBroken, ParryWindow (either source open),
 *    ParryRecovery, Parried, Teching, Knockdown and Tech.Lockout are SET from the fragment the frame wrote, never added, so a re-simulated
 *    frame ends with the same tags and Mover sees them in the same frame.
 *  - BlocksActions: ShieldStun, ShieldBroken, Punished and Knockdown forbid every command (the can-act veto of F2).
 *  - HeldCancelConditions: OnParry holds while the running timeline parried (LastParryFrame at or after its start).
 *
 * Simulation code: deterministic, no net mode, no resimulation branch, state only in the fragment. The attached data, the ability system's
 * attribute values (the rates at the end of F-1) and the running timeline's track are content, never state. Effects it applies (the mirror,
 * the push back) go through the ability system of the entity, which rolls back with the rest.
 */
class GSTNDEFENCENP_API FGSTN_DefenceTickExtension : public IGSTN_CombatTickExtension
{
public:
	/** After the parts (100) and the defaults (0) */
	static constexpr int32 ExtensionPriority = 110;

	/** Frames between two mirror writes while the value changes (m6 spec 4) */
	static constexpr int32 MirrorIntervalFrames = 16;

	/** Fixed tick of the model, seconds: the push back move's lifetime is frames times this */
	static constexpr float FrameSeconds = 0.016f;

	virtual uint8 GetTypeId() const override;
	virtual int32 GetPriority() const override { return ExtensionPriority; }
	virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override;
	virtual bool BlocksActions(const FGSTN_CombatTickContext& Context) const override;
	virtual uint32 HeldCancelConditions(const FGSTN_CombatTickContext& Context) const override;

	/** The rates the model drains and regenerates with: the attribute set's where the entity has one, else the rules' */
	static FGSTN_ShieldRates ReadRates(const UAbilitySystemComponent* AbilitySystem, const UGSTN_DefenceRules& Rules);

	/** Whether the bubble blocks at Frame: raised past the startup frames, not breaking, the shield available (the Combat.State.Shielding rule) */
	static bool ShieldBlocksAt(const FGSTN_DefenceFragment& Fragment, int32 Frame, const UGSTN_DefenceRules& Rules, bool bAvailable);

private:
	void StepClocks(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data);
	void ConsumeStamps(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data);
	void ProjectTags(FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData* Data) const;

	/** Writes the mirror attribute (where the entity has the set) and the fragment's mirror bookkeeping */
	static void WriteMirror(UAbilitySystemComponent* AbilitySystem, FGSTN_DefenceFragment& Fragment, double Value, int32 Frame);

	/** Queues the push back move of the fragment on the entity's Mover through its ability system */
	static void ApplyPushback(UAbilitySystemComponent* AbilitySystem, const FGSTN_DefenceFragment& Fragment);

	/** The parry window track a stamp names, resolved from the running timeline: the profile index and the hurtbox mask the fragment stores */
	static void ResolveParryTrack(const FGSTN_CombatTickContext& Context, const FGSTN_DefenceEntityData& Data, int32 TrackIndex, uint8& OutProfile, uint16& OutHurtboxMask);
};
