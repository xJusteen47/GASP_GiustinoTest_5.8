// Links, ability-system half (no Network Prediction, no Mover).

#include "GSTN_LinkInteractProvider.h"

#include "Data/GSTN_LinkKitExtension.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_InteractTags.h"
#include "GSTN_InteractTypes.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkRequirementLibrary.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_RelationshipResolver.h"

FName FGSTN_LinkInteractSource::TypeName()
{
	static const FName Name(TEXT("GSTNLinkInteractSource"));
	return Name;
}

FGSTN_InteractHostContext FGSTN_LinkInteractSource::ToHostContext() const
{
	FGSTN_InteractHostContext Context;
	Context.Data = this;
	Context.Type = TypeName();
	return Context;
}

uint8 FGSTN_LinkInteractProvider::GetProviderId() const
{
	return GSTNInteractProviders::LinkEntities;
}

int32 FGSTN_LinkInteractProvider::FindAcceptedOffer(const FGSTN_LinkInteractSource& Source, const FGSTN_LinkInteractEntity& Entity, int32 Frame,
	FGSTN_LinkRuleResult* OutRefusal)
{
	// The candidate's relationship, as a hit would see it under the default rule (no team = Neutral): an offer only for other relationships is
	// skipped before its rules, so it cannot claim a candidate a lower offer is meant for
	FGSTN_RelationshipQuery Query;
	Query.AttackerEntityId = Source.Interactor.EntityId;
	Query.AttackerTeamId = Source.Interactor.TeamId;
	Query.VictimEntityId = Entity.Side.EntityId;
	Query.VictimTeamId = Entity.Side.TeamId;
	const FGameplayTag Relationship = UGSTN_RelationshipResolver::DefaultRule(Query);

	for (int32 OfferIndex = 0; OfferIndex < Source.Offers.Num(); ++OfferIndex)
	{
		const FGSTN_LinkInteractOffer& Offer = Source.Offers[OfferIndex];
		if (!Offer.Relationships.IsEmpty() && !Offer.Relationships.HasTagExact(Relationship))
		{
			if (OutRefusal != nullptr)
			{
				*OutRefusal = FGSTN_LinkRuleResult::Refuse(EGSTN_LinkRefusal::Team);
			}
			continue;
		}

		// The same context the target's LinkRequest step will build, so the probe and the request cannot disagree on a refusal
		FGSTN_LinkRequestContext Context;
		Context.Frame = Frame;
		Context.LinkType = Offer.Definition != nullptr ? Offer.Definition->LinkType : FGameplayTag();
		Context.Initiator = Source.Interactor;
		Context.Target = Entity.Side;
		if (Source.Registry != nullptr)
		{
			UGSTN_LinkRequirementLibrary::ResolveDefinitions(*Source.Registry, Context);
		}
		// The offer's definition wins over the registry's: a caller may offer a definition the registry does not index (a test)
		Context.Definition = Offer.Definition;
		Context.InitiatorGrabRules = Offer.GrabRules;
		Context.InitiatorLatchRules = Offer.LatchRules;

		const FGSTN_LinkRuleResult Result = UGSTN_LinkRequirementLibrary::EvaluateRequest(Context);
		if (Result.bAccepted)
		{
			return OfferIndex;
		}
		if (OutRefusal != nullptr)
		{
			*OutRefusal = Result;
		}
	}
	return INDEX_NONE;
}

void FGSTN_LinkInteractProvider::GatherCandidates(const FGSTN_InteractQuery& Query, TArray<FGSTN_InteractCandidate>& OutCandidates) const
{
	const FGSTN_LinkInteractSource* Source = Query.HostContext.As<FGSTN_LinkInteractSource>(FGSTN_LinkInteractSource::TypeName());
	if (Source == nullptr || Query.Meanings == nullptr || Source->Offers.Num() == 0)
	{
		return;
	}

	const FVector Origin = FGSTN_InteractCandidate::SnapCm(Query.Origin);
	const float HardMaxReachCm = Query.Rules != nullptr ? Query.Rules->HardMaxReachCm : MAX_flt;

	for (const FGSTN_LinkInteractEntity& Entity : Source->Entities)
	{
		if (Entity.Side.EntityId == Source->Interactor.EntityId)
		{
			continue;
		}

		const FVector Location = FGSTN_InteractCandidate::SnapCm(Entity.Location);
		// Broad cut before the rules: nothing beyond the largest reach any candidate may have can be focused anyway
		if (FMath::Max(0.0, FVector::Dist(Origin, Location) - Entity.RadiusCm) > HardMaxReachCm)
		{
			continue;
		}

		const int32 OfferIndex = FindAcceptedOffer(*Source, Entity, Query.Frame);
		if (OfferIndex == INDEX_NONE)
		{
			continue;
		}

		const FGSTN_LinkInteractOffer& Offer = Source->Offers[OfferIndex];
		const int32 MeaningIndex = Query.Meanings->IndexOf(Offer.Meaning);
		if (MeaningIndex == 0)
		{
			// The meaning is not configured in the probe: offering the candidate would focus something no binding can act on
			continue;
		}

		FGSTN_InteractCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
		Candidate.Id = FGSTN_InteractCandidateId(GetProviderId(), static_cast<uint32>(Entity.Side.EntityId));
		Candidate.PositionCm = FGSTN_InteractCandidate::QuantiseCm(Location);
		Candidate.RadiusCm = FMath::Clamp(Entity.RadiusCm, 0, MAX_uint16);
		Candidate.MeaningIndex = MeaningIndex;
		Candidate.ReachOverrideCm = FMath::Clamp(FMath::RoundToInt(Offer.ReachCm), 0, MAX_uint16);
	}
}

FGSTN_LinkInteractOffer FGSTN_LinkInteractProvider::MakeGrabOffer(const UGSTN_LinkRegistry& Registry, const FGSTN_LinkKitExtension* InitiatorKit)
{
	FGSTN_LinkInteractOffer Offer;
	Offer.Definition = Registry.FindDefinition(GSTNLinkTags::Type_Grab);
	Offer.Meaning = GSTNLinkTags::Interact_Target_Grab;
	if (InitiatorKit != nullptr)
	{
		Offer.ReachCm = InitiatorKit->GrabRules.InteractReachCm;
		Offer.GrabRules = &InitiatorKit->GrabRules;
		Offer.LatchRules = &InitiatorKit->LatchRules;
	}
	else
	{
		// Without a kit extension the grab uses its rule's defaults, whose reach is FGSTN_GrabRules' default
		Offer.ReachCm = FGSTN_GrabRules().InteractReachCm;
	}
	return Offer;
}

bool FGSTN_LinkInteractProvider::CarriesKitExtension(const FGSTN_KitExtensionList* KitExtensions, const FGSTN_LinkKitExtension* LinkKit, const UScriptStruct* Required)
{
	if (Required == nullptr)
	{
		return true;
	}
	// A test body holds its link extension without a list
	if (LinkKit != nullptr && FGSTN_LinkKitExtension::StaticStruct()->IsChildOf(Required))
	{
		return true;
	}
	if (KitExtensions != nullptr)
	{
		for (const FInstancedStruct& Entry : KitExtensions->ExtensionData)
		{
			const UScriptStruct* Struct = Entry.GetScriptStruct();
			if (Struct != nullptr && Struct->IsChildOf(Required))
			{
				return true;
			}
		}
	}
	return false;
}

void FGSTN_LinkInteractProvider::MakeOffers(const UGSTN_LinkRegistry& Registry, const FGSTN_KitExtensionList* KitExtensions, const FGSTN_LinkKitExtension* LinkKit,
	TArray<FGSTN_LinkInteractOffer, TInlineAllocator<4>>& OutOffers)
{
	const UGSTN_LinkTypeDefinition* GrabDefinition = Registry.FindDefinition(GSTNLinkTags::Type_Grab);
	for (const FGSTN_LinkOfferEntry& Entry : Registry.GetInteractOffers())
	{
		if (!CarriesKitExtension(KitExtensions, LinkKit, Entry.Offer.RequiredKitExtension))
		{
			continue;
		}
		FGSTN_LinkInteractOffer Offer;
		if (Entry.Definition == GrabDefinition)
		{
			// The grab's reach and rules are the kit's grab rules (F12), not the offer's data
			Offer = MakeGrabOffer(Registry, LinkKit);
		}
		else
		{
			Offer.Definition = Entry.Definition;
			Offer.ReachCm = Entry.Offer.ReachCm;
			Offer.GrabRules = LinkKit != nullptr ? &LinkKit->GrabRules : nullptr;
			Offer.LatchRules = LinkKit != nullptr ? &LinkKit->LatchRules : nullptr;
		}
		Offer.Meaning = Entry.Offer.Meaning;
		Offer.Priority = Entry.Offer.Priority;
		// The alternatives below are copies of this offer, so they keep its relationships (the latch tried under the grab's meaning)
		Offer.Relationships = Entry.Offer.Relationships;
		if (Offer.Definition == nullptr)
		{
			continue;
		}

		// The type's alternatives are tried first under its meaning (the grab lists the latch), as the LinkRequest step tries them on a hit
		for (const FGameplayTag& Alternative : Offer.Definition->Alternatives)
		{
			const UGSTN_LinkTypeDefinition* Definition = Registry.FindDefinition(Alternative);
			const bool bListed = OutOffers.ContainsByPredicate([Definition, &Offer](const FGSTN_LinkInteractOffer& Listed)
			{
				return Listed.Definition == Definition && Listed.Meaning == Offer.Meaning;
			});
			if (Definition != nullptr && !bListed)
			{
				FGSTN_LinkInteractOffer& Added = OutOffers.Add_GetRef(Offer);
				Added.Definition = Definition;
			}
		}
		OutOffers.Add(Offer);
	}
}

void FGSTN_LinkInteractProvider::ProjectTargetTag(const FGSTN_InteractFocusResult& Focus, const FGSTN_InteractMeaningTable& Meanings,
	FGameplayTagContainer& InOutTags)
{
	const FGameplayTag Root = GSTNInteractTags::Root_Target;
	TArray<FGameplayTag> Stale;
	for (const FGameplayTag& Tag : InOutTags)
	{
		if (Tag.MatchesTag(Root))
		{
			Stale.Add(Tag);
		}
	}
	for (const FGameplayTag& Tag : Stale)
	{
		InOutTags.RemoveTag(Tag);
	}

	if (Focus.HasFocus() && !Focus.IsGhost() && !Focus.IsUnactionable() && Focus.MeaningIndex > 0)
	{
		const FGameplayTag Meaning = Meanings.TagOf(Focus.MeaningIndex);
		if (Meaning.IsValid())
		{
			InOutTags.AddTag(Meaning);
		}
	}
}
