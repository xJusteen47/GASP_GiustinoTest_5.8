// RemoveAttackHitboxTracks (dropkick hitboxes, 2026-11-23): a setup script rebuilds a move's hitboxes from its clips and runs again, so the
// helper must drop every attack hitbox track and keep the Hurtbox tracks (hitbox tracks by inheritance) and every other track.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_TimelineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RemoveAttackHitboxTracksTest, "GSTNCombat.Core.Timeline.RemoveAttackHitboxTracks", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RemoveAttackHitboxTracksTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 60;
	const TArray<FGSTN_HitboxSpec> Boxes = { FGSTN_HitboxSpec() };
	Data = UGSTN_TimelineUtils::AddHitboxTrack(Data, 15, 19, FGSTN_PartSelector(), Boxes);
	Data = UGSTN_TimelineUtils::AddHitboxTrack(Data, 19, 40, FGSTN_PartSelector(), Boxes);
	Data = UGSTN_TimelineUtils::AddBodyCapsuleTrack(Data, 10, 27, 35.f, 0.f, FVector(45.0, 0.0, 100.0));
	FGSTN_TimelineTrack_Hurtbox Hurtbox;
	Hurtbox.StartFrame = 0;
	Hurtbox.EndFrame = 60;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hurtbox>(Hurtbox));
	TestEqual(TEXT("four tracks before"), Data.Tracks.Num(), 4);

	const FGSTN_CombatTimelineData Clean = UGSTN_TimelineUtils::RemoveAttackHitboxTracks(Data);
	TestEqual(TEXT("both attack hitbox tracks removed, body capsule and hurtbox kept"), Clean.Tracks.Num(), 2);
	int32 Attack = 0;
	for (const TInstancedStruct<FGSTN_TimelineTrack>& Track : Clean.Tracks)
	{
		Attack += GSTNTimelineTracks::AsAttackHitbox(Track) ? 1 : 0;
	}
	TestEqual(TEXT("no attack hitbox left"), Attack, 0);
	TestEqual(TEXT("running it twice changes nothing"), UGSTN_TimelineUtils::RemoveAttackHitboxTracks(Clean).Tracks.Num(), 2);
	return true;
}

#endif
