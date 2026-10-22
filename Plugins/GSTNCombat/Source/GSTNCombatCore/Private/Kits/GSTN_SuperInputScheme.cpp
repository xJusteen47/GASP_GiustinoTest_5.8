// Pure combat logic (no GAS, no Network Prediction).

#include "Kits/GSTN_SuperInputScheme.h"
#include "Kits/GSTN_CombatAssetTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UGSTN_SuperInputScheme::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNCombatAssetTypes::SuperInputScheme(), GetFName());
}

void UGSTN_SuperInputScheme::Validate(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	if (Mode != EGSTN_SuperInputMode::FixedSlots)
	{
		if (!FixedSlots.IsEmpty())
		{
			OutWarnings.Add(FText::FromString(TEXT("FixedSlots are ignored in PerSuperCode mode.")));
		}
		return;
	}

	if (FixedSlots.IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("FixedSlots mode needs one slot per super slot and has none.")));
	}
	if (FixedSlots.Num() > GSTNCombatAssetTypes::MaxSuperSlots)
	{
		OutErrors.Add(FText::FromString(FString::Printf(TEXT("A kit has at most %d super slots and the scheme lists %d."), GSTNCombatAssetTypes::MaxSuperSlots, FixedSlots.Num())));
	}

	TSet<FGameplayTag> Seen;
	for (int32 Index = 0; Index < FixedSlots.Num(); ++Index)
	{
		const FGameplayTag& Command = FixedSlots[Index].Command;
		if (!Command.IsValid())
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("Fixed slot %d has no command."), Index)));
		}
		else if (Seen.Contains(Command))
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("Fixed slot %d repeats the command %s."), Index, *Command.ToString())));
		}
		Seen.Add(Command);
	}
}

#if WITH_EDITOR
EDataValidationResult UGSTN_SuperInputScheme::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Errors;
	TArray<FText> Warnings;
	Validate(Errors, Warnings);
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
		Result = EDataValidationResult::Invalid;
	}
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
