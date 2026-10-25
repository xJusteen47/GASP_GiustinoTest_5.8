// Platform runtime: components, colliders, the pose point (Mover, no GAS, no direct Network Prediction code).

#include "GSTN_PlatformDefinition.h"

#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformDefinition)

#if WITH_EDITOR
EDataValidationResult UGSTN_PlatformDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FText> Errors, Warnings;
	Spec.CollectFindings(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}
	return Errors.Num() > 0 ? EDataValidationResult::Invalid : Result;
}
#endif
