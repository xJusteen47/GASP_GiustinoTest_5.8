// Types used only by the GSTNCombat.Editor automation tests.

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "Timeline/GSTN_KeyedTrackAccess.h"
#include "GSTN_EditorTestTypes.generated.h"

/** One key of the test keyed track: a position and a size over time, as a hitbox key will be */
USTRUCT()
struct FGSTN_TestKey
{
	GENERATED_BODY()

	/** Absolute timeline frame */
	UPROPERTY()
	int32 Frame = 0;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float Radius = 10.f;
};

/**
 * A track with keys, standing in for the hitbox track of M4 so the model's key gestures are tested before that track exists.
 * Hidden so it never shows in a track picker.
 */
USTRUCT(meta = (Hidden))
struct FGSTN_TestKeyedTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGSTN_TestKey> Keys;
};

/**
 * A game's own track derived from a built-in one (a dash that is a movement with extras), to test that a track editor registered for
 * the derived struct wins and that, without one, the base's editor serves it. Hidden so it never shows in a track picker.
 */
USTRUCT(meta = (Hidden))
struct FGSTN_TestDerivedMovement : public FGSTN_TimelineTrack_Movement
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ExtraFrames = 0;
};

/** An object that holds a loadout, as a designer preset asset will: the preset validator takes any asset with such a property */
UCLASS(Transient)
class UGSTN_TestLoadoutHolder : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FGSTN_LoadoutData Loadout;
};

/** The adapter a keyed track registers: linear interpolation of position and size for a new key, stable sort by frame */
class FGSTN_TestKeyedAccess : public IGSTN_KeyedTrackAccess
{
public:
	virtual const UScriptStruct* GetKeyStruct() const override { return FGSTN_TestKey::StaticStruct(); }
	virtual int32 GetKeyCount(const FGSTN_TimelineTrack& Track) const override { return Cast(Track).Keys.Num(); }
	virtual const void* GetKey(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const override { return &Cast(Track).Keys[KeyIndex]; }
	virtual int32 GetKeyFrame(const FGSTN_TimelineTrack& Track, int32 KeyIndex) const override { return Cast(Track).Keys[KeyIndex].Frame; }
	virtual void SetKeyFrame(FGSTN_TimelineTrack& Track, int32 KeyIndex, int32 Frame) const override { Cast(Track).Keys[KeyIndex].Frame = Frame; }

	virtual int32 AppendKey(FGSTN_TimelineTrack& Track, int32 Frame, const void* KeyData) const override
	{
		TArray<FGSTN_TestKey>& Keys = Cast(Track).Keys;
		FGSTN_TestKey NewKey;
		if (KeyData != nullptr)
		{
			NewKey = *static_cast<const FGSTN_TestKey*>(KeyData);
		}
		else
		{
			// Values on the curve at Frame: the nearest keys before and after, blended
			const FGSTN_TestKey* Before = nullptr;
			const FGSTN_TestKey* After = nullptr;
			for (const FGSTN_TestKey& Key : Keys)
			{
				if (Key.Frame <= Frame && (Before == nullptr || Key.Frame >= Before->Frame))
				{
					Before = &Key;
				}
				if (Key.Frame >= Frame && (After == nullptr || Key.Frame < After->Frame))
				{
					After = &Key;
				}
			}
			if (Before != nullptr && After != nullptr && After->Frame != Before->Frame)
			{
				const float Alpha = static_cast<float>(Frame - Before->Frame) / static_cast<float>(After->Frame - Before->Frame);
				NewKey.Location = FMath::Lerp(Before->Location, After->Location, Alpha);
				NewKey.Radius = FMath::Lerp(Before->Radius, After->Radius, Alpha);
			}
			else if (const FGSTN_TestKey* Nearest = Before ? Before : After)
			{
				NewKey.Location = Nearest->Location;
				NewKey.Radius = Nearest->Radius;
			}
		}
		NewKey.Frame = Frame;
		return Keys.Add(NewKey);
	}

	virtual void RemoveKey(FGSTN_TimelineTrack& Track, int32 KeyIndex) const override { Cast(Track).Keys.RemoveAt(KeyIndex); }

	virtual void SortKeys(FGSTN_TimelineTrack& Track, TArray<int32>& OutOldToNew) const override
	{
		TArray<FGSTN_TestKey>& Keys = Cast(Track).Keys;
		TArray<int32> Order;
		for (int32 i = 0; i < Keys.Num(); ++i)
		{
			Order.Add(i);
		}
		Order.StableSort([&Keys](int32 A, int32 B) { return Keys[A].Frame < Keys[B].Frame; });

		TArray<FGSTN_TestKey> Sorted;
		OutOldToNew.SetNum(Keys.Num());
		for (int32 NewIndex = 0; NewIndex < Order.Num(); ++NewIndex)
		{
			Sorted.Add(Keys[Order[NewIndex]]);
			OutOldToNew[Order[NewIndex]] = NewIndex;
		}
		Keys = MoveTemp(Sorted);
	}

	virtual bool GetKeyTransform(const FGSTN_TimelineTrack& Track, int32 KeyIndex, FTransform& OutTransform) const override
	{
		const FGSTN_TestKey& Key = Cast(Track).Keys[KeyIndex];
		OutTransform = FTransform(FQuat::Identity, Key.Location, FVector(Key.Radius));
		return true;
	}

	virtual bool SetKeyTransform(FGSTN_TimelineTrack& Track, int32 KeyIndex, const FTransform& NewTransform) const override
	{
		FGSTN_TestKey& Key = Cast(Track).Keys[KeyIndex];
		Key.Location = NewTransform.GetLocation();
		Key.Radius = static_cast<float>(NewTransform.GetScale3D().X);
		return true;
	}

private:
	static FGSTN_TestKeyedTrack& Cast(FGSTN_TimelineTrack& Track) { return static_cast<FGSTN_TestKeyedTrack&>(Track); }
	static const FGSTN_TestKeyedTrack& Cast(const FGSTN_TimelineTrack& Track) { return static_cast<const FGSTN_TestKeyedTrack&>(Track); }
};
