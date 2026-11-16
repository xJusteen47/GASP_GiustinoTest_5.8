// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_HurtboxPose.h"

#include "Hits/GSTN_HitShapeUtils.h"
#include "Timeline/GSTN_TimelineTypes.h"

namespace
{
	double ShapeReach(const FGSTN_HitShape& Shape)
	{
		return Shape.Local.GetLocation().Size() + Shape.Extent.GetMax() * Shape.Local.GetScale3D().GetAbsMax();
	}

	EGSTN_HurtboxDetail ResolveDetail(const FGSTN_HurtboxPoseInput& Input)
	{
		const UGSTN_HurtboxSetDefinition& Set = *Input.Set;
		if (Input.Timeline)
		{
			switch (Input.Timeline->HurtboxDetail)
			{
			case EGSTN_HurtboxDetailOverride::Simple: return EGSTN_HurtboxDetail::Simple;
			case EGSTN_HurtboxDetailOverride::Detailed: return EGSTN_HurtboxDetail::Detailed;
			default: return Set.MoveDetail;
			}
		}
		return Input.KitMovement && Input.KitMovement->bOverrideDetail ? Input.KitMovement->MovementDetail : Set.MovementDetail;
	}

	/** The root-relative carrier of a Hurtbox track shape at a timeline frame; false for a space a hurtbox cannot live in, or a socket not baked */
	bool BuildExtensionCarrier(const FGSTN_TimelineTrack_Hurtbox& Track, const FGSTN_HitboxSpec& Spec, const FGSTN_CombatTimelineData& Timeline, int32 TimelineFrame,
		FTransform& OutCarrier)
	{
		// Same placement as an attack hitbox (Keys * group offset * space), with the root as identity: the result is root-relative
		const FTransform Key = UGSTN_HitShapeUtils::EvaluateKeys(Spec.Keys, TimelineFrame - Track.StartFrame) * Track.GetParentOffset(Spec);
		switch (Track.GetEffectiveSpace(Spec))
		{
		case EGSTN_HitboxSpace::Root:
			OutCarrier = Key;
			return true;
		case EGSTN_HitboxSpace::BakedSocket:
		{
			FTransform Socket;
			if (!Timeline.FindBakedSocket(Track.GetEffectiveSocket(Spec), TimelineFrame, Socket))
			{
				return false;
			}
			OutCarrier = Key * Socket;
			return true;
		}
		default:
			// WorldAtMoveStart and Projectile are attack spaces: a hurtbox stays on the body (the validator reports them)
			return false;
		}
	}
}

int32 FGSTN_HurtboxPose::ResolveCountsAs(const UGSTN_HurtboxSetDefinition& Set, FName CountsAs)
{
	if (!CountsAs.IsNone())
	{
		for (int32 Index = 0; Index < Set.Hurtboxes.Num(); ++Index)
		{
			if (Set.Hurtboxes[Index].Name == CountsAs)
			{
				return Index;
			}
		}
	}
	return 0;
}

double FGSTN_HurtboxPose::ComputeBoundRadius(const UGSTN_HurtboxSetDefinition* Set) const
{
	double Radius = 0.0;
	if (Set)
	{
		for (int32 Index = 0; Index < Set->Hurtboxes.Num(); ++Index)
		{
			if (IsPresent(Index))
			{
				Radius = FMath::Max(Radius, ShapeReach(GetShape(*Set, Index)));
			}
		}
	}
	for (const FGSTN_PosedHurtboxExtension& Extension : Extensions)
	{
		Radius = FMath::Max(Radius, ShapeReach(Extension.Shape));
	}
	return Radius;
}

void FGSTN_HurtboxPose::Build(const FGSTN_HurtboxPoseInput& Input, FGSTN_HurtboxPose& Out)
{
	Out.Reset();
	if (!Input.Set || Input.Set->Hurtboxes.IsEmpty())
	{
		return;
	}
	const UGSTN_HurtboxSetDefinition& Set = *Input.Set;
	const TArray<FGSTN_Hurtbox>& Boxes = Set.Hurtboxes;
	const int32 Num = Boxes.Num();

	// 1. Detail level: Present stays empty when every hurtbox is there (an old set of Both hurtboxes)
	const EGSTN_HurtboxDetail Detail = ResolveDetail(Input);
	const bool bKitShapes = !Input.Timeline && Input.KitMovement && !Input.KitMovement->Shapes.IsEmpty() && Detail == EGSTN_HurtboxDetail::Simple;
	auto MarkAbsent = [&Out, Num](int32 Index)
	{
		if (Out.Present.IsEmpty())
		{
			Out.Present.Init(true, Num);
		}
		Out.Present[Index] = false;
	};
	for (int32 Index = 0; Index < Num; ++Index)
	{
		// The kit's body replaces the set's: its SimpleOnly hurtboxes leave with it
		if (!Boxes[Index].IsUsedAt(Detail) || (bKitShapes && Boxes[Index].DetailUse == EGSTN_HurtboxDetailUse::SimpleOnly))
		{
			MarkAbsent(Index);
		}
	}

	if (Input.Timeline)
	{
		const FGSTN_CombatTimelineData& Timeline = *Input.Timeline;

		// 2. Bones: a following hurtbox moves by its bone's motion from the reference pose. Shapes stays empty when nothing follows or nothing is baked
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FGSTN_Hurtbox& Box = Boxes[Index];
			FTransform Bone;
			if (!Box.bFollowBone || Box.Bone.IsNone() || !Out.IsPresent(Index) || !Timeline.FindBakedSocket(Box.Bone, Input.TimelineFrame, Bone))
			{
				continue;
			}
			if (Out.Shapes.IsEmpty())
			{
				Out.Shapes.Reserve(Num);
				for (const FGSTN_Hurtbox& Each : Boxes)
				{
					Out.Shapes.Add(Each.Shape);
				}
			}
			// Shape relative to the bone in the reference pose, then carried by the bone of this frame
			Out.Shapes[Index].Local = Box.Shape.Local.GetRelativeTransform(Box.BoneReference) * Bone;
		}

		// 3. The move's extensions: the open Hurtbox tracks, in track order then shape order
		for (const TInstancedStruct<FGSTN_TimelineTrack>& Instance : Timeline.Tracks)
		{
			const FGSTN_TimelineTrack* Base = Instance.GetPtr<FGSTN_TimelineTrack>();
			if (!Base || Base->GetKind() != EGSTN_TimelineTrackKind::Hurtbox || !Base->Contains(Input.TimelineFrame))
			{
				continue;
			}
			const FGSTN_TimelineTrack_Hurtbox& Track = static_cast<const FGSTN_TimelineTrack_Hurtbox&>(*Base);
			const int32 CountsAs = ResolveCountsAs(Set, Track.CountsAs);
			bool bPlacedAny = false;
			for (const FGSTN_HitboxSpec& Spec : Track.Hitboxes)
			{
				FTransform Carrier;
				if (Out.Extensions.Num() >= MaxExtensions || !BuildExtensionCarrier(Track, Spec, Timeline, Input.TimelineFrame, Carrier))
				{
					continue;
				}
				FGSTN_PosedHurtboxExtension& Extension = Out.Extensions.AddDefaulted_GetRef();
				Extension.Shape = Spec.Shape;
				Extension.Shape.Local = Spec.Shape.Local * Carrier;
				Extension.HurtboxIndex = CountsAs;
				bPlacedAny = true;
			}
			// A replacing group (hand-made feet of one move) hides the hurtbox it stands for, only when it placed something
			if (Track.bReplaceCountsAs && bPlacedAny)
			{
				MarkAbsent(CountsAs);
			}
		}
	}
	else if (bKitShapes)
	{
		for (const FGSTN_MovementHurtboxShape& Shape : Input.KitMovement->Shapes)
		{
			if (Out.Extensions.Num() >= MaxExtensions)
			{
				break;
			}
			FGSTN_PosedHurtboxExtension& Extension = Out.Extensions.AddDefaulted_GetRef();
			Extension.Shape = Shape.Shape;
			Extension.HurtboxIndex = ResolveCountsAs(Set, Shape.CountsAs);
		}
	}

	// Nothing absent after all (a level every hurtbox belongs to): keep the cheap form
	if (!Out.Present.IsEmpty() && !Out.Present.Contains(false))
	{
		Out.Present.Reset();
	}
}
