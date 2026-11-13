// Automation tests of the link plugin: link type definitions, rules data and the outcome track (plan F5a).

#include "Data/GSTN_GrabRules.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "Data/GSTN_LinkOutcomeTrack.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_CombatCoreTags.h"
#include "GSTN_LinkSettings.h"
#include "GSTN_LinkTags.h"
#include "GSTN_LinkTestUtils.h"
#include "Misc/AutomationTest.h"
#include "StructUtils/InstancedStruct.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkDefinitionValidTest, "GSTNLink.Core.Data.DefinitionValid", GSTNLinkTest::Flags)
bool FGSTN_LinkDefinitionValidTest::RunTest(const FString& Parameters)
{
	UGSTN_LinkTypeDefinition* Grab = GSTNLinkTest::MakeGrabDefinition();
	TArray<FString> Errors;
	Grab->Validate(Errors);
	TestEqual(TEXT("A grab definition has no error"), Errors.Num(), 0);
	TestEqual(TEXT("The grab parks role 2"), Grab->GetParkedRoleIndex(), 2);
	TestEqual(TEXT("Role 1 is found by its tag"), Grab->FindRoleIndex(GSTNLinkTags::Role_Grabber.GetTag()), 1);
	TestEqual(TEXT("Role 2 is found by its tag"), Grab->FindRoleIndex(GSTNLinkTags::Role_Grabbed.GetTag()), 2);
	TestEqual(TEXT("A tag of neither role is not found"), Grab->FindRoleIndex(GSTNLinkTags::Role_Latcher.GetTag()), 0);
	TestEqual(TEXT("An invalid tag is not found"), Grab->FindRoleIndex(FGameplayTag()), 0);
	TestTrue(TEXT("The asset is found by the link type primary asset type"), Grab->GetPrimaryAssetId().PrimaryAssetType == GSTNLinkAssetTypes::LinkType());
	TestEqual(TEXT("The primary asset type is GSTN_LinkType"), GSTNLinkAssetTypes::LinkType().ToString(), FString(TEXT("GSTN_LinkType")));

#if WITH_EDITOR
	FDataValidationContext Context;
	TestEqual(TEXT("The asset validates"), static_cast<int32>(Grab->IsDataValid(Context)), static_cast<int32>(EDataValidationResult::Valid));
#endif

	// A new link type is data alone: a latch parks role 1 and lets both roles act, with no C++ and no registration
	UGSTN_LinkTypeDefinition* Latch = GSTNLinkTest::MakeLatchDefinition();
	TArray<FString> LatchErrors;
	Latch->Validate(LatchErrors);
	TestEqual(TEXT("A latch definition has no error"), LatchErrors.Num(), 0);
	TestEqual(TEXT("The latch parks role 1"), Latch->GetParkedRoleIndex(), 1);
	TestTrue(TEXT("The latched role keeps acting"), Latch->GetRoleDef(2).bActsWhileLinked);
	TestTrue(TEXT("Role 1 of the latch is the latcher"), Latch->GetRoleDef(1).RoleTag == GSTNLinkTags::Role_Latcher.GetTag());

	// A break rule names one of the two roles, a range is not negative
	Grab->Break.MaxDistanceCm = 400.f;
	FGSTN_RoleHitBreak& HitBreak = Grab->Break.HitBreaks.AddDefaulted_GetRef();
	HitBreak.RoleTag = GSTNLinkTags::Role_Grabbed.GetTag();
	HitBreak.MinKnockbackToBreak = 200.f;
	Errors.Reset();
	Grab->Validate(Errors);
	TestEqual(TEXT("A break rule for a role of the definition is fine"), Errors.Num(), 0);

	Grab->MaxFrames = -3;
	Errors.Reset();
	Grab->Validate(Errors);
	TestTrue(TEXT("A negative MaxFrames is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("MaxFrames")));

	// Both roles parked, or a parked Hand link, are reported
	UGSTN_LinkTypeDefinition* BothParked = GSTNLinkTest::MakeGrabDefinition();
	BothParked->InitiatorRole.bParked = true;
	Errors.Reset();
	BothParked->Validate(Errors);
	TestTrue(TEXT("Two parked roles are reported"), GSTNLinkTest::AnyContains(Errors, TEXT("both roles are parked")));
	UGSTN_LinkTypeDefinition* ParkedHand = GSTNLinkTest::MakeGrabDefinition();
	ParkedHand->Channel = EGSTN_LinkChannel::Hand;
	Errors.Reset();
	ParkedHand->Validate(Errors);
	TestTrue(TEXT("A parked Hand link is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("Hand-channel link never parks")));

	// The grab rules data: the defaults ignore the states a grab must not land on, and the optional plugins' states are by name
	const FGSTN_GrabRules Rules;
	TestTrue(TEXT("The default grab requirements ignore Linked"), Rules.Requirements.IgnoreTags.HasTagExact(GSTNLinkTags::Linked.GetTag()));
	TestTrue(TEXT("The default grab requirements ignore the grab immunity"), Rules.Requirements.IgnoreTags.HasTagExact(GSTNLinkTags::Immunity_Grab.GetTag()));
	TestTrue(TEXT("The default grab requirements ignore hitstun"), Rules.Requirements.IgnoreTags.HasTagExact(GSTNCombatTags::State_Hitstun.GetTag()));
	TestEqual(TEXT("Four state tags of other plugins are named, not declared"), Rules.IgnoreTagNames.Num(), 4);
	TestTrue(TEXT("The parts gate is respected by default for a grab"), Rules.bRespectGrabLocked);
	TestTrue(TEXT("The grab weight limit is positive"), Rules.MaxGrabWeight > 0.f);
	TestTrue(TEXT("A grab beats a shield by default"), Rules.bGrabBeatsShield);
	TestTrue(TEXT("The mash counts flicks by default"), Rules.Mash.bCountFlicks && Rules.Mash.Buttons.IsEmpty());
	TestTrue(TEXT("The mash range is ordered"), Rules.Mash.MinFrames <= Rules.Mash.MaxFrames);
	const FGSTN_LatchRules Latching;
	TestTrue(TEXT("The latch requirements ignore the latch immunity"), Latching.Requirements.IgnoreTags.HasTagExact(GSTNLinkTags::Immunity_Latch.GetTag()));
	TestTrue(TEXT("The latch is for heavier victims than the grab"), Latching.MinLatchWeight > Rules.MaxGrabWeight);

	// The kit extension rides the combat extension surface and is found by type
	FGSTN_LinkKitExtension Extension;
	Extension.GrabRules.MaxGrabWeight = 77.f;
	FGSTN_KitExtensionList List;
	List.ExtensionData.Add(FInstancedStruct::Make(Extension));
	const FGSTN_LinkKitExtension* Found = List.Find<FGSTN_LinkKitExtension>();
	TestNotNull(TEXT("The kit extension list finds the link entry"), Found);
	if (Found)
	{
		TestEqual(TEXT("The entry keeps its grab rules"), Found->GrabRules.MaxGrabWeight, 77.f);
	}

	// The settings carry the spec defaults
	const UGSTN_LinkSettings& Settings = UGSTN_LinkSettings::Get();
	TestEqual(TEXT("A predicted link waits 12 frames for its response"), Settings.RequestTimeoutFrames, 12);
	TestEqual(TEXT("A formation has a 3 frame orphan grace"), Settings.OrphanGraceFrames, 3);

	// The outcome track: a throw key needs a throw hit, keys are ordered inside the track
	FGSTN_LinkOutcomeTrack Track;
	Track.StartFrame = 10;
	Track.EndFrame = 40;
	FGSTN_LinkOutcomeKey& Key = Track.Keys.AddDefaulted_GetRef();
	Key.Frame = 20;
	Key.Outcome = EGSTN_LinkOutcome::Throw;
	Key.ThrowHit.HitType = GSTNLinkTags::HitType_Throw.GetTag();
	Errors.Reset();
	Track.Validate(TEXT("Track"), Errors);
	TestEqual(TEXT("A throw key with a throw hit is valid"), Errors.Num(), 0);
	TestNotNull(TEXT("The key is found at its frame"), Track.FindKeyAt(20));
	TestNull(TEXT("No key at another frame"), Track.FindKeyAt(21));
	Key.ThrowHit.HitType = FGameplayTag();
	Errors.Reset();
	Track.Validate(TEXT("Track"), Errors);
	TestTrue(TEXT("A throw key with a plain hit is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("Combat.HitType.Throw")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkUnknownRoleTagTest, "GSTNLink.Core.Data.UnknownRoleTagRejected", GSTNLinkTest::Flags)
bool FGSTN_LinkUnknownRoleTagTest::RunTest(const FString& Parameters)
{
	// A role tag that is not registered loads as an invalid tag (its plugin is missing): the definition is rejected, not crashed on
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
		Definition->InitiatorRole.RoleTag = FGameplayTag();
		TArray<FString> Errors;
		Definition->Validate(Errors);
		TestTrue(TEXT("An unset or unregistered initiator role tag is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("initiator role has no role tag")));
#if WITH_EDITOR
		FDataValidationContext Context;
		TestEqual(TEXT("IsDataValid fails with it"), static_cast<int32>(Definition->IsDataValid(Context)), static_cast<int32>(EDataValidationResult::Invalid));
#endif
	}
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
		Definition->TargetRole.RoleTag = FGameplayTag();
		TArray<FString> Errors;
		Definition->Validate(Errors);
		TestTrue(TEXT("An unset or unregistered target role tag is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("target role has no role tag")));
	}
	// A registered tag outside the role subtree is no role
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
		Definition->TargetRole.RoleTag = GSTNLinkTags::Type_Latch.GetTag();
		TArray<FString> Errors;
		Definition->Validate(Errors);
		TestTrue(TEXT("A role tag outside Combat.Link.Role is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("not under Combat.Link.Role")));
	}
	// Two roles with one tag cannot be told apart
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
		Definition->TargetRole.RoleTag = Definition->InitiatorRole.RoleTag;
		TArray<FString> Errors;
		Definition->Validate(Errors);
		TestTrue(TEXT("Equal role tags are reported"), GSTNLinkTest::AnyContains(Errors, TEXT("must be told apart")));
	}
	// A hit break for a role the definition does not have
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
		FGSTN_RoleHitBreak& HitBreak = Definition->Break.HitBreaks.AddDefaulted_GetRef();
		HitBreak.RoleTag = GSTNLinkTags::Role_Latcher.GetTag();
		TArray<FString> Errors;
		Definition->Validate(Errors);
		TestTrue(TEXT("A hit break for a foreign role is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("neither role of the definition")));
	}
	// The link type tag: unset, or outside the type subtree
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
		Definition->LinkType = FGameplayTag();
		TArray<FString> Errors;
		Definition->Validate(Errors);
		TestTrue(TEXT("An unset link type tag is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("link type tag is not set")));
		Definition->LinkType = GSTNLinkTags::Role_Grabber.GetTag();
		Errors.Reset();
		Definition->Validate(Errors);
		TestTrue(TEXT("A link type tag outside Combat.Link.Type is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("not under Combat.Link.Type")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkForbiddenWithBodyTest, "GSTNLink.Core.Data.ForbiddenWithBody", GSTNLinkTest::Flags)
bool FGSTN_LinkForbiddenWithBodyTest::RunTest(const FString& Parameters)
{
	// The grab is a Body type. A Hand type here stands in for a held object; it is built from the latch tag because the real held
	// type is declared by another plugin
	UGSTN_LinkTypeDefinition* Grab = GSTNLinkTest::MakeGrabDefinition();
	UGSTN_LinkTypeDefinition* Hand = GSTNLinkTest::MakeLatchDefinition();
	Hand->Channel = EGSTN_LinkChannel::Hand;
	Hand->InitiatorRole.bParked = false;
	TArray<FString> Errors;
	Hand->Validate(Errors);
	TestEqual(TEXT("A Hand type that parks nobody is valid"), Errors.Num(), 0);

	TestTrue(TEXT("A Hand link coexists with a Body link by default"), Hand->CanCoexistWith(*Grab));
	TestTrue(TEXT("The test is symmetric"), Grab->CanCoexistWith(*Hand));

	Hand->ForbiddenWithBody.Add(GSTNLinkTags::Type_Grab.GetTag());
	Errors.Reset();
	Hand->Validate(Errors);
	TestEqual(TEXT("Listing a Body type is valid on a Hand type"), Errors.Num(), 0);
	TestFalse(TEXT("A Hand link that forbids the grab does not coexist with it"), Hand->CanCoexistWith(*Grab));
	TestFalse(TEXT("The test is symmetric when forbidden"), Grab->CanCoexistWith(*Hand));

	// A Body link never coexists with another Body link; neither do two Hand links
	UGSTN_LinkTypeDefinition* OtherBody = GSTNLinkTest::MakeLatchDefinition();
	TestFalse(TEXT("Two Body links never coexist"), Grab->CanCoexistWith(*OtherBody));
	UGSTN_LinkTypeDefinition* OtherHand = GSTNLinkTest::MakeLatchDefinition();
	OtherHand->Channel = EGSTN_LinkChannel::Hand;
	OtherHand->InitiatorRole.bParked = false;
	TestFalse(TEXT("Two Hand links never coexist"), Hand->CanCoexistWith(*OtherHand));

	// The list is meaningless on a Body type, and an entry must be a link type
	Grab->ForbiddenWithBody.Add(GSTNLinkTags::Type_Latch.GetTag());
	Errors.Reset();
	Grab->Validate(Errors);
	TestTrue(TEXT("ForbiddenWithBody on a Body type is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("only meaningful on a Hand-channel type")));
	Hand->ForbiddenWithBody.Add(GSTNLinkTags::Role_Grabber.GetTag());
	Errors.Reset();
	Hand->Validate(Errors);
	TestTrue(TEXT("A non-type entry is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("not a registered tag under Combat.Link.Type")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
