// A sync fragment used only by the parts NP automation tests. Private: nothing outside the tests includes this.

#pragma once

#include "CoreMinimal.h"
#include "Sync/GSTN_SyncFragment.h"
#include "GSTN_PartsNPTestTypes.generated.h"

/** Two integers, discrete state: the other fragment of the two-fragment tests (registered under a test id) */
USTRUCT()
struct FGSTN_PartsTestFragment : public FGSTN_SyncFragment
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
		Out.Appendf("T frame %d count %d", Frame, Count);
	}
};
