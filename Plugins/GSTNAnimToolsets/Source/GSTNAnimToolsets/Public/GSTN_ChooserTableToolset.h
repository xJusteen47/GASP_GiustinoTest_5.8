#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "GSTN_ChooserTableToolset.generated.h"

class UChooserTable;

/** A column of a Chooser Table. */
USTRUCT(BlueprintType)
struct FGSTN_ChooserColumnInfo
{
	GENERATED_BODY()

	/** Column type path, e.g. "/Script/Chooser.FloatRangeColumn". */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	FString Type;

	/** Input type path, e.g. "/Script/Chooser.FloatContextProperty". Empty when the column has no input. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	FString InputType;

	/** Index of the chooser parameter the input reads, or -1 when it is not bound to a property. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	int32 ParameterIndex = -1;

	/** Property (or thread-safe function) names followed from the parameter, e.g. ["Speed"]. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	TArray<FName> PropertyChain;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	bool bDisabled = false;

	/** Cell value of the fallback row; output columns only, empty otherwise. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	FString FallbackValue;
};

/** A row of a Chooser Table. */
USTRUCT(BlueprintType)
struct FGSTN_ChooserRowInfo
{
	GENERATED_BODY()

	/** Result type path, e.g. "/Script/Chooser.AssetChooser". */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	FString ResultType;

	/** The whole result in Unreal text format. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	FString Result;

	/** The result's asset when the result type has an Asset property, else null. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	TObjectPtr<UObject> Asset = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	bool bDisabled = false;

	/** One value per column, in Unreal text format, in column order. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	TArray<FString> Cells;
};

/** A whole Chooser Table: signature, columns, rows and fallback. */
USTRUCT(BlueprintType)
struct FGSTN_ChooserTableInfo
{
	GENERATED_BODY()

	/** ObjectResult, ClassResult or NoPrimaryResult. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	FString ResultType;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	TObjectPtr<UClass> ResultClass = nullptr;

	/** Each parameter (context object or struct) in Unreal text format, by parameter index. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	TArray<FString> Parameters;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	TArray<FGSTN_ChooserColumnInfo> Columns;

	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	TArray<FGSTN_ChooserRowInfo> Rows;

	/** The fallback result in Unreal text format, empty when there is none. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	FString Fallback;

	/** Nested choosers of the asset; pass them as Chooser to edit them. */
	UPROPERTY(BlueprintReadWrite, Category = "GSTN|ChooserToolset")
	TArray<TObjectPtr<UChooserTable>> NestedChoosers;
};

/** What ListChooserTypes lists. */
UENUM(BlueprintType)
enum class EGSTN_ChooserTypeKind : uint8
{
	Column,
	Result,
	Parameter,
};

/**
 * Reads and edits Chooser Tables (and their nested choosers) the way the Chooser editor does: columns and their input bindings,
 * rows, cells, results, the fallback row. Values are Unreal text format strings, as in the details panel copy/paste. Indices are
 * 0-based; row -1 is the fallback.
 */
UCLASS(BlueprintType, MinimalAPI)
class UGSTN_ChooserTableToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Returns the whole table: signature, columns, rows with cells and results, fallback, nested choosers.
	 * @param Chooser The table, or a nested chooser.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GSTN|ChooserToolset")
	static FGSTN_ChooserTableInfo GetChooserTable(UChooserTable* Chooser);

	/**
	 * Lists the type names accepted for columns, row results or parameters.
	 * @param Kind Which types.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GSTN|ChooserToolset")
	static TArray<FString> ListChooserTypes(EGSTN_ChooserTypeKind Kind);

	/**
	 * Lists the input types a column accepts.
	 * @param Chooser The table.
	 * @param Column Column index.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "GSTN|ChooserToolset")
	static TArray<FString> ListChooserColumnInputTypes(UChooserTable* Chooser, int32 Column);

	/**
	 * Adds a column, placed like the editor does (filters, then outputs, then randomize).
	 * @param Chooser The table.
	 * @param ColumnType Type path, short name or display name, e.g. "FloatRangeColumn", "Bool", "OutputFloatColumn".
	 * @return The new column index.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static int32 AddChooserColumn(UChooserTable* Chooser, const FString& ColumnType);

	/**
	 * Removes a column and its cells.
	 * @param Chooser The table.
	 * @param Column Column index.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static void RemoveChooserColumn(UChooserTable* Chooser, int32 Column);

	/**
	 * Moves a column. Columns cannot go after a randomize column.
	 * @param Chooser The table.
	 * @param Column Column index.
	 * @param NewIndex Wanted index.
	 * @return The column's index after the move.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static int32 MoveChooserColumn(UChooserTable* Chooser, int32 Column, int32 NewIndex);

	/**
	 * Sets what a column reads: optionally changes its input type, then binds it to a property of a parameter.
	 * @param Chooser The table.
	 * @param Column Column index.
	 * @param InputType New input type (see ListChooserColumnInputTypes); empty keeps the current one.
	 * @param ParameterIndex Index of the class or struct parameter to read.
	 * @param PropertyChain Property or thread-safe function names to follow from the parameter, e.g. ["Speed"] or ["MyStruct", "MyFloat"].
	 *        Empty binds the parameter itself (struct inputs).
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static void SetChooserColumnInput(UChooserTable* Chooser, int32 Column, const FString& InputType, int32 ParameterIndex,
		const TArray<FName>& PropertyChain);

	/**
	 * Sets any property of a column by path, e.g. "bDisabled", "DefaultRowValue", "bWrapInput".
	 * @param Chooser The table.
	 * @param Column Column index.
	 * @param PropertyPath '.' separated path; steps into instanced structs.
	 * @param Value Unreal text format.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static void SetChooserColumnProperty(UChooserTable* Chooser, int32 Column, const FString& PropertyPath, const FString& Value);

	/**
	 * Adds a row with default cells.
	 * @param Chooser The table.
	 * @param ResultType Result type, e.g. "AssetChooser", "SoftAssetChooser", "EvaluateChooser"; empty for choosers with no primary result.
	 * @param RowIndex Insert before this row; at the end if unset.
	 * @param Asset For a result type with an Asset property: the asset to return. Null leaves it empty.
	 * @return The new row index.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static int32 AddChooserRow(UChooserTable* Chooser, const FString& ResultType, TOptional<int32> RowIndex, UObject* Asset = nullptr);

	/**
	 * Removes rows.
	 * @param Chooser The table.
	 * @param Rows Row indices.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static void RemoveChooserRows(UChooserTable* Chooser, const TArray<int32>& Rows);

	/**
	 * Moves a row so that it ends up at NewIndex.
	 * @param Chooser The table.
	 * @param Row Row index.
	 * @param NewIndex Wanted index.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static void MoveChooserRow(UChooserTable* Chooser, int32 Row, int32 NewIndex);

	/**
	 * Enables or disables a row.
	 * @param Chooser The table.
	 * @param Row Row index.
	 * @param bDisabled True to disable.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static void SetChooserRowDisabled(UChooserTable* Chooser, int32 Row, bool bDisabled);

	/**
	 * Sets a row's result (or the fallback's). Auto-populating columns update like in the editor.
	 * @param Chooser The table.
	 * @param Row Row index, or -1 for the fallback (created if missing).
	 * @param ResultType New result type; empty keeps the current one.
	 * @param Properties Result properties to set, by path, in Unreal text format, e.g. {"Asset": "/Game/Anims/Run.Run"}.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static void SetChooserRowResult(UChooserTable* Chooser, int32 Row, const FString& ResultType, const TMap<FString, FString>& Properties);

	/**
	 * Sets a cell, e.g. "(Min=0.0,Max=300.0,bNoMin=False,bNoMax=False)" for a float range or "MatchTrue" for a bool column.
	 * The current value in GetChooserTable shows the format.
	 * @param Chooser The table.
	 * @param Column Column index.
	 * @param Row Row index, or -1 for the fallback (output columns only).
	 * @param Value Unreal text format.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static void SetChooserCell(UChooserTable* Chooser, int32 Column, int32 Row, const FString& Value);

	/**
	 * Copies packages to new paths and remaps references between the copied packages only (AssetTools AdvancedCopyPackages,
	 * no dependency gathering, no UI). Use it to copy an AnimBlueprint together with its choosers so the copies point at each
	 * other; anything outside the map stays shared. References to objects of packages outside the map (e.g. a chooser's
	 * context class) are not touched: retarget them afterwards.
	 * @param SourceToDestPackages Long package names, e.g. {"/Game/A/ABP_X": "/Game/B/ABP_Y"}.
	 * @return true if the copy succeeded.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"), Category = "GSTN|ChooserToolset")
	static bool CopyPackagesRemapped(const TMap<FString, FString>& SourceToDestPackages);
};
