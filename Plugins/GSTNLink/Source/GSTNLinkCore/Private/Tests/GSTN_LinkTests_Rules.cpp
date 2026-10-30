// Automation tests of the link plugin: the grab and latch request rules (plan F5b).

#include "GSTN_CombatCoreTags.h"
#include "GSTN_LinkTestUtils.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Rules/GSTN_LinkRule_Latch.h"
#include "Sync/GSTN_SyncFragment.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 Refusal(const FGSTN_LinkRuleResult& Result)
	{
		return static_cast<int32>(Result.Refusal);
	}

	/** A grab definition that lists the grab rule, and a standard context for it */
	struct FGrabFixture
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
		UGSTN_LinkRule_Grab* Rule = NewObject<UGSTN_LinkRule_Grab>(Definition);
		FGSTN_LinkRequestContext Context;

		FGrabFixture()
		{
			Definition->Rules.Add(Rule);
			Context = GSTNLinkTest::MakeContext(Definition);
		}

		FGSTN_LinkRuleResult Evaluate() const { return Rule->Evaluate(Context); }
	};

	struct FLatchFixture
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeLatchDefinition();
		UGSTN_LinkRule_Latch* Rule = NewObject<UGSTN_LinkRule_Latch>(Definition);
		FGSTN_LinkRequestContext Context;

		FLatchFixture()
		{
			Definition->Rules.Add(Rule);
			Context = GSTNLinkTest::MakeContext(Definition);
			// A latch is for a large enemy
			Context.Target.EffectiveWeight = 400.f;
		}

		FGSTN_LinkRuleResult Evaluate() const { return Rule->Evaluate(Context); }
	};
}

// ---------------------------------------------------------------------------------------------------------------------------
// Grab
// ---------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRuleGrabRequirementsTest, "GSTNLink.Core.Rule.Grab.Requirements", GSTNLinkTest::Flags)
bool FGSTN_LinkRuleGrabRequirementsTest::RunTest(const FString& Parameters)
{
	FGrabFixture Fixture;
	TestTrue(TEXT("A standard grab on a free standard fighter is accepted"), Fixture.Evaluate().bAccepted);

	// The default requirements: a victim already linked, immune or in hitstun is not grabbed
	{
		FGrabFixture Linked;
		Linked.Context.Target.Tags.AddTag(GSTNLinkTags::Linked);
		TestEqual(TEXT("A victim that projects Linked is refused"), Refusal(Linked.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::Requirements));
	}
	{
		FGrabFixture Immune;
		Immune.Context.Target.Tags.AddTag(GSTNLinkTags::Immunity_Grab);
		TestEqual(TEXT("A boss with the grab immunity is refused"), Refusal(Immune.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::Requirements));
	}
	{
		FGrabFixture Hitstun;
		Hitstun.Context.Target.Tags.AddTag(GSTNCombatTags::State_Hitstun);
		TestEqual(TEXT("A victim in hitstun is refused"), Refusal(Hitstun.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::Requirements));
	}

	// The states of other plugins are by name; a name nobody declares is skipped, a name somebody declares counts
	{
		FGrabFixture Named;
		Named.Rule->Rules.IgnoreTagNames = { FName(TEXT("Combat.State.DoesNotExistAnywhere")) };
		TestTrue(TEXT("A name no plugin declares changes nothing"), Named.Evaluate().bAccepted);
		Named.Rule->Rules.IgnoreTagNames = { FName(TEXT("Combat.State.Busy")) };
		Named.Context.Target.Tags.AddTag(GSTNCombatTags::State_Busy);
		TestEqual(TEXT("A victim carrying a named state is refused"), Refusal(Named.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::Requirements));
	}

	// The default list names the optional plugins' states
	const FGSTN_GrabRules Defaults;
	int32 Resolved = 0;
	for (const FName& Name : Defaults.IgnoreTagNames)
	{
		Resolved += FGameplayTag::RequestGameplayTag(Name, false).IsValid() ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("%d of the %d default optional-plugin state names are declared in this run"), Resolved, Defaults.IgnoreTagNames.Num()));
	{
		FGrabFixture Optional;
		bool bAllRefused = true;
		for (const FName& Name : Defaults.IgnoreTagNames)
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Name, false);
			if (!Tag.IsValid())
			{
				continue;
			}
			Optional.Context.Target.Tags.Reset();
			Optional.Context.Target.Tags.AddTag(Tag);
			bAllRefused &= !Optional.Evaluate().bAccepted;
		}
		TestTrue(TEXT("Each default named state that exists refuses the grab"), bAllRefused);
	}

	// The shared checks
	{
		FGrabFixture Self;
		Self.Context.Target.EntityId = Self.Context.Initiator.EntityId;
		TestEqual(TEXT("Nobody grabs themselves"), Refusal(Self.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::SelfLink));
	}
	{
		FGrabFixture Busy;
		Busy.Context.Initiator.Links.Body.Phase = EGSTN_LinkPhase::Active;
		TestEqual(TEXT("A grabber already linked is refused"), Refusal(Busy.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::InitiatorBusy));
	}
	{
		FGrabFixture Busy;
		Busy.Context.Target.Links.Body.Phase = EGSTN_LinkPhase::Pending;
		TestEqual(TEXT("A victim with a pending request is refused: simultaneous grabs cannot both win"), Refusal(Busy.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TargetBusy));
	}
	{
		FGrabFixture Locked;
		Locked.Context.Target.Tags.AddTag(GSTNLinkTags::Lock_Link);
		TestEqual(TEXT("A victim that refuses links is refused"), Refusal(Locked.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TargetLocked));
	}
	{
		FGrabFixture Required;
		Required.Definition->RequestRequirements.RequireTags.AddTag(GSTNCombatTags::State_Busy);
		TestEqual(TEXT("The definition's own requirements apply"), Refusal(Required.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::RequestRequirements));
		Required.Context.Target.Tags.AddTag(GSTNCombatTags::State_Busy);
		TestTrue(TEXT("... and are met by the right state"), Required.Evaluate().bAccepted);
	}
	{
		FGrabFixture NoDefinition;
		NoDefinition.Context.Definition = nullptr;
		TestEqual(TEXT("A context without a definition is refused, not crashed on"), Refusal(NoDefinition.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::NoDefinition));
	}

	// The kit's requirements replace the rule's own copy
	{
		FGrabFixture Kit;
		FGSTN_GrabRules KitRules;
		KitRules.Requirements.IgnoreTags.Reset();
		KitRules.IgnoreTagNames.Reset();
		Kit.Context.InitiatorGrabRules = &KitRules;
		Kit.Context.Target.Tags.AddTag(GSTNCombatTags::State_Hitstun);
		TestTrue(TEXT("A kit that ignores nothing grabs a victim in hitstun"), Kit.Evaluate().bAccepted);
	}

	// A definition that lists a null rule is reported
	{
		UGSTN_LinkTypeDefinition* Definition = GSTNLinkTest::MakeGrabDefinition();
		Definition->Rules.Add(nullptr);
		TArray<FString> Errors;
		Definition->Validate(Errors);
		TestTrue(TEXT("An empty rule entry is reported"), GSTNLinkTest::AnyContains(Errors, TEXT("request rule 0 is empty")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRuleGrabWeightTest, "GSTNLink.Core.Rule.Grab.Weight", GSTNLinkTest::Flags)
bool FGSTN_LinkRuleGrabWeightTest::RunTest(const FString& Parameters)
{
	FGrabFixture Fixture;
	Fixture.Rule->Rules.MaxGrabWeight = 120.f;

	Fixture.Context.Target.EffectiveWeight = 100.f;
	TestTrue(TEXT("A standard fighter is light enough"), Fixture.Evaluate().bAccepted);
	Fixture.Context.Target.EffectiveWeight = 120.f;
	TestTrue(TEXT("A victim exactly at the limit is grabbed"), Fixture.Evaluate().bAccepted);
	Fixture.Context.Target.EffectiveWeight = 120.5f;
	TestEqual(TEXT("A hair above it is too heavy"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TooHeavy));
	Fixture.Context.Target.EffectiveWeight = 500.f;
	TestEqual(TEXT("A large enemy is too heavy"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TooHeavy));

	// A battered enemy gets lighter: the same fighter, less weight, now grabbable
	Fixture.Context.Target.EffectiveWeight = 110.f;
	TestTrue(TEXT("Losing weight makes a victim grabbable"), Fixture.Evaluate().bAccepted);

	// How heavy a target can be is the grabber's kit data
	FGSTN_GrabRules StrongKit;
	StrongKit.MaxGrabWeight = 300.f;
	Fixture.Context.Target.EffectiveWeight = 250.f;
	TestEqual(TEXT("A weak kit cannot grab it"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TooHeavy));
	Fixture.Context.InitiatorGrabRules = &StrongKit;
	TestTrue(TEXT("A strong kit can"), Fixture.Evaluate().bAccepted);
	StrongKit.MaxGrabWeight = 200.f;
	TestEqual(TEXT("... up to its limit"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TooHeavy));

	// A zero limit grabs nothing
	StrongKit.MaxGrabWeight = 0.f;
	Fixture.Context.Target.EffectiveWeight = 1.f;
	TestEqual(TEXT("A kit that cannot grab anything"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TooHeavy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRuleGrabImmunityFrameTest, "GSTNLink.Core.Rule.Grab.ImmunityFrame", GSTNLinkTest::Flags)
bool FGSTN_LinkRuleGrabImmunityFrameTest::RunTest(const FString& Parameters)
{
	FGrabFixture Fixture;
	Fixture.Context.Frame = 100;

	Fixture.Context.Target.GrabImmunityEndFrame = 0;
	TestTrue(TEXT("No immunity: accepted"), Fixture.Evaluate().bAccepted);

	Fixture.Context.Target.GrabImmunityEndFrame = 130;
	TestEqual(TEXT("Inside the immunity: refused"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::Immunity));
	Fixture.Context.Frame = 129;
	TestEqual(TEXT("The last immune frame: refused"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::Immunity));
	Fixture.Context.Frame = 130;
	TestTrue(TEXT("On the end frame the immunity is over"), Fixture.Evaluate().bAccepted);
	Fixture.Context.Frame = 5000;
	TestTrue(TEXT("Long after: accepted"), Fixture.Evaluate().bAccepted);

	// The latch's own immunity is another stamp and does not hold off a grab
	Fixture.Context.Frame = 100;
	Fixture.Context.Target.GrabImmunityEndFrame = 0;
	Fixture.Context.Target.LatchImmunityEndFrame = 500;
	TestTrue(TEXT("A latch immunity does not stop a grab"), Fixture.Evaluate().bAccepted);

	// A rule is a pure function of its context: the same context, the same answer, and the rule does not change
	Fixture.Context.Target.GrabImmunityEndFrame = 130;
	const FGSTN_LinkRuleResult A = Fixture.Evaluate();
	const FGSTN_LinkRuleResult B = Fixture.Evaluate();
	TestTrue(TEXT("Equal contexts, equal answers"), A.bAccepted == B.bAccepted && A.Refusal == B.Refusal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRuleGrabLockedByNameTest, "GSTNLink.Core.Rule.Grab.GrabLockedByName", GSTNLinkTest::Flags)
bool FGSTN_LinkRuleGrabLockedByNameTest::RunTest(const FString& Parameters)
{
	// This plugin never declares the tag: the parts plugin does, and it may not be loaded
	TArray<FGameplayTag> Declared;
	GSTNLinkTags::GetDeclaredTags(Declared);
	const FGameplayTag GrabLocked = FGameplayTag::RequestGameplayTag(TEXT("Combat.Part.GrabLocked"), false);
	TestFalse(TEXT("GSTNLink does not declare Combat.Part.GrabLocked"), GrabLocked.IsValid() && Declared.Contains(GrabLocked));

	FGrabFixture Fixture;
	if (GrabLocked.IsValid())
	{
		Fixture.Context.Target.Tags.AddTag(GrabLocked);
		TestEqual(TEXT("A victim whose grab gate is unmet is refused"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::GrabLocked));

		Fixture.Rule->Rules.bRespectGrabLocked = false;
		TestTrue(TEXT("The designer can switch the gate off"), Fixture.Evaluate().bAccepted);

		// The kit's rules carry the same switch
		Fixture.Rule->Rules.bRespectGrabLocked = true;
		FGSTN_GrabRules KitRules;
		KitRules.bRespectGrabLocked = false;
		Fixture.Context.InitiatorGrabRules = &KitRules;
		TestTrue(TEXT("The kit's rules decide, not the rule's copy"), Fixture.Evaluate().bAccepted);
		AddInfo(TEXT("Combat.Part.GrabLocked is declared in this run (the parts plugin is loaded): the refusal path was exercised"));
	}
	else
	{
		// Without the parts plugin the tag does not exist: reading it by name is a no-op and the weight rule alone decides
		TestTrue(TEXT("Without the parts plugin the by-name read changes nothing"), Fixture.Evaluate().bAccepted);
		Fixture.Context.Target.EffectiveWeight = 1000.f;
		TestEqual(TEXT("The weight rule still decides"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TooHeavy));
		AddInfo(TEXT("Combat.Part.GrabLocked is NOT declared in this run (the parts plugin is absent): only the no-op path was exercised"));
	}

	// Either way, an unknown name never makes a tag out of nothing
	TestFalse(TEXT("An unknown name is not a tag"), UGSTN_LinkRule::ContainsTagNamed(FGameplayTagContainer(GSTNCombatTags::State_Busy), TEXT("Combat.Part.DefinitelyNotDeclared")));
	TestTrue(TEXT("A declared name is found"), UGSTN_LinkRule::ContainsTagNamed(FGameplayTagContainer(GSTNCombatTags::State_Busy), TEXT("Combat.State.Busy")));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Latch
// ---------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRuleLatchMinWeightTest, "GSTNLink.Core.Rule.Latch.MinWeight", GSTNLinkTest::Flags)
bool FGSTN_LinkRuleLatchMinWeightTest::RunTest(const FString& Parameters)
{
	FLatchFixture Fixture;
	Fixture.Rule->Rules.MinLatchWeight = 250.f;

	Fixture.Context.Target.EffectiveWeight = 400.f;
	TestTrue(TEXT("A large enemy can be latched onto"), Fixture.Evaluate().bAccepted);
	Fixture.Context.Target.EffectiveWeight = 250.f;
	TestTrue(TEXT("A victim exactly at the minimum is latched"), Fixture.Evaluate().bAccepted);
	Fixture.Context.Target.EffectiveWeight = 249.5f;
	TestEqual(TEXT("A hair under the minimum is too light"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TooLight));
	Fixture.Context.Target.EffectiveWeight = 100.f;
	TestEqual(TEXT("A standard fighter is too light"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TooLight));

	// The kit decides the minimum
	FGSTN_LatchRules LightKit;
	LightKit.MinLatchWeight = 80.f;
	Fixture.Context.InitiatorLatchRules = &LightKit;
	TestTrue(TEXT("A kit with a lower minimum latches the standard fighter"), Fixture.Evaluate().bAccepted);

	// The grab and the latch cover different weights: one victim is never both, and a middle weight is neither when the gap is open
	UGSTN_LinkTypeDefinition* GrabDefinition = GSTNLinkTest::MakeGrabDefinition();
	UGSTN_LinkRule_Grab* GrabRule = NewObject<UGSTN_LinkRule_Grab>(GrabDefinition);
	FGSTN_LinkRequestContext GrabContext = GSTNLinkTest::MakeContext(GrabDefinition);
	Fixture.Context.InitiatorLatchRules = nullptr;
	for (const float Weight : { 60.f, 100.f, 120.f, 180.f, 249.f, 250.f, 400.f })
	{
		GrabContext.Target.EffectiveWeight = Weight;
		Fixture.Context.Target.EffectiveWeight = Weight;
		const bool bGrab = GrabRule->Evaluate(GrabContext).bAccepted;
		const bool bLatch = Fixture.Evaluate().bAccepted;
		TestFalse(FString::Printf(TEXT("Weight %.0f is never both grabbable and latchable"), Weight), bGrab && bLatch);
		TestTrue(FString::Printf(TEXT("Weight %.0f: grab is weight <= 120"), Weight), bGrab == (Weight <= 120.f));
		TestTrue(FString::Printf(TEXT("Weight %.0f: latch is weight >= 250"), Weight), bLatch == (Weight >= 250.f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRuleLatchImmunityTest, "GSTNLink.Core.Rule.Latch.ImmunityLatch", GSTNLinkTest::Flags)
bool FGSTN_LinkRuleLatchImmunityTest::RunTest(const FString& Parameters)
{
	FLatchFixture Fixture;
	TestTrue(TEXT("A large enemy without immunity is latched"), Fixture.Evaluate().bAccepted);

	// A boss that must not be ridden
	Fixture.Context.Target.Tags.AddTag(GSTNLinkTags::Immunity_Latch);
	TestEqual(TEXT("The latch immunity refuses"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::Requirements));

	// The grab immunity is another thing: a boss can be un-grabbable and still ridden
	FLatchFixture GrabImmune;
	GrabImmune.Context.Target.Tags.AddTag(GSTNLinkTags::Immunity_Grab);
	TestTrue(TEXT("The grab immunity does not stop a latch"), GrabImmune.Evaluate().bAccepted);

	// The latch has its own immunity frames
	FLatchFixture Frames;
	Frames.Context.Frame = 100;
	Frames.Context.Target.LatchImmunityEndFrame = 120;
	TestEqual(TEXT("Inside the latch immunity frames: refused"), Refusal(Frames.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::Immunity));
	Frames.Context.Frame = 120;
	TestTrue(TEXT("After them: accepted"), Frames.Evaluate().bAccepted);
	Frames.Context.Target.GrabImmunityEndFrame = 9999;
	TestTrue(TEXT("The grab's immunity frames do not hold off a latch"), Frames.Evaluate().bAccepted);

	// The parts gate is optional by data: off by default, honoured when asked, read by name
	const FGameplayTag GrabLocked = FGameplayTag::RequestGameplayTag(TEXT("Combat.Part.GrabLocked"), false);
	FLatchFixture Gate;
	if (GrabLocked.IsValid())
	{
		Gate.Context.Target.Tags.AddTag(GrabLocked);
		TestTrue(TEXT("By default a latch does not care about the grab gate"), Gate.Evaluate().bAccepted);
		Gate.Rule->Rules.bRespectGrabLocked = true;
		TestEqual(TEXT("With the option on, the gate refuses"), Refusal(Gate.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::GrabLocked));
	}
	else
	{
		Gate.Rule->Rules.bRespectGrabLocked = true;
		TestTrue(TEXT("Without the parts plugin the option is a no-op"), Gate.Evaluate().bAccepted);
	}

	// The default requirements are those of the latch rules
	const FGSTN_LatchRules Defaults;
	TestTrue(TEXT("Defaults ignore Linked"), Defaults.Requirements.IgnoreTags.HasTagExact(GSTNLinkTags::Linked.GetTag()));
	TestTrue(TEXT("Defaults ignore the latch immunity"), Defaults.Requirements.IgnoreTags.HasTagExact(GSTNLinkTags::Immunity_Latch.GetTag()));
	TestFalse(TEXT("Defaults do not ignore the grab immunity"), Defaults.Requirements.IgnoreTags.HasTagExact(GSTNLinkTags::Immunity_Grab.GetTag()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRuleLatchOnePerPartnerTest, "GSTNLink.Core.Rule.Latch.OnePerPartner", GSTNLinkTest::Flags)
bool FGSTN_LinkRuleLatchOnePerPartnerTest::RunTest(const FString& Parameters)
{
	FLatchFixture Fixture;
	TestTrue(TEXT("A free enemy is latched"), Fixture.Evaluate().bAccepted);

	// The enemy already carries a latcher: its Body slot is taken
	Fixture.Context.Target.Links.Body.LinkTypeIndex = 1;
	Fixture.Context.Target.Links.Body.Role = 2;
	Fixture.Context.Target.Links.Body.Phase = EGSTN_LinkPhase::Active;
	Fixture.Context.Target.Links.Body.PartnerId = FGSTN_EntityRef(GSTNEntityKinds::Fighter, 9, 1);
	const FGSTN_LinkRuleResult Second = Fixture.Evaluate();
	TestFalse(TEXT("A second latcher is refused"), Second.bAccepted);
	TestEqual(TEXT("The enemy is busy"), Refusal(Second), static_cast<int32>(EGSTN_LinkRefusal::TargetBusy));

	// A pending latch counts too, so two simultaneous requests cannot both win
	Fixture.Context.Target.Links.Body.Phase = EGSTN_LinkPhase::Pending;
	TestEqual(TEXT("A pending latcher blocks the second"), Refusal(Fixture.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::TargetBusy));

	// A latcher that is already latched somewhere cannot latch again
	FLatchFixture Latcher;
	Latcher.Context.Initiator.Links.Body.Phase = EGSTN_LinkPhase::Active;
	TestEqual(TEXT("The latcher is busy"), Refusal(Latcher.Evaluate()), static_cast<int32>(EGSTN_LinkRefusal::InitiatorBusy));

	// Once the first latcher is gone the enemy can be latched again
	Fixture.Context.Target.Links.Body.Reset();
	TestTrue(TEXT("After the latcher left, a new one is accepted"), Fixture.Evaluate().bAccepted);

	// The Hand slot of the enemy is not the latch's: a held object on it does not count
	Fixture.Context.Target.Links.Hand.Phase = EGSTN_LinkPhase::Active;
	TestTrue(TEXT("Another channel's link does not block a latch"), Fixture.Evaluate().bAccepted);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
