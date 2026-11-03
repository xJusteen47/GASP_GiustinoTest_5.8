// Automation tests of the data of the default movement abilities (combat-m3 C14): the blocked tags, the timelines and the move effects are
// properties of the ability classes, so a kit changes them by data. The movement itself is checked by the PIE scenarios (dash, ground_dodge,
// air_dodge, dash_tilted_gravity), which compare the host with a remote client.

#include "Abilities/GSTN_GA_Dash.h"
#include "GSTN_CombatCoreTags.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPDodgeBlockedTagsTest, "GSTNCombat.NP.Dodge.BlockedTagsAreData", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPDodgeBlockedTagsTest::RunTest(const FString& Parameters)
{
	const UGSTN_GA_MoveTimeline* Dodges[] = { GetDefault<UGSTN_GA_GroundDodge>(), GetDefault<UGSTN_GA_AirDodge>() };
	for (const UGSTN_GA_MoveTimeline* Dodge : Dodges)
	{
		const FGameplayTagContainer& Blocked = Dodge->GetBlockedTags();
		TestTrue(*FString::Printf(TEXT("%s is blocked in hitstun"), *Dodge->GetName()), Blocked.HasTagExact(GSTNCombatTags::State_Hitstun));
		TestTrue(*FString::Printf(TEXT("%s is blocked in hitstop"), *Dodge->GetName()), Blocked.HasTagExact(GSTNCombatTags::State_Hitstop));
	}
	TestFalse(TEXT("a dash is not a dodge: no blocked tags by default"), GetDefault<UGSTN_GA_Dash>()->GetBlockedTags().HasTagExact(GSTNCombatTags::State_Hitstun));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPDodgeDataTest, "GSTNCombat.NP.Dodge.DefaultAbilityData", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPDodgeDataTest::RunTest(const FString& Parameters)
{
	const UGSTN_GA_MoveTimeline* Abilities[] = { GetDefault<UGSTN_GA_Dash>(), GetDefault<UGSTN_GA_GroundDodge>(), GetDefault<UGSTN_GA_AirDodge>() };
	for (const UGSTN_GA_MoveTimeline* Ability : Abilities)
	{
		const FString Name = Ability->GetName();
		TestTrue(*(Name + TEXT(" has a move effect")), Ability->MoveEffect != nullptr);
		TestTrue(*(Name + TEXT(" has a timeline")), Ability->GetTimelineData().TotalFrames > 0);
		bool bMovement = false;
		for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Ability->GetTimelineData().Tracks)
		{
			const FGSTN_TimelineTrack* Base = Track.GetPtr<FGSTN_TimelineTrack>();
			bMovement |= Base && Base->GetKind() == EGSTN_TimelineTrackKind::Movement;
		}
		TestTrue(*(Name + TEXT(" has a movement track")), bMovement);
		if (Ability->MoveEffect)
		{
			const UGameplayEffect* Effect = Ability->MoveEffect.GetDefaultObject();
			TestEqual(*(Name + TEXT(" effect is instant")), Effect->DurationPolicy, EGameplayEffectDurationType::Instant);
			TestNotNull(*(Name + TEXT(" effect has the apply-move component")), Effect->FindComponent<UApplyMoveGameplayEffectComponent>());
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
