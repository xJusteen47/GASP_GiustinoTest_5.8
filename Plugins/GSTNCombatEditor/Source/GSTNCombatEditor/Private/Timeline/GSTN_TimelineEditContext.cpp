// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineEditContext.h"

#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

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

FGSTN_CombatTimelineData* FGSTN_TimelineEditContext::Get() const
{
	if (bInline)
	{
		// E13 resolves the inline property on the CDO of the Blueprint's generated class
		return nullptr;
	}
	UGSTN_CombatTimeline* Asset = Timeline.Get();
	return Asset ? &Asset->Data : nullptr;
}

UObject* FGSTN_TimelineEditContext::GetOwner() const
{
	return bInline ? static_cast<UObject*>(nullptr) : static_cast<UObject*>(Timeline.Get());
}

UObject* FGSTN_TimelineEditContext::GetTarget() const
{
	return bInline ? static_cast<UObject*>(Blueprint.Get()) : static_cast<UObject*>(Timeline.Get());
}

void FGSTN_TimelineEditContext::Modify() const
{
	if (UObject* Owner = GetOwner())
	{
		Owner->Modify();
	}
}

void FGSTN_TimelineEditContext::NotifyChanged(EPropertyChangeType::Type ChangeType) const
{
	if (bInline)
	{
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
	const UObject* Target = Context.GetTarget();
	if (Target == nullptr)
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
