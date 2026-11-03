// Sync fragments used only by the NP automation tests.

#pragma once

#include "CoreMinimal.h"
#include "Sync/GSTN_SyncFragment.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_CombatNPTestTypes.generated.h"

/** Two integers, discrete state (the default Interpolate: the newer frame) */
USTRUCT()
struct FGSTN_TestFragmentA : public FGSTN_SyncFragment
{
	GENERATED_BODY()

	int32 Frame = INDEX_NONE;
	uint8 Count = 0;

	virtual void NetSerialize(FArchive& Ar) override
	{
		Ar << Frame;
		Ar << Count;
	}

	virtual void ToString(FAnsiStringBuilderBase& Out) const override
	{
		Out.Appendf("A frame %d count %d", Frame, Count);
	}
};

/** One integer that blends between two frames */
USTRUCT()
struct FGSTN_TestFragmentB : public FGSTN_SyncFragment
{
	GENERATED_BODY()

	int32 Value = 0;

	virtual void NetSerialize(FArchive& Ar) override
	{
		Ar << Value;
	}

	virtual void Interpolate(const FGSTN_SyncFragment* From, const FGSTN_SyncFragment* To, float PCT) override
	{
		if (From)
		{
			Value = FMath::RoundToInt(FMath::Lerp(static_cast<float>(static_cast<const FGSTN_TestFragmentB*>(From)->Value), static_cast<float>(static_cast<const FGSTN_TestFragmentB*>(To)->Value), PCT));
		}
	}

	virtual void ToString(FAnsiStringBuilderBase& Out) const override
	{
		Out.Appendf("B value %d", Value);
	}
};

/** A track of no kind the model knows: the clock's track hook is registered for this struct (M6 F2) */
USTRUCT()
struct FGSTN_TestHoldTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()
};
