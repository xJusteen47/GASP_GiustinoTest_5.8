// Mode verbs, pure core.

#include "GSTN_ModeVerbTypes.h"

#include "GSTN_ModeVerbTags.h"

FIntVector FGSTN_ModeVerbPayload::QuantiseAimCm(const FVector& Point)
{
	// The interact records' own rounding, so an aim point and a record compare in the same integers
	return FGSTN_InteractCandidate::QuantiseCm(Point);
}

FGSTN_SimEvent FGSTN_ModeVerbPayload::ToEvent(const FGameplayTagContainer& IdentityTags) const
{
	FGSTN_SimEvent Event;
	Event.EventType = GSTNModeVerbTags::Message_Mode_Verb;
	Event.Tags = IdentityTags;
	if (VerbTag.IsValid())
	{
		Event.Tags.AddTag(VerbTag);
	}
	Event.Frame = ConfirmFrame;
	Event.Instigator = Instigator;
	Event.Target = Target;
	Event.Magnitude = Magnitude;
	return Event;
}

FGSTN_ModeVerbPayload FGSTN_ModeVerbPayload::FromEvent(const FGSTN_SimEvent& Event)
{
	FGSTN_ModeVerbPayload Payload;
	const FGameplayTag Root = GSTNModeVerbTags::Ability_Mode;
	for (const FGameplayTag& Tag : Event.Tags)
	{
		// The verb is the Ability.Mode.<Name> leaf; the identity tags of the prop are other roots
		if (Tag != Root && Tag.MatchesTag(Root))
		{
			Payload.VerbTag = Tag;
			break;
		}
	}
	Payload.Instigator = Event.Instigator;
	Payload.Target = Event.Target;
	Payload.ConfirmFrame = Event.Frame;
	Payload.Magnitude = Event.Magnitude;
	return Payload;
}

void FGSTN_VerbConfirmQueue::Add(const FGSTN_VerbConfirmRequest& Request)
{
	if (WasPublished(Request) || Pending.ContainsByPredicate([&Request](const FGSTN_VerbConfirmRequest& Other) { return Other.SameConfirmation(Request); }))
	{
		return;
	}
	if (Pending.Num() >= MaxPending)
	{
		// A host that never flushes must not grow without bound: the oldest goes first (same rule as FGSTN_SimEventOutbox)
		Pending.RemoveAt(0);
	}
	Pending.Add(Request);
}

bool FGSTN_VerbConfirmQueue::WasPublished(const FGSTN_VerbConfirmRequest& Request) const
{
	return History.ContainsByPredicate([&Request](const FGSTN_VerbConfirmRequest& Other) { return Other.SameConfirmation(Request); });
}

int32 FGSTN_VerbConfirmQueue::Flush(bool bAuthority, bool bResimulating, int32 ServerFrame, TFunctionRef<void(const FGSTN_VerbConfirmRequest&)> Publish)
{
	if (!bAuthority)
	{
		Pending.Reset();
		return 0;
	}
	if (bResimulating)
	{
		return 0;
	}

	// Split first: a publisher that ends up confirming again must not append to the array being walked
	TArray<FGSTN_VerbConfirmRequest> Due;
	for (int32 Index = 0; Index < Pending.Num();)
	{
		if (Pending[Index].Payload.ConfirmFrame <= ServerFrame)
		{
			Due.Add(Pending[Index]);
			Pending.RemoveAt(Index);
		}
		else
		{
			++Index;
		}
	}

	int32 Published = 0;
	for (const FGSTN_VerbConfirmRequest& Request : Due)
	{
		if (WasPublished(Request))
		{
			continue;
		}
		if (History.Num() >= MaxHistory)
		{
			History.RemoveAt(0);
		}
		History.Add(Request);
		Publish(Request);
		++Published;
	}
	return Published;
}

EGSTN_VerbRefusal FGSTN_RequestableGate::Evaluate(const FInput& Input)
{
	if (Input.Frame >= Input.ToFrame)
	{
		return EGSTN_VerbRefusal::Removed;
	}
	if (Input.Frame < Input.ActionableFrom)
	{
		return EGSTN_VerbRefusal::NotActionable;
	}
	if (!Input.bVerbAccepted)
	{
		return EGSTN_VerbRefusal::WrongVerb;
	}
	if (Input.UsesLeft == 0)
	{
		return EGSTN_VerbRefusal::Exhausted;
	}
	if (Input.DistanceCm > Input.ReachCm + Input.SlackCm)
	{
		return EGSTN_VerbRefusal::OutOfRange;
	}
	return EGSTN_VerbRefusal::None;
}

float FGSTN_RequestableGate::DistanceToSurfaceCm(const FVector& From, const FIntVector& PositionCm, int32 RadiusCm)
{
	const FVector Anchor(PositionCm.X, PositionCm.Y, PositionCm.Z);
	return FMath::Max(0.f, static_cast<float>(FVector::Dist(From, Anchor)) - static_cast<float>(RadiusCm));
}
