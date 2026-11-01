// Automation tests of the link plugin's ability-system half: the registry, the interact provider, the anim selection (plan F9).

#include "Animation/AnimInstance.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_CombatCoreTags.h"
#include "GSTN_InteractMath.h"
#include "GSTN_InteractRegistry.h"
#include "GSTN_InteractTags.h"
#include "GSTN_LinkAnimSelection.h"
#include "GSTN_LinkInteractProvider.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkRequirementLibrary.h"
#include "GSTN_LinkTags.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Rules/GSTN_LinkRule_Latch.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNLinkGASTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	// A link type that GSTNLink does not declare, as a plugin built on links would ship it (m6 spec 7.0 item 3 acceptance). Test builds
	// only; the names say so, and no production data can name them.
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestType, "Combat.Link.Type.GSTNTestOnly");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestRoleA, "Combat.Link.Role.GSTNTestOnlyA");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestRoleB, "Combat.Link.Role.GSTNTestOnlyB");
	// Two plugin break reasons, registered in the opposite of their name order
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestReasonA, "Combat.Link.BreakReason.GSTNTestOnlyA");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TestReasonB, "Combat.Link.BreakReason.GSTNTestOnlyB");

	UGSTN_LinkTypeDefinition* MakeDefinition(const FGameplayTag& Type, const FGameplayTag& RoleA, const FGameplayTag& RoleB, bool bParkTarget)
	{
		UGSTN_LinkTypeDefinition* Definition = NewObject<UGSTN_LinkTypeDefinition>(GetTransientPackage());
		Definition->LinkType = Type;
		Definition->Channel = EGSTN_LinkChannel::Body;
		Definition->InitiatorRole.RoleTag = RoleA;
		Definition->TargetRole.RoleTag = RoleB;
		Definition->TargetRole.bParked = bParkTarget;
		return Definition;
	}

	UGSTN_LinkTypeDefinition* MakeGrab()
	{
		UGSTN_LinkTypeDefinition* Definition = MakeDefinition(GSTNLinkTags::Type_Grab, GSTNLinkTags::Role_Grabber, GSTNLinkTags::Role_Grabbed, true);
		Definition->Rules.Add(NewObject<UGSTN_LinkRule_Grab>(Definition));
		return Definition;
	}

	UGSTN_LinkTypeDefinition* MakeLatch()
	{
		UGSTN_LinkTypeDefinition* Definition = MakeDefinition(GSTNLinkTags::Type_Latch, GSTNLinkTags::Role_Latcher, GSTNLinkTags::Role_Latched, false);
		Definition->InitiatorRole.bParked = true;
		Definition->InitiatorRole.bActsWhileLinked = true;
		Definition->TargetRole.bActsWhileLinked = true;
		Definition->Rules.Add(NewObject<UGSTN_LinkRule_Latch>(Definition));
		return Definition;
	}

	UGSTN_LinkTypeDefinition* MakeTestOnly()
	{
		return MakeDefinition(TestType, TestRoleA, TestRoleB, true);
	}

	FGSTN_KitLinkAnims MakeAnims(const TSoftClassPtr<UAnimInstance>& Layer)
	{
		FGSTN_KitLinkAnims Anims;
		Anims.AnimLayer = Layer;
		return Anims;
	}

	UGSTN_LinkRegistry* NewRegistry()
	{
		return NewObject<UGSTN_LinkRegistry>(GetTransientPackage());
	}

	/** Every Interact.Target tag in a container */
	int32 CountTargetTags(const FGameplayTagContainer& Tags)
	{
		int32 Count = 0;
		for (const FGameplayTag& Tag : Tags)
		{
			Count += Tag.MatchesTag(GSTNInteractTags::Root_Target) ? 1 : 0;
		}
		return Count;
	}

	/** An interactor at the origin facing +X, a grab offer and entities placed around it; the probe runs over the provider */
	struct FProbeRig
	{
		UGSTN_LinkRegistry* Registry = NewRegistry();
		UGSTN_LinkTypeDefinition* Grab = MakeGrab();
		FGSTN_LinkKitExtension Kit;
		TArray<FGSTN_LinkInteractOffer> Offers;
		TArray<FGSTN_LinkInteractEntity> Entities;
		FGSTN_LinkInteractSource Source;
		FGSTN_LinkInteractProvider Provider;
		FGSTN_InteractRules Rules;
		FGSTN_InteractRegistry Records;
		FGSTN_InteractMeaningTable Meanings;
		FGSTN_InteractFocusState State;
		FGameplayTagContainer InteractorTags;
		FVector Origin = FVector::ZeroVector;
		FVector Facing = FVector::ForwardVector;
		int32 Frame = 100;

		FProbeRig()
		{
			Registry->Build({ Grab });
			Offers.Add(FGSTN_LinkInteractProvider::MakeGrabOffer(*Registry, &Kit));

			// Grab first, then the generic root as a second meaning (a stand-in for a pickup record of the registry)
			FGSTN_InteractMeaning GrabMeaning;
			GrabMeaning.Tag = GSTNLinkTags::Interact_Target_Grab;
			FGSTN_InteractMeaning OtherMeaning;
			OtherMeaning.Tag = GSTNInteractTags::Root_Target;
			Meanings.Build({ GrabMeaning, OtherMeaning });

			Records.Configure(Rules.HardMaxReachCm, Rules.MaxRecords);
			Source.Interactor.EntityId = 1;
			Source.Registry = Registry;
		}

		FGSTN_LinkInteractEntity& AddEntity(int32 Id, const FVector& Location)
		{
			FGSTN_LinkInteractEntity& Entity = Entities.AddDefaulted_GetRef();
			Entity.Side.EntityId = Id;
			Entity.Location = Location;
			Entity.RadiusCm = 30;
			return Entity;
		}

		FGSTN_InteractQuery MakeQuery()
		{
			Source.Entities = Entities;
			Source.Offers = Offers;
			Providers.Reset();
			Providers.Add(&Provider);

			FGSTN_InteractQuery Query;
			Query.Frame = Frame;
			Query.Origin = Origin;
			Query.Facing = Facing;
			Query.Tags = &InteractorTags;
			Query.Rules = &Rules;
			Query.Registry = &Records;
			Query.Meanings = &Meanings;
			Query.Providers = Providers;
			Query.HostContext = Source.ToHostContext();
			return Query;
		}

		TArray<FGSTN_InteractCandidate> Gather()
		{
			TArray<FGSTN_InteractCandidate> Candidates;
			Provider.GatherCandidates(MakeQuery(), Candidates);
			return Candidates;
		}

		bool IsOffered(const TArray<FGSTN_InteractCandidate>& Candidates, int32 Id) const
		{
			return Candidates.ContainsByPredicate([Id](const FGSTN_InteractCandidate& Candidate) { return Candidate.Id.Handle == Id; });
		}

	private:
		TArray<const IGSTN_InteractCandidateProvider*> Providers;
	};
}

using namespace GSTNLinkGASTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRegistryStableIndicesTest, "GSTNLink.GAS.Registry.IndicesStableAcrossLoadOrder", GSTNLinkGASTest::Flags)
bool FGSTN_LinkRegistryStableIndicesTest::RunTest(const FString& Parameters)
{
	UGSTN_LinkTypeDefinition* Grab = MakeGrab();
	UGSTN_LinkTypeDefinition* Latch = MakeLatch();
	UGSTN_LinkTypeDefinition* TestOnly = MakeTestOnly();

	// The same set in three orders, through the code registration a test module or another plugin uses (no GSTNLink change)
	const TArray<TArray<UGSTN_LinkTypeDefinition*>> Orders = { { Grab, Latch, TestOnly }, { TestOnly, Latch, Grab }, { Latch, TestOnly, Grab } };
	TArray<UGSTN_LinkRegistry*> Registries;
	for (const TArray<UGSTN_LinkTypeDefinition*>& Order : Orders)
	{
		UGSTN_LinkRegistry* Registry = NewRegistry();
		for (UGSTN_LinkTypeDefinition* Definition : Order)
		{
			Registry->AddDefinition(Definition);
		}
		Registries.Add(Registry);
	}

	const UGSTN_LinkRegistry& First = *Registries[0];
	TestEqual(TEXT("three link types"), First.Num(), 3);
	TestTrue(TEXT("no build error"), First.GetBuildErrors().IsEmpty());
	for (const UGSTN_LinkRegistry* Registry : Registries)
	{
		for (const FGameplayTag& Type : { GSTNLinkTags::Type_Grab.GetTag(), GSTNLinkTags::Type_Latch.GetTag(), TestType.GetTag() })
		{
			TestEqual(FString::Printf(TEXT("%s has the same index whatever the order"), *Type.ToString()), Registry->GetTypeIndex(Type), First.GetTypeIndex(Type));
		}
		TestEqual(TEXT("same signature whatever the order"), Registry->GetSignature(), First.GetSignature());
	}

	// The index is the rank of the tag name (case-sensitive): GSTNTestOnly < Grab < Latch
	TestEqual(TEXT("test-only type is row 1"), static_cast<int32>(First.GetTypeIndex(TestType)), 1);
	TestEqual(TEXT("grab is row 2"), static_cast<int32>(First.GetTypeIndex(GSTNLinkTags::Type_Grab)), 2);
	TestEqual(TEXT("latch is row 3"), static_cast<int32>(First.GetTypeIndex(GSTNLinkTags::Type_Latch)), 3);
	TestTrue(TEXT("index 0 is no link"), First.GetDefinition(0) == nullptr);

	// Acceptance (m6 spec 7.0 item 3): the test-only type gets an index and its roles from its definition alone
	const uint8 TestIndex = First.GetTypeIndex(TestType);
	TestTrue(TEXT("test-only role 1"), First.GetRoleTag(TestIndex, 1) == TestRoleA.GetTag());
	TestTrue(TEXT("test-only role 2"), First.GetRoleTag(TestIndex, 2) == TestRoleB.GetTag());
	TestEqual(TEXT("role tag to role index"), First.FindRoleIndex(TestIndex, TestRoleB), 2);
	TestEqual(TEXT("parked role of the test-only type"), First.GetEntry(TestIndex)->ParkedRoleIndex, 2);
	TestTrue(TEXT("its definition is the registered one"), First.GetDefinition(TestIndex) == TestOnly);

	// Parked roles that do not act are what the base ability blocks on: the grabbed and the test-only target, not the latcher
	FGameplayTagContainer Blocking;
	UGSTN_LinkRequirementLibrary::GetActionBlockingRoleTags(First, Blocking);
	TestTrue(TEXT("grabbed blocks"), Blocking.HasTagExact(GSTNLinkTags::Role_Grabbed));
	TestTrue(TEXT("test-only parked role blocks"), Blocking.HasTagExact(TestRoleB));
	TestFalse(TEXT("latcher acts"), Blocking.HasTagExact(GSTNLinkTags::Role_Latcher));
	TestFalse(TEXT("grabber acts"), Blocking.HasTagExact(GSTNLinkTags::Role_Grabber));

	// Removing the test-only type shifts the others by the same rule on every machine; the signature tells the sets apart
	UGSTN_LinkRegistry* Without = Registries[1];
	Without->RemoveDefinition(TestOnly);
	TestEqual(TEXT("grab is row 1 without the test-only type"), static_cast<int32>(Without->GetTypeIndex(GSTNLinkTags::Type_Grab)), 1);
	TestTrue(TEXT("a different set has a different signature"), Without->GetSignature() != First.GetSignature());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRegistryUnknownTagTest, "GSTNLink.GAS.Registry.UnknownTagSkipped", GSTNLinkGASTest::Flags)
bool FGSTN_LinkRegistryUnknownTagTest::RunTest(const FString& Parameters)
{
	// A type whose plugin is missing: its tags do not resolve, so the definition holds empty tags, as a loaded asset does
	UGSTN_LinkTypeDefinition* MissingType = MakeDefinition(FGameplayTag::RequestGameplayTag(TEXT("Combat.Link.Type.GSTNNotRegistered"), false),
		TestRoleA, TestRoleB, true);
	UGSTN_LinkTypeDefinition* MissingRole = MakeDefinition(TestType, TestRoleA,
		FGameplayTag::RequestGameplayTag(TEXT("Combat.Link.Role.GSTNNotRegistered"), false), true);
	UGSTN_LinkTypeDefinition* Grab = MakeGrab();
	UGSTN_LinkTypeDefinition* SecondGrab = MakeGrab();

	TArray<FString> Errors;
	MissingType->Validate(Errors);
	TestTrue(TEXT("the definition's own validation (IsDataValid) reports the missing type tag"), Errors.Num() > 0);
	Errors.Reset();

	AddExpectedMessagePlain(TEXT("Link registry: Skipped"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 0);
	UGSTN_LinkRegistry* Registry = NewRegistry();
	Registry->Build({ MissingType, Grab, MissingRole, SecondGrab }, &Errors);

	TestEqual(TEXT("only the grab is indexed"), Registry->Num(), 1);
	TestEqual(TEXT("the grab is row 1"), static_cast<int32>(Registry->GetTypeIndex(GSTNLinkTags::Type_Grab)), 1);
	TestEqual(TEXT("the test-only type with a missing role is not indexed"), static_cast<int32>(Registry->GetTypeIndex(TestType)), 0);
	TestTrue(TEXT("errors name the missing type's definition"), Errors.ContainsByPredicate([MissingType](const FString& Error) { return Error.Contains(MissingType->GetName()); }));
	TestTrue(TEXT("errors name the missing role's definition"), Errors.ContainsByPredicate([MissingRole](const FString& Error) { return Error.Contains(MissingRole->GetName()); }));
	TestTrue(TEXT("the duplicate grab is reported"), Errors.ContainsByPredicate([](const FString& Error) { return Error.Contains(TEXT("already defined")); }));
	TestEqual(TEXT("the errors are kept on the registry"), Registry->GetBuildErrors().Num(), Errors.Num());
	TestTrue(TEXT("the duplicate keeps the definition whose path sorts first"),
		Registry->GetDefinition(1) == (Grab->GetPathName().Compare(SecondGrab->GetPathName(), ESearchCase::CaseSensitive) < 0 ? Grab : SecondGrab));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkRegistryReasonAppendTest, "GSTNLink.GAS.Registry.ReasonTableAppend", GSTNLinkGASTest::Flags)
bool FGSTN_LinkRegistryReasonAppendTest::RunTest(const FString& Parameters)
{
	// Generic reasons keep the enum's indices, so the core's break math and the registry agree without a lookup
	FGSTN_LinkReasonTable Table;
	TestEqual(TEXT("Escape is the enum's index"), static_cast<int32>(Table.IndexOf(GSTNLinkTags::BreakReason_Escape)), static_cast<int32>(EGSTN_LinkBreakReason::Escape));
	TestTrue(TEXT("index of Forced is its tag"), Table.TagOf(static_cast<uint8>(EGSTN_LinkBreakReason::Forced)) == GSTNLinkTags::BreakReason_Forced.GetTag());
	TestFalse(TEXT("the gap below FirstPlugin has no tag"), Table.TagOf(static_cast<uint8>(EGSTN_LinkBreakReason::FirstPlugin) - 1).IsValid());
	TestEqual(TEXT("appending a generic reason returns its fixed index"), static_cast<int32>(Table.Append(GSTNLinkTags::BreakReason_Hit)), static_cast<int32>(EGSTN_LinkBreakReason::Hit));
	TestEqual(TEXT("an invalid tag is refused"), static_cast<int32>(Table.Append(FGameplayTag())), 0);
	TestEqual(TEXT("no plugin reason yet"), Table.NumPluginReasons(), 0);

	// Plugin reasons from FirstPlugin on, ordered by name whatever the append order
	FGSTN_LinkReasonTable Forward;
	Forward.Append(TestReasonA);
	Forward.Append(TestReasonB);
	FGSTN_LinkReasonTable Backward;
	Backward.Append(TestReasonB);
	Backward.Append(TestReasonA);
	const int32 First = static_cast<int32>(EGSTN_LinkBreakReason::FirstPlugin);
	TestEqual(TEXT("A is FirstPlugin"), static_cast<int32>(Forward.IndexOf(TestReasonA)), First);
	TestEqual(TEXT("B is FirstPlugin + 1"), static_cast<int32>(Forward.IndexOf(TestReasonB)), First + 1);
	TestEqual(TEXT("A has the same index in the other order"), static_cast<int32>(Backward.IndexOf(TestReasonA)), First);
	TestEqual(TEXT("B has the same index in the other order"), static_cast<int32>(Backward.IndexOf(TestReasonB)), First + 1);
	TestTrue(TEXT("index to tag"), Backward.TagOf(static_cast<uint8>(First + 1)) == TestReasonB.GetTag());
	TestEqual(TEXT("appending twice changes nothing"), static_cast<int32>(Backward.Append(TestReasonA)), First);
	TestEqual(TEXT("two plugin reasons"), Backward.NumPluginReasons(), 2);

	// The registry appends the names plugins registered from their startup, at every build, and its signature covers them
	UGSTN_LinkRegistry::RegisterPluginReason(TestReasonB.GetTag().GetTagName());
	UGSTN_LinkRegistry::RegisterPluginReason(TEXT("Combat.Link.BreakReason.GSTNNotRegistered"));
	UGSTN_LinkRegistry* Registry = NewRegistry();
	Registry->Build({ MakeGrab() });
	const uint32 SignatureWithB = Registry->GetSignature();
	TestEqual(TEXT("the registered reason is in the registry's table"), static_cast<int32>(Registry->GetReasons().IndexOf(TestReasonB)), First);
	TestEqual(TEXT("a name no plugin declares is skipped"), Registry->GetReasons().NumPluginReasons(), 1);
	TestEqual(TEXT("an appended reason sorts before B"), static_cast<int32>(Registry->AppendReason(TestReasonA)), First);
	TestEqual(TEXT("B moved after A"), static_cast<int32>(Registry->GetReasons().IndexOf(TestReasonB)), First + 1);
	TestTrue(TEXT("the signature covers the reasons"), Registry->GetSignature() != SignatureWithB);
	UGSTN_LinkRegistry::UnregisterPluginReason(TestReasonB.GetTag().GetTagName());
	UGSTN_LinkRegistry::UnregisterPluginReason(TEXT("Combat.Link.BreakReason.GSTNNotRegistered"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkInteractFiltersTest, "GSTNLink.GAS.Interact.ProviderFiltersUngrabbable", GSTNLinkGASTest::Flags)
bool FGSTN_LinkInteractFiltersTest::RunTest(const FString& Parameters)
{
	FProbeRig Rig;
	Rig.Kit.GrabRules.MaxGrabWeight = 120.f;

	Rig.AddEntity(2, FVector(120.f, 0.f, 0.f));                                                    // plain enemy: grabbable
	Rig.AddEntity(3, FVector(60.f, 0.f, 0.f)).Side.Tags.AddTag(GSTNLinkTags::Immunity_Grab);       // a boss, dead ahead and nearer
	Rig.AddEntity(4, FVector(70.f, 10.f, 0.f)).Side.EffectiveWeight = 400.f;                       // too heavy for this kit
	Rig.AddEntity(5, FVector(70.f, -10.f, 0.f)).Side.GrabImmunityEndFrame = Rig.Frame + 10;        // just released
	Rig.AddEntity(6, FVector(80.f, 20.f, 0.f)).Side.Tags.AddTag(GSTNLinkTags::Lock_Link);          // refuses every link
	FGSTN_LinkInteractEntity& Busy = Rig.AddEntity(7, FVector(80.f, -20.f, 0.f));                  // already grabbed by someone
	Busy.Side.Links.Body.LinkTypeIndex = 1;
	Busy.Side.Links.Body.Role = 2;
	Busy.Side.Links.Body.Phase = EGSTN_LinkPhase::Active;
	Rig.AddEntity(8, FVector(1000.f, 0.f, 0.f));                                                   // out of every reach
	Rig.AddEntity(1, FVector(10.f, 0.f, 0.f));                                                     // the interactor itself
	const FGameplayTag GrabLocked = FGameplayTag::RequestGameplayTag(TEXT("Combat.Part.GrabLocked"), false);
	if (GrabLocked.IsValid())
	{
		Rig.AddEntity(9, FVector(65.f, 5.f, 0.f)).Side.Tags.AddTag(GrabLocked);                    // parts plugin: needs a part broken
	}

	const TArray<FGSTN_InteractCandidate> Candidates = Rig.Gather();
	TestEqual(TEXT("one grab candidate"), Candidates.Num(), 1);
	TestTrue(TEXT("the plain enemy is offered"), Rig.IsOffered(Candidates, 2));
	if (Candidates.Num() == 1)
	{
		TestEqual(TEXT("provider id"), static_cast<int32>(Candidates[0].Id.Provider), static_cast<int32>(GSTNInteractProviders::LinkEntities));
		TestEqual(TEXT("grab meaning"), Candidates[0].MeaningIndex, Rig.Meanings.IndexOf(GSTNLinkTags::Interact_Target_Grab));
		TestEqual(TEXT("reach is the kit's InteractReachCm"), Candidates[0].ReachOverrideCm, FMath::RoundToInt(Rig.Kit.GrabRules.InteractReachCm));
	}

	// The refusals are the rules' own, so the probe and the LinkRequest step cannot disagree
	FGSTN_LinkInteractSource& Source = Rig.Source;
	auto RefusalOf = [&Source, &Rig](int32 EntityIndex)
	{
		FGSTN_LinkRuleResult Refusal;
		FGSTN_LinkInteractProvider::FindAcceptedOffer(Source, Rig.Entities[EntityIndex], Rig.Frame, &Refusal);
		return Refusal.Refusal;
	};
	TestTrue(TEXT("immunity by tag"), RefusalOf(1) == EGSTN_LinkRefusal::Requirements);
	TestTrue(TEXT("weight"), RefusalOf(2) == EGSTN_LinkRefusal::TooHeavy);
	TestTrue(TEXT("immunity frames"), RefusalOf(3) == EGSTN_LinkRefusal::Immunity);
	TestTrue(TEXT("Combat.Lock.Link"), RefusalOf(4) == EGSTN_LinkRefusal::TargetLocked);
	TestTrue(TEXT("busy body channel"), RefusalOf(5) == EGSTN_LinkRefusal::TargetBusy);
	if (GrabLocked.IsValid())
	{
		TestTrue(TEXT("part gate"), RefusalOf(8) == EGSTN_LinkRefusal::GrabLocked);
	}

	// The probe focuses the grabbable one although an ungrabbable boss stands nearer and dead ahead, and projects the grab on it only
	FGameplayTagContainer Projected;
	const FGSTN_InteractFocusResult Focus = UGSTN_InteractMath::UpdateFocus(Rig.MakeQuery(), Rig.State);
	TestTrue(TEXT("focus is the grabbable enemy"), Focus.FocusId == FGSTN_InteractCandidateId(GSTNInteractProviders::LinkEntities, 2));
	FGSTN_LinkInteractProvider::ProjectTargetTag(Focus, Rig.Meanings, Projected);
	TestTrue(TEXT("Interact.Target.Grab projected"), Projected.HasTagExact(GSTNLinkTags::Interact_Target_Grab));

	// Once the grabbable one becomes immune too, nothing is offered and the tag goes
	Rig.Entities[0].Side.Tags.AddTag(GSTNLinkTags::Immunity_Grab);
	Rig.Frame += 1;
	const FGSTN_InteractFocusResult None = UGSTN_InteractMath::UpdateFocus(Rig.MakeQuery(), Rig.State);
	FGSTN_LinkInteractProvider::ProjectTargetTag(None, Rig.Meanings, Projected);
	TestFalse(TEXT("no focus on an ungrabbable crowd"), None.HasFocus());
	TestEqual(TEXT("no Interact.Target tag on an ungrabbable crowd"), CountTargetTags(Projected), 0);

	// Data-driven per type: a latch offer (its own rule) picks up the heavy enemy the grab refused, under its own meaning
	FGSTN_LinkInteractOffer LatchOffer;
	LatchOffer.Definition = MakeLatch();
	LatchOffer.Meaning = GSTNInteractTags::Root_Target;
	Rig.Offers.Add(LatchOffer);
	const TArray<FGSTN_InteractCandidate> WithLatch = Rig.Gather();
	TestTrue(TEXT("the heavy enemy is a latch candidate"), WithLatch.ContainsByPredicate([&Rig](const FGSTN_InteractCandidate& Candidate)
	{
		return Candidate.Id.Handle == 4 && Candidate.MeaningIndex == Rig.Meanings.IndexOf(GSTNInteractTags::Root_Target);
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkInteractOneTagTest, "GSTNLink.GAS.Interact.OneTargetTagPerFrame", GSTNLinkGASTest::Flags)
bool FGSTN_LinkInteractOneTagTest::RunTest(const FString& Parameters)
{
	FProbeRig Rig;
	Rig.Rules.MinDwellFrames = 0;
	Rig.AddEntity(2, FVector(100.f, 60.f, 0.f));
	Rig.AddEntity(3, FVector(100.f, -60.f, 0.f));

	// A static record of the other meaning (a pickup) in front, from the probe's own registry
	FGSTN_InteractCandidate Record;
	Record.Id = FGSTN_InteractCandidateId(GSTNInteractProviders::Registry, 77);
	Record.PositionCm = FGSTN_InteractCandidate::QuantiseCm(FVector(110.f, 0.f, 0.f));
	Record.MeaningIndex = Rig.Meanings.IndexOf(GSTNInteractTags::Root_Target);
	TestTrue(TEXT("record added"), Rig.Records.Add(Record));

	// A stale tag from an earlier frame must not survive the projection
	FGameplayTagContainer Tags;
	Tags.AddTag(GSTNLinkTags::Interact_Target_Grab);

	bool bSawGrab = false;
	bool bSawOther = false;
	for (int32 Step = 0; Step < 36; ++Step)
	{
		// Sweep the facing from left to right across the three candidates
		const float Yaw = -60.f + Step * (120.f / 35.f);
		Rig.Facing = FRotator(0.f, Yaw, 0.f).Vector();
		Rig.Frame += 1;

		const FGSTN_InteractFocusResult Focus = UGSTN_InteractMath::UpdateFocus(Rig.MakeQuery(), Rig.State);
		FGSTN_LinkInteractProvider::ProjectTargetTag(Focus, Rig.Meanings, Tags);

		const int32 Count = CountTargetTags(Tags);
		TestTrue(FString::Printf(TEXT("frame %d: at most one Interact.Target tag"), Rig.Frame), Count <= 1);
		TestEqual(FString::Printf(TEXT("frame %d: a tag exactly when the focus is actionable"), Rig.Frame), Count,
			Focus.HasFocus() && Focus.MeaningIndex > 0 ? 1 : 0);
		if (Count == 1)
		{
			const FGameplayTag Meaning = Rig.Meanings.TagOf(Focus.MeaningIndex);
			TestTrue(FString::Printf(TEXT("frame %d: the tag is the focus meaning"), Rig.Frame), Tags.HasTagExact(Meaning));
			bSawGrab |= Meaning == GSTNLinkTags::Interact_Target_Grab;
			bSawOther |= Meaning == GSTNInteractTags::Root_Target;
		}
	}
	TestTrue(TEXT("the sweep focused a grab candidate"), bSawGrab);
	TestTrue(TEXT("the sweep focused the record of the other meaning"), bSawOther);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkAnimByRoleTest, "GSTNLink.GAS.Anim.LayerByRole", GSTNLinkGASTest::Flags)
bool FGSTN_LinkAnimByRoleTest::RunTest(const FString& Parameters)
{
	// Soft classes are paths: nothing is loaded, as on a dedicated server
	const TSoftClassPtr<UAnimInstance> KitGrabbed(FSoftObjectPath(TEXT("/GSTNLinkTest/ABP_KitGrabbed.ABP_KitGrabbed_C")));
	const TSoftClassPtr<UAnimInstance> KitGrabber(FSoftObjectPath(TEXT("/GSTNLinkTest/ABP_KitGrabber.ABP_KitGrabber_C")));
	const TSoftClassPtr<UAnimInstance> WeaponGrabber(FSoftObjectPath(TEXT("/GSTNLinkTest/ABP_WeaponGrabber.ABP_WeaponGrabber_C")));

	UGSTN_PrimaryKitDefinition* Kit = NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage());
	FGSTN_LinkKitExtension KitExtension;
	KitExtension.AnimsByRole.Add(GSTNLinkTags::Role_Grabbed, MakeAnims(KitGrabbed));
	KitExtension.AnimsByRole.Add(GSTNLinkTags::Role_Grabber, MakeAnims(KitGrabber));
	Kit->Extensions.ExtensionData.Add(FInstancedStruct::Make(KitExtension));

	UGSTN_WeaponDefinition* Weapon = NewObject<UGSTN_WeaponDefinition>(GetTransientPackage());
	FGSTN_LinkKitExtension WeaponExtension;
	WeaponExtension.AnimsByRole.Add(GSTNLinkTags::Role_Grabber, MakeAnims(WeaponGrabber));
	Weapon->Extensions.ExtensionData.Add(FInstancedStruct::Make(WeaponExtension));

	TestTrue(TEXT("kit layer of the grabbed"), UGSTN_LinkAnimSelection::SelectRoleAnimLayer(Kit, Weapon, GSTNLinkTags::Role_Grabbed) == KitGrabbed);
	TestTrue(TEXT("the weapon's grabber layer wins"), UGSTN_LinkAnimSelection::SelectRoleAnimLayer(Kit, Weapon, GSTNLinkTags::Role_Grabber) == WeaponGrabber);
	TestTrue(TEXT("the kit's grabber layer without the weapon"), UGSTN_LinkAnimSelection::SelectRoleAnimLayer(Kit, nullptr, GSTNLinkTags::Role_Grabber) == KitGrabber);
	TestTrue(TEXT("a role without an entry plays nothing extra"), UGSTN_LinkAnimSelection::SelectRoleAnimLayer(Kit, Weapon, GSTNLinkTags::Role_Latcher).IsNull());

	// From a link state through the registry: the state carries indices, never a tag
	UGSTN_LinkRegistry* Registry = NewRegistry();
	Registry->Build({ MakeGrab(), MakeLatch() });
	FGSTN_LinkState State;
	State.LinkTypeIndex = Registry->GetTypeIndex(GSTNLinkTags::Type_Grab);
	State.Role = 2;
	State.Phase = EGSTN_LinkPhase::Active;
	TestTrue(TEXT("grabbed state selects the grabbed layer"), UGSTN_LinkAnimSelection::SelectForLinkState(*Registry, State, Kit, Weapon) == KitGrabbed);
	TestTrue(TEXT("an idle state selects nothing"), UGSTN_LinkAnimSelection::SelectForLinkState(*Registry, FGSTN_LinkState(), Kit, Weapon).IsNull());

	TArray<TSoftClassPtr<UAnimInstance>> Layers;
	UGSTN_LinkAnimSelection::GatherRoleAnimLayers(Kit, Weapon, Layers);
	TestEqual(TEXT("three distinct layers to preload"), Layers.Num(), 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
