// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_MinionDefinition.h"

#include "GSTN_MinionTags.h"

#if WITH_EDITOR
#include "Hits/GSTN_HurtboxTypes.h"
#include "Misc/DataValidation.h"
#include "Timeline/GSTN_CombatTimeline.h"
#endif

FPrimaryAssetId UGSTN_MinionDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNMinion::DefinitionAssetType(), GetFName());
}

int32 UGSTN_MinionDefinition::FindMoveIndex(const FGameplayTag& Command) const
{
	if (!Command.IsValid())
	{
		return INDEX_NONE;
	}
	return Moves.IndexOfByPredicate([&Command](const FGSTN_MinionMove& Move) { return Move.Command == Command; });
}

#if WITH_EDITOR

namespace
{
	/**
	 * Checks one timeline reference: set, loadable and short enough for the 10-bit start stamp. Returns false when an error was
	 * reported. bRequired false lets an empty reference pass.
	 */
	bool CheckTimeline(FDataValidationContext& Context, const TSoftObjectPtr<UGSTN_CombatTimeline>& Ref, const FString& Label, bool bRequired)
	{
		if (Ref.IsNull())
		{
			if (bRequired)
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("%s has no timeline."), *Label)));
			}
			return !bRequired;
		}

		const UGSTN_CombatTimeline* Timeline = Ref.LoadSynchronous();
		if (!Timeline)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s points at a timeline that does not load."), *Label)));
			return false;
		}
		if (Timeline->Data.TotalFrames > GSTNMinion::MaxTimelineFrames)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("%s is %d frames long; a minion timeline may have at most %d."),
				*Label, Timeline->Data.TotalFrames, GSTNMinion::MaxTimelineFrames)));
			return false;
		}
		return true;
	}
}

EDataValidationResult UGSTN_MinionDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	bool bValid = true;
	const auto Fail = [&bValid, &Context](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		bValid = false;
	};

	if (!TypeTag.IsValid() || TypeTag == GSTNMinionTags::Type || !TypeTag.MatchesTag(GSTNMinionTags::Type))
	{
		Fail(TEXT("TypeTag must be a leaf under Minion.Type."));
	}

	if (!(Stats.MaxHealth > 0.f))
	{
		Fail(TEXT("MaxHealth must be above 0."));
	}
	if (!(Stats.Weight > 0.f))
	{
		Fail(TEXT("Weight must be above 0."));
	}
	if (!(Stats.CapsuleRadius > 0.f) || !(Stats.CapsuleHalfHeight > 0.f))
	{
		Fail(TEXT("The capsule radius and half height must be above 0."));
	}
	if (Stats.RunSpeed < Stats.WalkSpeed)
	{
		Context.AddWarning(FText::FromString(TEXT("RunSpeed is below WalkSpeed; the net speed field is scaled by RunSpeed.")));
	}
	if (MaxHitsPerFrame < 1)
	{
		Fail(TEXT("MaxHitsPerFrame must be at least 1."));
	}

	if (HurtboxSet.IsNull())
	{
		Fail(TEXT("A hurtbox set is required."));
	}
	else if (!HurtboxSet.LoadSynchronous())
	{
		Fail(TEXT("The hurtbox set does not load."));
	}

	if (Moves.Num() > GSTNMinion::MaxMoves)
	{
		Fail(FString::Printf(TEXT("%d moves; at most %d fit the 6-bit MoveId."), Moves.Num(), GSTNMinion::MaxMoves));
	}
	TSet<FGameplayTag> SeenCommands;
	for (int32 Index = 0; Index < Moves.Num(); ++Index)
	{
		const FGSTN_MinionMove& Move = Moves[Index];
		const FString Label = FString::Printf(TEXT("Move %d"), Index);
		if (!Move.Command.IsValid())
		{
			Fail(FString::Printf(TEXT("%s has no command."), *Label));
		}
		else
		{
			bool bAlreadySeen = false;
			SeenCommands.Add(Move.Command, &bAlreadySeen);
			if (bAlreadySeen)
			{
				Fail(FString::Printf(TEXT("%s repeats the command %s."), *Label, *Move.Command.ToString()));
			}
		}
		if (Move.MinRange > Move.MaxRange)
		{
			Fail(FString::Printf(TEXT("%s has MinRange above MaxRange."), *Label));
		}
		bValid &= CheckTimeline(Context, Move.Timeline, Label, true);
	}

	bValid &= CheckTimeline(Context, Reactions.Hitstun, TEXT("The Hitstun reaction"), true);
	bValid &= CheckTimeline(Context, Reactions.Death, TEXT("The Death reaction"), true);
	bValid &= CheckTimeline(Context, Reactions.Spawn, TEXT("The Spawn reaction"), false);
	bValid &= CheckTimeline(Context, Reactions.PushBack, TEXT("The PushBack reaction"), false);
	bValid &= CheckTimeline(Context, Reactions.Launched, TEXT("The Launched reaction"), false);
	bValid &= CheckTimeline(Context, Reactions.Knockdown, TEXT("The Knockdown reaction"), false);
	bValid &= CheckTimeline(Context, Reactions.GetUp, TEXT("The GetUp reaction"), false);

	if (bOverrideLOD && !LODOverride.IsConsistent())
	{
		Fail(TEXT("The LOD override is inconsistent: the borders must increase, the hysteresis stay below 0.5 and every interval be at least 1."));
	}

	if (!bValid)
	{
		return EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}

#endif // WITH_EDITOR
