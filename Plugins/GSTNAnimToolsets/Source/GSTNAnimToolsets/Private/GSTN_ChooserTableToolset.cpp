#include "GSTN_ChooserTableToolset.h"

#include "Chooser.h"
#include "ChooserTableEditorLibrary.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "ToolsetRegistry/RaiseScriptError.h"

namespace GSTN::ChooserToolset
{
	/**
	 * Collects the warnings the Chooser editor library logs while one call runs: the library reports failures only as a log
	 * warning and a false/INDEX_NONE return, and the warning is the useful part of the error.
	 */
	class FScopedWarningCapture : public FOutputDevice
	{
	public:
		FScopedWarningCapture()
		{
			GLog->AddOutputDevice(this);
		}

		virtual ~FScopedWarningCapture() override
		{
			GLog->RemoveOutputDevice(this);
		}

		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Verbosity <= ELogVerbosity::Warning)
			{
				FScopeLock Lock(&Mutex);
				Lines.Add(Message);
			}
		}

		virtual bool CanBeUsedOnMultipleThreads() const override
		{
			return true;
		}

		FString Text() const
		{
			FScopeLock Lock(&Mutex);
			return FString::Join(Lines, TEXT(" "));
		}

	private:
		mutable FCriticalSection Mutex;
		TArray<FString> Lines;
	};

	/** Raises "<What> failed: <library warnings>" when bOk is false. */
	bool Check(bool bOk, const FScopedWarningCapture& Capture, const FString& What)
	{
		if (!bOk)
		{
			const FString Details = Capture.Text();
			UE::ToolsetRegistry::RaiseScriptError(Details.IsEmpty() ? What + TEXT(" failed.") : FString::Printf(TEXT("%s failed: %s"), *What, *Details));
		}
		return bOk;
	}

	bool RequireChooser(const UChooserTable* Chooser)
	{
		if (!Chooser)
		{
			UE::ToolsetRegistry::RaiseScriptError(TEXT("Chooser is null."));
		}
		return Chooser != nullptr;
	}

	/** The Asset property of a result struct, if it has one. */
	UObject* ResultAsset(const FInstancedStruct& Result)
	{
		const UScriptStruct* Type = Result.GetScriptStruct();
		const FObjectPropertyBase* AssetProperty = Type ? CastField<FObjectPropertyBase>(Type->FindPropertyByName(TEXT("Asset"))) : nullptr;
		if (AssetProperty)
		{
			return AssetProperty->GetObjectPropertyValue_InContainer(Result.GetMemory());
		}
		if (const FSoftObjectProperty* SoftProperty = Type ? CastField<FSoftObjectProperty>(Type->FindPropertyByName(TEXT("Asset"))) : nullptr)
		{
			return SoftProperty->GetPropertyValue_InContainer(Result.GetMemory()).Get();
		}
		return nullptr;
	}
}

using namespace GSTN::ChooserToolset;
using Lib = UChooserTableEditorLibrary;

FGSTN_ChooserTableInfo UGSTN_ChooserTableToolset::GetChooserTable(UChooserTable* Chooser)
{
	if (!RequireChooser(Chooser))
	{
		return {};
	}
	FScopedWarningCapture Capture;
	FGSTN_ChooserTableInfo Info;
	Info.ResultType = Lib::GetResultType(Chooser);
	Info.ResultClass = Lib::GetResultClass(Chooser);
	for (int32 Index = 0; Index < Lib::GetNumParameters(Chooser); ++Index)
	{
		Info.Parameters.Add(Lib::GetParameterType(Chooser, Index) + TEXT(" ") + Lib::GetParameterProperty(Chooser, Index, FString()));
	}

	const int32 NumColumns = Lib::GetNumColumns(Chooser);
	const int32 NumRows = Lib::GetNumRows(Chooser);
	for (int32 Column = 0; Column < NumColumns; ++Column)
	{
		FGSTN_ChooserColumnInfo& ColumnInfo = Info.Columns.AddDefaulted_GetRef();
		ColumnInfo.Type = Lib::GetColumnType(Chooser, Column);
		ColumnInfo.InputType = Lib::GetColumnInputType(Chooser, Column);
		ColumnInfo.bDisabled = Lib::IsColumnDisabled(Chooser, Column);
		if (!Lib::GetColumnInputBinding(Chooser, Column, ColumnInfo.ParameterIndex, ColumnInfo.PropertyChain))
		{
			ColumnInfo.ParameterIndex = INDEX_NONE;
			ColumnInfo.PropertyChain.Reset();
		}
		// FChooserColumnBase has no exported StaticStruct, so the typed FInstancedStruct getters cannot be used here.
		static const UScriptStruct* ColumnBase = FindObject<UScriptStruct>(nullptr, TEXT("/Script/Chooser.ChooserColumnBase"));
		const FInstancedStruct* ColumnStruct = Chooser->ColumnsStructs.IsValidIndex(Column) ? &Chooser->ColumnsStructs[Column] : nullptr;
		const FChooserColumnBase* ColumnData = ColumnStruct && ColumnBase && ColumnStruct->GetScriptStruct()
			&& ColumnStruct->GetScriptStruct()->IsChildOf(ColumnBase) ? reinterpret_cast<const FChooserColumnBase*>(ColumnStruct->GetMemory()) : nullptr;
		if (ColumnData && ColumnData->HasOutputs())
		{
			ColumnInfo.FallbackValue = Lib::GetFallbackCellValue(Chooser, Column);
		}
	}
	for (int32 Row = 0; Row < NumRows; ++Row)
	{
		FGSTN_ChooserRowInfo& RowInfo = Info.Rows.AddDefaulted_GetRef();
		RowInfo.ResultType = Lib::GetRowResultType(Chooser, Row);
		RowInfo.Result = Lib::GetRowResultProperty(Chooser, Row, FString());
		RowInfo.Asset = Chooser->ResultsStructs.IsValidIndex(Row) ? ResultAsset(Chooser->ResultsStructs[Row]) : nullptr;
		RowInfo.bDisabled = Lib::IsRowDisabled(Chooser, Row);
		for (int32 Column = 0; Column < NumColumns; ++Column)
		{
			RowInfo.Cells.Add(Lib::GetCellValue(Chooser, Column, Row));
		}
	}
	if (Lib::HasFallback(Chooser))
	{
		Info.Fallback = Lib::GetFallbackResultProperty(Chooser, FString());
	}
	Info.NestedChoosers = TArray<TObjectPtr<UChooserTable>>(Lib::GetNestedChoosers(Chooser));
	return Info;
}

TArray<FString> UGSTN_ChooserTableToolset::ListChooserTypes(EGSTN_ChooserTypeKind Kind)
{
	switch (Kind)
	{
	case EGSTN_ChooserTypeKind::Result: return Lib::GetAvailableResultTypes();
	case EGSTN_ChooserTypeKind::Parameter: return Lib::GetAvailableParameterTypes();
	default: return Lib::GetAvailableColumnTypes();
	}
}

TArray<FString> UGSTN_ChooserTableToolset::ListChooserColumnInputTypes(UChooserTable* Chooser, int32 Column)
{
	if (!RequireChooser(Chooser))
	{
		return {};
	}
	FScopedWarningCapture Capture;
	if (!Check(Column >= 0 && Column < Lib::GetNumColumns(Chooser), Capture, FString::Printf(TEXT("Column %d"), Column)))
	{
		return {};
	}
	return Lib::GetAvailableColumnInputTypes(Chooser, Column);
}

int32 UGSTN_ChooserTableToolset::AddChooserColumn(UChooserTable* Chooser, const FString& ColumnType)
{
	if (!RequireChooser(Chooser))
	{
		return INDEX_NONE;
	}
	FScopedWarningCapture Capture;
	const int32 Column = Lib::AddColumn(Chooser, ColumnType);
	Check(Column != INDEX_NONE, Capture, FString::Printf(TEXT("Adding a '%s' column"), *ColumnType));
	return Column;
}

void UGSTN_ChooserTableToolset::RemoveChooserColumn(UChooserTable* Chooser, int32 Column)
{
	if (RequireChooser(Chooser))
	{
		FScopedWarningCapture Capture;
		Check(Lib::DeleteColumn(Chooser, Column), Capture, FString::Printf(TEXT("Removing column %d"), Column));
	}
}

int32 UGSTN_ChooserTableToolset::MoveChooserColumn(UChooserTable* Chooser, int32 Column, int32 NewIndex)
{
	if (!RequireChooser(Chooser))
	{
		return INDEX_NONE;
	}
	FScopedWarningCapture Capture;
	const int32 Moved = Lib::MoveColumn(Chooser, Column, NewIndex);
	Check(Moved != INDEX_NONE, Capture, FString::Printf(TEXT("Moving column %d"), Column));
	return Moved;
}

void UGSTN_ChooserTableToolset::SetChooserColumnInput(UChooserTable* Chooser, int32 Column, const FString& InputType,
	int32 ParameterIndex, const TArray<FName>& PropertyChain)
{
	if (!RequireChooser(Chooser))
	{
		return;
	}
	FScopedWarningCapture Capture;
	if (!InputType.IsEmpty() && !Check(Lib::SetColumnInputType(Chooser, Column, InputType), Capture,
		FString::Printf(TEXT("Setting input type '%s' on column %d"), *InputType, Column)))
	{
		return;
	}
	Check(Lib::SetColumnInputBinding(Chooser, Column, ParameterIndex, PropertyChain), Capture,
		FString::Printf(TEXT("Binding column %d to parameter %d [%s]"), Column, ParameterIndex,
			*FString::JoinBy(PropertyChain, TEXT("."), [](const FName& Name) { return Name.ToString(); })));
}

void UGSTN_ChooserTableToolset::SetChooserColumnProperty(UChooserTable* Chooser, int32 Column, const FString& PropertyPath, const FString& Value)
{
	if (RequireChooser(Chooser))
	{
		FScopedWarningCapture Capture;
		Check(Lib::SetColumnProperty(Chooser, Column, PropertyPath, Value), Capture,
			FString::Printf(TEXT("Setting '%s' on column %d"), *PropertyPath, Column));
	}
}

int32 UGSTN_ChooserTableToolset::AddChooserRow(UChooserTable* Chooser, const FString& ResultType, TOptional<int32> RowIndex, UObject* Asset)
{
	if (!RequireChooser(Chooser))
	{
		return INDEX_NONE;
	}
	FScopedWarningCapture Capture;
	const int32 Row = RowIndex.IsSet() ? Lib::InsertRow(Chooser, RowIndex.GetValue(), ResultType) : Lib::AddRow(Chooser, ResultType);
	if (!Check(Row != INDEX_NONE, Capture, FString::Printf(TEXT("Adding a row with result '%s'"), *ResultType)))
	{
		return INDEX_NONE;
	}
	if (Asset)
	{
		Check(Lib::SetRowResultProperty(Chooser, Row, TEXT("Asset"), Asset->GetPathName()), Capture,
			FString::Printf(TEXT("Setting the asset of row %d"), Row));
	}
	return Row;
}

void UGSTN_ChooserTableToolset::RemoveChooserRows(UChooserTable* Chooser, const TArray<int32>& Rows)
{
	if (RequireChooser(Chooser))
	{
		FScopedWarningCapture Capture;
		Check(Lib::DeleteRows(Chooser, Rows), Capture, TEXT("Removing rows"));
	}
}

void UGSTN_ChooserTableToolset::MoveChooserRow(UChooserTable* Chooser, int32 Row, int32 NewIndex)
{
	if (RequireChooser(Chooser))
	{
		FScopedWarningCapture Capture;
		Check(Lib::MoveRow(Chooser, Row, NewIndex), Capture, FString::Printf(TEXT("Moving row %d to %d"), Row, NewIndex));
	}
}

void UGSTN_ChooserTableToolset::SetChooserRowDisabled(UChooserTable* Chooser, int32 Row, bool bDisabled)
{
	if (RequireChooser(Chooser))
	{
		FScopedWarningCapture Capture;
		Check(Lib::SetRowDisabled(Chooser, Row, bDisabled), Capture, FString::Printf(TEXT("Disabling row %d"), Row));
	}
}

void UGSTN_ChooserTableToolset::SetChooserRowResult(UChooserTable* Chooser, int32 Row, const FString& ResultType,
	const TMap<FString, FString>& Properties)
{
	if (!RequireChooser(Chooser))
	{
		return;
	}
	FScopedWarningCapture Capture;
	const bool bFallback = Row == INDEX_NONE;
	const FString Where = bFallback ? FString(TEXT("the fallback")) : FString::Printf(TEXT("row %d"), Row);
	if (bFallback && ResultType.IsEmpty() && !Lib::HasFallback(Chooser))
	{
		Check(false, Capture, TEXT("The table has no fallback: pass a ResultType to create it. Editing the fallback"));
		return;
	}
	if (!ResultType.IsEmpty())
	{
		const bool bTypeSet = bFallback ? Lib::SetFallbackResultType(Chooser, ResultType) : Lib::SetRowResultType(Chooser, Row, ResultType);
		if (!Check(bTypeSet, Capture, FString::Printf(TEXT("Setting result type '%s' on %s"), *ResultType, *Where)))
		{
			return;
		}
	}
	for (const TPair<FString, FString>& Property : Properties)
	{
		const bool bSet = bFallback ? Lib::SetFallbackResultProperty(Chooser, Property.Key, Property.Value)
			: Lib::SetRowResultProperty(Chooser, Row, Property.Key, Property.Value);
		if (!Check(bSet, Capture, FString::Printf(TEXT("Setting '%s' on the result of %s"), *Property.Key, *Where)))
		{
			return;
		}
	}
}

void UGSTN_ChooserTableToolset::SetChooserCell(UChooserTable* Chooser, int32 Column, int32 Row, const FString& Value)
{
	if (!RequireChooser(Chooser))
	{
		return;
	}
	FScopedWarningCapture Capture;
	const bool bSet = Row == INDEX_NONE ? Lib::SetFallbackCellValue(Chooser, Column, Value) : Lib::SetCellValue(Chooser, Column, Row, Value);
	Check(bSet, Capture, FString::Printf(TEXT("Setting cell (column %d, row %d)"), Column, Row));
}
