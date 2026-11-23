// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTN_SurfaceBehaviourComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "GSTNSurface"

UGSTN_SurfaceBehaviourComponent::UGSTN_SurfaceBehaviourComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

const FGSTN_SurfaceBehaviour& UGSTN_SurfaceBehaviourComponent::GetBehaviourFor(const UPrimitiveComponent* Collider) const
{
	if (Collider && !PerComponent.IsEmpty())
	{
		if (const FGSTN_SurfaceBehaviour* Override = PerComponent.Find(Collider->GetFName()))
		{
			return *Override;
		}
	}
	return Default;
}

const UGSTN_SurfaceBehaviourComponent* UGSTN_SurfaceBehaviourComponent::FindFor(const AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	// The component set of an actor is unordered, so a plain "first" can differ between machines: choose by name
	const UGSTN_SurfaceBehaviourComponent* Best = nullptr;
	TInlineComponentArray<UGSTN_SurfaceBehaviourComponent*, 2> Candidates(Owner);
	for (const UGSTN_SurfaceBehaviourComponent* Candidate : Candidates)
	{
		if (Candidate && (!Best || Candidate->GetFName().Compare(Best->GetFName()) < 0))
		{
			Best = Candidate;
		}
	}
	return Best;
}

void UGSTN_SurfaceBehaviourComponent::CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	Default.Validate(OutErrors);
	for (const TPair<FName, FGSTN_SurfaceBehaviour>& Entry : PerComponent)
	{
		TArray<FText> EntryErrors;
		if (!Entry.Value.Validate(EntryErrors))
		{
			for (const FText& Error : EntryErrors)
			{
				OutErrors.Add(FText::Format(LOCTEXT("PerComponentEntryError", "PerComponent '{0}': {1}"),
					FText::FromName(Entry.Key), Error));
			}
		}
	}

	// Names can be checked only against a known owner (a placed actor or a Blueprint's preview), not on a bare template
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives(Owner);
	for (const TPair<FName, FGSTN_SurfaceBehaviour>& Entry : PerComponent)
	{
		const bool bFound = Primitives.ContainsByPredicate([&Entry](const UPrimitiveComponent* Primitive)
		{
			return Primitive && Primitive->GetFName() == Entry.Key;
		});
		if (!bFound)
		{
			OutErrors.Add(FText::Format(LOCTEXT("PerComponentUnknown",
				"PerComponent names '{0}', which is not a primitive component of {1}"),
				FText::FromName(Entry.Key), FText::FromString(Owner->GetName())));
		}
	}

	// An error, not a warning: the extra components change what is simulated (which one is read), and a level that ships with
	// two is a content mistake even though the pick by name keeps the machines in agreement
	TInlineComponentArray<UGSTN_SurfaceBehaviourComponent*> SurfaceComponents(Owner);
	if (SurfaceComponents.Num() > 1)
	{
		OutErrors.Add(FText::Format(LOCTEXT("SeveralSurfaceComponents",
			"{0} has {1} surface behaviour components: only the one with the lowest name is read. Keep one and use PerComponent instead"),
			FText::FromString(Owner->GetName()), SurfaceComponents.Num()));
	}
}

#if WITH_EDITOR
EDataValidationResult UGSTN_SurfaceBehaviourComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Errors;
	TArray<FText> Warnings;
	CollectFindings(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}
	return Errors.IsEmpty() ? Result : EDataValidationResult::Invalid;
}
#endif

#undef LOCTEXT_NAMESPACE
