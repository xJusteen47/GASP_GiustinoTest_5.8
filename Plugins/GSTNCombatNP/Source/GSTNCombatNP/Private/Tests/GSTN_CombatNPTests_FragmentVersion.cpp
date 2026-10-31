// Automation tests of the connect-time fragment agreement (FGSTN_FragmentNetworkVersion): the signature of the registered sync
// fragments and the network version it is folded into. A real connect between two processes with different plugin sets is a PIE
// or packaged check; these pin the rule both sides apply.

#include "Misc/AutomationTest.h"
#include "Misc/NetworkVersion.h"
#include "Simulation/GSTN_FragmentNetworkVersion.h"
#include "Sync/GSTN_SyncFragment.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FragmentSignatureTest, "GSTNCombat.NP.FragmentVersion.Signature", GSTN_NP_TEST_FLAGS)

bool FGSTN_FragmentSignatureTest::RunTest(const FString& Parameters)
{
	using FEntry = TPair<uint8, FString>;
	const FEntry Parts(GSTNFeatureIds::Parts, TEXT("/Script/GSTNPartsNP.GSTN_PartsFragment"));
	const FEntry Defence(GSTNFeatureIds::Defence, TEXT("/Script/GSTNDefenceNP.GSTN_DefenceFragment"));
	const FEntry Link(GSTNFeatureIds::Link, TEXT("/Script/GSTNLinkNP.GSTN_LinkFragment"));

	const uint32 All = FGSTN_SyncFragmentRegistry::ComputeSignature({Parts, Defence, Link});
	TestEqual(TEXT("nothing registered: 0"), FGSTN_SyncFragmentRegistry::ComputeSignature({}), 0u);
	TestNotEqual(TEXT("a non-empty set is never 0"), All, 0u);
	TestEqual(TEXT("independent of the registration (load) order"), FGSTN_SyncFragmentRegistry::ComputeSignature({Link, Parts, Defence}), All);
	TestNotEqual(TEXT("one plugin missing (GSTNLink off) changes it"), FGSTN_SyncFragmentRegistry::ComputeSignature({Parts, Defence}), All);
	TestNotEqual(TEXT("the same id with another struct changes it"),
		FGSTN_SyncFragmentRegistry::ComputeSignature({Parts, Defence, FEntry(GSTNFeatureIds::Link, TEXT("/Script/Other.Other_Fragment"))}), All);
	TestNotEqual(TEXT("the same struct under another id changes it"),
		FGSTN_SyncFragmentRegistry::ComputeSignature({Parts, Defence, FEntry(GSTNFeatureIds::FirstGame, Link.Value)}), All);
	TestEqual(TEXT("test ids do not count (automation registers them while a session may run)"),
		FGSTN_SyncFragmentRegistry::ComputeSignature({Parts, Defence, Link, FEntry(GSTNFeatureIds::TestA, TEXT("/Script/X.Test"))}), All);

	// The live registry follows the same rule
	const uint32 Live = FGSTN_SyncFragmentRegistry::GetSignature();
	TArray<FEntry> LiveEntries;
	for (int32 Id = 1; Id < GSTNFeatureIds::FirstTest; ++Id)
	{
		if (const UScriptStruct* Struct = FGSTN_SyncFragmentRegistry::FindStruct(static_cast<uint8>(Id)))
		{
			LiveEntries.Emplace(static_cast<uint8>(Id), Struct->GetPathName());
		}
	}
	TestEqual(TEXT("GetSignature = ComputeSignature of the registered pairs"), Live, FGSTN_SyncFragmentRegistry::ComputeSignature(LiveEntries));
	AddInfo(FString::Printf(TEXT("this build: signature 0x%08x, %s"), Live, *FGSTN_SyncFragmentRegistry::Describe()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_FragmentNetworkVersionTest, "GSTNCombat.NP.FragmentVersion.NetworkVersion", GSTN_NP_TEST_FLAGS)

bool FGSTN_FragmentNetworkVersionTest::RunTest(const FString& Parameters)
{
	const uint32 Base = 0x1234ABCDu;
	const uint32 WithLink = FGSTN_SyncFragmentRegistry::ComputeSignature({{GSTNFeatureIds::Parts, TEXT("/Script/A.P")}, {GSTNFeatureIds::Link, TEXT("/Script/A.L")}});
	const uint32 WithoutLink = FGSTN_SyncFragmentRegistry::ComputeSignature({{GSTNFeatureIds::Parts, TEXT("/Script/A.P")}});

	TestEqual(TEXT("no fragments: the engine/project version unchanged"), FGSTN_FragmentNetworkVersion::Combine(Base, 0), Base);
	TestNotEqual(TEXT("fragments change the version"), FGSTN_FragmentNetworkVersion::Combine(Base, WithLink), Base);
	TestEqual(TEXT("same base and set: same version"), FGSTN_FragmentNetworkVersion::Combine(Base, WithLink), FGSTN_FragmentNetworkVersion::Combine(Base, WithLink));
	TestFalse(TEXT("a client without GSTNLink is incompatible with a server that has it"),
		FGSTN_FragmentNetworkVersion::IsCompatible(FGSTN_FragmentNetworkVersion::Combine(Base, WithLink), FGSTN_FragmentNetworkVersion::Combine(Base, WithoutLink), false));
	TestTrue(TEXT("equal sets are compatible"),
		FGSTN_FragmentNetworkVersion::IsCompatible(FGSTN_FragmentNetworkVersion::Combine(Base, WithLink), FGSTN_FragmentNetworkVersion::Combine(Base, WithLink), false));

	// Installed by the module: the live local version is the engine's base combined with the live signature, and the engine's own
	// compatibility check uses the same rule
	const uint32 Expected = FGSTN_FragmentNetworkVersion::Combine(FGSTN_FragmentNetworkVersion::GetBaseVersion(), FGSTN_SyncFragmentRegistry::GetSignature());
	TestEqual(TEXT("the live network version carries the fragment signature"), FNetworkVersion::GetLocalNetworkVersion(), Expected);
	AddExpectedError(TEXT("Network version mismatch"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("the engine check refuses a different signature"), FNetworkVersion::IsNetworkCompatible(Expected, FGSTN_FragmentNetworkVersion::Combine(Expected, 7u)));
	TestTrue(TEXT("the engine check accepts the same version"), FNetworkVersion::IsNetworkCompatible(Expected, Expected));
	return true;
}

#endif
