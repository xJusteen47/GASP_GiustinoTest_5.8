// The tests of the move graph authoring library (CP3 of the Blueprint seams): a script builds a working move graph without a hand on the editor.
// The Blueprint is made here, in a temporary package, from the real ability class and compiled for real; no project content is loaded.

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Graph/GSTN_MoveGraphAuthoring.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveGraphAuthoringTest, "GSTNCombat.Editor.MoveGraph.BuildsAndCompiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_MoveGraphAuthoringTest::RunTest(const FString& Parameters)
{
	using Lib = UGSTN_MoveGraphAuthoring;
	const FString Name = FString::Printf(TEXT("GSTNMoveGraphTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	TStrongObjectPtr<UPackage> Package(CreatePackage(*(FString(TEXT("/Temp/")) + Name)));
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(UGSTN_GA_BaseCombat::StaticClass(), Package.Get(), FName(*Name),
		BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName(TEXT("GSTNMoveGraphTest"))));
	if (!TestTrue(TEXT("the ability Blueprint was made"), Blueprint.IsValid()))
	{
		return false;
	}
	UBlueprint* BP = Blueprint.Get();

	FString Error;
	TestFalse(TEXT("an unknown type is refused"), Lib::SetMemberVariable(BP, TEXT("Bad"), TEXT("quaternion"), FString(), FString(), FString(), Error));
	TestTrue(TEXT("the sync state variable"), Lib::SetMemberVariable(BP, TEXT("State"), TEXT("struct:/Script/AbilitySystemSimulation.SyncInteger"),
		FString(), TEXT("Example"), TEXT("0 idle, 1 armed"), Error));
	TestTrue(TEXT("a tag setting"), Lib::SetMemberVariable(BP, TEXT("WindowTag"), TEXT("struct:/Script/GameplayTags.GameplayTag"),
		TEXT("(TagName=\"\")"), TEXT("Example"), FString(), Error));
	TArray<FString> Messages;
	TestEqual(TEXT("the empty Blueprint compiles"), Lib::CompileAndReport(BP, Messages), 0);

	// On Move Key -> Branch(KeyTag matches WindowTag) -> Set State (Make Sync Integer 1) -> Open Move Tag Window(WindowTag, 12)
	const FString OnKey = Lib::AddEventNode(BP, TEXT("K2_OnMoveKey"), 0, 0, Error);
	const FString Matches = Lib::AddCallNode(BP, TEXT("/Script/GameplayTags.BlueprintGameplayTagLibrary"), TEXT("MatchesTag"), 300, 150, Error);
	const FString Tag = Lib::AddVariableNode(BP, TEXT("WindowTag"), false, 0, 250, Error);
	const FString Branch = Lib::AddBranchNode(BP, 600, 0, Error);
	const FString Make = Lib::AddStructNode(BP, TEXT("/Script/AbilitySystemSimulation.SyncInteger"), true, 700, 200, Error);
	const FString Set = Lib::AddVariableNode(BP, TEXT("State"), true, 900, 0, Error);
	const FString Open = Lib::AddCallNode(BP, FString(), TEXT("OpenMoveTagWindow"), 1200, 0, Error);
	if (!TestTrue(TEXT("every node was made: ") + Error, !OnKey.IsEmpty() && !Matches.IsEmpty() && !Tag.IsEmpty() && !Branch.IsEmpty() && !Make.IsEmpty()
		&& !Set.IsEmpty() && !Open.IsEmpty()))
	{
		return false;
	}
	TestTrue(TEXT("a pin that does not exist is reported"), !Lib::ConnectPins(BP, OnKey, TEXT("NoSuchPin"), Branch, TEXT("execute"), Error) && Error.Contains(TEXT("NoSuchPin")));
	const TPair<bool, const TCHAR*> Links[] = {
		{ Lib::ConnectPins(BP, OnKey, TEXT("then"), Branch, TEXT("execute"), Error), TEXT("event -> branch") },
		{ Lib::ConnectPins(BP, OnKey, TEXT("KeyTag"), Matches, TEXT("TagOne"), Error), TEXT("key tag -> matches") },
		{ Lib::ConnectPins(BP, Tag, NAME_None, Matches, TEXT("TagTwo"), Error), TEXT("setting -> matches") },
		{ Lib::ConnectPins(BP, Matches, TEXT("ReturnValue"), Branch, TEXT("Condition"), Error), TEXT("matches -> condition") },
		{ Lib::ConnectPins(BP, Branch, TEXT("then"), Set, TEXT("execute"), Error), TEXT("branch -> set") },
		{ Lib::ConnectPins(BP, Make, NAME_None, Set, TEXT("State"), Error), TEXT("make -> set value") },
		{ Lib::ConnectPins(BP, Set, TEXT("then"), Open, TEXT("execute"), Error), TEXT("set -> open window") },
		{ Lib::ConnectPins(BP, Tag, NAME_None, Open, TEXT("Tag"), Error), TEXT("setting -> window tag") },
	};
	for (const TPair<bool, const TCHAR*>& Link : Links)
	{
		TestTrue(Link.Value, Link.Key);
	}
	TestTrue(TEXT("pin defaults: ") + Error, Lib::SetPinDefault(BP, Make, TEXT("Value"), TEXT("1"), Error)
		&& Lib::SetPinDefault(BP, Open, TEXT("Frames"), TEXT("12"), Error) && Lib::SetPinDefault(BP, Matches, TEXT("bExactMatch"), TEXT("true"), Error));

	TestEqual(TEXT("the graph compiles: ") + FString::Join(Messages, TEXT(" | ")), Lib::CompileAndReport(BP, Messages), 0);
	TArray<FString> Findings;
	TestEqual(TEXT("no simulation rule error"), Lib::CheckSimulationRules(BP, Findings), 0);
	TestEqual(TEXT("no finding at all (a sync write, no latent node): ") + FString::Join(Findings, TEXT(" | ")), Findings.Num(), 0);
	const TArray<FString> Lines = Lib::DescribeEventGraph(BP);
	TestTrue(TEXT("the description lists the links"), Lines.ContainsByPredicate([](const FString& Line) { return Line.Contains(TEXT("-> ")); }));

	TestTrue(TEXT("a rerun can start from an empty graph"), Lib::ClearEventGraph(BP) >= 7);
	TestTrue(TEXT("and the variables stay"), Lib::SetMemberVariable(BP, TEXT("State"), TEXT("struct:/Script/AbilitySystemSimulation.SyncInteger"),
		FString(), FString(), FString(), Error));

	Package->ClearDirtyFlag();
	BP->ClearFlags(RF_Standalone | RF_Public);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
