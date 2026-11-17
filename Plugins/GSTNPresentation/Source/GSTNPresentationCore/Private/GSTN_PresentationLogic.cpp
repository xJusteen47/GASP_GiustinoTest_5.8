// Presentation events and their effects (no GAS, no Network Prediction, no Mover).

#include "GSTN_PresentationLogic.h"

#include "GSTN_PresentationTags.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PresentationLogic)

// Scale and table ----------------------------------------------------------------------------------------------------------------------

float FGSTN_PresentationScale::Evaluate(float Magnitude) const
{
	const double Low = InputRange.X;
	const double High = InputRange.Y;
	// An empty range is a step at High; the divisor is never zero (C4723)
	const double Span = High - Low;
	double T = FMath::Abs(Span) <= UE_KINDA_SMALL_NUMBER ? (Magnitude >= High ? 1.0 : 0.0) : (Magnitude - Low) / (FMath::Abs(Span) > 0.0 ? Span : 1.0);
	if (bClampInput)
	{
		T = FMath::Clamp(T, 0.0, 1.0);
	}
	const double Exp = FMath::Max(static_cast<double>(Exponent), 0.05);
	T = T >= 0.0 ? FMath::Pow(T, Exp) : -FMath::Pow(-T, Exp);
	return static_cast<float>(FMath::Lerp(OutputRange.X, OutputRange.Y, T));
}

const FGSTN_PresentationCue* UGSTN_PresentationTable::FindOwnCue(const FGameplayTag& EventTag) const
{
	if (!EventTag.IsValid())
	{
		return nullptr;
	}
	// The exact row, else the nearest ancestor: of the rows whose tag the event matches, the longest name is the most specific
	const FGSTN_PresentationCue* Best = nullptr;
	int32 BestLength = -1;
	for (const FGSTN_PresentationCue& Cue : Cues)
	{
		if (!Cue.EventTag.IsValid())
		{
			continue;
		}
		if (Cue.EventTag == EventTag)
		{
			return &Cue;
		}
		if (EventTag.MatchesTag(Cue.EventTag))
		{
			const int32 Length = Cue.EventTag.GetTagName().GetStringLength();
			if (Length > BestLength)
			{
				Best = &Cue;
				BestLength = Length;
			}
		}
	}
	return Best;
}

const FGSTN_PresentationCue* UGSTN_PresentationTable::FindCue(const FGameplayTag& EventTag) const
{
	const UGSTN_PresentationTable* Table = this;
	for (int32 Depth = 0; Table && Depth < 16; ++Depth, Table = Table->Parent)
	{
		if (const FGSTN_PresentationCue* Cue = Table->FindOwnCue(EventTag))
		{
			return Cue;
		}
	}
	return nullptr;
}

bool UGSTN_PresentationTable::FindCueBP(FGameplayTag EventTag, FGSTN_PresentationCue& OutCue) const
{
	if (const FGSTN_PresentationCue* Cue = FindCue(EventTag))
	{
		OutCue = *Cue;
		return true;
	}
	return false;
}

#if WITH_EDITOR
EDataValidationResult UGSTN_PresentationTable::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TSet<FGameplayTag> SeenTags;
	for (int32 Index = 0; Index < Cues.Num(); ++Index)
	{
		const FGSTN_PresentationCue& Cue = Cues[Index];
		if (!Cue.EventTag.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Cues[%d] has no event tag"), Index)));
			Result = EDataValidationResult::Invalid;
		}
		else if (SeenTags.Contains(Cue.EventTag))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Cues[%d]: a second row for %s (only the first ever plays)"), Index, *Cue.EventTag.ToString())));
			Result = EDataValidationResult::Invalid;
		}
		SeenTags.Add(Cue.EventTag);
	}
	const UGSTN_PresentationTable* Table = Parent;
	for (int32 Depth = 0; Table; ++Depth, Table = Table->Parent)
	{
		if (Table == this || Depth >= 16)
		{
			Context.AddError(FText::FromString(TEXT("The Parent chain loops back to this table (or is deeper than 16)")));
			return EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

// Derivation -----------------------------------------------------------------------------------------------------------------------------

FGameplayTagContainer FGSTN_PresentationDeriveRules::MakeDefaultQuietTags()
{
	FGameplayTagContainer Out;
	const FGameplayTag Knockdown = FGameplayTag::RequestGameplayTag(FName(TEXT("Combat.State.Knockdown")), false);
	if (Knockdown.IsValid())
	{
		Out.AddTag(Knockdown);
	}
	return Out;
}

TArray<FGSTN_PresentationTagRule> FGSTN_PresentationDeriveRules::MakeDefaultTagRules()
{
	// By name: the defence plugin owns these tags and may be disabled (an invalid watched tag never matches)
	auto Rule = [](const TCHAR* Watched, const FGameplayTag& OnAdded, const FGameplayTag& OnRemoved = FGameplayTag(), const TCHAR* HeldWhile = nullptr)
	{
		FGSTN_PresentationTagRule Out;
		Out.WatchedTag = FGameplayTag::RequestGameplayTag(FName(Watched), false);
		Out.OnAdded = OnAdded;
		Out.OnRemoved = OnRemoved;
		if (HeldWhile)
		{
			Out.HeldWhileTag = FGameplayTag::RequestGameplayTag(FName(HeldWhile), false);
		}
		return Out;
	};
	TArray<FGSTN_PresentationTagRule> Rules;
	Rules.Add(Rule(TEXT("Combat.State.Shielding"), GSTNPresentationTags::Shield_On));
	Rules.Add(Rule(TEXT("Combat.State.ShieldStun"), GSTNPresentationTags::Shield_Hit));
	Rules.Add(Rule(TEXT("Combat.State.ShieldBroken"), GSTNPresentationTags::Shield_Break));
	// The trail starts at the launch, not in the hitstop freeze of the hit that caused it (polish M2 review r1)
	Rules.Add(Rule(TEXT("Combat.State.Tumble"), GSTNPresentationTags::Tumble_Start, GSTNPresentationTags::Tumble_End, TEXT("Combat.State.Hitstop")));
	Rules.Add(Rule(TEXT("Combat.State.Knockdown"), GSTNPresentationTags::Knockdown, GSTNPresentationTags::GetUp));
	// The small default bump off a wall or ceiling (combat rules WallImpact / CeilingImpact): its window is projected as a state tag
	Rules.Add(Rule(TEXT("Combat.State.Bump.Wall"), GSTNPresentationTags::Bump_Wall));
	Rules.Add(Rule(TEXT("Combat.State.Bump.Ceiling"), GSTNPresentationTags::Bump_Ceiling));
	return Rules;
}

namespace GSTNPresentationDerive
{
	static FVector Horizontal(const FVector& V, const FVector& Up)
	{
		return V - Up * FVector::DotProduct(V, Up);
	}

	static void Emit(TArray<FGSTN_PresentationEvent>& Out, const FGameplayTag& Tag, int32 Frame, float Magnitude, const FVector& Location,
		const FVector& Direction, bool bExactFrame = false)
	{
		if (!Tag.IsValid())
		{
			return;
		}
		FGSTN_PresentationEvent& Event = Out.AddDefaulted_GetRef();
		Event.EventTag = Tag;
		Event.Frame = Frame;
		Event.Magnitude = Magnitude;
		Event.Location = Location;
		Event.Direction = Direction;
		Event.bExactFrame = bExactFrame;
	}

	static FGameplayTag SkidTag(EGSTN_PresentationSkidPhase Phase)
	{
		switch (Phase)
		{
		case EGSTN_PresentationSkidPhase::Enter: return GSTNPresentationTags::Skid_Enter;
		case EGSTN_PresentationSkidPhase::Loop: return GSTNPresentationTags::Skid_Loop;
		case EGSTN_PresentationSkidPhase::Exit: return GSTNPresentationTags::Skid_Exit;
		default: return FGameplayTag();
		}
	}
}

void FGSTN_PresentationDeriver::Step(const FGSTN_PresentationSnapshot& C, const FGSTN_PresentationDeriveRules& Rules, TArray<FGSTN_PresentationEvent>& Out)
{
	using namespace GSTNPresentationDerive;
	using namespace GSTNPresentationTags;
	if (!C.bValid)
	{
		return;
	}
	if (!Previous.bValid)
	{
		Previous = C;
		return;
	}
	const FGSTN_PresentationSnapshot& P = Previous;
	const FVector Up = C.Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const float UpBefore = FVector::DotProduct(P.Velocity, Up);
	const float UpNow = FVector::DotProduct(C.Velocity, Up);
	const FVector HBefore = Horizontal(P.Velocity, Up);
	const FVector HNow = Horizontal(C.Velocity, Up);
	const int32 F = C.Frame;
	const bool bNewHit = C.LastHitFrame != INDEX_NONE && C.LastHitFrame != P.LastHitFrame;
	if (bNewHit)
	{
		// A new launch: its bounces are its own
		LastGroundBounceFrame = INDEX_NONE;
		LastSurfaceBounceFrame = INDEX_NONE;
	}

	// Air: jumps and landings (a reacting victim's contacts are bounces or a knockdown, below; a downed one makes none)
	const bool bQuiet = C.StateTags.HasAny(Rules.QuietAirTags) || P.StateTags.HasAny(Rules.QuietAirTags);
	if (P.bGrounded && !C.bGrounded && !C.bReacting && !bQuiet && UpNow >= Rules.JumpMinUpSpeed)
	{
		// At the last grounded location: one frame later the character is already a jump step in the air (review r1: ~20 cm high)
		Emit(Out, Jump_Ground, F, UpNow, P.Location, Up);
	}
	else if (!P.bGrounded && !C.bGrounded && C.AirJumpsUsed > P.AirJumpsUsed)
	{
		Emit(Out, Jump_Air, F, FMath::Max(UpNow, 0.f), C.Location, Up);
	}
	if (!P.bGrounded && C.bGrounded && !P.bReacting && !C.bReacting && !bQuiet && -UpBefore >= Rules.LandMinFallSpeed)
	{
		Emit(Out, Land, F, -UpBefore, C.Location, Up);
	}

	// Bounces of a reacting victim. The simulation's own count first: a bounce it decided is one, even when the velocity never showed the way in. In that step the
	// velocity reading below stays silent, so one bounce is one event
	const auto CountRose = [bNewHit](int32 Before, int32 Now)
	{
		return Before != INDEX_NONE && Now != INDEX_NONE && Now > 0 && (Now > Before || (bNewHit && Now != Before));
	};
	const bool bCountedGround = C.bReacting && !C.bSplat && CountRose(P.GroundBounceCount, C.GroundBounceCount);
	const bool bCountedWall = C.bReacting && !C.bSplat && CountRose(P.WallBounceCount, C.WallBounceCount);
	if (bCountedGround)
	{
		Emit(Out, GroundBounce, F, FMath::Max(-UpBefore, UpNow), C.Location, Up);
		LastGroundBounceFrame = F;
	}
	if (bCountedWall)
	{
		// Walls and ceilings share the count; the simulation says which face it was (SurfaceImpactFrame), else a ceiling turns the rise into a
		// fall and a wall turns the speed along the floor
		const bool bSaysFace = C.SurfaceImpactFrame != INDEX_NONE;
		const bool bCeiling = bSaysFace ? C.bSurfaceImpactCeiling
			: (UpBefore > 0.f && UpNow < 0.f && FMath::Abs(UpBefore - UpNow) >= (HNow - HBefore).Size());
		if (bCeiling)
		{
			Emit(Out, WallBounce_Ceiling, F, FMath::Max(UpBefore, -UpNow), C.Location, -Up);
		}
		else
		{
			const FVector Change = HNow - HBefore;
			const FVector Normal = !Change.IsNearlyZero() ? Change.GetSafeNormal() : HNow.GetSafeNormal();
			const float Into = FMath::Max(-FVector::DotProduct(HBefore, Normal), FVector::DotProduct(HNow, Normal));
			Emit(Out, WallBounce, F, Into, C.Location, Normal);
		}
		LastSurfaceBounceFrame = F;
	}
	// Then the velocity reflected in one frame (gravity and drag change it far less than BounceMinSpeed per frame), for the bounces no count
	// covers (a source without counts, a surface row's or a hit's forced wall bounce). A new hit also turns the velocity around: never a bounce.
	// Nor does a wall or ceiling contact the simulation decided (a bump pushes off the face) or a second turn soon after a bounce of the same kind
	const auto Recent = [F, &Rules](int32 Then)
	{
		return Then != INDEX_NONE && F >= Then && F - Then <= Rules.BounceRefireFrames;
	};
	const bool bSurfaceDecided = Recent(C.SurfaceImpactFrame);
	if (P.bReacting && C.bReacting && !C.bSplat && !bNewHit && !bCountedGround && !bCountedWall)
	{
		if (UpBefore <= -Rules.BounceMinSpeed && UpNow > 0.f)
		{
			if (!Recent(LastGroundBounceFrame))
			{
				Emit(Out, GroundBounce, F, -UpBefore, C.Location, Up);
				LastGroundBounceFrame = F;
			}
		}
		else if (UpBefore >= Rules.BounceMinSpeed && UpNow < 0.f)
		{
			if (!bSurfaceDecided && !Recent(LastSurfaceBounceFrame))
			{
				Emit(Out, WallBounce_Ceiling, F, UpBefore, C.Location, -Up);
				LastSurfaceBounceFrame = F;
			}
		}
		else if (!bSurfaceDecided && !Recent(LastSurfaceBounceFrame))
		{
			// A reflection changes the velocity along the surface normal only: the change's direction is the normal (pointing away from the wall)
			const FVector Normal = (HNow - HBefore).GetSafeNormal();
			const float Into = -FVector::DotProduct(HBefore, Normal);
			if (!Normal.IsZero() && Into >= Rules.BounceMinSpeed && FVector::DotProduct(HNow, Normal) > 0.f)
			{
				Emit(Out, WallBounce, F, Into, C.Location, Normal);
				LastSurfaceBounceFrame = F;
			}
		}
	}
	if (!P.bSplat && C.bSplat)
	{
		Emit(Out, Splat, F, P.Velocity.Size(), C.Location, -P.Velocity.GetSafeNormal());
	}
	if (bNewHit)
	{
		Emit(Out, Hit, C.LastHitFrame, C.LastHitStrength, C.Location, C.LastHitDirection, true);
	}

	// Run stages and stop
	if (C.bGrounded)
	{
		if (P.RunStage <= 0 && C.RunStage >= 1)
		{
			Emit(Out, RunStart, F, HNow.Size(), C.Location, HNow.GetSafeNormal());
		}
		else if (P.RunStage >= 1 && C.RunStage > P.RunStage)
		{
			Emit(Out, StageUp, F, static_cast<float>(C.RunStage), C.Location, HNow.GetSafeNormal());
		}
		else if (P.RunStage >= 1 && C.RunStage <= 0 && P.SkidPhase == EGSTN_PresentationSkidPhase::None
			&& C.SkidPhase == EGSTN_PresentationSkidPhase::None)
		{
			Emit(Out, StopRelease, F, HBefore.Size(), C.Location, HBefore.GetSafeNormal());
		}
	}

	// Any drop of the stage, in the air too: the end of a sustained stage effect (sprint lines) must never be missed
	if (C.RunStage < P.RunStage)
	{
		Emit(Out, StageDown, F, static_cast<float>(FMath::Max(C.RunStage, 0)), C.Location, HNow.GetSafeNormal());
	}

	// Skid phases; then a run the other way soon after the skid is the turn
	if (C.SkidPhase != P.SkidPhase && C.SkidPhase != EGSTN_PresentationSkidPhase::None)
	{
		if (C.SkidPhase == EGSTN_PresentationSkidPhase::Enter || P.SkidPhase == EGSTN_PresentationSkidPhase::None)
		{
			const FVector Dir = HBefore.IsNearlyZero() ? HNow : HBefore;
			SkidDirection = Dir.GetSafeNormal();
		}
		Emit(Out, SkidTag(C.SkidPhase), F, FMath::Max(HNow.Size(), HBefore.Size()), C.Location, SkidDirection);
	}
	if (P.SkidPhase != EGSTN_PresentationSkidPhase::None && C.SkidPhase == EGSTN_PresentationSkidPhase::None)
	{
		FramesSinceSkid = 0;
	}
	else if (FramesSinceSkid >= 0)
	{
		++FramesSinceSkid;
	}
	if (FramesSinceSkid >= 0)
	{
		if (FramesSinceSkid > Rules.TurnWindowFrames)
		{
			FramesSinceSkid = -1;
		}
		else if (C.bGrounded && C.RunStage >= 1 && !SkidDirection.IsZero()
			&& FVector::DotProduct(HNow.GetSafeNormal(), SkidDirection) < -0.5f)
		{
			Emit(Out, Turn180, F, HNow.Size(), C.Location, HNow.GetSafeNormal());
			FramesSinceSkid = -1;
		}
	}

	// Tag rules: a state tag or a move tag appears or goes away. A move tag counts again when a new move starts (its start frame changes)
	for (const FGSTN_PresentationTagRule& Rule : Rules.TagRules)
	{
		if (!Rule.WatchedTag.IsValid())
		{
			continue;
		}
		const FGameplayTagContainer& Before = Rule.bMoveTag ? P.MoveTags : P.StateTags;
		const FGameplayTagContainer& Now = Rule.bMoveTag ? C.MoveTags : C.StateTags;
		// A held rule also needs the hold tag absent (always looked up in the state tags)
		const bool bHeld = Rule.HeldWhileTag.IsValid();
		bool bHad = Before.HasTag(Rule.WatchedTag) && !(bHeld && P.StateTags.HasTag(Rule.HeldWhileTag));
		const bool bHas = Now.HasTag(Rule.WatchedTag) && !(bHeld && C.StateTags.HasTag(Rule.HeldWhileTag));
		const bool bRestarted = Rule.bMoveTag && bHad && bHas && C.MoveStartFrame != P.MoveStartFrame;
		if (bRestarted)
		{
			Emit(Out, Rule.OnRemoved, F, 0.f, C.Location, FVector::ZeroVector);
			bHad = false;
		}
		if (!bHad && bHas)
		{
			const bool bStamped = Rule.bMoveTag && C.MoveStartFrame != INDEX_NONE;
			Emit(Out, Rule.OnAdded, bStamped ? C.MoveStartFrame : F, 0.f, C.Location, HNow.GetSafeNormal(), bStamped);
		}
		else if (bHad && !bHas)
		{
			Emit(Out, Rule.OnRemoved, F, 0.f, C.Location, FVector::ZeroVector);
		}
	}

	Previous = C;
}

// Dedupe -------------------------------------------------------------------------------------------------------------------------------

bool FGSTN_PresentationDedupe::Accept(const FGameplayTag& Tag, int32 Frame, bool bExactFrame)
{
	if (Frame == INDEX_NONE)
	{
		return true;
	}
	const int32 Window = bExactFrame ? 0 : FMath::Max(FrameWindow, 0);
	for (const FEntry& Entry : Seen)
	{
		if (Entry.Tag == Tag && FMath::Abs(Entry.Frame - Frame) <= Window)
		{
			return false;
		}
	}
	Seen.Add({Tag, Frame});
	NewestFrame = FMath::Max(NewestFrame, Frame);
	const int32 Oldest = NewestFrame - FMath::Max(MemoryFrames, Window + 1);
	Seen.RemoveAll([Oldest](const FEntry& Entry) { return Entry.Frame < Oldest; });
	return true;
}
