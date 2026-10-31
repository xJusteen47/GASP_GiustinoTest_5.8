// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineEditContext.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	/**
	 * The default object of the Blueprint's generated class when it is an ability with an inline timeline. Looked up on every call: a
	 * compile creates a new default object. Null while the Blueprint compiles (the class is half built), when it was never compiled, or
	 * when it no longer derives from the ability. Never creates the default object.
	 */
	UGSTN_GA_BaseCombat* ResolveAbilityCDO(const UBlueprint* Blueprint)
	{
		if (Blueprint == nullptr || Blueprint->bBeingCompiled)
		{
			return nullptr;
		}
		UClass* Class = Blueprint->GeneratedClass;
		if (Class == nullptr || !Class->IsChildOf(UGSTN_GA_BaseCombat::StaticClass()) || Class->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			return nullptr;
		}
		return Cast<UGSTN_GA_BaseCombat>(Class->GetDefaultObject(false));
	}
}

FGSTN_TimelineEditContext FGSTN_TimelineEditContext::ForAsset(UGSTN_CombatTimeline* Timeline)
{
	FGSTN_TimelineEditContext Context;
	Context.Timeline = Timeline;
	return Context;
}

FGSTN_TimelineEditContext FGSTN_TimelineEditContext::ForBlueprint(UBlueprint* Blueprint)
{
	FGSTN_TimelineEditContext Context;
	Context.Blueprint = Blueprint;
	Context.bInline = true;
	return Context;
}

FGSTN_TimelineEditContext FGSTN_TimelineEditContext::FromOwnerObject(UObject* Owner)
{
	if (UGSTN_CombatTimeline* Asset = Cast<UGSTN_CombatTimeline>(Owner))
	{
		return ForAsset(Asset);
	}
	if (Owner != nullptr && Owner->HasAnyFlags(RF_ClassDefaultObject) && Owner->IsA<UGSTN_GA_BaseCombat>())
	{
		// Only the default object that is the live one of its Blueprint: the skeleton class and the old classes of a reinstancing have their own
		UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Owner->GetClass());
		if (Blueprint != nullptr && ResolveAbilityCDO(Blueprint) == Owner)
		{
			return ForBlueprint(Blueprint);
		}
	}
	return FGSTN_TimelineEditContext();
}

bool FGSTN_TimelineEditContext::CanEditInline(const UBlueprint* Blueprint)
{
	return Blueprint != nullptr && Blueprint->GeneratedClass != nullptr && Blueprint->GeneratedClass->IsChildOf(UGSTN_GA_BaseCombat::StaticClass());
}

bool FGSTN_TimelineEditContext::IsInlineShadowedByAsset() const
{
	const UGSTN_GA_BaseCombat* Ability = bInline ? ResolveAbilityCDO(Blueprint.Get()) : nullptr;
	return Ability != nullptr && !Ability->TimelineAsset.IsNull();
}

FGSTN_CombatTimelineData* FGSTN_TimelineEditContext::Get() const
{
	if (bInline)
	{
		UGSTN_GA_BaseCombat* Ability = ResolveAbilityCDO(Blueprint.Get());
		return Ability ? &Ability->Timeline : nullptr;
	}
	UGSTN_CombatTimeline* Asset = Timeline.Get();
	return Asset ? &Asset->Data : nullptr;
}

UObject* FGSTN_TimelineEditContext::GetOwner() const
{
	return bInline ? static_cast<UObject*>(ResolveAbilityCDO(Blueprint.Get())) : static_cast<UObject*>(Timeline.Get());
}

UObject* FGSTN_TimelineEditContext::GetTarget() const
{
	return bInline ? static_cast<UObject*>(Blueprint.Get()) : static_cast<UObject*>(Timeline.Get());
}

void FGSTN_TimelineEditContext::Modify() const
{
	if (UObject* Owner = GetOwner())
	{
		// A default object made by a compile does not carry the flag, and without it Modify() records nothing and undo has nothing to restore
		if (!Owner->HasAnyFlags(RF_Transactional))
		{
			Owner->SetFlags(RF_Transactional);
		}
		Owner->Modify();
	}
}

void FGSTN_TimelineEditContext::NotifyChanged(EPropertyChangeType::Type ChangeType) const
{
	if (bInline)
	{
		if (UGSTN_GA_BaseCombat* Ability = ResolveAbilityCDO(Blueprint.Get()))
		{
			// The Timeline property is the one the Class Defaults panel would have named, so listeners see the same event as for a panel edit
			FProperty* TimelineProperty = UGSTN_GA_BaseCombat::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UGSTN_GA_BaseCombat, Timeline));
			FPropertyChangedEvent Event(TimelineProperty, ChangeType);
			Ability->PostEditChangeProperty(Event);

			// The data lives in the default object, not in the Blueprint, so the Blueprint is told it is out of date and dirty. It is not recompiled
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint.Get(), Event);
		}
		return;
	}
	if (UGSTN_CombatTimeline* Asset = Timeline.Get())
	{
		// The Data property is the one a details panel edit would have named, so listeners that watch it see the same event
		FProperty* DataProperty = UGSTN_CombatTimeline::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UGSTN_CombatTimeline, Data));
		FPropertyChangedEvent Event(DataProperty, ChangeType);
		Asset->PostEditChangeProperty(Event);
	}
}

FText FGSTN_TimelineEditContext::GetDisplayName() const
{
	if (const UObject* Target = GetTarget())
	{
		return FText::FromString(Target->GetName());
	}
	return LOCTEXT("NoTimeline", "No timeline");
}

namespace GSTNTimelineContextRegistry
{
	TArray<TWeakPtr<FGSTN_TimelineContextLease>>& Leases()
	{
		static TArray<TWeakPtr<FGSTN_TimelineContextLease>> Instance;
		return Instance;
	}

	/** The live lease on a target, dropping the expired ones on the way */
	TSharedPtr<FGSTN_TimelineContextLease> Find(const UObject* Target)
	{
		TArray<TWeakPtr<FGSTN_TimelineContextLease>>& All = Leases();
		All.RemoveAll([](const TWeakPtr<FGSTN_TimelineContextLease>& Lease) { return !Lease.IsValid(); });
		for (const TWeakPtr<FGSTN_TimelineContextLease>& Weak : All)
		{
			if (TSharedPtr<FGSTN_TimelineContextLease> Lease = Weak.Pin())
			{
				if (Lease->GetTarget() == Target)
				{
					return Lease;
				}
			}
		}
		return nullptr;
	}
}

FGSTN_TimelineContextLease::~FGSTN_TimelineContextLease() = default;

TSharedPtr<FGSTN_TimelineContextLease> FGSTN_TimelineEditContextRegistry::Acquire(const FGSTN_TimelineEditContext& Context, TFunction<void()> OnFocusRequested)
{
	// A context that does not resolve (a Blueprint that is not an ability, one that is being compiled) has nothing to edit and claims nothing
	const UObject* Target = Context.GetTarget();
	if (Target == nullptr || !Context.IsValid())
	{
		return nullptr;
	}
	if (const TSharedPtr<FGSTN_TimelineContextLease> Existing = GSTNTimelineContextRegistry::Find(Target))
	{
		if (Existing->OnFocusRequested)
		{
			Existing->OnFocusRequested();
		}
		return nullptr;
	}

	TSharedPtr<FGSTN_TimelineContextLease> Lease = MakeShareable(new FGSTN_TimelineContextLease());
	Lease->Target = Target;
	Lease->OnFocusRequested = MoveTemp(OnFocusRequested);
	GSTNTimelineContextRegistry::Leases().Add(Lease);
	return Lease;
}

bool FGSTN_TimelineEditContextRegistry::IsOpen(const FGSTN_TimelineEditContext& Context)
{
	const UObject* Target = Context.GetTarget();
	return Target != nullptr && GSTNTimelineContextRegistry::Find(Target).IsValid();
}

#undef LOCTEXT_NAMESPACE
