// Tests of the anim graph authoring library (editor): GSTNAnimation.Editor.*
//
// A transient anim Blueprint (never saved): the Blend Poses by int of the override slot seam must come out with every pose pin, its blend
// time pins and the index pin, because the desktop run of setup_anim_layers.py (polish M0) failed when only poses 0 and 1 existed.

#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "GSTN_AnimGraphAuthoringLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AnimGraphBlendIntPinsTest, "GSTNAnimation.Editor.Authoring.BlendPosesByIntPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_AnimGraphBlendIntPinsTest::RunTest(const FString& Parameters)
{
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FKismetEditorUtilities::CreateBlueprint(UAnimInstance::StaticClass(),
		GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UAnimBlueprint::StaticClass(), TEXT("ABP_GSTN_AuthoringTest")),
		BPTYPE_Normal, UAnimBlueprint::StaticClass(), UAnimBlueprintGeneratedClass::StaticClass()));
	UEdGraph* AnimGraph = AnimBlueprint ? UGSTN_AnimGraphAuthoringLibrary::FindGraph(AnimBlueprint, UEdGraphSchema_K2::GN_AnimGraph) : nullptr;
	if (!TestNotNull(TEXT("transient anim Blueprint with an AnimGraph"), AnimGraph))
	{
		return false;
	}

	// The seam of polish M0: base + wall + skid + run start
	UEdGraphNode* Blend = UGSTN_AnimGraphAuthoringLibrary::SpawnBlendPosesByInt(AnimGraph, 4, FVector2D::ZeroVector);
	if (!TestNotNull(TEXT("blend spawned"), Blend))
	{
		return false;
	}
	TestTrue(TEXT("pose pins >= 4"), UGSTN_AnimGraphAuthoringLibrary::CountBlendPosePins(Blend) >= 4);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		TestNotNull(*FString::Printf(TEXT("BlendPose_%d"), Index), Blend->FindPin(*FString::Printf(TEXT("BlendPose_%d"), Index), EGPD_Input));
		TestNotNull(*FString::Printf(TEXT("BlendTime_%d"), Index), Blend->FindPin(*FString::Printf(TEXT("BlendTime_%d"), Index), EGPD_Input));
	}
	TestNotNull(TEXT("ActiveChildIndex"), Blend->FindPin(TEXT("ActiveChildIndex"), EGPD_Input));

	// A bigger seam later (more override kinds) works the same way
	UEdGraphNode* Six = UGSTN_AnimGraphAuthoringLibrary::SpawnBlendPosesByInt(AnimGraph, 6, FVector2D(0.0, 400.0));
	TestTrue(TEXT("six poses"), Six && UGSTN_AnimGraphAuthoringLibrary::CountBlendPosePins(Six) >= 6);

	AddExpectedError(TEXT("needs a graph and at least 2 poses"), EAutomationExpectedErrorFlags::Contains, 1);
	TestNull(TEXT("fewer than 2 poses refused"), UGSTN_AnimGraphAuthoringLibrary::SpawnBlendPosesByInt(AnimGraph, 1, FVector2D::ZeroVector));

	AnimBlueprint->MarkAsGarbage();
	return true;
}

#endif
