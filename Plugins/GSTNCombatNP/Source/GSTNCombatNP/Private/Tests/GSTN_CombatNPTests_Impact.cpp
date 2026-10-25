// Automation tests of the surface impacts of a tumbling victim (combat-m4-hits 7b, M4 H11). No world: the contact record is made from a hand
// built hit, the step is the pure function FGSTN_CombatVictim::StepImpact over a sync core, as in the victim tests.

#include "Misc/AutomationTest.h"
#include "Engine/HitResult.h"
#include "GSTN_SurfaceTable.h"
#include "GSTN_SurfaceSettings.h"
#include "GSTN_SurfaceUtils.h"
#include "GSTN_SurfaceTags.h"
#include "Hits/GSTN_ImpactMath.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "Movement/GSTN_LayeredMove_Knockback.h"
#include "Movement/GSTN_TumbleContact.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Rules/GSTN_CombatRules.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatHitState.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	uint8 TypeIndex(const TCHAR* Leaf)
	{
		return UGSTN_SurfaceUtils::GetSurfaceTypeIndex(FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Surface.Type.%s"), Leaf)), false));
	}

	/** Wall (splat 20 frames, min 300), Glass (break only, min 600, residual 0.7), Trampoline (bounce forced, restitution 1.2): the Core test's rows */
	struct FImpactFixture
	{
		TStrongObjectPtr<UGSTN_SurfaceImpactTable> Table;
		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		FGSTN_CombatSyncCore Core;
		FGSTN_ImpactResponse Response;
		FGSTN_ImpactStepInput In;
		FGSTN_ImpactStepResult Out;

		FImpactFixture()
		{
			Table.Reset(NewObject<UGSTN_SurfaceImpactTable>(GetTransientPackage()));
			FGSTN_SurfaceImpactRow Wall;
			Wall.Surface = FGameplayTag::RequestGameplayTag(TEXT("Surface.Type.Wall"));
			Wall.DefaultOutcome = EGSTN_ImpactOutcome::Splat;
			Wall.MinImpactSpeed = 300.f;
			Wall.SplatFrames = 20;
			Wall.Restitution = 0.5f;
			Wall.ImpactDamageScale = 2.f;
			FGSTN_SurfaceImpactRow Glass;
			Glass.Surface = FGameplayTag::RequestGameplayTag(TEXT("Surface.Type.Glass"));
			Glass.DefaultOutcome = EGSTN_ImpactOutcome::BreakSurface;
			Glass.Allowed.bSplat = Glass.Allowed.bBounce = false;
			Glass.MinImpactSpeed = 600.f;
			Glass.ResidualSpeedScale = 0.7f;
			FGSTN_SurfaceImpactRow Trampoline;
			Trampoline.Surface = FGameplayTag::RequestGameplayTag(TEXT("Surface.Type.Trampoline"));
			Trampoline.DefaultOutcome = EGSTN_ImpactOutcome::Bounce;
			Trampoline.bForceDefault = true;
			Trampoline.Restitution = 1.2f;
			Table->Rows = { Wall, Glass, Trampoline };
			Table->Compile();

			Rules.Reset(NewObject<UGSTN_CombatRules>(GetTransientPackage()));
			Rules->BaseImpactDamage = 5.f;

			Core.ActionState = EGSTN_ActionState::Tumble;
			Core.Hit.HitstunEndFrame = 100;
			In.Rules = Rules.Get();
			In.Table = Table.Get();
			In.Response = &Response;
			In.Velocity = FVector(0.0, 0.0, -200.0);
		}

		/** A contact on the face with normal -X (a wall the victim flew into along +X), made on ContactFrame */
		void SetContact(const TCHAR* Leaf, double Speed, int32 ContactFrame, EGSTN_TumbleContactKind Kind = EGSTN_TumbleContactKind::Wall, const FVector& Normal = FVector(-1.0, 0.0, 0.0))
		{
			In.Contact = FGSTN_TumbleContactSyncState();
			In.Contact.Kind = Kind;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				In.Contact.NormalQ[Axis] = static_cast<int16>(FMath::RoundToInt(Normal[Axis] * 32767.0));
			}
			In.Contact.ImpactSpeedQ = static_cast<uint16>(Speed);
			In.Contact.ContactFrame = ContactFrame;
			In.Contact.SurfaceTypeIndex = TypeIndex(Leaf);
		}

		const FGSTN_ImpactStepResult& Step(int32 Frame)
		{
			In.Frame = Frame;
			FGSTN_CombatVictim::StepImpact(In, Core, Out);
			return Out;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactContactRecordWrittenTest, "GSTNCombat.NP.Hits.Impact.ContactRecordWritten", GSTN_NP_TEST_FLAGS)
bool FGSTN_ImpactContactRecordWrittenTest::RunTest(const FString& Parameters)
{
	const FVector Up = FVector::UpVector;
	const FVector Velocity(1000.0, 0.0, -200.0);

	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.ImpactNormal = FVector(-1.0, 0.0, 0.0);
	FGSTN_TumbleContactSyncState Wall = FGSTN_TumbleContactSyncState::FromHit(Hit, Velocity, Up, 42);
	TestTrue(TEXT("a wall is a wall contact"), Wall.Kind == EGSTN_TumbleContactKind::Wall);
	TestEqual(TEXT("the speed into the face"), static_cast<int32>(Wall.ImpactSpeedQ), 1000);
	TestEqual(TEXT("the contact frame"), Wall.ContactFrame, 42);
	TestTrue(TEXT("the normal survives the quantisation"), Wall.GetNormal().Equals(FVector(-1.0, 0.0, 0.0), 1.0e-4));
	TestEqual(TEXT("an unmarked wall resolves to the default surface type"), static_cast<int32>(Wall.SurfaceTypeIndex),
		static_cast<int32>(UGSTN_SurfaceUtils::GetSurfaceTypeIndex(UGSTN_SurfaceSettings::Get().Default.SurfaceType)));

	Hit.ImpactNormal = FVector(0.0, 0.0, 1.0);
	TestTrue(TEXT("a face along up is a floor"), FGSTN_TumbleContactSyncState::FromHit(Hit, FVector(0.0, 0.0, -900.0), Up, 7).Kind == EGSTN_TumbleContactKind::Floor);
	Hit.ImpactNormal = FVector(0.0, 0.0, -1.0);
	TestTrue(TEXT("a face against up is a ceiling"), FGSTN_TumbleContactSyncState::FromHit(Hit, FVector(0.0, 0.0, 900.0), Up, 7).Kind == EGSTN_TumbleContactKind::Ceiling);
	Hit.ImpactNormal = FVector(-1.0, 0.0, 0.0);
	TestFalse(TEXT("moving away from the face is no contact"), FGSTN_TumbleContactSyncState::FromHit(Hit, FVector(-500.0, 0.0, 0.0), Up, 7).IsSet());
	Hit.bBlockingHit = false;
	TestFalse(TEXT("a hit that does not block is no contact"), FGSTN_TumbleContactSyncState::FromHit(Hit, Velocity, Up, 7).IsSet());

	// The record is quantised, so it is equal bit for bit after the wire; it rides in the knockback move
	FGSTN_LayeredMove_Knockback Move;
	Move.Contact = Wall;
	FBitWriter Writer(1024, true);
	Move.NetSerialize(Writer);
	FGSTN_LayeredMove_Knockback Loaded;
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	Loaded.NetSerialize(Reader);
	TestFalse(TEXT("the wire read cleanly"), Reader.IsError());
	TestTrue(TEXT("the contact survives the wire of the move"), Loaded.Contact == Wall);

	// And a move that never touched anything stays unset
	FGSTN_LayeredMove_Knockback Clean;
	FBitWriter CleanWriter(1024, true);
	Clean.NetSerialize(CleanWriter);
	FGSTN_LayeredMove_Knockback CleanLoaded;
	CleanLoaded.Contact = Wall;
	FBitReader CleanReader(CleanWriter.GetData(), CleanWriter.GetNumBits());
	CleanLoaded.NetSerialize(CleanReader);
	TestFalse(TEXT("an unset record loads as unset"), CleanLoaded.Contact.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactResolvedOneFrameAfterTest, "GSTNCombat.NP.Hits.Impact.ResolvedOneFrameAfterContact", GSTN_NP_TEST_FLAGS)
bool FGSTN_ImpactResolvedOneFrameAfterTest::RunTest(const FString& Parameters)
{
	FImpactFixture F;
	F.SetContact(TEXT("Wall"), 1000.0, 10);

	TestTrue(TEXT("the frame of the contact itself decides nothing (Mover has not finished it)"), F.Step(10).Outcome == EGSTN_ImpactOutcome::None);
	TestTrue(TEXT("one frame later the contact resolves"), F.Step(11).Outcome == EGSTN_ImpactOutcome::Splat);

	// A record that stays in the move is not resolved again
	FImpactFixture Again;
	Again.SetContact(TEXT("Wall"), 1000.0, 10);
	TestTrue(TEXT("resolved at 11"), Again.Step(11).Outcome == EGSTN_ImpactOutcome::Splat);
	Again.Core.ActionState = EGSTN_ActionState::Tumble;
	TestTrue(TEXT("not at 12"), Again.Step(12).Outcome == EGSTN_ImpactOutcome::None);

	// Nothing resolves for a victim that does not tumble, in a hitstop, or hit this frame
	FImpactFixture Other;
	Other.SetContact(TEXT("Wall"), 1000.0, 10);
	Other.Core.ActionState = EGSTN_ActionState::Hitstun;
	TestTrue(TEXT("not tumbling"), Other.Step(11).Outcome == EGSTN_ImpactOutcome::None);
	Other.Core.ActionState = EGSTN_ActionState::Tumble;
	Other.Core.HitstopStartFrame = 10;
	Other.Core.HitstopEndFrame = 14;
	TestTrue(TEXT("in a hitstop"), Other.Step(11).Outcome == EGSTN_ImpactOutcome::None);
	Other.Core.HitstopEndFrame = INDEX_NONE;
	Other.Core.HitstopStartFrame = INDEX_NONE;
	Other.In.bStaggeredThisFrame = true;
	TestTrue(TEXT("a hit of this frame wins"), Other.Step(11).Outcome == EGSTN_ImpactOutcome::None);
	Other.In.bStaggeredThisFrame = false;
	TestTrue(TEXT("and then it resolves"), Other.Step(11).Outcome == EGSTN_ImpactOutcome::Splat);

	// A floor resolves only on a surface that forces its outcome
	FImpactFixture Floor;
	Floor.SetContact(TEXT("Wall"), 1000.0, 10, EGSTN_TumbleContactKind::Floor, FVector(0.0, 0.0, 1.0));
	TestTrue(TEXT("landing on an ordinary floor is not an impact"), Floor.Step(11).Outcome == EGSTN_ImpactOutcome::None);
	Floor.SetContact(TEXT("Trampoline"), 1000.0, 10, EGSTN_TumbleContactKind::Floor, FVector(0.0, 0.0, 1.0));
	TestTrue(TEXT("a trampoline floor bounces a tumbling victim"), Floor.Step(11).Outcome == EGSTN_ImpactOutcome::Bounce);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactSplatEndsActionableTest, "GSTNCombat.NP.Hits.Impact.SplatEndsActionable", GSTN_NP_TEST_FLAGS)
bool FGSTN_ImpactSplatEndsActionableTest::RunTest(const FString& Parameters)
{
	FImpactFixture F;
	F.SetContact(TEXT("Wall"), 1000.0, 10);
	const FGSTN_ImpactStepResult& Result = F.Step(11);
	TestTrue(TEXT("splat"), Result.Outcome == EGSTN_ImpactOutcome::Splat);
	TestTrue(TEXT("the state is Splat"), F.Core.ActionState == EGSTN_ActionState::Splat);
	TestEqual(TEXT("it lasts the row's frames"), F.Core.Hit.ImpactEndFrame, 31);
	TestEqual(TEXT("held at rest for as long"), Result.FreezeFrames, 20);
	TestTrue(TEXT("at rest"), Result.bFreezeAtRest);
	TestEqual(TEXT("damage = the rules' base times the row's scale"), Result.Damage, 10.f);
	TestTrue(TEXT("the combat state locks while pinned"), FGSTN_CombatVictim::IsActionLocked(F.Core));

	FGSTN_ReactionStepInput Reaction;
	Reaction.Rules = F.Rules.Get();
	Reaction.bGrounded = false;
	Reaction.Frame = 30;
	FGSTN_CombatVictim::StepReaction(Reaction, F.Core);
	TestTrue(TEXT("still pinned on the last frame"), F.Core.ActionState == EGSTN_ActionState::Splat);
	Reaction.Frame = 31;
	FGSTN_CombatVictim::StepReaction(Reaction, F.Core);
	TestTrue(TEXT("Actionable at the end frame, in the air"), F.Core.ActionState == EGSTN_ActionState::Actionable);
	TestEqual(TEXT("the end frame is spent"), F.Core.Hit.ImpactEndFrame, static_cast<int32>(INDEX_NONE));

	// Below the speed of the row nothing happens
	FImpactFixture Slow;
	Slow.SetContact(TEXT("Wall"), 250.0, 10);
	TestTrue(TEXT("under the threshold"), Slow.Step(11).Outcome == EGSTN_ImpactOutcome::None);
	TestTrue(TEXT("and the victim goes on tumbling"), Slow.Core.ActionState == EGSTN_ActionState::Tumble);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactBounceKeepsTumbleTest, "GSTNCombat.NP.Hits.Impact.BounceKeepsTumble", GSTN_NP_TEST_FLAGS)
bool FGSTN_ImpactBounceKeepsTumbleTest::RunTest(const FString& Parameters)
{
	FImpactFixture F;
	F.Core.Hit.HitstunEndFrame = 14;
	// A trampoline wall (a forced bounce) hit at 1000 cm/s along +X, slid down to (0, 0, -200) at the end of the contact frame
	F.SetContact(TEXT("Trampoline"), 1000.0, 10);
	const FGSTN_ImpactStepResult& Result = F.Step(11);
	TestTrue(TEXT("bounce"), Result.Outcome == EGSTN_ImpactOutcome::Bounce);
	TestTrue(TEXT("the victim keeps tumbling"), F.Core.ActionState == EGSTN_ActionState::Tumble);
	TestTrue(TEXT("a launch move"), Result.Move.Kind == FGSTN_ReactionMove::EKind::Launch);
	// Reflected about the normal and scaled by the restitution: (tangent + n * speed) * 1.2
	TestTrue(TEXT("the reflected velocity"), Result.Move.Velocity.Equals(FVector(-1200.0, 0.0, -240.0), 0.5));
	TestEqual(TEXT("the relaunched move lives as long as a tumble's probe"), Result.Move.Frames, FGSTN_CombatVictim::TumbleProbeFrames);
	TestTrue(TEXT("the hitstun covers the minimum flight (not the whole life of the probe: the victim lands before that)"), F.Core.Hit.HitstunEndFrame == 11 + FGSTN_CombatVictim::MinBounceFrames);
	TestEqual(TEXT("no freeze"), Result.FreezeFrames, 0);

	// The move that was flying into the wall probes once more on the frame of the relaunch: that record is the contact just decided, not a new one
	F.In.Contact.ContactFrame = 11;
	TestTrue(TEXT("the probe record of the relaunch frame is not a second impact"), F.Step(12).Outcome == EGSTN_ImpactOutcome::None);
	F.In.Contact.ContactFrame = 12;
	TestTrue(TEXT("a contact made after it is"), F.Step(13).Outcome == EGSTN_ImpactOutcome::Bounce);

	// The wall row forced to bounce by the hit data
	FImpactFixture Forced;
	Forced.SetContact(TEXT("Wall"), 1000.0, 10);
	FGSTN_ImpactProfile Profile;
	Profile.Force = EGSTN_ImpactOutcome::Bounce;
	Forced.Core.Hit.ImpactProfileQ = Profile.Pack();
	const FGSTN_ImpactStepResult& Bounced = Forced.Step(11);
	TestTrue(TEXT("a forced bounce on a wall"), Bounced.Outcome == EGSTN_ImpactOutcome::Bounce);
	TestTrue(TEXT("with the wall's restitution 0.5"), Bounced.Move.Velocity.Equals(FVector(-500.0, 0.0, -100.0), 0.5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactBreakRequestOutsideSimTest, "GSTNCombat.NP.Hits.Impact.BreakRequestOutsideSim", GSTN_NP_TEST_FLAGS)
bool FGSTN_ImpactBreakRequestOutsideSimTest::RunTest(const FString& Parameters)
{
	FImpactFixture F;
	F.SetContact(TEXT("Glass"), 1000.0, 10);
	const FGSTN_ImpactStepResult& Result = F.Step(11);
	TestTrue(TEXT("break"), Result.Outcome == EGSTN_ImpactOutcome::BreakSurface);

	// The simulation only writes the request; the surface itself is broken by the component at the end of the frame, outside it
	TestTrue(TEXT("the request is in the sync state"), F.Core.Requests.Find(EGSTN_RequestType::ImpactSurface, 11, static_cast<int16>(TypeIndex(TEXT("Glass")))) != nullptr);
	TestEqual(TEXT("a break does no impact damage"), Result.Damage, 0.f);
	TestTrue(TEXT("the victim keeps tumbling"), F.Core.ActionState == EGSTN_ActionState::Tumble);
	TestTrue(TEXT("a stop of the row's frames while the surface breaks"), F.Core.IsInHitstop(11) && F.Core.IsInHitstop(15) && !F.Core.IsInHitstop(16));
	TestEqual(TEXT("frozen for the rest of it"), Result.FreezeFrames, 5);

	// The launch goes out after the stop with ResidualSpeedScale of the speed it came with: (tangent - n * speed) * 0.7 = (700, 0, -140)
	TestTrue(TEXT("a pending launch"), F.Core.Hit.HasPendingReaction() && F.Core.Hit.GetPendingReaction() == EGSTN_HitReaction::Launch);
	TestTrue(TEXT("with the residual speed"), F.Core.Hit.GetPendingLaunch().Equals(FVector(700.0, 0.0, -140.0), 4.0));

	FGSTN_ReactionStepInput Reaction;
	Reaction.Rules = F.Rules.Get();
	Reaction.bGrounded = false;
	Reaction.Frame = 16;
	TestTrue(TEXT("nothing moves while the window is open or the frame after it"), FGSTN_CombatVictim::StepReaction(Reaction, F.Core).Kind == FGSTN_ReactionMove::EKind::None);
	Reaction.Frame = 17;
	const FGSTN_ReactionMove Move = FGSTN_CombatVictim::StepReaction(Reaction, F.Core);
	TestTrue(TEXT("the victim goes on through the broken surface"), Move.Kind == FGSTN_ReactionMove::EKind::Launch && Move.Velocity.Equals(FVector(700.0, 0.0, -140.0), 4.0));

	// The old move probes the glass on the frames of the stop (the glass breaks at the end of the first): those records are not new contacts
	FImpactFixture Again;
	Again.SetContact(TEXT("Glass"), 1000.0, 10);
	TestTrue(TEXT("break"), Again.Step(11).Outcome == EGSTN_ImpactOutcome::BreakSurface);
	Again.Core.ActionState = EGSTN_ActionState::Tumble;
	Again.Core.HitstopStartFrame = Again.Core.HitstopEndFrame = INDEX_NONE;
	Again.In.Contact.ContactFrame = 17;
	TestTrue(TEXT("the record of the relaunch frame is not a second break"), Again.Step(18).Outcome == EGSTN_ImpactOutcome::None);
	Again.In.Contact.ContactFrame = 18;
	TestTrue(TEXT("a contact after it is"), Again.Step(19).Outcome == EGSTN_ImpactOutcome::BreakSurface);

	// Slower than the glass needs: it holds
	FImpactFixture Slow;
	Slow.SetContact(TEXT("Glass"), 500.0, 10);
	TestTrue(TEXT("below the threshold the glass holds"), Slow.Step(11).Outcome == EGSTN_ImpactOutcome::None);
	TestFalse(TEXT("and no request is made"), Slow.Core.Requests.Find(EGSTN_RequestType::ImpactSurface, 11, static_cast<int16>(TypeIndex(TEXT("Glass")))) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactForcedAndForbiddenTest, "GSTNCombat.NP.Hits.Impact.ForcedAndForbiddenByHitData", GSTN_NP_TEST_FLAGS)
bool FGSTN_ImpactForcedAndForbiddenTest::RunTest(const FString& Parameters)
{
	// The profile packs into 16 bits and unpacks to what every machine decides with
	FGSTN_ImpactProfile Profile;
	Profile.Force = EGSTN_ImpactOutcome::BreakSurface;
	Profile.Forbid = { EGSTN_ImpactOutcome::Splat, EGSTN_ImpactOutcome::Bounce };
	Profile.bForceIgnoresSurface = true;
	Profile.bNoTech = true;
	Profile.SpeedScale = 1.5f;
	const FGSTN_ImpactProfile Back = FGSTN_ImpactProfile::Unpack(Profile.Pack());
	TestTrue(TEXT("force"), Back.Force == EGSTN_ImpactOutcome::BreakSurface);
	TestTrue(TEXT("forbid"), Back.Forbid.Contains(EGSTN_ImpactOutcome::Splat) && Back.Forbid.Contains(EGSTN_ImpactOutcome::Bounce) && !Back.Forbid.Contains(EGSTN_ImpactOutcome::BreakSurface));
	TestTrue(TEXT("the flags"), Back.bForceIgnoresSurface && Back.bNoTech);
	TestEqual(TEXT("the scale in 1/32"), Back.SpeedScale, 1.5f);
	TestEqual(TEXT("the default profile packs to zero (an idle victim pays nothing)"), FGSTN_ImpactProfile().Pack(), static_cast<uint16>(0));
	TestTrue(TEXT("and unpacks to the default"), FGSTN_ImpactProfile::Unpack(0).IsDefault());

	// The packed profile rides in the hit state of a tumbling victim and vanishes when it stops tumbling
	FGSTN_HitSyncState State;
	State.ImpactProfileQ = Profile.Pack();
	FGSTN_HitLiveContext Tumbling;
	Tumbling.Frame = 50;
	Tumbling.bTumbling = true;
	FBitWriter Writer(256, true);
	State.NetSerialize(Writer, Tumbling);
	FGSTN_HitSyncState Loaded;
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	Loaded.NetSerialize(Reader, Tumbling);
	TestEqual(TEXT("the profile survives the wire"), static_cast<int32>(Loaded.ImpactProfileQ), static_cast<int32>(State.ImpactProfileQ));
	FGSTN_HitLiveContext Landed = Tumbling;
	Landed.bTumbling = false;
	TestTrue(TEXT("a victim that no longer tumbles has no profile in its canonical form"), State.Canonical(Landed).ImpactProfileQ == 0 && State.IsDefault(Landed));

	// The three layers on a wall: the hit forces a bounce, forbids a splat, or forbids what the surface does by default
	FImpactFixture F;
	F.SetContact(TEXT("Wall"), 1000.0, 10);
	FGSTN_ImpactProfile Forbid;
	Forbid.Forbid = { EGSTN_ImpactOutcome::Splat };
	F.Core.Hit.ImpactProfileQ = Forbid.Pack();
	TestTrue(TEXT("a jab never splats"), F.Step(11).Outcome == EGSTN_ImpactOutcome::None);

	FImpactFixture Forced;
	Forced.SetContact(TEXT("Wall"), 1000.0, 10);
	FGSTN_ImpactProfile Force;
	Force.Force = EGSTN_ImpactOutcome::BreakSurface;
	Forced.Core.Hit.ImpactProfileQ = Force.Pack();
	TestTrue(TEXT("a finisher breaks the wall it hits"), Forced.Step(11).Outcome == EGSTN_ImpactOutcome::BreakSurface);

	// Force and Forbid of the same outcome cancel; the surface decides
	FImpactFixture Both;
	Both.SetContact(TEXT("Glass"), 1000.0, 10);
	FGSTN_ImpactProfile Cancel;
	Cancel.Force = EGSTN_ImpactOutcome::Splat;
	Cancel.Forbid = { EGSTN_ImpactOutcome::Splat };
	Both.Core.Hit.ImpactProfileQ = Cancel.Pack();
	TestTrue(TEXT("the surface default remains"), Both.Step(11).Outcome == EGSTN_ImpactOutcome::BreakSurface);

	// The victim layer: a heavy enemy never splats, and the hit's SpeedScale judges the speed
	FImpactFixture Heavy;
	Heavy.SetContact(TEXT("Wall"), 1000.0, 10);
	Heavy.Response.Default.Allowed.bSplat = false;
	TestTrue(TEXT("the victim restricts the surface"), Heavy.Step(11).Outcome == EGSTN_ImpactOutcome::None);
	FImpactFixture Scaled;
	Scaled.SetContact(TEXT("Wall"), 400.0, 10);
	FGSTN_ImpactProfile Gentle;
	Gentle.SpeedScale = 0.5f;
	Scaled.Core.Hit.ImpactProfileQ = Gentle.Pack();
	TestTrue(TEXT("a gentle hit judges 400 cm/s as 200"), Scaled.Step(11).Outcome == EGSTN_ImpactOutcome::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactMaterialMappingBakedTest, "GSTNCombat.NP.Hits.Impact.MaterialMappingBaked", GSTN_NP_TEST_FLAGS)
bool FGSTN_ImpactMaterialMappingBakedTest::RunTest(const FString& Parameters)
{
	// The physical material to surface type mapping is baked outside the simulation (GSTNSurface); a contact on a mapped material carries its type index
	UPhysicalMaterial* GlassMaterial = NewObject<UPhysicalMaterial>(GetTransientPackage());
	const FGameplayTag Glass = FGameplayTag::RequestGameplayTag(TEXT("Surface.Type.Glass"), false);
	if (!TestTrue(TEXT("the glass surface type exists"), Glass.IsValid()))
	{
		return false;
	}
	FGSTN_SurfaceBehaviour GlassBehaviour;
	GlassBehaviour.SurfaceType = Glass;

	UGSTN_SurfaceSettings& Settings = UGSTN_SurfaceSettings::GetMutable();
	const TMap<TSoftObjectPtr<UPhysicalMaterial>, FGSTN_SurfaceBehaviour> Saved = Settings.PhysicalMaterials;
	Settings.PhysicalMaterials.Add(TSoftObjectPtr<UPhysicalMaterial>(GlassMaterial), GlassBehaviour);
	Settings.Rebake();

	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.ImpactNormal = FVector(-1.0, 0.0, 0.0);
	Hit.PhysMaterial = GlassMaterial;
	const FGSTN_TumbleContactSyncState Contact = FGSTN_TumbleContactSyncState::FromHit(Hit, FVector(1000.0, 0.0, 0.0), FVector::UpVector, 5);
	TestEqual(TEXT("the mapped material gives the glass type index"), static_cast<int32>(Contact.SurfaceTypeIndex), static_cast<int32>(UGSTN_SurfaceUtils::GetSurfaceTypeIndex(Glass)));
	TestTrue(TEXT("which is not the default's"), Contact.SurfaceTypeIndex != UGSTN_SurfaceUtils::GetSurfaceTypeIndex(UGSTN_SurfaceSettings::Get().Default.SurfaceType));

	// The impact table finds its row by that index, wherever the row sits in the asset
	FImpactFixture F;
	TestTrue(TEXT("the table row of the index is the glass row"), F.Table->GetRow(Contact.SurfaceTypeIndex) && F.Table->GetRow(Contact.SurfaceTypeIndex)->DefaultOutcome == EGSTN_ImpactOutcome::BreakSurface);

	Settings.PhysicalMaterials = Saved;
	Settings.Rebake();
	return true;
}

#endif
