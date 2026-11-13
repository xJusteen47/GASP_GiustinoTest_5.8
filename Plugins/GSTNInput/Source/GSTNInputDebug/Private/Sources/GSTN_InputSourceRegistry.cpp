// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "Sources/GSTN_InputSourceRegistry.h"

#include "GameFramework/Pawn.h"

FGSTN_InputSourceRegistry& FGSTN_InputSourceRegistry::Get()
{
	static FGSTN_InputSourceRegistry Instance;
	return Instance;
}

void FGSTN_InputSourceRegistry::RegisterFinder(FName Key, FGSTN_InputSourceFinder Finder)
{
	for (FFinderEntry& Entry : Finders)
	{
		if (Entry.Key == Key)
		{
			Entry.Finder = MoveTemp(Finder);
			return;
		}
	}
	Finders.Add({ Key, MoveTemp(Finder) });
}

void FGSTN_InputSourceRegistry::UnregisterFinder(FName Key)
{
	Finders.RemoveAll([Key](const FFinderEntry& Entry) { return Entry.Key == Key; });
}

void FGSTN_InputSourceRegistry::RegisterDefaultProfile(FName Key, FGSTN_InputProfileFactory Factory)
{
	for (FProfileEntry& Entry : Profiles)
	{
		if (Entry.Key == Key)
		{
			Entry.Factory = MoveTemp(Factory);
			return;
		}
	}
	Profiles.Add({ Key, MoveTemp(Factory) });
}

void FGSTN_InputSourceRegistry::UnregisterDefaultProfile(FName Key)
{
	Profiles.RemoveAll([Key](const FProfileEntry& Entry) { return Entry.Key == Key; });
}

TSharedPtr<IGSTN_InputHistorySource> FGSTN_InputSourceRegistry::FindSource(APawn& Pawn) const
{
	// A component that implements the interface knows its own state best: it needs no finder
	TArray<UActorComponent*> Components;
	Pawn.GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->GetClass()->ImplementsInterface(UGSTN_InputHistoryProvider::StaticClass()))
		{
			if (IGSTN_InputHistoryProvider* Provider = Cast<IGSTN_InputHistoryProvider>(Component))
			{
				if (TSharedPtr<IGSTN_InputHistorySource> Source = Provider->CreateInputHistorySource())
				{
					return Source;
				}
			}
		}
	}

	for (const FFinderEntry& Entry : Finders)
	{
		if (Entry.Finder)
		{
			if (TSharedPtr<IGSTN_InputHistorySource> Source = Entry.Finder(Pawn))
			{
				return Source;
			}
		}
	}
	return nullptr;
}

FGSTN_InputDisplayProfile FGSTN_InputSourceRegistry::GetDefaultProfile() const
{
	for (const FProfileEntry& Entry : Profiles)
	{
		if (Entry.Factory)
		{
			FGSTN_InputDisplayProfile Profile = Entry.Factory();
			if (!Profile.ButtonTags.IsEmpty())
			{
				return Profile;
			}
		}
	}
	FGSTN_InputDisplayProfile Generic;
	Generic.Label = NSLOCTEXT("GSTNInputDebug", "GenericProfile", "No command set known: only the notation is played");
	return Generic;
}
