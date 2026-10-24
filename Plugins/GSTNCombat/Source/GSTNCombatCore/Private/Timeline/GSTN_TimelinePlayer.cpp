#include "Timeline/GSTN_TimelinePlayer.h"

namespace
{
	/** Did a frame-keyed thing at Key happen between the previous evaluation and this one? */
	FORCEINLINE bool Crossed(int32 Previous, int32 Key, int32 Current)
	{
		return Previous < Key && Key <= Current;
	}

	template <typename T>
	FORCEINLINE void EnsureCapacity(TArray<const T*>& Array, int32 Count)
	{
		if (Array.Max() < Count)
		{
			Array.Reserve(Count);
		}
	}

	void EvaluateMovement(const FGSTN_TimelineTrack_Movement& Track, int32 Previous, int32 Current, FGSTN_TimelineOutput& Output)
	{
		if (Track.Contains(Current))
		{
			Output.ActiveMovement.Add(&Track);
		}
		if (Crossed(Previous, Track.StartFrame, Current))
		{
			Output.MovementStarts.Add(&Track);
		}
		if (Crossed(Previous, Track.EndFrame, Current))
		{
			Output.MovementEnds.Add(&Track);
		}
	}
}

void FGSTN_TimelineOutput::Reserve(const FGSTN_CombatTimelineData& Data)
{
	int32 Cancel = 0, TagTracks = 0, Moves = 0, Event = 0, Switch = 0;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		const FGSTN_TimelineTrack* Track = Instance.GetPtr<FGSTN_TimelineTrack>();
		if (!Track)
		{
			continue;
		}
		switch (Track->GetKind())
		{
		case EGSTN_TimelineTrackKind::CancelWindow: ++Cancel; break;
		case EGSTN_TimelineTrackKind::Tags: ++TagTracks; break;
		case EGSTN_TimelineTrackKind::Movement: ++Moves; break;
		case EGSTN_TimelineTrackKind::Event: ++Event; break;
		case EGSTN_TimelineTrackKind::WeaponSwitch: ++Switch; break;
		default: break;
		}
	}

	// The recoil movement (of the timeline's response or of a fallback) is reported through the same lists
	++Moves;

	EnsureCapacity(CancelWindows, Cancel);
	EnsureCapacity(TagWindows, TagTracks);
	EnsureCapacity(ActiveMovement, Moves);
	EnsureCapacity(MovementStarts, Moves);
	EnsureCapacity(MovementEnds, Moves);
	EnsureCapacity(Events, Event);
	EnsureCapacity(WeaponSwitches, Switch);
}

void FGSTN_TimelineOutput::Reset()
{
	Frame = 0;
	TotalFrames = 0;
	bFinished = false;
	bBusy = false;
	bRecoil = false;
	bTailCancel = false;
	TailStartFrame = INDEX_NONE;
	TailBlendOutFrames = 0;
	bInOverrideWindow = false;
	DeferredCommitFrame = 0;
	bCommitThisFrame = false;
	RecoilCueTag = FGameplayTag();
	bRecoilStartsNow = false;
	CancelWindows.Reset();
	TagWindows.Reset();
	ActiveMovement.Reset();
	MovementStarts.Reset();
	MovementEnds.Reset();
	Events.Reset();
	WeaponSwitches.Reset();
}

bool FGSTN_TimelinePlayer::IsInTail(const FGSTN_CombatTimelineData& Data, int32 Frame)
{
	const int32 Start = Data.GetTailStartFrame();
	return Start != INDEX_NONE && Frame >= Start && Frame < Data.TotalFrames;
}

void FGSTN_TimelinePlayer::Evaluate(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineEvalParams& Params, FGSTN_TimelineOutput& Output)
{
	Output.Reserve(Data);
	Output.Reset();

	// The recoil replaces the whole timeline: its own length, its own movement, no tail and no override window
	if (Params.bRicochet)
	{
		const FGSTN_RicochetResponse* Response = Data.GetRicochetResponse();
		if (!Response && Params.RicochetFallback && Params.RicochetFallback->IsAuthored())
		{
			Response = Params.RicochetFallback;
		}
		if (Response)
		{
			const int32 Current = Params.RecoilFrame;
			Output.Frame = Current;
			Output.bRecoil = true;
			Output.TotalFrames = Response->RecoilFrames;
			Output.bFinished = Current >= Response->RecoilFrames;
			Output.bBusy = !Output.bFinished;
			if (Crossed(Params.PreviousRecoilFrame, 0, Current))
			{
				Output.bRecoilStartsNow = true;
				Output.RecoilCueTag = Response->CueTag;
			}
			if (Response->bHasMovement)
			{
				EvaluateMovement(Response->Movement, Params.PreviousRecoilFrame, Current, Output);
			}
			return;
		}
	}

	const int32 Current = Params.Frame - Params.PausedFrames;
	const int32 Previous = Params.PreviousFrame;

	Output.Frame = Current;
	Output.TotalFrames = Data.TotalFrames;
	Output.bFinished = Current >= Data.TotalFrames;
	Output.bBusy = Current < Data.FirstActionableFrame;

	Output.TailStartFrame = Data.GetTailStartFrame();
	Output.TailBlendOutFrames = Data.Tail.ResolveBlendOutFrames(Data.TotalFrames, Data.FirstActionableFrame);
	Output.bTailCancel = IsInTail(Data, Current);

	Output.DeferredCommitFrame = Data.ResolveOverrideFrames(Params.DefaultOverrideFrames);
	Output.bInOverrideWindow = Current >= 0 && Current < Output.DeferredCommitFrame;
	Output.bCommitThisFrame = Crossed(Previous, Output.DeferredCommitFrame, Current);

	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		const FGSTN_TimelineTrack* Track = Instance.GetPtr<FGSTN_TimelineTrack>();
		if (!Track)
		{
			continue;
		}

		switch (Track->GetKind())
		{
		case EGSTN_TimelineTrackKind::CancelWindow:
			if (Track->Contains(Current))
			{
				Output.CancelWindows.Add(static_cast<const FGSTN_TimelineTrack_CancelWindow*>(Track));
			}
			break;
		case EGSTN_TimelineTrackKind::Tags:
			if (Track->Contains(Current))
			{
				Output.TagWindows.Add(static_cast<const FGSTN_TimelineTrack_Tags*>(Track));
			}
			break;
		case EGSTN_TimelineTrackKind::Movement:
			EvaluateMovement(*static_cast<const FGSTN_TimelineTrack_Movement*>(Track), Previous, Current, Output);
			break;
		case EGSTN_TimelineTrackKind::Event:
			if (Crossed(Previous, Track->StartFrame, Current))
			{
				Output.Events.Add(static_cast<const FGSTN_TimelineTrack_Event*>(Track));
			}
			break;
		case EGSTN_TimelineTrackKind::WeaponSwitch:
		{
			const FGSTN_TimelineTrack_WeaponSwitch* Switch = static_cast<const FGSTN_TimelineTrack_WeaponSwitch*>(Track);
			if (Crossed(Previous, Switch->SwapFrame, Current))
			{
				Output.WeaponSwitches.Add(Switch);
			}
			break;
		}
		case EGSTN_TimelineTrackKind::Hitbox:
		case EGSTN_TimelineTrackKind::HurtboxModifier:
		default:
			// Reported by EvaluateHits, not by this per-frame output
			break;
		}
	}
}

void FGSTN_TimelineFrameResult::Reserve(const FGSTN_CombatTimelineData& Data)
{
	int32 HitboxCount = 0;
	int32 ModifierCount = 0;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		const FGSTN_TimelineTrack* Track = Instance.GetPtr<FGSTN_TimelineTrack>();
		if (!Track)
		{
			continue;
		}
		if (Track->GetKind() == EGSTN_TimelineTrackKind::Hitbox)
		{
			HitboxCount += static_cast<const FGSTN_TimelineTrack_Hitbox*>(Track)->Hitboxes.Num();
		}
		else if (Track->GetKind() == EGSTN_TimelineTrackKind::HurtboxModifier)
		{
			++ModifierCount;
		}
	}
	if (Hitboxes.Max() < HitboxCount)
	{
		Hitboxes.Reserve(HitboxCount);
	}
	EnsureCapacity(HurtModifiers, ModifierCount);
}

void FGSTN_TimelineFrameResult::Reset()
{
	Frame = 0;
	Hitboxes.Reset();
	HurtModifiers.Reset();
}

void FGSTN_TimelinePlayer::EvaluateHits(const FGSTN_CombatTimelineData& Data, int32 Frame, FGSTN_TimelineFrameResult& Result)
{
	Result.Reserve(Data);
	Result.Reset();
	Result.Frame = Frame;

	for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Data.Tracks)
	{
		const FGSTN_TimelineTrack* Track = Instance.GetPtr<FGSTN_TimelineTrack>();
		if (!Track || !Track->Contains(Frame))
		{
			continue;
		}
		if (Track->GetKind() == EGSTN_TimelineTrackKind::Hitbox)
		{
			const FGSTN_TimelineTrack_Hitbox* HitboxTrack = static_cast<const FGSTN_TimelineTrack_Hitbox*>(Track);
			for (const FGSTN_HitboxSpec& Spec : HitboxTrack->Hitboxes)
			{
				FGSTN_ActiveHitbox& Active = Result.Hitboxes.AddDefaulted_GetRef();
				Active.Spec = &Spec;
				Active.Track = HitboxTrack;
				Active.TrackFrame = Frame - Track->StartFrame;
			}
		}
		else if (Track->GetKind() == EGSTN_TimelineTrackKind::HurtboxModifier)
		{
			Result.HurtModifiers.Add(static_cast<const FGSTN_TimelineTrack_HurtboxModifier*>(Track));
		}
	}
}

FGSTN_HurtboxFrameState FGSTN_TimelinePlayer::ResolveHurtState(const FGSTN_TimelineFrameResult& Result, const FGSTN_Hurtbox& Hurtbox)
{
	FGSTN_HurtboxFrameState State;
	for (const FGSTN_TimelineTrack_HurtboxModifier* Modifier : Result.HurtModifiers)
	{
		if (!Modifier->ShapeOverrides.IsEmpty() || !Modifier->Addresses(Hurtbox))
		{
			continue;
		}
		switch (Modifier->Kind)
		{
		case EGSTN_HurtboxModifierKind::Intangible:
			State.State = EGSTN_HurtState::Intangible;
			break;
		case EGSTN_HurtboxModifierKind::Invincible:
			if (State.State != EGSTN_HurtState::Intangible)
			{
				State.State = EGSTN_HurtState::Invincible;
			}
			break;
		case EGSTN_HurtboxModifierKind::Armor:
			if (State.State == EGSTN_HurtState::Normal || State.State == EGSTN_HurtState::Armor)
			{
				State.State = EGSTN_HurtState::Armor;
				State.ArmorThreshold = FGSTN_StaggerThreshold::Max(State.ArmorThreshold, Modifier->ArmorThreshold);
			}
			break;
		}
	}
	return State;
}
