// Links, ability-system half (no Network Prediction, no Mover).

#include "GSTN_LinkInteractProvider.h"

#include "Data/GSTN_LinkKitExtension.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_InteractTags.h"
#include "GSTN_InteractTypes.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkRequirementLibrary.h"
#include "GSTN_LinkTags.h"

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
	for (int32 OfferIndex = 0; OfferIndex < Source.Offers.Num(); ++OfferIndex)
	{
		const FGSTN_LinkInteractOffer& Offer = Source.Offers[OfferIndex];

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
