// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_HitboxKeyedAccess.h"

#include "Hits/GSTN_HitShapeUtils.h"

namespace
{
	FGSTN_TimelineTrack_Hitbox& AsHitbox(FGSTN_TimelineTrack& Track)
	{
		return static_cast<FGSTN_TimelineTrack_Hitbox&>(Track);
	}

	const FGSTN_TimelineTrack_Hitbox& AsHitbox(const FGSTN_TimelineTrack& Track)
	{
		return static_cast<const FGSTN_TimelineTrack_Hitbox&>(Track);
	}
}

bool FGSTN_HitboxKeyedAccess::Locate(const FGSTN_TimelineTrack_Hitbox& Track, int32 FlatIndex, int32& OutSpec, int32& OutKeyInSpec)
{
	if (FlatIndex < 0)
	{
		return false;
	}
	int32 Remaining = FlatIndex;
	for (int32 Spec = 0; Spec < Track.Hitboxes.Num(); ++Spec)
	{
		const int32 Count = Track.Hitboxes[Spec].Keys.Num();
		if (Remaining < Count)
		{
			OutSpec = Spec;
			OutKeyInSpec = Remaining;
			return true;
		}
		Remaining -= Count;
	}
	return false;
}

int32 FGSTN_HitboxKeyedAccess::ToFlat(const FGSTN_TimelineTrack_Hitbox& Track, int32 Spec, int32 KeyInSpec)
{
	int32 Flat = KeyInSpec;
	for (int32 Index = 0; Index < Spec && Index < Track.Hitboxes.Num(); ++Index)
	{
		Flat += Track.Hitboxes[Index].Keys.Num();
	}
	return Flat;
}

FGSTN_HitboxSpec FGSTN_HitboxKeyedAccess::MakeDefaultSpec()
{
	FGSTN_HitboxSpec Spec;
	Spec.Shape.Type = EGSTN_ShapeType::Sphere;
	Spec.Shape.Extent = FVector(30.0, 30.0, 30.0);
	FGSTN_HitboxKey Key;
	Key.Frame = 0;
	Key.Offset = FVector(60.0, 0.0, 0.0);
	Spec.Keys.Add(Key);
	return Spec;
}

const UScriptStruct* FGSTN_HitboxKeyedAccess::GetKeyStruct() const
{
	return FGSTN_HitboxKey::StaticStruct();
}

int32 FGSTN_HitboxKeyedAccess::GetKeyCount(const FGSTN_TimelineTrack& Track) const
{
	int32 Count = 0;
	for (const FGSTN_HitboxSpec& Spec : AsHitbox(Track).Hitboxes)
	{
		Count += Spec.Keys.Num();
	}
	return Count;
}

const void* FGSTN_HitboxKeyedAccess::GetKey(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const
{
	int32 Spec = 0;
	int32 Key = 0;
	return Locate(AsHitbox(Track), KeyIndex, Spec, Key) ? &AsHitbox(Track).Hitboxes[Spec].Keys[Key] : nullptr;
}

int32 FGSTN_HitboxKeyedAccess::GetKeyFrame(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const
{
	int32 Spec = 0;
	int32 Key = 0;
	return Locate(AsHitbox(Track), KeyIndex, Spec, Key) ? Track.StartFrame + AsHitbox(Track).Hitboxes[Spec].Keys[Key].Frame : INDEX_NONE;
}

void FGSTN_HitboxKeyedAccess::SetKeyFrame(FGSTN_TimelineTrack& Track, int32 KeyIndex, int32 Frame) const
{
	int32 Spec = 0;
	int32 Key = 0;
	if (Locate(AsHitbox(Track), KeyIndex, Spec, Key))
	{
		// The key counts from the first frame of the track; a frame before it is a negative key, which the validator reports
		AsHitbox(Track).Hitboxes[Spec].Keys[Key].Frame = Frame - Track.StartFrame;
	}
}

int32 FGSTN_HitboxKeyedAccess::AppendKey(FGSTN_TimelineTrack& Track, int32 Frame, const void* KeyData) const
{
	return AppendKeyInLane(Track, 0, Frame, KeyData);
}

int32 FGSTN_HitboxKeyedAccess::GetLaneCount(const FGSTN_TimelineTrack& Track) const
{
	return AsHitbox(Track).Hitboxes.Num();
}

int32 FGSTN_HitboxKeyedAccess::GetKeyLane(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const
{
	int32 Spec = 0;
	int32 Key = 0;
	return Locate(AsHitbox(Track), KeyIndex, Spec, Key) ? Spec : 0;
}

int32 FGSTN_HitboxKeyedAccess::AppendKeyInLane(FGSTN_TimelineTrack& Track, int32 Lane, int32 Frame, const void* KeyData) const
{
	FGSTN_TimelineTrack_Hitbox& Hitbox = AsHitbox(Track);
	const int32 RelativeFrame = Frame - Track.StartFrame;

	// A track that has no hitbox yet gets one, with the key the caller asked for: the first key IS the hitbox. The default spec brings a key of its
	// own, which is replaced by the requested one
	if (Hitbox.Hitboxes.IsEmpty())
	{
		FGSTN_HitboxSpec Spec = FGSTN_HitboxKeyedAccess::MakeDefaultSpec();
		Spec.Keys.Reset();
		Hitbox.Hitboxes.Add(MoveTemp(Spec));
	}
	const int32 Spec = FMath::Clamp(Lane, 0, Hitbox.Hitboxes.Num() - 1);
	TArray<FGSTN_HitboxKey>& Keys = Hitbox.Hitboxes[Spec].Keys;

	FGSTN_HitboxKey NewKey;
	if (KeyData != nullptr)
	{
		NewKey = *static_cast<const FGSTN_HitboxKey*>(KeyData);
	}
	else if (!Keys.IsEmpty())
	{
		// The values the runtime evaluates at this frame, so the path does not change. Keys are sorted (SortKeys runs after every append)
		const FTransform OnPath = UGSTN_HitShapeUtils::EvaluateKeys(Keys, RelativeFrame);
		NewKey.Offset = OnPath.GetLocation();
		NewKey.Rotation = OnPath.GetRotation().Rotator();
		NewKey.Scale = OnPath.GetScale3D();
	}
	else
	{
		// The first key of a hitbox that has none: the default sphere's place in front of the root
		NewKey.Offset = MakeDefaultSpec().Keys[0].Offset;
	}
	NewKey.Frame = RelativeFrame;

	Keys.Add(NewKey);
	return ToFlat(Hitbox, Spec, Keys.Num() - 1);
}

void FGSTN_HitboxKeyedAccess::RemoveKey(FGSTN_TimelineTrack& Track, int32 KeyIndex) const
{
	int32 Spec = 0;
	int32 Key = 0;
	if (Locate(AsHitbox(Track), KeyIndex, Spec, Key))
	{
		AsHitbox(Track).Hitboxes[Spec].Keys.RemoveAt(Key);
	}
}

void FGSTN_HitboxKeyedAccess::SortKeys(FGSTN_TimelineTrack& Track, TArray<int32>& OutOldToNew) const
{
	FGSTN_TimelineTrack_Hitbox& Hitbox = AsHitbox(Track);
	OutOldToNew.Reset();
	int32 Base = 0;
	for (FGSTN_HitboxSpec& Spec : Hitbox.Hitboxes)
	{
		TArray<FGSTN_HitboxKey>& Keys = Spec.Keys;
		TArray<int32> Order;
		Order.Reserve(Keys.Num());
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			Order.Add(Index);
		}
		// Stable: two keys on one frame keep their order
		Order.StableSort([&Keys](int32 A, int32 B) { return Keys[A].Frame < Keys[B].Frame; });

		TArray<FGSTN_HitboxKey> Sorted;
		Sorted.Reserve(Keys.Num());
		OutOldToNew.AddUninitialized(Keys.Num());
		for (int32 NewIndex = 0; NewIndex < Order.Num(); ++NewIndex)
		{
			Sorted.Add(Keys[Order[NewIndex]]);
			OutOldToNew[Base + Order[NewIndex]] = Base + NewIndex;
		}
		Keys = MoveTemp(Sorted);
		Base += Keys.Num();
	}
}

bool FGSTN_HitboxKeyedAccess::GetKeyTransform(const FGSTN_TimelineTrack& Track, int32 KeyIndex, FTransform& OutTransform) const
{
	const FGSTN_HitboxKey* Key = static_cast<const FGSTN_HitboxKey*>(GetKey(Track, KeyIndex));
	if (Key == nullptr)
	{
		return false;
	}
	OutTransform = FTransform(Key->Rotation.Quaternion(), Key->Offset, Key->Scale);
	return true;
}

bool FGSTN_HitboxKeyedAccess::SetKeyTransform(FGSTN_TimelineTrack& Track, int32 KeyIndex, const FTransform& NewTransform) const
{
	int32 Spec = 0;
	int32 KeyInSpec = 0;
	if (!Locate(AsHitbox(Track), KeyIndex, Spec, KeyInSpec))
	{
		return false;
	}
	FGSTN_HitboxKey& Key = AsHitbox(Track).Hitboxes[Spec].Keys[KeyInSpec];
	Key.Offset = NewTransform.GetLocation();
	Key.Rotation = NewTransform.GetRotation().Rotator();
	Key.Scale = NewTransform.GetScale3D();
	return true;
}
