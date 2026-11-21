// Surface audio and haptics (docs/notes/surface-feedback.md).

#include "GSTN_SurfaceResponseTypes.h"

#include "GSTN_PresentationTags.h"
#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SurfaceResponseTypes)

namespace
{
	/** Nodes in a tag (Foley.Event.Run = 3): a deeper row tag is a more specific answer */
	int32 TagDepth(const FGameplayTag& Tag)
	{
		int32 Depth = 1;
		for (const TCHAR Char : Tag.ToString())
		{
			Depth += Char == TEXT('.') ? 1 : 0;
		}
		return Depth;
	}
}

const TArray<FGameplayTag>& UGSTN_SurfaceResponseTable::DefaultFootwearOrder()
{
	// Built on first use, not at static init: native tags are registered by the module, after statics run
	static const TArray<FGameplayTag> Order = {
		GSTNPresentationTags::Footwear_Clay, GSTNPresentationTags::Footwear_Metal, GSTNPresentationTags::Footwear_Barefoot,
		GSTNPresentationTags::Footwear_ShoeSole};
	return Order;
}

int32 UGSTN_SurfaceResponseTable::ScoreRow(const FGSTN_SurfaceResponseRow& Row, const FGameplayTag& EventTag, const FGameplayTag& Footwear,
	EPhysicalSurface Floor)
{
	if (!Row.EventTag.IsValid() || !EventTag.MatchesTag(Row.EventTag))
	{
		return -1;
	}
	if (Row.Footwear.IsValid() && Row.Footwear != Footwear)
	{
		return -1;
	}
	if (!Row.bAnyFloor && Row.Floor.GetValue() != Floor)
	{
		return -1;
	}
	// Event specificity first (the research's chain: exact row, footwear x any floor, event default, then the parent event tag)
	return TagDepth(Row.EventTag) * 4 + (Row.Footwear.IsValid() ? 2 : 0) + (Row.bAnyFloor ? 0 : 1);
}

const FGSTN_SurfaceResponseRow* UGSTN_SurfaceResponseTable::FindRow(const FGameplayTag& EventTag, const FGameplayTag& Footwear,
	EPhysicalSurface Floor) const
{
	const UGSTN_SurfaceResponseTable* Table = this;
	for (int32 Depth = 0; Table && Depth < 16; ++Depth, Table = Table->Parent)
	{
		const FGSTN_SurfaceResponseRow* Best = nullptr;
		int32 BestScore = -1;
		for (const FGSTN_SurfaceResponseRow& Row : Table->Rows)
		{
			const int32 Score = ScoreRow(Row, EventTag, Footwear, Floor);
			if (Score > BestScore)  // strictly greater: a tie keeps the first row
			{
				Best = &Row;
				BestScore = Score;
			}
		}
		if (Best)
		{
			return Best;
		}
	}
	return nullptr;
}

bool UGSTN_SurfaceResponseTable::NeedsFloor(const FGameplayTag& EventTag) const
{
	const UGSTN_SurfaceResponseTable* Table = this;
	for (int32 Depth = 0; Table && Depth < 16; ++Depth, Table = Table->Parent)
	{
		for (const FGSTN_SurfaceResponseRow& Row : Table->Rows)
		{
			if (!Row.bAnyFloor && Row.EventTag.IsValid() && EventTag.MatchesTag(Row.EventTag))
			{
				return true;
			}
		}
	}
	return false;
}

int32 UGSTN_SurfaceResponseTable::GetFootwearParam(FGameplayTag Footwear) const
{
	const TArray<FGameplayTag>& Order = FootwearParamOrder.Num() > 0 ? FootwearParamOrder : DefaultFootwearOrder();
	const int32 Index = Order.IndexOfByKey(Footwear);
	return Index == INDEX_NONE ? 0 : Index;
}

bool UGSTN_SurfaceResponseTable::FindRowBP(FGameplayTag EventTag, FGameplayTag Footwear, TEnumAsByte<EPhysicalSurface> Floor,
	FGSTN_SurfaceResponseRow& OutRow) const
{
	if (const FGSTN_SurfaceResponseRow* Row = FindRow(EventTag, Footwear, Floor.GetValue()))
	{
		OutRow = *Row;
		return true;
	}
	return false;
}

#if WITH_EDITOR
EDataValidationResult UGSTN_SurfaceResponseTable::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FGSTN_SurfaceResponseRow& Row = Rows[Index];
		if (!Row.EventTag.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Rows[%d] has no event tag"), Index)));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		for (int32 Other = 0; Other < Index; ++Other)
		{
			const FGSTN_SurfaceResponseRow& Prev = Rows[Other];
			if (Prev.EventTag == Row.EventTag && Prev.Footwear == Row.Footwear && Prev.bAnyFloor == Row.bAnyFloor
				&& (Row.bAnyFloor || Prev.Floor == Row.Floor))
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("Rows[%d] repeats the key of Rows[%d] (%s): only the first ever plays"),
					Index, Other, *Row.EventTag.ToString())));
				Result = EDataValidationResult::Invalid;
			}
		}
		if (Row.PitchRange.X <= 0.0 || Row.PitchRange.Y < Row.PitchRange.X)
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Rows[%d]: PitchRange must be > 0 with min <= max"), Index)));
			Result = EDataValidationResult::Invalid;
		}
	}
	const UGSTN_SurfaceResponseTable* Table = Parent;
	for (int32 Depth = 0; Table; ++Depth, Table = Table->Parent)
	{
		if (Table == this || Depth >= 16)
		{
			Context.AddError(FText::FromString(TEXT("The Parent chain loops back to this table (or is deeper than 16)")));
			return EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif
