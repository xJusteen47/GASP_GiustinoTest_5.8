#include "GSTN_ChooserTableToolset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Chooser.h"
#include "ChooserTableEditorLibrary.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Templates/UniquePtr.h"
#include "Tests/TestAssetSandbox.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FGSTN_ChooserTableToolsetSpec, "GSTN.Toolsets.ChooserTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FTestAssetSandbox> Sandbox;
	UChooserTable* Table = nullptr;
	UObject* Asset = nullptr;

	FString CaptureError(TFunction<void()> Body)
	{
		UE::ToolsetRegistry::FToolCallExceptionHandler Handler;
		Handler.CaptureErrorsIn(MoveTemp(Body));
		return Handler.GetException();
	}

	/** An empty chooser with one class parameter (Actor), as the Chooser editor creates them. */
	UChooserTable* CreateTable()
	{
		const FString Name = FTestAssetSandbox::UniqueName(TEXT("CHT_GSTNToolsetTest"));
		UPackage* Package = CreatePackage(*(Sandbox->Folder() / Name));
		UChooserTable* Created = NewObject<UChooserTable>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Created);
		UChooserTableEditorLibrary::AddParameter(Created, TEXT("Class Parameter"));
		UChooserTableEditorLibrary::SetParameterProperty(Created, 0, TEXT("Class"), TEXT("/Script/Engine.Actor"));
		return Created;
	}

END_DEFINE_SPEC(FGSTN_ChooserTableToolsetSpec)

void FGSTN_ChooserTableToolsetSpec::Define()
{
	BeforeEach([this]()
	{
		Sandbox = MakeUnique<FTestAssetSandbox>(TEXT("GSTNChooserToolset"));
		Table = CreateTable();
		Asset = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	});

	AfterEach([this]()
	{
		Table = nullptr;
		Asset = nullptr;
		Sandbox.Reset();
	});

	Describe("Columns", [this]()
	{
		It("lists column types", [this]()
		{
			const TArray<FString> Types = UGSTN_ChooserTableToolset::ListChooserTypes(EGSTN_ChooserTypeKind::Column);
			TestTrue(TEXT("bool column"), Types.ContainsByPredicate([](const FString& Type) { return Type.EndsWith(TEXT("BoolColumn")); }));
		});

		It("adds a float range column bound to a parameter property", [this]()
		{
			const int32 Column = UGSTN_ChooserTableToolset::AddChooserColumn(Table, TEXT("FloatRangeColumn"));
			TestEqual(TEXT("index"), Column, 0);
			TestTrue(TEXT("input types"), UGSTN_ChooserTableToolset::ListChooserColumnInputTypes(Table, Column).Num() > 0);
			UGSTN_ChooserTableToolset::SetChooserColumnInput(Table, Column, FString(), 0, {TEXT("CustomTimeDilation")});
			const FGSTN_ChooserTableInfo Info = UGSTN_ChooserTableToolset::GetChooserTable(Table);
			if (TestEqual(TEXT("one column"), Info.Columns.Num(), 1))
			{
				TestTrue(TEXT("type"), Info.Columns[0].Type.EndsWith(TEXT("FloatRangeColumn")));
				TestEqual(TEXT("parameter"), Info.Columns[0].ParameterIndex, 0);
				TestTrue(TEXT("chain"), Info.Columns[0].PropertyChain.Num() == 1 && Info.Columns[0].PropertyChain[0] == TEXT("CustomTimeDilation"));
			}
		});

		It("sets a column property, moves and removes columns", [this]()
		{
			UGSTN_ChooserTableToolset::AddChooserColumn(Table, TEXT("Bool"));
			UGSTN_ChooserTableToolset::AddChooserColumn(Table, TEXT("FloatRangeColumn"));
			UGSTN_ChooserTableToolset::SetChooserColumnProperty(Table, 0, TEXT("bDisabled"), TEXT("True"));
			TestTrue(TEXT("disabled"), UGSTN_ChooserTableToolset::GetChooserTable(Table).Columns[0].bDisabled);
			TestEqual(TEXT("moved"), UGSTN_ChooserTableToolset::MoveChooserColumn(Table, 1, 0), 0);
			UGSTN_ChooserTableToolset::RemoveChooserColumn(Table, 0);
			const FGSTN_ChooserTableInfo Info = UGSTN_ChooserTableToolset::GetChooserTable(Table);
			TestTrue(TEXT("bool left"), Info.Columns.Num() == 1 && Info.Columns[0].Type.EndsWith(TEXT("BoolColumn")));
		});

		It("raises on an unknown column type, with the library's reason", [this]()
		{
			const FString Error = CaptureError([this]() { UGSTN_ChooserTableToolset::AddChooserColumn(Table, TEXT("NopeColumn")); });
			TestTrue(Error, Error.Contains(TEXT("NopeColumn")));
		});

		It("raises when the bound property does not exist", [this]()
		{
			UGSTN_ChooserTableToolset::AddChooserColumn(Table, TEXT("FloatRangeColumn"));
			const FString Error = CaptureError([this]() { UGSTN_ChooserTableToolset::SetChooserColumnInput(Table, 0, FString(), 0, {TEXT("NoSuchProperty")}); });
			TestTrue(Error, Error.Contains(TEXT("Binding column 0")));
		});
	});

	Describe("Rows", [this]()
	{
		It("adds asset rows, edits cells, disables and moves rows", [this]()
		{
			UGSTN_ChooserTableToolset::AddChooserColumn(Table, TEXT("FloatRangeColumn"));
			const int32 First = UGSTN_ChooserTableToolset::AddChooserRow(Table, TEXT("AssetChooser"), {}, Asset);
			const int32 Second = UGSTN_ChooserTableToolset::AddChooserRow(Table, TEXT("AssetChooser"), 0);
			TestEqual(TEXT("first"), First, 0);
			TestEqual(TEXT("inserted before"), Second, 0);
			UGSTN_ChooserTableToolset::SetChooserCell(Table, 0, 1, TEXT("(Min=0.0,Max=300.0,bNoMin=False,bNoMax=False)"));
			UGSTN_ChooserTableToolset::SetChooserRowDisabled(Table, 0, true);
			FGSTN_ChooserTableInfo Info = UGSTN_ChooserTableToolset::GetChooserTable(Table);
			if (TestEqual(TEXT("rows"), Info.Rows.Num(), 2))
			{
				TestTrue(TEXT("asset"), Info.Rows[1].Asset == Asset);
				TestTrue(TEXT("cell"), Info.Rows[1].Cells.Num() == 1 && Info.Rows[1].Cells[0].Contains(TEXT("Max=300")));
				TestTrue(TEXT("disabled"), Info.Rows[0].bDisabled);
			}
			UGSTN_ChooserTableToolset::MoveChooserRow(Table, 1, 0);
			Info = UGSTN_ChooserTableToolset::GetChooserTable(Table);
			TestTrue(TEXT("moved"), Info.Rows[0].Asset == Asset);
			UGSTN_ChooserTableToolset::RemoveChooserRows(Table, {0, 1});
			TestEqual(TEXT("removed"), UGSTN_ChooserTableToolset::GetChooserTable(Table).Rows.Num(), 0);
		});

		It("sets a row result and the fallback", [this]()
		{
			UGSTN_ChooserTableToolset::AddChooserRow(Table, TEXT("AssetChooser"), {});
			UGSTN_ChooserTableToolset::SetChooserRowResult(Table, 0, FString(), {{TEXT("Asset"), Asset->GetPathName()}});
			UGSTN_ChooserTableToolset::SetChooserRowResult(Table, -1, TEXT("AssetChooser"), {{TEXT("Asset"), Asset->GetPathName()}});
			const FGSTN_ChooserTableInfo Info = UGSTN_ChooserTableToolset::GetChooserTable(Table);
			TestTrue(TEXT("row asset"), Info.Rows[0].Asset == Asset);
			TestTrue(TEXT("fallback"), Info.Fallback.Contains(Asset->GetName()));
		});

		It("raises on bad indices, a missing fallback and a null table", [this]()
		{
			TestTrue(TEXT("cell"), CaptureError([this]() { UGSTN_ChooserTableToolset::SetChooserCell(Table, 3, 0, TEXT("x")); }).Contains(TEXT("Setting cell")));
			TestTrue(TEXT("move"), CaptureError([this]() { UGSTN_ChooserTableToolset::MoveChooserRow(Table, 5, 0); }).Contains(TEXT("Moving row 5")));
			TestTrue(TEXT("fallback"), CaptureError([this]() { UGSTN_ChooserTableToolset::SetChooserRowResult(Table, -1, FString(), {}); }).Contains(TEXT("no fallback")));
			TestTrue(TEXT("null"), CaptureError([]() { UGSTN_ChooserTableToolset::GetChooserTable(nullptr); }).Contains(TEXT("null")));
		});
	});
}

#endif
