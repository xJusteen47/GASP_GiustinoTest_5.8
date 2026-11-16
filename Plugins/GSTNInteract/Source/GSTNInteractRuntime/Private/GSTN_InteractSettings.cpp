// Interaction project settings and the rules asset (interaction.md §9).

#include "GSTN_InteractSettings.h"

#include "GSTN_InteractPresenter.h"
#include "GSTN_InteractRulesAsset.h"

UGSTN_InteractSettings::UGSTN_InteractSettings()
{
	CategoryName = TEXT("Plugins");
	PresenterClasses.Add(UGSTN_InteractPresenter_Debug::StaticClass());
}

FGSTN_InteractRules UGSTN_InteractSettings::GetSanitisedDefaultRules() const
{
	FGSTN_InteractRules Rules = DefaultRules;
	Rules.Sanitise();
	return Rules;
}

void UGSTN_InteractRulesAsset::PostLoad()
{
	Super::PostLoad();
	Rules.Sanitise();
}

#if WITH_EDITOR
void UGSTN_InteractRulesAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Rules.Sanitise();
}
#endif
