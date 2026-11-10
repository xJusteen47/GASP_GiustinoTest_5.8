// Held objects, pure core tests of slice 3 (GSTNHeld.Core.*): the kit bindings the loadout extension adds, the slot priorities, the tier.

#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSlots.h"
#include "GSTN_HeldTags.h"
#include "GSTN_HeldTypes.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldKitBindingsTest, "GSTNHeld.Core.KitBindings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldKitBindingsTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHeldTags;
	const FSoftObjectPath Pickup(TEXT("/Script/Test.Pickup"));
	const FSoftObjectPath Drop(TEXT("/Script/Test.Drop"));
	const FSoftObjectPath Throw(TEXT("/Script/Test.Throw"));

	TArray<FGSTN_BindingInput> Bindings;
	TestEqual(TEXT("an empty kit gets the three"), UGSTN_HeldSlots::AppendKitBindings(Pickup, Drop, Throw, Bindings), 3);
	if (!TestEqual(TEXT("three bindings"), Bindings.Num(), 3))
	{
		return false;
	}
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_HeldSlots::Build();
	const FSoftObjectPath* Expected[] = { &Pickup, &Drop, &Throw };
	for (int32 Row = 0; Row < 3; ++Row)
	{
		TestEqual(TEXT("command in row order"), Bindings[Row].Command, Slots[Row].Command);
		TestEqual(TEXT("ability in row order"), Bindings[Row].Ability, *Expected[Row]);
		TestTrue(TEXT("carries its slot"), Bindings[Row].bHasSlotOverride && Bindings[Row].SlotOverride.Command == Slots[Row].Command
			&& Bindings[Row].SlotOverride.PriorityOffset == Slots[Row].PriorityOffset);
	}
	TestTrue(TEXT("drop and throw require carrying"), Bindings[1].Requirements.RequireTags.HasTagExact(Held_Carrying)
		&& Bindings[2].Requirements.RequireTags.HasTagExact(Held_Carrying));
	TestEqual(TEXT("compiling again adds nothing"), UGSTN_HeldSlots::AppendKitBindings(Pickup, Drop, Throw, Bindings), 0);

	// A kit that binds the throw itself keeps its own; the other two are added
	TArray<FGSTN_BindingInput> Own;
	FGSTN_BindingInput& KitThrow = Own.AddDefaulted_GetRef();
	KitThrow.Command = Command_Held_Throw;
	KitThrow.Ability = FSoftObjectPath(TEXT("/Script/Test.KitThrow"));
	TestEqual(TEXT("the kit's throw wins"), UGSTN_HeldSlots::AppendKitBindings(Pickup, Drop, Throw, Own), 2);
	TestEqual(TEXT("one throw binding, the kit's"), Own.FilterByPredicate([](const FGSTN_BindingInput& B) { return B.Command == Command_Held_Throw; }).Num(), 1);
	TestEqual(TEXT("and it is untouched"), Own[0].Ability, FSoftObjectPath(TEXT("/Script/Test.KitThrow")));

	TArray<FGSTN_BindingInput> NoAbility;
	TestEqual(TEXT("an empty ability path binds nothing"), UGSTN_HeldSlots::AppendKitBindings(FSoftObjectPath(), Drop, Throw, NoAbility), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldPickupTargetTest, "GSTNHeld.Core.PickupTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldPickupTargetTest::RunTest(const FString& Parameters)
{
	const FVector From = FVector::ZeroVector;
	const TArray<FGSTN_PickupCandidate> Candidates = {
		{ 4, FVector(150.f, 0.f, 0.f), 200.f, true },
		{ 2, FVector(0.f, 150.f, 0.f), 200.f, true },   // same distance, lower id: the target
		{ 1, FVector(50.f, 0.f, 0.f), 200.f, false },   // nearer but held or flying
		{ 3, FVector(500.f, 0.f, 0.f), 200.f, true } }; // out of reach
	TestEqual(TEXT("nearest available in reach, lower id on a tie"), FGSTN_HeldRules::FindPickupTarget(From, Candidates), 2);
	TestEqual(TEXT("nothing in reach"), FGSTN_HeldRules::FindPickupTarget(FVector(0.f, 0.f, 5000.f), Candidates), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("no candidates"), FGSTN_HeldRules::FindPickupTarget(From, TArray<FGSTN_PickupCandidate>()), static_cast<int32>(INDEX_NONE));
	// The same as the host's race with that one request
	TArray<FGSTN_PickupAssignment> Assignments;
	const FGSTN_PickupRequest Request{ 7, 10, From };
	FGSTN_HeldRules::ResolvePickups(MakeArrayView(&Request, 1), Candidates, Assignments);
	TestTrue(TEXT("the host would give the same object"), Assignments.Num() == 1 && Assignments[0].ObjectId == 2);
	return true;
}

namespace
{
	FGSTN_InputSlotSpec MakeStandIn(const FGameplayTag& Command, int32 Offset, bool bWithCrouchHold)
	{
		FGSTN_InputSlotSpec Spec;
		Spec.Command = Command;
		Spec.PriorityOffset = Offset;
		Spec.bFallThroughWhenUnavailable = true;
		FGSTN_InputStep Step;
		FGSTN_InputCondition_Press Press;
		Press.Buttons.AddTag(GSTNCombatButtonTags::Button_Interact.GetTag());
		Step.AllOf.Add(TInstancedStruct<FGSTN_InputCondition>::Make<FGSTN_InputCondition_Press>(Press));
		if (bWithCrouchHold)
		{
			FGSTN_InputCondition_Hold Hold;
			Hold.Button = GSTNCombatButtonTags::Button_Crouch.GetTag();
			Hold.MinFrames = 0;
			Step.AllOf.Add(TInstancedStruct<FGSTN_InputCondition>::Make<FGSTN_InputCondition_Hold>(Hold));
		}
		Spec.Pattern.Steps.Add(MoveTemp(Step));
		return Spec;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HeldSlotPrioritiesTest, "GSTNHeld.Core.SlotPriorities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_HeldSlotPrioritiesTest::RunTest(const FString& Parameters)
{
	// The tier the plugin contributes puts the held commands above the strong attacks (RT with an object in hand throws)
	const UGSTN_InputSettings& Input = *GetDefault<UGSTN_InputSettings>();
	const FGameplayTag Strong = FGameplayTag::RequestGameplayTag(TEXT("Command.Strong"), false);
	const int32 HeldTier = Input.GetCommandTier(GSTNHeldTags::Command_Held_Throw);
	TestTrue(TEXT("held commands have a tier"), HeldTier > 0);
	if (Strong.IsValid())
	{
		TestTrue(TEXT("above the strong attacks"), HeldTier > Input.GetCommandTier(Strong));
	}

	// Stand-ins for the link and mount slots of the interact press (offsets from UGSTN_LinkSlots and FGSTN_MountLoadout; tags by name, no
	// dependency): none may be ambiguous with a held slot, whatever plugins a kit combines
	struct FStandIn { const TCHAR* Tag; int32 Offset; bool bCrouch; };
	const FStandIn StandIns[] = {
		{ TEXT("Command.Link.Grab"), 0, false }, { TEXT("Command.Link.Mount"), -1, false }, { TEXT("Command.Link.Release"), 100, false },
		{ TEXT("Command.Link.Dismount"), 101, false }, { TEXT("Command.Link.Drop"), 102, true } };
	TArray<FGSTN_InputSlotSpec> Specs = UGSTN_HeldSlots::Build();
	const int32 HeldCount = Specs.Num();
	for (const FStandIn& StandIn : StandIns)
	{
		const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(StandIn.Tag, false);
		if (Tag.IsValid())
		{
			Specs.Add(MakeStandIn(Tag, StandIn.Offset, StandIn.bCrouch));
		}
	}
	TArray<const FGSTN_InputSlotSpec*> Pointers;
	for (const FGSTN_InputSlotSpec& Spec : Specs)
	{
		Pointers.Add(&Spec);
	}
	for (const FGSTN_SlotConflict& Conflict : UGSTN_InputSlotValidation::FindConflicts(Pointers, Input))
	{
		const bool bInvolvesHeld = (Conflict.SlotA >= 0 && Conflict.SlotA < HeldCount) || (Conflict.SlotB >= 0 && Conflict.SlotB < HeldCount);
		if (bInvolvesHeld && Conflict.IsError())
		{
			AddError(Conflict.Message);
		}
	}
	return true;
}

#endif
