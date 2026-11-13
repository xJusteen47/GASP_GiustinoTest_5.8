#include "Tests/GSTN_TestUtils.h"
#include "Input/GSTN_CommandLocks.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameplayTagContainer MakeOwner(std::initializer_list<FGameplayTag> Tags)
	{
		FGameplayTagContainer Owner;
		for (const FGameplayTag& Tag : Tags)
		{
			Owner.AddTag(Tag);
		}
		return Owner;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LockMappingTest, "GSTNInput.Core.Input.Locks.Mapping", GSTN_CORE_TEST_FLAGS)
bool FGSTN_LockMappingTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("a leaf maps to its own lock tag"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNTestTags::Attack_Neutral) == GSTNTestTags::Lock_AttackNeutral.GetTag());
	TestTrue(TEXT("a root maps to its lock tag"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNTestTags::Attack) == GSTNTestTags::Lock_Attack.GetTag());
	TestTrue(TEXT("the Command root maps to the global lock"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNInputTags::Command) == GSTNInputTags::Lock_Command.GetTag());
	TestFalse(TEXT("a command whose lock tag is not declared maps to nothing"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNTestTags::Special_Neutral).IsValid());
	TestFalse(TEXT("a tag outside the Command root maps to nothing"), UGSTN_CommandLocks::GetLockTagForCommand(GSTNInputTags::Context_Airborne).IsValid());
	TestFalse(TEXT("an invalid tag maps to nothing"), UGSTN_CommandLocks::GetLockTagForCommand(FGameplayTag()).IsValid());

	const FGameplayTagContainer Leaf = UGSTN_CommandLocks::GetLockTagsForCommand(GSTNTestTags::Attack_Neutral);
	TestTrue(TEXT("a leaf is locked by its own tag"), Leaf.HasTagExact(GSTNTestTags::Lock_AttackNeutral));
	TestTrue(TEXT("and by its root's"), Leaf.HasTagExact(GSTNTestTags::Lock_Attack));
	TestFalse(TEXT("the global lock is not part of the per-command list"), Leaf.HasTagExact(GSTNInputTags::Lock_Command));

	const FGameplayTagContainer Sibling = UGSTN_CommandLocks::GetLockTagsForCommand(GSTNTestTags::Attack_Directional);
	TestTrue(TEXT("a sibling is locked by the root's tag"), Sibling.HasTagExact(GSTNTestTags::Lock_Attack));
	TestFalse(TEXT("but not by the other leaf's"), Sibling.HasTagExact(GSTNTestTags::Lock_AttackNeutral));

	TestFalse(TEXT("another root's lock tags do not include the attack's"), UGSTN_CommandLocks::GetLockTagsForCommand(GSTNTestTags::Strong_Neutral).HasTagExact(GSTNTestTags::Lock_Attack));
	TestTrue(TEXT("an invalid command has no lock tags"), UGSTN_CommandLocks::GetLockTagsForCommand(FGameplayTag()).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LockRootsTest, "GSTNInput.Core.Input.Locks.Roots", GSTN_CORE_TEST_FLAGS)
bool FGSTN_LockRootsTest::RunTest(const FString& Parameters)
{
	const FGameplayTagContainer AttackLocked = MakeOwner({ GSTNTestTags::Lock_Attack.GetTag() });
	TestTrue(TEXT("a root lock locks a leaf"), UGSTN_CommandLocks::IsCommandLocked(AttackLocked, GSTNTestTags::Attack_Neutral));
	TestTrue(TEXT("a root lock locks every leaf"), UGSTN_CommandLocks::IsCommandLocked(AttackLocked, GSTNTestTags::Attack_CrouchDirectional));
	TestTrue(TEXT("a root lock locks the root command"), UGSTN_CommandLocks::IsCommandLocked(AttackLocked, GSTNTestTags::Attack));
	TestFalse(TEXT("another root is free"), UGSTN_CommandLocks::IsCommandLocked(AttackLocked, GSTNTestTags::Strong_Neutral));
	TestFalse(TEXT("a root lock is not the global lock, although the container also answers to its parent tags"),
		UGSTN_CommandLocks::HasGlobalLock(AttackLocked));
	TestTrue(TEXT("(the parent trap) the container does contain Input.Lock.Command through its parents"), AttackLocked.HasTag(GSTNInputTags::Lock_Command));

	const FGameplayTagContainer LeafLocked = MakeOwner({ GSTNTestTags::Lock_AttackNeutral.GetTag() });
	TestTrue(TEXT("a leaf lock locks the leaf"), UGSTN_CommandLocks::IsCommandLocked(LeafLocked, GSTNTestTags::Attack_Neutral));
	TestFalse(TEXT("and not its sibling"), UGSTN_CommandLocks::IsCommandLocked(LeafLocked, GSTNTestTags::Attack_Directional));
	TestFalse(TEXT("nor the root command"), UGSTN_CommandLocks::IsCommandLocked(LeafLocked, GSTNTestTags::Attack));

	TestFalse(TEXT("a root without a lock tag is free under another root's lock"), UGSTN_CommandLocks::IsCommandLocked(AttackLocked, GSTNTestTags::Special_Neutral));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LockGlobalTest, "GSTNInput.Core.Input.Locks.Global", GSTN_CORE_TEST_FLAGS)
bool FGSTN_LockGlobalTest::RunTest(const FString& Parameters)
{
	const FGameplayTagContainer Global = MakeOwner({ GSTNInputTags::Lock_Command.GetTag() });
	TestTrue(TEXT("the global lock is detected"), UGSTN_CommandLocks::HasGlobalLock(Global));
	TestTrue(TEXT("it locks a leaf"), UGSTN_CommandLocks::IsCommandLocked(Global, GSTNTestTags::Attack_Neutral));
	TestTrue(TEXT("it locks a root without lock tag"), UGSTN_CommandLocks::IsCommandLocked(Global, GSTNTestTags::Special_Neutral));
	TestTrue(TEXT("it locks any command"), UGSTN_CommandLocks::IsCommandLocked(Global, GSTNTestTags::Combo_AttackSpecial_Neutral));

	const FGameplayTagContainer Unrelated = MakeOwner({ GSTNInputTags::Context_Airborne.GetTag(), GSTNTestTags::Button_Attack.GetTag() });
	TestFalse(TEXT("unrelated tags lock nothing"), UGSTN_CommandLocks::IsCommandLocked(Unrelated, GSTNTestTags::Attack_Neutral));
	TestFalse(TEXT("an empty owner locks nothing"), UGSTN_CommandLocks::IsCommandLocked(FGameplayTagContainer(), GSTNTestTags::Attack_Neutral));
	TestFalse(TEXT("an empty lock list is never locked"), UGSTN_CommandLocks::IsLockedBy(Global, FGameplayTagContainer()));

	// Both: still locked, and the check does not depend on the order the tags were added in
	const FGameplayTagContainer Both = MakeOwner({ GSTNTestTags::Lock_Attack.GetTag(), GSTNInputTags::Lock_Command.GetTag() });
	TestTrue(TEXT("global plus root"), UGSTN_CommandLocks::IsCommandLocked(Both, GSTNTestTags::Strong_Neutral));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
