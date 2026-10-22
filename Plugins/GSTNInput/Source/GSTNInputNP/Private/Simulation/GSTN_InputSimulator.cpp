// The per-frame input simulation a host model embeds (GAS-NPP fork).

#include "Simulation/GSTN_InputSimulator.h"

#include "Abilities/GameplayAbility.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CommandLocks.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_DirectionClassifier.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputTags.h"
#include "Input/GSTN_SlotResolver.h"

namespace
{
	/**
	 * Index in table.Bindings of the binding the context picks for a command, INDEX_NONE if none holds. The candidates are
	 * the command's own bindings, already in declaration order, so the first of equally demanding ones wins on every machine.
	 */
	int32 SelectBindingFor(const FGSTN_CommandTable& Table, int32 CommandIndex, const FGameplayTagContainer& Context)
	{
		const FGSTN_CommandTable::FCommand& Command = Table.Commands[CommandIndex];

		TArray<const FGSTN_TagRequirements*, TInlineAllocator<8>> Candidates;
		for (int32 Offset = 0; Offset < Command.NumBindings; ++Offset)
		{
			Candidates.Add(&Table.Bindings[Command.FirstBinding + Offset].Requirements);
		}

		const int32 Selected = UGSTN_CommandBindingUtils::SelectBinding(Candidates, Context);
		return Selected == INDEX_NONE ? INDEX_NONE : Command.FirstBinding + Selected;
	}
}

FGSTN_InputTickResult FGSTN_InputSimulator::Tick(const FGSTN_InputTickArgs& Args, const IGSTN_InputSimHost& Host)
{
	check(Args.Cmd && Args.In && Args.Out && Args.Table && Args.Buttons && Args.Settings);

	const FGSTN_InputCmd& Cmd = *Args.Cmd;
	const FGSTN_InputSimState& In = *Args.In;
	FGSTN_InputSimState& Out = *Args.Out;
	const FGSTN_CommandTable& Table = *Args.Table;
	const int32 Frame = Args.Frame;

	FGSTN_InputTickResult Result;

	// 1. Sample. A tap shorter than a frame is pressed and released on the same frame; the stick is classified in every
	// direction space with the facing the host had at the end of the previous frame.
	const uint32 PrevHeld = In.History.GetHeldButtons();
	FGSTN_InputSample Sample;
	Sample.Frame = Frame;
	Sample.Held = Cmd.HeldButtons;
	Sample.Pressed = (Cmd.HeldButtons & ~PrevHeld) | Cmd.PressedButtons;
	Sample.Released = (PrevHeld | Sample.Pressed) & ~Cmd.HeldButtons;
	Sample.Stick = FGSTN_DirectionClassifier::Classify(
		FVector2D(FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickX), FGSTN_DirectionClassifier::DequantizeStickAxis(Cmd.StickY)),
		FGSTN_DirectionClassifier::DequantizeYaw(Cmd.CameraYaw), Host.GetFacingYaw(), In.Classifier, Frame, *Args.Settings, Out.Classifier);
	Result.Pressed = Sample.Pressed;

	// 2. Append
	Out.History.Append(Sample);

	// 3. Watched tags: the mask of the previous frame's tags against this one's tells which appeared since
	FGameplayTagContainer OwnerTags;
	Host.GetOwnerTags(OwnerTags);

	FGSTN_BufferFrameEvents Events;
	uint64 WatchedMask = 0;
	const int32 NumWatched = FMath::Min(Table.WatchedTags.Num(), FGSTN_CommandTable::MaxWatchedTags);
	for (int32 Bit = 0; Bit < NumWatched; ++Bit)
	{
		if (OwnerTags.HasTagExact(Table.WatchedTags[Bit]))
		{
			WatchedMask |= uint64(1) << Bit;
			if (!(In.WatchedTagMask & (uint64(1) << Bit)))
			{
				Events.TagsAdded.AddTag(Table.WatchedTags[Bit]);
			}
		}
	}
	Out.WatchedTagMask = WatchedMask;

	// 4. Reset rules. An activation made last frame is an event of this one: its ability's effects are in the tags only
	// from now on, so the rules see both together.
	if (In.LastActivatedFrame != INDEX_NONE && In.LastActivatedFrame == Frame - 1)
	{
		Events.EventTags.AddTag(GSTNInputTags::BufferEvent_AbilityActivated);
	}
	const uint8 Fingerprint = Host.GetContextFingerprint();
	Host.AppendFrameEvents(In.PrevContext, Fingerprint, Events);
	Out.PrevContext = Fingerprint;

	FGSTN_BufferResetRules::Apply(Out.History, *Args.Buttons, Args.GlobalResetRules, Events, Frame);
	if (Table.ResetRules)
	{
		FGSTN_BufferResetRules::Apply(Out.History, *Args.Buttons, *Table.ResetRules, Events, Frame);
	}

	// 5. Locks. The global lock stops everything before any matching: the presses stay in the history, so they can still come
	// out within their buffer once the lock ends. Per-root locks are checked per command below.
	if (UGSTN_CommandLocks::HasGlobalLock(OwnerTags))
	{
		return Result;
	}

	// 6. Resolve. Nothing to do without commands to start, or when a forced command names none
	const int32 ForcedCommandIndex = Table.FromForcedCommand(Cmd.ForcedCommand);
	if (Table.CandidateSlots.IsEmpty() && ForcedCommandIndex == INDEX_NONE)
	{
		return Result;
	}

	FGameplayTagContainer Context = OwnerTags;
	Host.AppendContextTags(Context);

	// The binding a command would start now: no lock holds it, the host allows it, a binding holds in this context and its
	// ability could be activated. A command without any of these is "unavailable" to the resolver.
	auto FindExecutableBinding = [&](int32 CommandIndex) -> int32
	{
		if (Table.IsCommandLocked(CommandIndex, OwnerTags) || !Host.IsCommandAllowed(CommandIndex))
		{
			return INDEX_NONE;
		}
		const int32 BindingIndex = SelectBindingFor(Table, CommandIndex, Context);
		return BindingIndex != INDEX_NONE && Host.CanExecuteBinding(BindingIndex) ? BindingIndex : INDEX_NONE;
	};

	int32 CommandIndex = INDEX_NONE;
	int32 BindingIndex = INDEX_NONE;
	uint8 RequestButton = 0;

	if (ForcedCommandIndex != INDEX_NONE)
	{
		// An explicit request (AI, script): it replaces the pattern matching of this frame and consumes no press
		CommandIndex = ForcedCommandIndex;
		BindingIndex = FindExecutableBinding(CommandIndex);
	}
	else
	{
		FGSTN_ResolveParams Params;
		Params.Buttons = Args.Buttons;
		Params.Now = Frame;
		Params.PausedStart = Args.PausedStart;
		Params.PausedEnd = Args.PausedEnd;

		const FGSTN_SlotResolution Resolution = FGSTN_SlotResolver::Resolve(Table.CandidateSlots, Out.History, Params, *Args.Settings,
			[&](int32 SlotIndex) { return FindExecutableBinding(Table.CandidateCommands[SlotIndex]) != INDEX_NONE; });

		if (Resolution.SlotIndex != INDEX_NONE)
		{
			CommandIndex = Table.CandidateCommands[Resolution.SlotIndex];
			BindingIndex = FindExecutableBinding(CommandIndex);

			if (BindingIndex != INDEX_NONE)
			{
				// 1 + the button of the press that completed the command, for abilities that end when it is released; a command
				// with no press (0 is the forced command's) says so with its own value
				RequestButton = Resolution.Match.TerminalPressEvent != INDEX_NONE
					? static_cast<uint8>(Out.History.GetButtonEvent(Resolution.Match.TerminalPressEvent).Button + 1)
					: FGSTN_InputSimState::CommandWithoutButton;

				// The winner's presses and the losers of its press group, so the loser does not come out a few frames later
				for (const uint8 EventSlot : Resolution.EventsToConsume)
				{
					Out.History.Consume(EventSlot);
				}
			}
		}
	}

	// 7. Request
	if (BindingIndex != INDEX_NONE)
	{
		Out.RequestedBinding = static_cast<uint8>(BindingIndex + 1);
		Out.RequestButton = RequestButton;
		Out.RequestFrame = Frame;
		Out.LastActivatedFrame = Frame;
		Result.CommandIndex = CommandIndex;
		Result.BindingIndex = BindingIndex;

		// The authoritative event: written here on every machine and every re-simulation, published only by the authority's host
		if (Args.Outbox)
		{
			FGSTN_SimEvent Event;
			Event.EventType = GSTNInputTags::Event_CommandExecuted;
			Event.Frame = Frame;
			Event.Tags.AddTag(Table.Commands[CommandIndex].Tag);
			Event.Tags.AppendTags(Context);
			if (const UGameplayAbility* Ability = Table.Bindings[BindingIndex].Ability.GetDefaultObject())
			{
				Event.Tags.AppendTags(Ability->GetAssetTags());
			}
			Args.Outbox->Add(MoveTemp(Event));
		}
	}

	return Result;
}
