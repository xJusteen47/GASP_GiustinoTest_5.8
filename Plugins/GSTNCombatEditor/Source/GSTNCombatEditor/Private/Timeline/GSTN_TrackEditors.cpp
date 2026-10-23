// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TrackEditors.h"

#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "UObject/Class.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace GSTNTrackEditors
{
	namespace
	{
		/** How many tag leaves a bar shows before "+N": the bar is a few dozen pixels wide, the tooltip would be the place for the rest */
		constexpr int32 MaxLeavesShown = 3;

		FString LeafOf(const FGameplayTag& Tag)
		{
			const FString Name = Tag.GetTagName().ToString();
			int32 DotIndex = INDEX_NONE;
			return Name.FindLastChar(TEXT('.'), DotIndex) ? Name.Mid(DotIndex + 1) : Name;
		}

		/** Structs are named by the track struct they handle, so the built-ins find their struct once */
		template <typename TTrack>
		const UScriptStruct* StructOf()
		{
			return TTrack::StaticStruct();
		}

		/** CancelWindow: the label is the commands it cancels into, with the condition when it is not "always" */
		class FCancelWindowTrackEditor final : public IGSTN_TrackEditor
		{
		public:
			virtual const UScriptStruct* GetTrackStruct() const override { return StructOf<FGSTN_TimelineTrack_CancelWindow>(); }
			virtual FText GetDisplayName() const override { return LOCTEXT("CancelWindowName", "Cancel Window"); }
			virtual FName GetIconBrushName() const override { return TEXT("Icons.Next"); }

			virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const override
			{
				const FGSTN_TimelineTrack_CancelWindow& Window = static_cast<const FGSTN_TimelineTrack_CancelWindow&>(Track);
				TArray<FGameplayTag> Targets;
				Window.IntoCommands.GetGameplayTagArray(Targets);
				const FText Into = Targets.IsEmpty() ? LOCTEXT("CancelNoTarget", "(no target)") : JoinTagLeaves(Targets);
				switch (Window.Condition)
				{
				case EGSTN_CancelCondition::OnHit: return FText::Format(LOCTEXT("CancelOnHit", "{0}  [on hit]"), Into);
				case EGSTN_CancelCondition::OnBlock: return FText::Format(LOCTEXT("CancelOnBlock", "{0}  [on block]"), Into);
				case EGSTN_CancelCondition::OnWhiff: return FText::Format(LOCTEXT("CancelOnWhiff", "{0}  [on whiff]"), Into);
				default: return Into;
				}
			}
		};

		/** Movement: the velocity curve it plays and how it mixes with the character's own velocity */
		class FMovementTrackEditor final : public IGSTN_TrackEditor
		{
		public:
			virtual const UScriptStruct* GetTrackStruct() const override { return StructOf<FGSTN_TimelineTrack_Movement>(); }
			virtual FText GetDisplayName() const override { return LOCTEXT("MovementName", "Movement"); }
			virtual FName GetIconBrushName() const override { return TEXT("Icons.Transform"); }

			virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const override
			{
				const FGSTN_TimelineTrack_Movement& Move = static_cast<const FGSTN_TimelineTrack_Movement&>(Track);
				const FText Mix = Move.MixMode == EGSTN_MoveMixMode::Additive ? LOCTEXT("MovementAdditive", "additive") : LOCTEXT("MovementOverride", "override");
				if (Move.VelocityCurve.IsNull())
				{
					return FText::Format(LOCTEXT("MovementNoCurve", "Movement (no curve, {0})"), Mix);
				}
				return FText::Format(LOCTEXT("MovementCurve", "{0} ({1})"), FText::FromString(Move.VelocityCurve.GetAssetName()), Mix);
			}
		};

		/** Tags: the tags the owner carries while the window is open */
		class FTagsTrackEditor final : public IGSTN_TrackEditor
		{
		public:
			virtual const UScriptStruct* GetTrackStruct() const override { return StructOf<FGSTN_TimelineTrack_Tags>(); }
			virtual FText GetDisplayName() const override { return LOCTEXT("TagsName", "Tags"); }
			virtual FName GetIconBrushName() const override { return TEXT("Icons.Details"); }

			virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const override
			{
				const FGSTN_TimelineTrack_Tags& Tags = static_cast<const FGSTN_TimelineTrack_Tags&>(Track);
				TArray<FGameplayTag> All;
				Tags.Tags.GetGameplayTagArray(All);
				return All.IsEmpty() ? LOCTEXT("TagsNone", "(no tags)") : JoinTagLeaves(All);
			}
		};

		/** Event: a diamond on one frame; the cue and the gameplay event it fires */
		class FEventTrackEditor final : public IGSTN_TrackEditor
		{
		public:
			virtual const UScriptStruct* GetTrackStruct() const override { return StructOf<FGSTN_TimelineTrack_Event>(); }
			virtual FText GetDisplayName() const override { return LOCTEXT("EventName", "Event"); }
			virtual FName GetIconBrushName() const override { return TEXT("Icons.Event"); }
			virtual int32 GetDefaultLength() const override { return 1; }

			virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const override
			{
				const FGSTN_TimelineTrack_Event& Event = static_cast<const FGSTN_TimelineTrack_Event&>(Track);
				const bool bCue = Event.CueTag.IsValid();
				const bool bEvent = Event.GameplayEventTag.IsValid();
				if (bCue && bEvent)
				{
					return FText::Format(LOCTEXT("EventCueAndEvent", "Cue {0}, event {1}"), FText::FromString(LeafOf(Event.CueTag)), FText::FromString(LeafOf(Event.GameplayEventTag)));
				}
				if (bCue)
				{
					return FText::Format(LOCTEXT("EventCue", "Cue {0}"), FText::FromString(LeafOf(Event.CueTag)));
				}
				if (bEvent)
				{
					return FText::Format(LOCTEXT("EventEvent", "Event {0}"), FText::FromString(LeafOf(Event.GameplayEventTag)));
				}
				return LOCTEXT("EventEmpty", "Event (nothing set)");
			}
		};

		/** WeaponSwitch: a diamond on SwapFrame, the frame the weapon changes. The swap frame follows the frame the track is added at */
		class FWeaponSwitchTrackEditor final : public IGSTN_TrackEditor
		{
		public:
			virtual const UScriptStruct* GetTrackStruct() const override { return StructOf<FGSTN_TimelineTrack_WeaponSwitch>(); }
			virtual FText GetDisplayName() const override { return LOCTEXT("WeaponSwitchName", "Weapon Switch"); }
			virtual FName GetIconBrushName() const override { return TEXT("Icons.Convert"); }
			virtual int32 GetDefaultLength() const override { return 1; }

			virtual void InitNewTrack(FGSTN_TimelineTrack& Track, int32 AddedAtFrame, const FGSTN_CombatTimelineData& Data) const override
			{
				// The switch keeps its own copy of the frame: a default of 0 would swap at the start whatever the track says
				static_cast<FGSTN_TimelineTrack_WeaponSwitch&>(Track).SwapFrame = AddedAtFrame;
			}

			virtual void Validate(const FGSTN_TimelineTrack& Track, const FGSTN_CombatTimelineData& Data, FGSTN_ValidationReport& Report) const override
			{
				// The editor draws and moves the switch on SwapFrame and the runtime fires it there; a different StartFrame is a stale copy
				const FGSTN_TimelineTrack_WeaponSwitch& Switch = static_cast<const FGSTN_TimelineTrack_WeaponSwitch&>(Track);
				if (Switch.StartFrame != Switch.SwapFrame)
				{
					Report.Add(EGSTN_Severity::Warning, TEXT("WeaponSwitch.FrameMismatch"),
						FText::Format(LOCTEXT("SwitchFrameMismatch", "The weapon switch starts at frame {0} but swaps at frame {1}. It swaps at {1}; moving it in the editor sets both."),
							FText::AsNumber(Switch.StartFrame), FText::AsNumber(Switch.SwapFrame)),
						nullptr, FGSTN_FindingLocation::OnFrame(Switch.SwapFrame));
				}
			}

			virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const override
			{
				const FGSTN_TimelineTrack_WeaponSwitch& Switch = static_cast<const FGSTN_TimelineTrack_WeaponSwitch&>(Track);
				switch (Switch.Target)
				{
				case EGSTN_SwitchTarget::Next: return LOCTEXT("SwitchNext", "Next weapon");
				case EGSTN_SwitchTarget::Previous: return LOCTEXT("SwitchPrevious", "Previous weapon");
				default: return FText::Format(LOCTEXT("SwitchIndex", "Weapon {0}"), FText::AsNumber(Switch.Index));
				}
			}
		};

		TArray<const UScriptStruct*>& RegisteredByBuiltIns()
		{
			static TArray<const UScriptStruct*> Instance;
			return Instance;
		}

		void RegisterOne(TSharedRef<const IGSTN_TrackEditor> Editor)
		{
			FGSTN_TrackEditorRegistry::Get().Register(Editor);
			RegisteredByBuiltIns().Add(Editor->GetTrackStruct());
		}
	}

	FText JoinTagLeaves(const TArray<FGameplayTag>& Tags)
	{
		FString Joined;
		const int32 Shown = FMath::Min(Tags.Num(), MaxLeavesShown);
		for (int32 Index = 0; Index < Shown; ++Index)
		{
			Joined += Index == 0 ? LeafOf(Tags[Index]) : FString(TEXT(", ")) + LeafOf(Tags[Index]);
		}
		if (Tags.Num() > Shown)
		{
			return FText::Format(LOCTEXT("TagLeavesMore", "{0} +{1}"), FText::FromString(Joined), FText::AsNumber(Tags.Num() - Shown));
		}
		return FText::FromString(Joined);
	}

	void RegisterBuiltIns()
	{
		RegisterOne(MakeShared<FCancelWindowTrackEditor>());
		RegisterOne(MakeShared<FMovementTrackEditor>());
		RegisterOne(MakeShared<FTagsTrackEditor>());
		RegisterOne(MakeShared<FEventTrackEditor>());
		RegisterOne(MakeShared<FWeaponSwitchTrackEditor>());
	}

	void UnregisterBuiltIns()
	{
		for (const UScriptStruct* Struct : RegisteredByBuiltIns())
		{
			FGSTN_TrackEditorRegistry::Get().Unregister(Struct);
		}
		RegisteredByBuiltIns().Reset();
	}
}

#undef LOCTEXT_NAMESPACE
