#include "Tests/GSTN_CombatTestUtils.h"
#include "Input/GSTN_CombatLockTags.h"
#include "Input/GSTN_CommandLocks.h"
#include "Input/GSTN_InputTags.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CombatLockTagsTest, "GSTNCombat.Core.Locks.NativeRoots", GSTN_CORE_TEST_FLAGS)
bool FGSTN_CombatLockTagsTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("lock tag name"), GSTNCombatLockTags::Attack.GetTag().GetTagName(), FName(TEXT("Input.Lock.Command.Attack")));
	TestTrue(TEXT("lock tags are under the input plugin's lock root"), GSTNCombatLockTags::Super.GetTag().MatchesTag(GSTNInputTags::Lock_Command.GetTag()));

	// Each root's lock tag is the one the input plugin derives from the root's command tag
	TestTrue(TEXT("attack"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNCommandTags::Attack) == GSTNCombatLockTags::Attack.GetTag());
	TestTrue(TEXT("strong"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNCommandTags::Strong) == GSTNCombatLockTags::Strong.GetTag());
	TestTrue(TEXT("special"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNCommandTags::Special) == GSTNCombatLockTags::Special.GetTag());
	TestTrue(TEXT("combo"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNCommandTags::Combo) == GSTNCombatLockTags::Combo.GetTag());
	TestTrue(TEXT("super"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNCommandTags::Super) == GSTNCombatLockTags::Super.GetTag());

	// A stance that owns the attack lock stops the attack family only; the combo chord (a different root) still comes out
	FGameplayTagContainer Stance;
	Stance.AddTag(GSTNCombatLockTags::Attack);
	TestTrue(TEXT("attack leaf locked"), UGSTN_CommandLocks::IsCommandLocked(Stance, GSTNCommandTags::Attack_CrouchDirectional));
	TestFalse(TEXT("strong free"), UGSTN_CommandLocks::IsCommandLocked(Stance, GSTNCommandTags::Strong_Neutral));
	TestFalse(TEXT("special free"), UGSTN_CommandLocks::IsCommandLocked(Stance, GSTNCommandTags::Special_Neutral));
	TestFalse(TEXT("combo free (it is a chord of attack and special, but its own root)"), UGSTN_CommandLocks::IsCommandLocked(Stance, GSTNCommandTags::Combo_AttackSpecial_Neutral));
	TestFalse(TEXT("not a global lock"), UGSTN_CommandLocks::HasGlobalLock(Stance));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
