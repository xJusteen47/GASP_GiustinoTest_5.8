// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Cosmetics/GSTN_AttachmentProvider.h"

namespace
{
	struct FRegisteredProvider
	{
		TSharedRef<IGSTN_AttachmentProvider> Provider;
		int32 Priority = 0;
	};

	TMap<FName, FRegisteredProvider>& GetProviders()
	{
		static TMap<FName, FRegisteredProvider> Providers;
		return Providers;
	}
}

void FGSTN_AttachmentProviderRegistry::Register(FName Id, TSharedRef<IGSTN_AttachmentProvider> Provider, int32 Priority)
{
	GetProviders().Add(Id, FRegisteredProvider{ MoveTemp(Provider), Priority });
}

bool FGSTN_AttachmentProviderRegistry::Unregister(FName Id)
{
	return GetProviders().Remove(Id) > 0;
}

bool FGSTN_AttachmentProviderRegistry::IsRegistered(FName Id)
{
	return GetProviders().Contains(Id);
}

void FGSTN_AttachmentProviderRegistry::Gather(const FGSTN_AttachmentContext& Context, TArray<FGSTN_AttachmentRequest>& OutRequests)
{
	TArray<FName> Ids;
	GetProviders().GenerateKeyArray(Ids);
	Ids.Sort([](const FName& A, const FName& B)
	{
		const int32 PriorityA = GetProviders()[A].Priority;
		const int32 PriorityB = GetProviders()[B].Priority;
		return PriorityA != PriorityB ? PriorityA < PriorityB : A.LexicalLess(B);
	});
	for (const FName& Id : Ids)
	{
		// Looked up and copied out each time: a provider may register or remove another one while it runs
		if (const FRegisteredProvider* Found = GetProviders().Find(Id))
		{
			const TSharedRef<IGSTN_AttachmentProvider> Provider = Found->Provider;
			Provider->GatherAttachments(Context, OutRequests);
		}
	}
}
