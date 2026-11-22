// Automation tests of the combat tick extension registry. No world: the registry and the hook context are plain data.

#include "GSTN_CombatNPTestTypes.h"
#include "Misc/AutomationTest.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTickExtension.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** Records (type id, hook point) of each call; changes nothing */
	class FRecordingExtension : public IGSTN_CombatTickExtension
	{
	public:
		FRecordingExtension(uint8 InTypeId, int32 InPriority, TArray<TPair<uint8, EGSTN_CombatHookPoint>>& InLog)
			: TypeId(InTypeId), Priority(InPriority), Log(InLog)
		{
		}

		virtual uint8 GetTypeId() const override { return TypeId; }
		virtual int32 GetPriority() const override { return Priority; }
		virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override { Log.Emplace(TypeId, Point); }

	private:
		uint8 TypeId;
		int32 Priority;
		TArray<TPair<uint8, EGSTN_CombatHookPoint>>& Log;
	};

	using FHookLog = TArray<TPair<uint8, EGSTN_CombatHookPoint>>;

	/** The type ids that ran at Point, in the order they ran */
	TArray<uint8> IdsAt(const FHookLog& Log, EGSTN_CombatHookPoint Point)
	{
		TArray<uint8> Ids;
		for (const TPair<uint8, EGSTN_CombatHookPoint>& Entry : Log)
		{
			if (Entry.Value == Point)
			{
				Ids.Add(Entry.Key);
			}
		}
		return Ids;
	}

	void RunAllPoints(const FGSTN_CombatTickExtensionRegistry& Registry, FGSTN_CombatTickContext& Context)
	{
		for (uint8 Point = 0; Point < static_cast<uint8>(EGSTN_CombatHookPoint::Count); ++Point)
		{
			Registry.Run(static_cast<EGSTN_CombatHookPoint>(Point), Context);
		}
	}

	TArray<uint8> Bytes(FGSTN_CombatSyncState& State)
	{
		FBitWriter Writer(256, true);
		State.NetSerialize(FNetSerializeParams(Writer));
		TArray<uint8> Result;
		Result.Append(Writer.GetData(), Writer.GetNumBytes());
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TickExtensionOrderByPriorityTest, "GSTNCombat.NP.TickExtension.OrderByPriority", GSTN_NP_TEST_FLAGS)

bool FGSTN_TickExtensionOrderByPriorityTest::RunTest(const FString& Parameters)
{
	FHookLog Log;
	// (type id, priority); equal priorities run by type id
	FRecordingExtension Late(5, 10, Log);
	FRecordingExtension First(7, -5, Log);
	FRecordingExtension Middle(3, 0, Log);
	FRecordingExtension LateLowId(2, 10, Log);
	FRecordingExtension LateHighId(9, 10, Log);

	FGSTN_CombatSyncState In;
	FGSTN_CombatSyncState Out;
	FGSTN_CombatTickContext Context(0, In, Out);

	{
		FGSTN_CombatTickExtensionRegistry Registry;
		TestTrue(TEXT("register"), Registry.Register(&Late));
		TestTrue(TEXT("register"), Registry.Register(&First));
		TestTrue(TEXT("register"), Registry.Register(&LateHighId));
		TestTrue(TEXT("register"), Registry.Register(&Middle));
		TestTrue(TEXT("register"), Registry.Register(&LateLowId));

		Registry.Run(EGSTN_CombatHookPoint::AfterExecute, Context);
		TestTrue(TEXT("priority first, then type id"), IdsAt(Log, EGSTN_CombatHookPoint::AfterExecute) == TArray<uint8>({ 7, 3, 2, 5, 9 }));
	}

	// The same set registered in the opposite order runs in the same order: it does not depend on plugin load order
	{
		Log.Reset();
		FGSTN_CombatTickExtensionRegistry Registry;
		Registry.Register(&LateLowId);
		Registry.Register(&Middle);
		Registry.Register(&LateHighId);
		Registry.Register(&First);
		Registry.Register(&Late);

		Registry.Run(EGSTN_CombatHookPoint::BeforeCandidates, Context);
		TestTrue(TEXT("same order"), IdsAt(Log, EGSTN_CombatHookPoint::BeforeCandidates) == TArray<uint8>({ 7, 3, 2, 5, 9 }));

		// A second extension with a type id already taken has no defined place: refused
		FRecordingExtension Duplicate(3, -100, Log);
		AddExpectedError(TEXT("type id 3 is already registered"), EAutomationExpectedErrorFlags::Contains, -1);
		TestFalse(TEXT("duplicate type id"), Registry.Register(&Duplicate));

		Log.Reset();
		Registry.Unregister(&Middle);
		Registry.Run(EGSTN_CombatHookPoint::AfterClocks, Context);
		TestTrue(TEXT("unregistered extensions stop running"), IdsAt(Log, EGSTN_CombatHookPoint::AfterClocks) == TArray<uint8>({ 7, 2, 5, 9 }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TickExtensionEmptyHooksNoEffectTest, "GSTNCombat.NP.TickExtension.EmptyHooksNoEffect", GSTN_NP_TEST_FLAGS)

bool FGSTN_TickExtensionEmptyHooksNoEffectTest::RunTest(const FString& Parameters)
{
	FGSTN_SyncFragmentRegistry::Register<FGSTN_TestFragmentA>(GSTNFeatureIds::TestA);

	FGSTN_CombatSyncState In;
	In.Core.Input.RequestFrame = 9;
	In.Core.HitstopStartFrame = 4;
	In.Core.HitstopEndFrame = 8;
	FGSTN_SyncFragmentList::FindOrAdd<FGSTN_TestFragmentA>(In.Fragments).Frame = 55;
	FGSTN_CombatSyncState Out = In;

	const TArray<uint8> InBefore = Bytes(In);
	const TArray<uint8> OutBefore = Bytes(Out);
	FGSTN_CombatTickContext Context(12, In, Out);

	// Nothing registered: the empty loops
	{
		const FGSTN_CombatTickExtensionRegistry Registry;
		RunAllPoints(Registry, Context);
		TestTrue(TEXT("the input state is unchanged"), Bytes(In) == InBefore);
		TestTrue(TEXT("the output state is unchanged"), Bytes(Out) == OutBefore);
		TestFalse(TEXT("and still equal"), Out.ShouldReconcile(In));
	}

	// An extension that only observes: it is called at each of the four points once, in order, and nothing changes
	{
		FHookLog Log;
		FRecordingExtension Observer(4, 0, Log);
		FGSTN_CombatTickExtensionRegistry Registry;
		Registry.Register(&Observer);
		RunAllPoints(Registry, Context);

		TestEqual(TEXT("one call per hook point"), Log.Num(), static_cast<int32>(EGSTN_CombatHookPoint::Count));
		for (int32 Index = 0; Index < Log.Num(); ++Index)
		{
			TestTrue(*FString::Printf(TEXT("hook point %d is called in declaration order"), Index), Log[Index].Value == static_cast<EGSTN_CombatHookPoint>(Index));
		}
		TestTrue(TEXT("the input state is unchanged"), Bytes(In) == InBefore);
		TestTrue(TEXT("the output state is unchanged"), Bytes(Out) == OutBefore);
	}

	FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::TestA);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
