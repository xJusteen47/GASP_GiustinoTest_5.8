#include "GSTN_ExtTestStep_Brace.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNExtTestDefence)

namespace GSTNExtTestDefenceTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitType_Braceable, "Combat.HitType.GSTNExtTestBraceable", "Test only: a hit type the extension test's brace step softens");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Braced, "Combat.Event.GSTNExtTestBraced", "Test only: published by the brace step for a braced hit");
}

EGSTN_StepResult UGSTN_ExtTestStep_Brace::Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const
{
	const FGameplayTag& HitType = View.Incoming.Hit.HitType;
	if (HitType.IsValid() && HitType.MatchesTag(GSTNExtTestDefenceTags::HitType_Braceable))
	{
		Out.KnockbackScale = BracedKnockbackScale;
		Out.bNoFlinch = true;
		Out.Events.Add(GSTNExtTestDefenceTags::Event_Braced);
	}
	return EGSTN_StepResult::Continue;
}
