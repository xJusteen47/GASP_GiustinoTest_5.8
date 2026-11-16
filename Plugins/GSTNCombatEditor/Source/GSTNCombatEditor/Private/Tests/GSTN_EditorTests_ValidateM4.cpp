#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "IMessageLogListing.h"
#include "Logging/TokenizedMessage.h"
#include "MessageLogModule.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "Tests/GSTN_EditorTestTypes.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_CombatValidators.h"
#include "Validation/GSTN_EditorValidators.h"
#include "Validation/GSTN_ValidateAll.h"
#include "Validation/GSTN_ValidationMessageLog.h"
#include "Validation/GSTN_ValidationPanelModel.h"

#if WITH_DEV_AUTOMATION_TESTS

// The tests of the M4 validation rules (plan m5 E11): each code of combat-m5-editor 9.2 that needs hit data has a failing and a passing case
// built in code, then the hurtbox set validator, Validate all over the registry, the panel's model and the Message Log. Transient objects only.

#define GSTN_VALIDATE_M4_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNValidateM4Test
{
	using namespace GSTNValidationCodes;

	FGSTN_CombatTimelineData MakeData()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 20;
		Data.OverrideFrames = 0;
		return Data;
	}

	FGSTN_TimelineRuleContext MakeContext()
	{
		FGSTN_TimelineRuleContext Context;
		Context.Core.TickRate = 60.f;
		return Context;
	}

	FGSTN_ValidationReport Run(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineRuleContext& Context = MakeContext())
	{
		FGSTN_ValidationReport Report;
		FGSTN_CombatValidators::ValidateTimeline(Data, Context, Report);
		return Report;
	}

	FGSTN_HitboxKey KeyAt(int32 Frame)
	{
		FGSTN_HitboxKey Key;
		Key.Frame = Frame;
		return Key;
	}

	/** A valid hitbox: a sphere with one key at the first frame, a real hit */
	FGSTN_HitboxSpec MakeSpec(uint8 Group = 0, int32 Priority = 0, float Radius = 40.f)
	{
		FGSTN_HitboxSpec Spec;
		Spec.Shape.Type = EGSTN_ShapeType::Sphere;
		Spec.Shape.Extent = FVector(Radius, Radius, Radius);
		Spec.Keys.Add(KeyAt(0));
		Spec.HitGroup = Group;
		Spec.Data.Damage = 5.f;
		Spec.Data.Priority = Priority;
		return Spec;
	}

	/** Adds a hitbox track over [Start, End) with the specs and returns its index */
	int32 AddHitbox(FGSTN_CombatTimelineData& Data, int32 Start, int32 End, std::initializer_list<FGSTN_HitboxSpec> Specs)
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		Track.Part.GroupName = TEXT("Hands");
		for (const FGSTN_HitboxSpec& Spec : Specs)
		{
			Track.Hitboxes.Add(Spec);
		}
		return Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
	}

	FGSTN_HitboxSpec& Spec(FGSTN_CombatTimelineData& Data, int32 Track, int32 Index = 0)
	{
		return Data.Tracks[Track].GetMutable<FGSTN_TimelineTrack_Hitbox>().Hitboxes[Index];
	}

	FGameplayTag MakeUnregisteredTag(FName Name)
	{
		FGameplayTag Tag;
		const FNameProperty* Property = CastField<FNameProperty>(FGameplayTag::StaticStruct()->FindPropertyByName(TEXT("TagName")));
		check(Property);
		Property->SetPropertyValue_InContainer(&Tag, Name);
		return Tag;
	}

	/**
	 * Findings other than UnknownTrack. The hitbox and hurtbox modifier rows arrive with E5b, until then the registry has no track editor for them
	 * and every such track gets the (correct) generic-row warning; the cases below are about the hit data, not about that.
	 */
	int32 CountRealFindings(const FGSTN_ValidationReport& Report)
	{
		return Report.Num() - Report.CountCode(UnknownTrack());
	}

	bool HasFindingAt(const FGSTN_ValidationReport& Report, FName Code, EGSTN_Severity Severity, int32 Track)
	{
		return Report.Findings.ContainsByPredicate([&](const FGSTN_ValidationFinding& Finding) { return Finding.Code == Code && Finding.Severity == Severity && Finding.Where.TrackIndex == Track; });
	}
}

using namespace GSTNValidateM4Test;

// A valid hitbox timeline has nothing to say: the baseline the failing cases add a defect to
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateM4CleanTest, "GSTNCombat.Editor.Validate.M4CleanTimelineIsQuiet", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateM4CleanTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeData();
	AddHitbox(Data, 6, 10, { MakeSpec() });
	const FGSTN_ValidationReport Report = Run(Data);
	TestEqual(TEXT("no finding at all"), CountRealFindings(Report), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateHitboxKeysTest, "GSTNCombat.Editor.Validate.HitboxKeys", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateHitboxKeysTest::RunTest(const FString& Parameters)
{
	// A key past the end of the window never plays: error, located on the key's frame inside the track
	FGSTN_CombatTimelineData Data = MakeData();
	AddHitbox(Data, 6, 10, { MakeSpec() });
	Spec(Data, 0).Keys.Add(KeyAt(4)); // the window has 4 frames: 0..3
	FGSTN_ValidationReport Report = Run(Data);
	TestTrue(TEXT("a key outside the window is an error"), HasFindingAt(Report, HitboxKeys(), EGSTN_Severity::Error, 0));
	const FGSTN_ValidationFinding* Finding = Report.FindCode(HitboxKeys());
	TestTrue(TEXT("pointing at the last frame of the track"), Finding != nullptr && Finding->Where.Frame == 9);

	Spec(Data, 0).Keys.Last().Frame = 3;
	TestFalse(TEXT("the last frame of the window is inside it"), Run(Data).HasCode(HitboxKeys()));

	// Out of order keys are Core's wording, classified into the same code
	Spec(Data, 0).Keys.Add(KeyAt(2));
	TestTrue(TEXT("keys out of order"), Run(Data).HasCode(HitboxKeys()));
	Spec(Data, 0).Keys.Pop();

	// A shape with no volume, also Core's
	Spec(Data, 0).Shape.Extent = FVector::ZeroVector;
	Report = Run(Data);
	TestTrue(TEXT("a shape with no volume is a HitboxKeys error"), HasFindingAt(Report, HitboxKeys(), EGSTN_Severity::Error, 0));
	TestFalse(TEXT("and does not fall through to the generic code"), Report.HasCode(CoreOther()));
	Spec(Data, 0).Shape.Extent = FVector(40.0);

	// Two hitboxes of one group open together with the same Priority and different hits: a tie nobody chose
	AddHitbox(Data, 7, 12, { MakeSpec(0, 0, 25.f) });
	Spec(Data, 1).Data.Damage = 9.f;
	TestTrue(TEXT("a priority tie between different hits is a warning"), HasFindingAt(Run(Data), HitboxKeys(), EGSTN_Severity::Warning, 1));

	// A sweetspot has the higher priority: meant, silent
	Spec(Data, 1).Data.Priority = 5;
	TestFalse(TEXT("different priorities are a sweetspot"), Run(Data).HasCode(HitboxKeys()));

	// Another group, or windows that do not meet: nothing to resolve
	Spec(Data, 1).Data.Priority = 0;
	Spec(Data, 1).HitGroup = 1;
	TestFalse(TEXT("another group"), Run(Data).HasCode(HitboxKeys()));
	Spec(Data, 1).HitGroup = 0;
	Data.Tracks[1].GetMutable<FGSTN_TimelineTrack_Hitbox>().StartFrame = 12;
	Data.Tracks[1].GetMutable<FGSTN_TimelineTrack_Hitbox>().EndFrame = 15;
	TestFalse(TEXT("windows that do not overlap"), Run(Data).HasCode(HitboxKeys()));

	// A hitbox with no key stays where its space puts it: M4 allows it, so it is not reported
	FGSTN_CombatTimelineData Static = MakeData();
	AddHitbox(Static, 6, 10, { MakeSpec() });
	Spec(Static, 0).Keys.Empty();
	TestFalse(TEXT("no key is allowed"), Run(Static).HasCode(HitboxKeys()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateHitboxOverlapTest, "GSTNCombat.Editor.Validate.OverlapSameKindHitbox", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateHitboxOverlapTest::RunTest(const FString& Parameters)
{
	// The same shape in the same place, same group, open together: the second adds nothing
	FGSTN_CombatTimelineData Data = MakeData();
	AddHitbox(Data, 6, 12, { MakeSpec() });
	AddHitbox(Data, 8, 14, { MakeSpec() });
	TestTrue(TEXT("same group, same shape, overlapping windows"), HasFindingAt(Run(Data), OverlapSameKind(), EGSTN_Severity::Warning, 1));

	// A different shape in the group is a second part of the attack
	Spec(Data, 1).Shape.Extent = FVector(10.0);
	TestFalse(TEXT("another shape"), Run(Data).HasCode(OverlapSameKind()));
	Spec(Data, 1).Shape.Extent = FVector(40.0);

	// Another group is another attack
	Spec(Data, 1).HitGroup = 3;
	TestFalse(TEXT("another group"), Run(Data).HasCode(OverlapSameKind()));
	Spec(Data, 1).HitGroup = 0;

	// Windows that follow each other (multi-hit) are fine
	Data.Tracks[1].GetMutable<FGSTN_TimelineTrack_Hitbox>().StartFrame = 12;
	Data.Tracks[1].GetMutable<FGSTN_TimelineTrack_Hitbox>().EndFrame = 16;
	TestFalse(TEXT("a multi-hit does not overlap"), Run(Data).HasCode(OverlapSameKind()));

	// Two identical specs in one track share its window
	FGSTN_CombatTimelineData Twin = MakeData();
	AddHitbox(Twin, 6, 12, { MakeSpec(), MakeSpec() });
	TestTrue(TEXT("two identical specs of one track"), Run(Twin).HasCode(OverlapSameKind()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateHitDataTest, "GSTNCombat.Editor.Validate.HitData", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateHitDataTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeData();
	AddHitbox(Data, 6, 10, { MakeSpec() });
	TestFalse(TEXT("valid hit data"), Run(Data).HasCode(HitData()));

	Spec(Data, 0).Data.Damage = -3.f;
	FGSTN_ValidationReport Report = Run(Data);
	TestTrue(TEXT("negative damage is an error"), HasFindingAt(Report, HitData(), EGSTN_Severity::Error, 0));
	Spec(Data, 0).Data.Damage = 5.f;

	Spec(Data, 0).Data.BaseKnockback = -1.f;
	TestTrue(TEXT("negative knockback"), Run(Data).HasCode(HitData()));
	Spec(Data, 0).Data.BaseKnockback = 100.f;

	Spec(Data, 0).Data.HitstopMode = EGSTN_HitstopMode::Override;
	Spec(Data, 0).Data.HitstopFramesOverride = -2;
	TestTrue(TEXT("a negative hitstop override"), Run(Data).HasCode(HitData()));
	Spec(Data, 0).Data.HitstopMode = EGSTN_HitstopMode::Auto;
	// The negative frames only count in Override mode: Auto ignores the field
	TestFalse(TEXT("the override is ignored in Auto mode"), Run(Data).HasCode(HitData()));

	// A fixed knockback with growth: the growth is dead data (warning, not error)
	Spec(Data, 0).Data.FixedKnockback = 300.f;
	Spec(Data, 0).Data.KnockbackGrowth = 2.f;
	Report = Run(Data);
	TestTrue(TEXT("fixed with growth is a warning"), HasFindingAt(Report, HitData(), EGSTN_Severity::Warning, 0));
	TestFalse(TEXT("and not an error"), Report.HasErrors());
	Spec(Data, 0).Data.KnockbackGrowth = 0.f;
	TestFalse(TEXT("fixed alone is fine"), Run(Data).HasCode(HitData()));
	Spec(Data, 0).Data.FixedKnockback = -1.f;

	// Tags the table does not know: a hit type and a cue
	Spec(Data, 0).Data.HitType = MakeUnregisteredTag(TEXT("Combat.HitType.GoneTag"));
	TestTrue(TEXT("an unregistered hit type"), HasFindingAt(Run(Data), HitData(), EGSTN_Severity::Error, 0));
	Spec(Data, 0).Data.HitType = GSTNCombatHitTags::HitType_Normal.GetTag();
	TestFalse(TEXT("a registered hit type"), Run(Data).HasCode(HitData()));
	Spec(Data, 0).Data.CueTag = MakeUnregisteredTag(TEXT("GameplayCue.GoneCue"));
	TestTrue(TEXT("an unregistered cue"), Run(Data).HasCode(HitData()));
	Spec(Data, 0).Data.CueTag = FGameplayTag();

	// A grab is data only in M4: Core's note, classified as HitData (a warning)
	Spec(Data, 0).Data.HitType = GSTNCombatHitTags::HitType_Grab.GetTag();
	TestTrue(TEXT("a data-only hit type is a HitData warning"), HasFindingAt(Run(Data), HitData(), EGSTN_Severity::Warning, 0));
	Spec(Data, 0).Data.HitType = FGameplayTag();

	// A variant that overrides fields is checked like the base hit
	FGSTN_HitVariant Variant;
	Variant.Override.Fields = static_cast<int32>(EGSTN_HitDataField::Damage);
	Variant.Override.Values.Damage = -8.f;
	Spec(Data, 0).RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Ally.GetTag(), Variant);
	TestTrue(TEXT("an overriding variant with negative damage"), Run(Data).HasCode(HitData()));
	Spec(Data, 0).RelationshipVariants[GSTNCombatHitTags::Relationship_Ally.GetTag()].Override.Fields = 0;
	TestFalse(TEXT("a variant that overrides nothing is not read"), Run(Data).HasCode(HitData()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateVariantsTest, "GSTNCombat.Editor.Validate.Variants", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateVariantsTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeData();
	AddHitbox(Data, 6, 10, { MakeSpec() });

	FGSTN_HitVariant Good;
	Good.ShapeScale = 1.5f;
	Spec(Data, 0).RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Ally.GetTag(), Good);
	TestFalse(TEXT("a variant under Combat.Relationship"), Run(Data).HasCode(Variants()));

	// The key is not a relationship: no victim ever has it
	Spec(Data, 0).RelationshipVariants.Add(GSTNCombatHitTags::HitType_Normal.GetTag(), Good);
	TestTrue(TEXT("a key outside Combat.Relationship"), HasFindingAt(Run(Data), Variants(), EGSTN_Severity::Error, 0));
	Spec(Data, 0).RelationshipVariants.Remove(GSTNCombatHitTags::HitType_Normal.GetTag());

	// A scale of 0 that does not ignore
	FGSTN_HitVariant Zero;
	Zero.ShapeScale = 0.f;
	Spec(Data, 0).RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Enemy.GetTag(), Zero);
	TestTrue(TEXT("scale 0 without ignore"), HasFindingAt(Run(Data), Variants(), EGSTN_Severity::Error, 0));
	Zero.bIgnore = true;
	Spec(Data, 0).RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Enemy.GetTag(), Zero);
	TestFalse(TEXT("scale 0 with ignore is how a hitbox is turned off"), Run(Data).HasCode(Variants()));

	// A negative multiplier
	FGSTN_HitVariant Negative;
	Negative.Override.DamageMultiplier = -1.f;
	Spec(Data, 0).RelationshipVariants.Add(GSTNCombatHitTags::Relationship_Neutral.GetTag(), Negative);
	TestTrue(TEXT("a negative multiplier"), Run(Data).HasCode(Variants()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateHurtStateTest, "GSTNCombat.Editor.Validate.HurtState", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateHurtStateTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeData();
	FGSTN_TimelineTrack_HurtboxModifier Modifier;
	Modifier.StartFrame = 4;
	Modifier.EndFrame = 12;
	Modifier.Kind = EGSTN_HurtboxModifierKind::Armor;
	Modifier.AffectedHurtboxes.Add(TEXT("Torso"));
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_HurtboxModifier>(Modifier));

	// Armor with a threshold that was never set: Core words it as a warning, the rule says it as an error, and only one of them is listed
	FGSTN_ValidationReport Report = Run(Data);
	TestTrue(TEXT("armor with no threshold is an error"), HasFindingAt(Report, HurtState(), EGSTN_Severity::Error, 0));
	TestEqual(TEXT("reported once"), Report.CountCode(HurtState()), 1);

	FGSTN_TimelineTrack_HurtboxModifier& Track = Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_HurtboxModifier>();
	Track.ArmorThreshold.Value = 0.f;
	TestTrue(TEXT("a threshold of 0 never lets a hit through"), Run(Data).HasCode(HurtState()));
	Track.ArmorThreshold.Value = 30.f;
	TestFalse(TEXT("a positive threshold"), Run(Data).HasCode(HurtState()));
	Track.ArmorThreshold = FGSTN_StaggerThreshold();
	Track.ArmorThreshold.bInfinite = true;
	TestFalse(TEXT("super armor"), Run(Data).HasCode(HurtState()));
	Track.ArmorThreshold.bInfinite = false;
	Track.ArmorThreshold.Value = 30.f;

	// Names against the hurtbox set the kit uses
	const TSet<FName> Known = { TEXT("Torso"), TEXT("Head") };
	FGSTN_TimelineRuleContext Context = MakeContext();
	Context.KnownHurtboxNames = &Known;
	TestFalse(TEXT("a hurtbox the set has"), Run(Data, Context).HasCode(HurtState()));
	Track.AffectedHurtboxes.Add(TEXT("Tail"));
	TestTrue(TEXT("a hurtbox the set lacks"), HasFindingAt(Run(Data, Context), HurtState(), EGSTN_Severity::Error, 0));
	TestFalse(TEXT("names are not checked when no set is known"), Run(Data).HasCode(HurtState()));

	// Invincible and Intangible have no threshold to check
	Track.Kind = EGSTN_HurtboxModifierKind::Invincible;
	Track.ArmorThreshold = FGSTN_StaggerThreshold();
	Track.AffectedHurtboxes.Pop();
	TestFalse(TEXT("only Armor reads the threshold"), Run(Data, Context).HasCode(HurtState()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateBakedSocketsTest, "GSTNCombat.Editor.Validate.BakedSockets", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateBakedSocketsTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeData();
	FGSTN_HitboxSpec Socketed = MakeSpec();
	Socketed.Space = EGSTN_HitboxSpace::BakedSocket;
	Socketed.Socket = TEXT("hand_r");
	AddHitbox(Data, 6, 10, { Socketed });

	// No frames at all: the hitbox would read nothing
	FGSTN_ValidationReport Report = Run(Data);
	TestTrue(TEXT("a socket with no baked frames"), HasFindingAt(Report, BakedSocketMissing(), EGSTN_Severity::Error, 0));
	TestEqual(TEXT("Core's warning about the same thing is not listed twice"), Report.CountCode(BakedSocketMissing()), 1);

	// Frames, but not as many as the timeline
	FGSTN_BakedSocketFrames Frames;
	Frames.Frames.SetNum(24);
	Data.BakedSockets.Add(TEXT("hand_r"), Frames);
	Report = Run(Data);
	TestTrue(TEXT("a bake of the wrong length"), HasFindingAt(Report, BakedLength(), EGSTN_Severity::Error, 0));
	TestFalse(TEXT("the socket is baked"), Report.HasCode(BakedSocketMissing()));
	TestEqual(TEXT("Core's warning about the length is not listed twice"), Report.CountCode(BakedLength()), 1);

	Data.BakedSockets[TEXT("hand_r")].Frames.SetNum(30);
	TestEqual(TEXT("a bake of exactly TotalFrames is clean"), CountRealFindings(Run(Data)), 0);

	// A longer bake is just as stale as a shorter one
	Data.BakedSockets[TEXT("hand_r")].Frames.SetNum(34);
	TestTrue(TEXT("a bake that is too long"), Run(Data).HasCode(BakedLength()));
	Data.BakedSockets[TEXT("hand_r")].Frames.SetNum(30);

	// A socket nobody reads is a note
	Data.BakedSockets.Add(TEXT("foot_l"), Frames);
	Report = Run(Data);
	TestTrue(TEXT("a baked socket no hitbox uses is an info"), Report.FindCode(BakedStale()) != nullptr && Report.FindCode(BakedStale())->Severity == EGSTN_Severity::Info);
	TestFalse(TEXT("an info does not invalidate"), Report.HasErrors());

	// A BakedSocket hitbox with no socket name is Core's error, classified
	Spec(Data, 0).Socket = NAME_None;
	TestTrue(TEXT("a BakedSocket hitbox with no socket"), HasFindingAt(Run(Data), BakedSocketMissing(), EGSTN_Severity::Error, 0));

	// A Root space hitbox needs no bake
	FGSTN_CombatTimelineData Root = MakeData();
	AddHitbox(Root, 6, 10, { MakeSpec() });
	TestFalse(TEXT("Root space"), Run(Root).HasCode(BakedSocketMissing()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateHurtboxSetTest, "GSTNCombat.Editor.Validate.HurtboxSetValidator", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateHurtboxSetTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage()));

	// No hurtbox at all: nothing could be hit
	FGSTN_ValidationReport Report;
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), Report);
	TestTrue(TEXT("an empty set is an error"), Report.FindCode(HurtboxEmpty()) != nullptr && Report.FindCode(HurtboxEmpty())->Severity == EGSTN_Severity::Error);

	const FGameplayTag Zone = GSTNCombatHitTags::Hurtbox_Feet.GetTag();
	auto MakeBox = [&](const TCHAR* Name)
	{
		FGSTN_Hurtbox Box;
		Box.Name = Name;
		Box.Shape.Type = EGSTN_ShapeType::Sphere;
		Box.Shape.Extent = FVector(20.0);
		Box.Tags.AddTag(Zone);
		return Box;
	};
	Set->Hurtboxes = { MakeBox(TEXT("Torso")), MakeBox(TEXT("Head")) };
	Report = FGSTN_ValidationReport();
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), Report);
	TestEqual(TEXT("a good set has nothing to say"), Report.Num(), 0);

	// Duplicate name
	Set->Hurtboxes.Add(MakeBox(TEXT("Head")));
	Report = FGSTN_ValidationReport();
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), Report);
	TestTrue(TEXT("a name used twice"), Report.HasCode(HurtboxDuplicateName()));
	Set->Hurtboxes.Pop();

	// Extent 0
	Set->Hurtboxes[0].Shape.Extent = FVector::ZeroVector;
	Report = FGSTN_ValidationReport();
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), Report);
	TestTrue(TEXT("a shape with no volume"), Report.HasCode(HurtboxExtent()));
	Set->Hurtboxes[0].Shape.Extent = FVector(20.0);

	// A tag outside the root, and no tag at all (a warning from Core)
	Set->Hurtboxes[1].Tags = FGameplayTagContainer(GSTNCombatHitTags::HitType_Grab.GetTag());
	Report = FGSTN_ValidationReport();
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), Report);
	TestTrue(TEXT("a tag under another root is an error"), Report.FindCode(HurtboxTag()) != nullptr && Report.FindCode(HurtboxTag())->Severity == EGSTN_Severity::Error);
	Set->Hurtboxes[1].Tags.Reset();
	Report = FGSTN_ValidationReport();
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), Report);
	TestTrue(TEXT("no tag is a warning"), Report.FindCode(HurtboxTag()) != nullptr && Report.FindCode(HurtboxTag())->Severity == EGSTN_Severity::Warning);
	TestTrue(TEXT("a warning does not invalidate"), Report.IsValid());

	// Negative reaction scale: Core's rule, in the generic code
	Set->Hurtboxes[1].Tags.AddTag(Zone);
	Set->Hurtboxes[1].ReactionScale.WeightScale = -1.f;
	Report = FGSTN_ValidationReport();
	FGSTN_CombatValidators::CheckHurtboxSet(*Set, Set.Get(), Report);
	TestTrue(TEXT("a reaction scale below 0"), Report.HasCode(HurtboxData()));

	// The validator class sees the set and nothing else
	TestTrue(TEXT("BuildReportForAsset takes a hurtbox set"), UGSTN_CombatValidatorBase::BuildReportForAsset(Set.Get(), Report));
	TStrongObjectPtr<UGSTN_CombatTimeline> NotASet(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	FGSTN_ValidationReport Unused;
	TestTrue(TEXT("and a timeline is taken by its own rules"), UGSTN_CombatValidatorBase::BuildReportForAsset(NotASet.Get(), Unused));
	TStrongObjectPtr<UGSTN_TestLoadoutHolder> Holder(NewObject<UGSTN_TestLoadoutHolder>(GetTransientPackage()));
	TestTrue(TEXT("anything holding a loadout is taken"), UGSTN_CombatValidatorBase::BuildReportForAsset(Holder.Get(), Unused));
	TestFalse(TEXT("an asset no validator takes is not validated"), UGSTN_CombatValidatorBase::BuildReportForAsset(GetTransientPackage(), Unused));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateCoreHitMessagesTest, "GSTNCombat.Editor.Validate.CoreHitMessagesClassified", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateCoreHitMessagesTest::RunTest(const FString& Parameters)
{
	// Every message of Core's hitbox checks lands in a code of the table: the classifier reads Core's wording, so a changed message
	// fails here instead of silently becoming Timeline.Core
	FGSTN_CombatTimelineData Data = MakeData();
	FGSTN_HitboxSpec Bad = MakeSpec();
	Bad.Shape.Extent = FVector::ZeroVector;											// no volume
	Bad.Keys.Add(KeyAt(0));															// keys not sorted
	Bad.Space = EGSTN_HitboxSpace::BakedSocket;										// no socket named
	Bad.Data.HitType = GSTNCombatHitTags::HitType_Grab.GetTag();					// data only in M4
	Bad.Data.HitstopMode = EGSTN_HitstopMode::Override;
	Bad.Data.HitstopFramesOverride = 999;											// above MaxHitstop
	AddHitbox(Data, 6, 10, { Bad });
	FGSTN_HitboxSpec Unbaked = MakeSpec();
	Unbaked.Space = EGSTN_HitboxSpace::BakedSocket;
	Unbaked.Socket = TEXT("hand_r");
	AddHitbox(Data, 12, 16, { Unbaked });
	FGSTN_TimelineTrack_HurtboxModifier Armor;
	Armor.StartFrame = 2;
	Armor.EndFrame = 6;
	Armor.Kind = EGSTN_HurtboxModifierKind::Armor;
	Armor.AffectedHurtboxes.Add(TEXT("Torso"));
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_HurtboxModifier>(Armor));

	FGSTN_ValidationReport Report;
	FGSTN_CombatValidators::CheckCoreRules(Data, MakeContext(), Report);
	TestEqual(TEXT("nothing falls through to the generic code"), Report.CountCode(CoreOther()), 0);
	TestTrue(TEXT("no volume and unsorted keys"), Report.HasCode(HitboxKeys()));
	TestTrue(TEXT("no socket named"), Report.HasCode(BakedSocketMissing()));
	TestTrue(TEXT("data only in M4 and the hitstop clamp"), Report.CountCode(HitData()) >= 2);
	TestFalse(TEXT("the Core wording of an unbaked socket and a short bake is replaced by the rules"), Report.Findings.ContainsByPredicate([](const FGSTN_ValidationFinding& Finding)
	{
		return Finding.Message.ToString().Contains(TEXT("has no baked frames yet")) || Finding.Message.ToString().Contains(TEXT("armor window with no ArmorThreshold")) || Finding.Message.ToString().Contains(TEXT("an Armor window with no"));
	}));

	// The hurtbox set's messages are pinned the same way by the previous test; here the codes are distinct from the timeline's
	TestTrue(TEXT("codes are unique names"), HitboxKeys() != HitData() && HurtState() != Variants() && BakedLength() != BakedStale());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateReportMatchesValidatorTest, "GSTNCombat.Editor.Validate.BuildReportMatchesValidator", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateReportMatchesValidatorTest::RunTest(const FString& Parameters)
{
	// Validate all and Data Validation run the same code on a timeline: the report of BuildReportForAsset is the validator's, finding for finding
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	AddHitbox(Timeline->Data, 6, 10, { MakeSpec() });
	Timeline->Data.TotalFrames = 30;
	Spec(Timeline->Data, 0).Data.Damage = -1.f;

	FGSTN_ValidationReport FromAll;
	TestTrue(TEXT("a timeline is validated"), UGSTN_CombatValidatorBase::BuildReportForAsset(Timeline.Get(), FromAll));
	FGSTN_ValidationReport Direct;
	FGSTN_CombatValidators::ValidateTimeline(Timeline->Data, FGSTN_CombatValidators::MakeTimelineContext(Timeline->Data, Timeline.Get(), 62.5f, true), Direct);
	TestEqual(TEXT("same number of findings"), FromAll.Num(), Direct.Num());
	TestTrue(TEXT("the negative damage is in it"), FromAll.HasCode(HitData()));
	TestTrue(TEXT("every finding names the asset"), !FromAll.Findings.ContainsByPredicate([&](const FGSTN_ValidationFinding& Finding) { return Finding.Asset.Get() != Timeline.Get(); }));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Validate all, the panel, the Message Log

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateAllNestedTest, "GSTNCombat.Editor.Validate.ValidateAllOverRegistryFindsNestedAssets", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateAllNestedTest::RunTest(const FString& Parameters)
{
	// In-memory assets announced to the registry, in folders three levels deep and in a sibling folder the prefix must not reach
	const FString Root = TEXT("/Temp/GSTNValidateAllTest");
	struct FMade { TStrongObjectPtr<UObject> Object; };
	TArray<FMade> Made;
	auto MakeTimeline = [&](const FString& Path, bool bBroken) -> UGSTN_CombatTimeline*
	{
		UPackage* Package = CreatePackage(*Path);
		UGSTN_CombatTimeline* Timeline = NewObject<UGSTN_CombatTimeline>(Package, FName(*FPaths::GetBaseFilename(Path)), RF_Public | RF_Standalone | RF_Transactional);
		Timeline->Data = MakeData();
		AddHitbox(Timeline->Data, 6, 10, { MakeSpec() });
		if (bBroken)
		{
			Spec(Timeline->Data, 0).Data.Damage = -4.f;
		}
		FAssetRegistryModule::AssetCreated(Timeline);
		Made.Add({ TStrongObjectPtr<UObject>(Timeline) });
		return Timeline;
	};
	MakeTimeline(Root / TEXT("A/B/C/Deep_Broken"), true);
	MakeTimeline(Root / TEXT("A/Shallow_Clean"), false);
	MakeTimeline(TEXT("/Temp/GSTNValidateAllOther/Elsewhere_Broken"), true);

	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(CreatePackage(*(Root / TEXT("A/B/EmptySet"))), TEXT("EmptySet"), RF_Public | RF_Standalone));
	FAssetRegistryModule::AssetCreated(Set.Get());

	GSTNValidateAll::FOptions Options;
	Options.PathPrefix = Root;
	const GSTNValidateAll::FResult Result = GSTNValidateAll::Run(Options);

	TestEqual(TEXT("the three assets under the prefix: two timelines and the hurtbox set"), Result.Assets.Num(), 3);
	TestEqual(TEXT("all validated"), Result.GetCheckedCount(), 3);
	bool bDeepBroken = false;
	bool bShallowClean = false;
	bool bEmptySet = false;
	for (const GSTNValidateAll::FAssetResult& Asset : Result.Assets)
	{
		bDeepBroken |= Asset.AssetData.AssetName == TEXT("Deep_Broken") && Asset.Report.HasCode(HitData());
		bShallowClean |= Asset.AssetData.AssetName == TEXT("Shallow_Clean") && CountRealFindings(Asset.Report) == 0;
		bEmptySet |= Asset.AssetData.AssetName == TEXT("EmptySet") && Asset.Report.HasCode(HurtboxEmpty());
		TestTrue(TEXT("nothing outside the prefix"), Asset.AssetData.PackagePath.ToString().StartsWith(Root));
	}
	TestTrue(TEXT("the broken timeline three levels down is found"), bDeepBroken);
	TestTrue(TEXT("the clean one is listed, with nothing on it"), bShallowClean);
	TestTrue(TEXT("the empty hurtbox set is found"), bEmptySet);
	TestTrue(TEXT("errors are counted"), Result.HasErrors());

	// The default scope leaves /Temp alone (unsaved assets are not project content)
	GSTNValidateAll::FOptions Default;
	TArray<FAssetData> InDefaultScope;
	GSTNValidateAll::CollectAssets(Default, InDefaultScope);
	TestFalse(TEXT("the default scope skips /Temp"), InDefaultScope.ContainsByPredicate([](const FAssetData& Data) { return Data.PackagePath.ToString().StartsWith(TEXT("/Temp")); }));

	for (const FMade& Item : Made)
	{
		FAssetRegistryModule::AssetDeleted(Item.Object.Get());
	}
	FAssetRegistryModule::AssetDeleted(Set.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidatePanelModelTest, "GSTNCombat.Editor.Validate.PanelModelGroupsFiltersAndSorts", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidatePanelModelTest::RunTest(const FString& Parameters)
{
	auto MakeAsset = [](const TCHAR* Name, std::initializer_list<EGSTN_Severity> Severities)
	{
		GSTNValidateAll::FAssetResult Asset;
		Asset.bValidated = true;
		Asset.AssetData.AssetName = Name;
		for (const EGSTN_Severity Severity : Severities)
		{
			Asset.Report.Add(Severity, Severity == EGSTN_Severity::Error ? HitData() : Variants(), FText::FromString(FString::Printf(TEXT("finding of %s"), Name)));
		}
		return Asset;
	};

	GSTNValidateAll::FResult Result;
	Result.Assets = { MakeAsset(TEXT("B_Warns"), { EGSTN_Severity::Warning, EGSTN_Severity::Info }), MakeAsset(TEXT("A_Clean"), {}), MakeAsset(TEXT("C_Errors"), { EGSTN_Severity::Error, EGSTN_Severity::Warning }), MakeAsset(TEXT("A_Notes"), { EGSTN_Severity::Info }) };

	FGSTN_ValidationPanelModel Model;
	TestFalse(TEXT("nothing has run"), Model.HasResult());
	TestTrue(TEXT("the summary says what to do"), Model.GetSummary().ToString().Contains(TEXT("Validate all")));

	int32 Changes = 0;
	Model.OnChanged.AddLambda([&Changes]() { ++Changes; });
	Model.SetResult(Result);
	TestEqual(TEXT("one change"), Changes, 1);
	TestEqual(TEXT("the clean asset has no row"), Model.GetRoots().Num(), 3);
	if (Model.GetRoots().Num() == 3)
	{
		TestEqual(TEXT("the asset with an error first"), Model.GetAsset(*Model.GetRoots()[0]).AssetData.AssetName, FName(TEXT("C_Errors")));
		TestEqual(TEXT("then warnings"), Model.GetAsset(*Model.GetRoots()[1]).AssetData.AssetName, FName(TEXT("B_Warns")));
		TestEqual(TEXT("then notes"), Model.GetAsset(*Model.GetRoots()[2]).AssetData.AssetName, FName(TEXT("A_Notes")));
		TestEqual(TEXT("an asset row holds its findings"), Model.GetRoots()[0]->Children.Num(), 2);
		TestTrue(TEXT("its icon is the worst severity"), Model.GetWorstSeverity(*Model.GetRoots()[0]) == EGSTN_Severity::Error);
		TestTrue(TEXT("a finding row resolves to a finding"), Model.GetFinding(*Model.GetRoots()[0]->Children[0]) != nullptr);
		TestTrue(TEXT("an asset row does not"), Model.GetFinding(*Model.GetRoots()[0]) == nullptr);
	}
	TestTrue(TEXT("the summary counts what was checked"), Model.GetSummary().ToString().Contains(TEXT("4 assets checked: 1 errors, 2 warnings, 2 notes")));

	FGSTN_ValidationPanelModel::FFilter Filter;
	Filter.bInfos = false;
	Filter.bWarnings = false;
	Model.SetFilter(Filter);
	TestEqual(TEXT("errors only"), Model.GetRoots().Num(), 1);
	Filter = FGSTN_ValidationPanelModel::FFilter();
	Filter.Text = TEXT("a_notes");
	Model.SetFilter(Filter);
	TestEqual(TEXT("the text filter matches the asset name, ignoring case"), Model.GetRoots().Num(), 1);
	Filter.Text = TEXT("Timeline.HitData");
	Model.SetFilter(Filter);
	TestEqual(TEXT("and the code"), Model.GetRoots().Num(), 1);
	Filter.Text = TEXT("no such text");
	Model.SetFilter(Filter);
	TestEqual(TEXT("nothing matches"), Model.GetRoots().Num(), 0);

	// A re-checked asset replaces its row
	Model.SetFilter(FGSTN_ValidationPanelModel::FFilter());
	TestTrue(TEXT("replace one asset"), Model.ReplaceAssetResult(MakeAsset(TEXT("C_Errors"), {})));
	TestEqual(TEXT("it is clean now"), Model.GetRoots().Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidatePanelLocationTest, "GSTNCombat.Editor.Validate.PanelFindingLocationSelectsKey", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidatePanelLocationTest::RunTest(const FString& Parameters)
{
	// The function the panel (and the timeline's own Frame Data tab) calls: a location selects the key it names and puts the playhead on its frame
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	Timeline->Data.TotalFrames = 40;
	FGSTN_TestKeyedTrack Track;
	Track.StartFrame = 4;
	Track.EndFrame = 20;
	for (const int32 Frame : { 5, 9, 14 })
	{
		FGSTN_TestKey Key;
		Key.Frame = Frame;
		Track.Keys.Add(Key);
	}
	Timeline->Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TestKeyedTrack>(Track));
	const TSharedRef<FGSTN_TestKeyedAccess> Access = MakeShared<FGSTN_TestKeyedAccess>();
	FGSTN_KeyedTrackAccessRegistry::Register(FGSTN_TestKeyedTrack::StaticStruct(), Access);

	{
		FGSTN_TimelineEditorModel Model(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
		FGSTN_FindingLocation Where;
		Where.TrackIndex = 0;
		Where.KeyIndex = 1;
		Where.Frame = 9;
		TestTrue(TEXT("the location moved something"), FGSTN_TimelineActions::GoToFinding(Model, Where));
		TestTrue(TEXT("the key is selected"), Model.IsSelected(FGSTN_Selection::Key(0, 1)));
		TestFalse(TEXT("and not the window"), Model.IsSelected(FGSTN_Selection::Window(0)));
		TestEqual(TEXT("the playhead is on the frame"), Model.GetPlayhead(), 9);

		Where.KeyIndex = INDEX_NONE;
		TestTrue(TEXT("a location with no key selects the window"), FGSTN_TimelineActions::GoToFinding(Model, Where));
		TestTrue(TEXT("the window is selected"), Model.IsSelected(FGSTN_Selection::Window(0)));
	}
	FGSTN_KeyedTrackAccessRegistry::Unregister(FGSTN_TestKeyedTrack::StaticStruct());

	// Opening a finding that is not about a timeline, or has no place, must not need an editor to decide that
	TestFalse(TEXT("no asset, nowhere to go"), GSTNValidationNavigation::OpenFinding(nullptr, FGSTN_FindingLocation::OnTrack(0, 3)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ValidateMessageLogTest, "GSTNCombat.Editor.Validate.MessageLogReceivesFindings", GSTN_VALIDATE_M4_FLAGS)
bool FGSTN_ValidateMessageLogTest::RunTest(const FString& Parameters)
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	const TSharedRef<IMessageLogListing> Listing = MessageLogModule.GetLogListing(GSTNValidationLog::GetLogName());
	TestEqual(TEXT("the category is GSTNCombat"), GSTNValidationLog::GetLogName(), FName(TEXT("GSTNCombat")));

	// The Message Log mirrors an error to the output log, where the automation framework would count it as a failure of this test
	AddExpectedError(TEXT("an error finding of the M4 test"), EAutomationExpectedErrorFlags::Contains, 1);

	FGSTN_ValidationReport Report;
	Report.Add(EGSTN_Severity::Error, HitData(), FText::FromString(TEXT("an error finding of the M4 test")));
	Report.Add(EGSTN_Severity::Warning, Variants(), FText::FromString(TEXT("a warning finding of the M4 test")));
	GSTNValidationLog::WriteReport(Report, FText::FromString(TEXT("M4 test page")), false, false);

	int32 Errors = 0;
	int32 Warnings = 0;
	for (const TSharedRef<FTokenizedMessage>& Message : Listing->GetFilteredMessages())
	{
		const FString Text = Message->ToText().ToString();
		Errors += (Message->GetSeverity() == EMessageSeverity::Error && Text.Contains(TEXT("an error finding of the M4 test"))) ? 1 : 0;
		Warnings += (Message->GetSeverity() == EMessageSeverity::Warning && Text.Contains(TEXT("a warning finding of the M4 test"))) ? 1 : 0;
	}
	TestEqual(TEXT("the error reached the log with its severity"), Errors, 1);
	TestEqual(TEXT("the warning too"), Warnings, 1);
	Listing->ClearMessages();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
