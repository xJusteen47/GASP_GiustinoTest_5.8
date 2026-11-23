// Links, pure core tests of M11 K2 (GSTNLink.Core.KitThrows.*): the Smash-style throw slots, the kit throw fill and the project settings.

#include "Data/GSTN_LinkKitExtension.h"
#include "GSTN_LinkSettings.h"
#include "GSTN_LinkTags.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_LinkSlots.h"
#include "Input/GSTN_SlotResolver.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Tests/GSTN_LinkTestUtils.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Bit order of the scripts: A Attack, B Strong, C Special, D Crouch, E Shield, F Jump, G Interact */
	TArray<FGameplayTag> KitThrowButtons()
	{
		return { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag(),
			GSTNCombatButtonTags::Button_Crouch.GetTag(), GSTNCombatButtonTags::Button_Shield.GetTag(), GSTNCombatButtonTags::Button_Jump.GetTag(),
			GSTNCombatButtonTags::Button_Interact.GetTag() };
	}

	/** The values the input tests write out (a new object copies the project's config, which designers tune), with the link tier */
	TStrongObjectPtr<UGSTN_InputSettings> MakeInputSettings()
	{
		TStrongObjectPtr<UGSTN_InputSettings> Settings(NewObject<UGSTN_InputSettings>(GetTransientPackage()));
		Settings->DefaultBufferFrames = 8;
		Settings->bHoldBufferByDefault = true;
		Settings->MaxHoldBufferFrames = 0;
		Settings->DefaultSimultaneityFrames = 3;
		Settings->SimultaneousFrames = 2;
		Settings->BufferConflictPolicy = EGSTN_BufferConflictPolicy::MostRecent;
		Settings->bChordGraceByDefault = false;
		Settings->ChordGraceFrames = 3;
		Settings->DiagonalHalfAngle = 22.5f;
		Settings->WorldReferenceYaw = 0.f;
		Settings->CommandTiers.Empty();
		Settings->CommandTiers.Add(FGameplayTag::RequestGameplayTag(TEXT("Command.Link")), 360);
		Settings->ExclusiveButtons = { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag() };
		return Settings;
	}

	/** The grabber's slots in a mode: the pummel of the slot set and the four throws of the mode */
	TArray<FGSTN_InputSlotSpec> GrabberSlots(EGSTN_LinkThrowInputMode Mode)
	{
		using namespace GSTNLinkTags;
		TArray<FGSTN_InputSlotSpec> Slots;
		for (const FGSTN_InputSlotSpec& Spec : UGSTN_LinkSlots::Build())
		{
			if (Spec.Command == Command_Link_Pummel)
			{
				Slots.Add(Spec);
			}
		}
		for (const FGameplayTag& Throw : { Command_Link_Throw_Forward.GetTag(), Command_Link_Throw_Back.GetTag(), Command_Link_Throw_Up.GetTag(), Command_Link_Throw_Down.GetTag() })
		{
			Slots.Add(UGSTN_LinkSlots::BuildThrowSlot(Throw, Mode));
		}
		return Slots;
	}

	/** The command the slots trigger at the script's last frame (facing and camera at yaw 0: stick up is forward), empty if none */
	FString CommandFor(FAutomationTestBase& Test, const TArray<FGSTN_InputSlotSpec>& Slots, const TCHAR* Notation, const UGSTN_InputSettings& Settings)
	{
		FGSTN_InputScript Script(KitThrowButtons());
		FString Error;
		FGSTN_InputHistory History;
		const bool bOk = Script.Parse(Notation, Error) && Script.Advance(History, Settings, Script.GetLastFrame());
		Test.TestTrue(*FString::Printf(TEXT("script plays: %s (%s)"), Notation, *Error), bOk);
		const FGSTN_ButtonMap Map(KitThrowButtons());
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Slots)
		{
			Pointers.Add(&Spec);
		}
		FGSTN_ResolveParams Params;
		Params.Buttons = &Map;
		Params.Now = History.GetLatestFrame();
		const FGSTN_SlotResolution Resolution = FGSTN_SlotResolver::Resolve(Pointers, History, Params, Settings, [](int32) { return true; });
		return Resolution.SlotIndex == INDEX_NONE ? FString() : Slots[Resolution.SlotIndex].Command.ToString();
	}

	FString Name(const FNativeGameplayTag& Tag)
	{
		return Tag.GetTag().ToString();
	}

	FGSTN_BindingInput* FindRow(TArray<FGSTN_BindingInput>& Bindings, const FGameplayTag& Command, bool bWithOverride)
	{
		return Bindings.FindByPredicate([&](const FGSTN_BindingInput& Binding) { return Binding.Command == Command && Binding.bHasSlotOverride == bWithOverride; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkKitThrowsStickTest, "GSTNLink.Core.KitThrows.StickPicksThrow", GSTNLinkTest::Flags)

bool FGSTN_LinkKitThrowsStickTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLinkTags;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeInputSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = GrabberSlots(EGSTN_LinkThrowInputMode::Stick);
	TestEqual(TEXT("stick forward = forward throw"), CommandFor(*this, Slots, TEXT("5:8t"), *Settings), Name(Command_Link_Throw_Forward));
	TestEqual(TEXT("diagonal forward = forward throw"), CommandFor(*this, Slots, TEXT("5:9t"), *Settings), Name(Command_Link_Throw_Forward));
	TestEqual(TEXT("stick back = back throw"), CommandFor(*this, Slots, TEXT("5:2t"), *Settings), Name(Command_Link_Throw_Back));
	TestEqual(TEXT("jump = up throw"), CommandFor(*this, Slots, TEXT("5:+F"), *Settings), Name(Command_Link_Throw_Up));
	TestEqual(TEXT("crouch = down throw"), CommandFor(*this, Slots, TEXT("5:+D"), *Settings), Name(Command_Link_Throw_Down));
	TestEqual(TEXT("attack, stick neutral = pummel"), CommandFor(*this, Slots, TEXT("5:+A"), *Settings), Name(Command_Link_Pummel));
	TestEqual(TEXT("sideways = nothing"), CommandFor(*this, Slots, TEXT("5:6t"), *Settings), FString());
	TestEqual(TEXT("a light push only aims"), CommandFor(*this, Slots, TEXT("5:8a"), *Settings), FString());
	TestEqual(TEXT("a stick held since long before (the run into the grab) does not throw"), CommandFor(*this, Slots, TEXT("0:8t 30:8t"), *Settings), FString());
	TestEqual(TEXT("the stick re-entering forward throws"), CommandFor(*this, Slots, TEXT("0:8t 20:5 30:8t"), *Settings), Name(Command_Link_Throw_Forward));
	return true;
}

namespace
{
	/** The grabber's slots in Notation mode: the moved pummel and the mode's throws */
	TArray<FGSTN_InputSlotSpec> NotationSlots(EGSTN_GrabTurnErgonomics Ergonomics)
	{
		// The put-down slot is left out: the scripts' button map has no dodge
		TArray<FGSTN_InputSlotSpec> Slots = { UGSTN_LinkSlots::BuildNotationPummelSlot() };
		for (const FGameplayTag& Throw : UGSTN_LinkSlots::GetThrowCommands(EGSTN_LinkThrowInputMode::Notation, Ergonomics))
		{
			Slots.Add(UGSTN_LinkSlots::BuildThrowSlot(Throw, EGSTN_LinkThrowInputMode::Notation, Ergonomics));
		}
		return Slots;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkKitThrowsNotationTest, "GSTNLink.Core.KitThrows.NotationPicksThrow", GSTNLinkTest::Flags)

bool FGSTN_LinkKitThrowsNotationTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLinkTags;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeInputSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = NotationSlots(EGSTN_GrabTurnErgonomics::Instant);
	TestEqual(TEXT("the stick alone throws nothing (it walks)"), CommandFor(*this, Slots, TEXT("0:5 5:8t"), *Settings), FString());
	TestEqual(TEXT("jump alone throws nothing (it jumps)"), CommandFor(*this, Slots, TEXT("5:+F"), *Settings), FString());
	TestEqual(TEXT("attack, neutral = N."), CommandFor(*this, Slots, TEXT("5:+A"), *Settings), Name(Command_Link_Throw_Neutral));
	TestEqual(TEXT("attack + forward = F."), CommandFor(*this, Slots, TEXT("4:8t 5:+A"), *Settings), Name(Command_Link_Throw_Forward));
	TestEqual(TEXT("instant: attack + back = F. too (the start aim turns)"), CommandFor(*this, Slots, TEXT("4:2t 5:+A"), *Settings), Name(Command_Link_Throw_Forward));
	TestEqual(TEXT("instant: attack + side = F."), CommandFor(*this, Slots, TEXT("4:6t 5:+A"), *Settings), Name(Command_Link_Throw_Forward));
	TestEqual(TEXT("crouch + attack = Cr."), CommandFor(*this, Slots, TEXT("2:+D 5:+A"), *Settings), Name(Command_Link_Throw_Down));
	TestEqual(TEXT("crouch + tilt + attack = Cr.F."), CommandFor(*this, Slots, TEXT("2:+D 4:8t 5:+A"), *Settings), Name(Command_Link_Throw_CrouchForward));
	TestEqual(TEXT("strong, neutral = pummel"), CommandFor(*this, Slots, TEXT("5:+B"), *Settings), Name(Command_Link_Pummel));

	const TArray<FGSTN_InputSlotSpec> WithBack = NotationSlots(EGSTN_GrabTurnErgonomics::BackThrow);
	TestEqual(TEXT("back throw mode: attack + back = B."), CommandFor(*this, WithBack, TEXT("4:2t 5:+A"), *Settings), Name(Command_Link_Throw_Back));
	TestEqual(TEXT("back throw mode: attack + forward = F."), CommandFor(*this, WithBack, TEXT("4:8t 5:+A"), *Settings), Name(Command_Link_Throw_Forward));
	TestFalse(TEXT("instant binds no back throw"), UGSTN_LinkSlots::GetThrowCommands(EGSTN_LinkThrowInputMode::Notation, EGSTN_GrabTurnErgonomics::Instant)
		.Contains(Command_Link_Throw_Back.GetTag()));

	// The fill: N./Cr.F. added from the common set, the authored up throw hidden, the pummel moved, the put-down row bound to the release ability
	FGSTN_LinkThrowSet Fallback;
	Fallback.Forward = FSoftClassPath(TEXT("/Script/GSTNLinkTest.F"));
	Fallback.Down = FSoftClassPath(TEXT("/Script/GSTNLinkTest.D"));
	Fallback.Neutral = FSoftClassPath(TEXT("/Script/GSTNLinkTest.N"));
	Fallback.CrouchForward = FSoftClassPath(TEXT("/Script/GSTNLinkTest.CF"));
	FGSTN_KitThrowFill Fill(EGSTN_LinkThrowInputMode::Notation);
	Fill.Fallback = &Fallback;
	Fill.bPutDownWithDodge = true;
	const FSoftObjectPath ReleaseAbility(TEXT("/Script/GSTNLinkTest.Release"));
	TArray<FGSTN_BindingInput> Bindings;
	auto AddRow = [&Bindings](const FGameplayTag& Command, const TCHAR* Ability, bool bGrabber)
	{
		// By value into the array: a reference into it would not survive the next add
		FGSTN_BindingInput Row;
		Row.Command = Command;
		Row.Ability = FSoftObjectPath(Ability);
		if (bGrabber)
		{
			Row.Requirements.RequireTags.AddTag(Role_Grabber);
		}
		Bindings.Add(MoveTemp(Row));
	};
	AddRow(Command_Link_Grab, TEXT("/Script/GSTNLinkTest.Grab"), false);
	AddRow(Command_Link_Release, TEXT("/Script/GSTNLinkTest.Release"), true);
	AddRow(Command_Link_Pummel, TEXT("/Script/GSTNLinkTest.Pummel"), true);
	AddRow(Command_Link_Throw_Up, TEXT("/Script/GSTNLinkTest.Up"), false);
	TestEqual(TEXT("four throws bound"), UGSTN_LinkSlots::ApplyKitThrows(Fill, Bindings), 4);
	const FGSTN_BindingInput* Neutral = FindRow(Bindings, Command_Link_Throw_Neutral, true);
	TestTrue(TEXT("N. from the common set"), Neutral && Neutral->Ability == static_cast<const FSoftObjectPath&>(Fallback.Neutral));
	TestTrue(TEXT("Cr.F. from the common set"), FindRow(Bindings, Command_Link_Throw_CrouchForward, true) != nullptr);
	const FGSTN_BindingInput* UpRow = FindRow(Bindings, Command_Link_Throw_Up, false);
	TestTrue(TEXT("the authored up throw is hidden"), UpRow && UpRow->Ability.IsNull());
	const FGSTN_BindingInput* PummelRow = FindRow(Bindings, Command_Link_Pummel, true);
	TestTrue(TEXT("the pummel moved to strong"), PummelRow != nullptr);
	const FGSTN_BindingInput* PutDown = FindRow(Bindings, Command_Link_Release_PutDown, true);
	TestTrue(TEXT("the put-down row plays the release ability"), PutDown && PutDown->Ability == ReleaseAbility
		&& PutDown->Requirements.RequireTags.HasTagExact(Role_Grabber));
	const int32 Rows = Bindings.Num();
	UGSTN_LinkSlots::ApplyKitThrows(Fill, Bindings);
	TestEqual(TEXT("a second fill adds nothing"), Bindings.Num(), Rows);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkKitThrowsSpecialTest, "GSTNLink.Core.KitThrows.StickPlusSpecial", GSTNLinkTest::Flags)

bool FGSTN_LinkKitThrowsSpecialTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLinkTags;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeInputSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = GrabberSlots(EGSTN_LinkThrowInputMode::StickPlusSpecial);
	TestEqual(TEXT("stick alone throws nothing"), CommandFor(*this, Slots, TEXT("5:8t"), *Settings), FString());
	TestEqual(TEXT("special + forward"), CommandFor(*this, Slots, TEXT("4:8t 5:+C"), *Settings), Name(Command_Link_Throw_Forward));
	TestEqual(TEXT("special + back"), CommandFor(*this, Slots, TEXT("4:2t 5:+C"), *Settings), Name(Command_Link_Throw_Back));
	TestEqual(TEXT("special alone = up"), CommandFor(*this, Slots, TEXT("5:+C"), *Settings), Name(Command_Link_Throw_Up));
	TestEqual(TEXT("special + crouch held = down"), CommandFor(*this, Slots, TEXT("2:+D 5:+C"), *Settings), Name(Command_Link_Throw_Down));
	TestEqual(TEXT("attack = pummel"), CommandFor(*this, Slots, TEXT("5:+A"), *Settings), Name(Command_Link_Pummel));

	// The special grab: special with crouch held, never special alone
	const TArray<FGSTN_InputSlotSpec> Grab = { UGSTN_LinkSlots::BuildSpecialCrouchGrabSlot() };
	TestEqual(TEXT("special + crouch held = grab"), CommandFor(*this, Grab, TEXT("2:+D 5:+C"), *Settings), Name(Command_Link_Grab_Special));
	TestEqual(TEXT("special alone is no grab"), CommandFor(*this, Grab, TEXT("5:+C"), *Settings), FString());
	TestTrue(TEXT("special grab falls through without a target"), Grab[0].bFallThroughWhenUnavailable);

	// Buttons mode keeps the F12 rows
	TestEqual(TEXT("buttons: crouch + attack = down"), CommandFor(*this, GrabberSlots(EGSTN_LinkThrowInputMode::Buttons), TEXT("2:+D 5:+A"), *Settings), Name(Command_Link_Throw_Down));
	TestEqual(TEXT("buttons: the stick alone throws nothing"), CommandFor(*this, GrabberSlots(EGSTN_LinkThrowInputMode::Buttons), TEXT("5:8t"), *Settings), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkKitThrowsFillTest, "GSTNLink.Core.KitThrows.Fill", GSTNLinkTest::Flags)

bool FGSTN_LinkKitThrowsFillTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLinkTags;
	const FSoftObjectPath GrabAbility(TEXT("/Script/Test.GrabMove"));
	FGSTN_LinkThrowSet Fallback;
	Fallback.Forward = FSoftClassPath(TEXT("/Script/Test.FallbackForward"));
	Fallback.Back = FSoftClassPath(TEXT("/Script/Test.FallbackBack"));
	Fallback.Up = FSoftClassPath(TEXT("/Script/Test.FallbackUp"));
	Fallback.Down = FSoftClassPath(TEXT("/Script/Test.FallbackDown"));
	FGSTN_LinkThrowSet Kit;
	Kit.Back = FSoftClassPath(TEXT("/Script/Test.KitBack"));
	const FGameplayTag Blocker = Role_Latcher;

	FGSTN_KitThrowFill Fill(EGSTN_LinkThrowInputMode::Stick);
	Fill.bSpecialCrouchGrab = true;
	Fill.KitThrows = &Kit;
	Fill.Fallback = &Fallback;
	Fill.BlockStickOnly.AddTag(Blocker);

	// A kit that does not grab is left alone
	TArray<FGSTN_BindingInput> NoGrab;
	NoGrab.AddDefaulted_GetRef().Command = Command_Link_Pummel;
	TestEqual(TEXT("no grab, nothing bound"), UGSTN_LinkSlots::ApplyKitThrows(Fill, NoGrab), 0);
	TestEqual(TEXT("no grab, rows unchanged"), NoGrab.Num(), 1);

	// A grabbing kit with no throws: four rows (the kit's back, the common rest), the mode's slots, the special grab
	TArray<FGSTN_BindingInput> Bindings;
	FGSTN_BindingInput& GrabRow = Bindings.AddDefaulted_GetRef();
	GrabRow.Command = Command_Link_Grab;
	GrabRow.Requirements.RequireTags.AddTag(Interact_Target_Grab);
	GrabRow.Ability = GrabAbility;
	TestEqual(TEXT("four throws bound"), UGSTN_LinkSlots::ApplyKitThrows(Fill, Bindings), 4);
	TestEqual(TEXT("grab + four throws + special grab"), Bindings.Num(), 6);
	FGSTN_BindingInput* Forward = FindRow(Bindings, Command_Link_Throw_Forward, true);
	FGSTN_BindingInput* Back = FindRow(Bindings, Command_Link_Throw_Back, true);
	FGSTN_BindingInput* Up = FindRow(Bindings, Command_Link_Throw_Up, true);
	FGSTN_BindingInput* SpecialGrab = FindRow(Bindings, Command_Link_Grab_Special, true);
	if (!TestNotNull(TEXT("forward row with the mode's slot"), Forward) || !TestNotNull(TEXT("back row"), Back) || !TestNotNull(TEXT("up row"), Up)
		|| !TestNotNull(TEXT("special grab row"), SpecialGrab))
	{
		return false;
	}
	TestEqual(TEXT("forward from the common set"), Forward->Ability, FSoftObjectPath(Fallback.Forward));
	TestEqual(TEXT("back from the kit's set"), Back->Ability, FSoftObjectPath(Kit.Back));
	TestTrue(TEXT("grabber only"), Forward->Requirements.RequireTags.HasTagExact(Role_Grabber));
	TestTrue(TEXT("stick-only throws carry the stick blocker"), Forward->Requirements.IgnoreTags.HasTagExact(Blocker) && Back->Requirements.IgnoreTags.HasTagExact(Blocker));
	TestFalse(TEXT("the up throw (jump) does not"), Up->Requirements.IgnoreTags.HasTagExact(Blocker));
	TestTrue(TEXT("the slot is the binding's command"), Forward->SlotOverride.Command == Command_Link_Throw_Forward.GetTag());
	TestEqual(TEXT("special grab: the grab's ability"), SpecialGrab->Ability, GrabAbility);
	TestTrue(TEXT("special grab: the grab's requirement"), SpecialGrab->Requirements.RequireTags.HasTagExact(Interact_Target_Grab));
	TestEqual(TEXT("special grab: its offset"), SpecialGrab->SlotOverride.PriorityOffset, UGSTN_LinkSlots::SpecialGrabPriorityOffset);

	// Again (a second compile of the same input): nothing doubles
	TestEqual(TEXT("again: four"), UGSTN_LinkSlots::ApplyKitThrows(Fill, Bindings), 4);
	TestEqual(TEXT("again: six rows"), Bindings.Num(), 6);

	// The kit's own rows: kept (the kit set replaces their ability), a row bound to nothing hides the throw, Buttons mode keeps the set's slot
	TArray<FGSTN_BindingInput> Own;
	FGSTN_BindingInput& OwnGrab = Own.AddDefaulted_GetRef();
	OwnGrab.Command = Command_Link_Grab;
	OwnGrab.Ability = GrabAbility;
	FGSTN_BindingInput& OwnForward = Own.AddDefaulted_GetRef();
	OwnForward.Command = Command_Link_Throw_Forward;
	OwnForward.Ability = FSoftObjectPath(TEXT("/Script/Test.OwnForward"));
	FGSTN_BindingInput& OwnBack = Own.AddDefaulted_GetRef();
	OwnBack.Command = Command_Link_Throw_Back;
	OwnBack.Ability = FSoftObjectPath(TEXT("/Script/Test.OwnBack"));
	Own.AddDefaulted_GetRef().Command = Command_Link_Throw_Up;
	FGSTN_KitThrowFill Buttons(EGSTN_LinkThrowInputMode::Buttons);
	Buttons.KitThrows = &Kit;
	Buttons.Fallback = &Fallback;
	TestEqual(TEXT("hidden up: three bound"), UGSTN_LinkSlots::ApplyKitThrows(Buttons, Own), 3);
	TestEqual(TEXT("own forward kept"), Own[1].Ability, FSoftObjectPath(TEXT("/Script/Test.OwnForward")));
	TestEqual(TEXT("kit set replaces own back"), Own[2].Ability, FSoftObjectPath(Kit.Back));
	TestTrue(TEXT("hidden up stays unbound"), Own[3].Ability.IsNull());
	TestFalse(TEXT("buttons: no slot override"), Own[1].bHasSlotOverride);
	TestEqual(TEXT("buttons: down added from the common set, no special grab"), Own.Num(), 5);

	// A designer slot replaces the mode's shape
	TArray<FGSTN_InputSlotSpec> Overrides;
	FGSTN_InputSlotSpec& Custom = Overrides.Add_GetRef(UGSTN_LinkSlots::BuildThrowSlot(Command_Link_Throw_Up, EGSTN_LinkThrowInputMode::StickPlusSpecial));
	Custom.PriorityOffset = 7;
	FGSTN_KitThrowFill WithOverride(EGSTN_LinkThrowInputMode::Stick);
	WithOverride.Fallback = &Fallback;
	WithOverride.SlotOverrides = &Overrides;
	TArray<FGSTN_BindingInput> Designed;
	FGSTN_BindingInput& DesignedGrab = Designed.AddDefaulted_GetRef();
	DesignedGrab.Command = Command_Link_Grab;
	DesignedGrab.Ability = GrabAbility;
	UGSTN_LinkSlots::ApplyKitThrows(WithOverride, Designed);
	const FGSTN_BindingInput* DesignedUp = FindRow(Designed, Command_Link_Throw_Up, true);
	TestTrue(TEXT("the designer's up slot"), DesignedUp && DesignedUp->SlotOverride.PriorityOffset == 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkKitThrowsSettingsTest, "GSTNLink.Core.KitThrows.ProjectSettings", GSTNLinkTest::Flags)

bool FGSTN_LinkKitThrowsSettingsTest::RunTest(const FString& Parameters)
{
	// The owner's decisions of 2026-11-11 in DefaultGSTNLink.ini
	const UGSTN_LinkSettings& Settings = UGSTN_LinkSettings::Get();
	// grab-link (owner 2026-11-23): the project notation, the stick free to walk while holding; instant turn; dodge puts down
	TestEqual(TEXT("the notation picks the throw"), Settings.ThrowInputMode, EGSTN_LinkThrowInputMode::Notation);
	TestEqual(TEXT("instant turn ergonomics"), Settings.GrabTurnErgonomics, EGSTN_GrabTurnErgonomics::Instant);
	TestTrue(TEXT("dodge puts down"), Settings.bPutDownWithDodge);
	TestTrue(TEXT("N. and Cr.F. in the common set"), Settings.FallbackThrows.Neutral.IsValid() && Settings.FallbackThrows.CrouchForward.IsValid());
	TestTrue(TEXT("held by the neck"), Settings.HoldPose.bHoldByNeck);
	TestTrue(TEXT("applied"), Settings.bApplyKitThrows);
	TestTrue(TEXT("special + crouch grabs"), Settings.bGrabWithSpecialCrouch);
	TestTrue(TEXT("a full common set"), Settings.FallbackThrows.Forward.IsValid() && Settings.FallbackThrows.Back.IsValid()
		&& Settings.FallbackThrows.Up.IsValid() && Settings.FallbackThrows.Down.IsValid());

	// The back throw's elevation above 90 leans the launch behind the thrower (the launch math takes cos and sin of it)
	const FVector Launch = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::AttackerFacing, 140.f, EGSTN_SpecialAngle::None, FVector::UpVector, FVector::ForwardVector,
		FVector::ForwardVector, FVector::ZeroVector, true, 1800.f, FGSTN_SakuraiParams());
	TestTrue(TEXT("140 degrees: backwards and up"), Launch.X < -0.7 && Launch.Z > 0.6);
	return true;
}

#endif
