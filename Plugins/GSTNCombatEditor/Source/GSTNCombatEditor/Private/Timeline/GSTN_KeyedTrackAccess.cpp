// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_KeyedTrackAccess.h"

#include "UObject/Class.h"

namespace
{
	TMap<TWeakObjectPtr<const UScriptStruct>, TSharedRef<const IGSTN_KeyedTrackAccess>>& Adapters()
	{
		static TMap<TWeakObjectPtr<const UScriptStruct>, TSharedRef<const IGSTN_KeyedTrackAccess>> Instance;
		return Instance;
	}
}

void FGSTN_KeyedTrackAccessRegistry::Register(const UScriptStruct* TrackStruct, TSharedRef<const IGSTN_KeyedTrackAccess> Access)
{
	if (TrackStruct != nullptr)
	{
		Adapters().Add(TrackStruct, MoveTemp(Access));
	}
}

void FGSTN_KeyedTrackAccessRegistry::Unregister(const UScriptStruct* TrackStruct)
{
	Adapters().Remove(TrackStruct);
}

const IGSTN_KeyedTrackAccess* FGSTN_KeyedTrackAccessRegistry::Find(const UScriptStruct* TrackStruct)
{
	// Walk up the struct chain so an adapter written for a base track serves its children
	for (const UStruct* Struct = TrackStruct; Struct != nullptr; Struct = Struct->GetSuperStruct())
	{
		if (const TSharedRef<const IGSTN_KeyedTrackAccess>* Found = Adapters().Find(Cast<UScriptStruct>(Struct)))
		{
			return &Found->Get();
		}
	}
	return nullptr;
}
