// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TrackEditors.h"

#include "Timeline/GSTN_HitboxKeyedAccess.h"
#include "Timeline/GSTN_TimelineColors.h"
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

		/** The part a hitbox or a hurtbox modifier is attached to, as words: the name, the group or the leaf of the tag */
		FText DescribeSelector(const FGSTN_PartSelector& Part)
		{
			TArray<FString> Words;
			if (!Part.PartName.IsNone())
			{
				Words.Add(Part.PartName.ToString());
			}
			if (!Part.GroupName.IsNone())
			{
				Words.Add(Part.GroupName.ToString());
			}
			if (Part.PartTag.IsValid())
			{
				Words.Add(LeafOf(Part.PartTag));
			}
			return Words.IsEmpty() ? LOCTEXT("NoPart", "(no part)") : FText::FromString(FString::Join(Words, TEXT("/")));
		}

		/** Hitbox: one window, several hitboxes. The label names the part and each hitbox by its group and where its space is */
		class FHitboxTrackEditor final : public IGSTN_TrackEditor
		{
		public:
			virtual const UScriptStruct* GetTrackStruct() const override { return StructOf<FGSTN_TimelineTrack_Hitbox>(); }
			virtual FText GetDisplayName() const override { return LOCTEXT("HitboxName", "Hitbox"); }
			virtual FName GetIconBrushName() const override { return TEXT("Icons.Target"); }
			virtual int32 GetDefaultLength() const override { return 3; }

			virtual void InitNewTrack(FGSTN_TimelineTrack& Track, int32 AddedAtFrame, const FGSTN_CombatTimelineData& Data) const override
			{
				// A hitbox track with no hitbox hits nothing and has no lane to put a key on: it starts with the default sphere in front of the root.
				// The part is left empty on purpose: it is the designer's choice, and the validator asks for it
				static_cast<FGSTN_TimelineTrack_Hitbox&>(Track).Hitboxes.Add(FGSTN_HitboxKeyedAccess::MakeDefaultSpec());
			}

			virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const override
			{
				const FGSTN_TimelineTrack_Hitbox& Hitbox = static_cast<const FGSTN_TimelineTrack_Hitbox&>(Track);
				const FText Part = DescribeSelector(Hitbox.Part);
				if (Hitbox.Hitboxes.IsEmpty())
				{
					return FText::Format(LOCTEXT("HitboxNoSpecs", "{0}: no hitbox"), Part);
				}
				TArray<FString> Specs;
				const int32 Shown = FMath::Min(Hitbox.Hitboxes.Num(), MaxLeavesShown);
				for (int32 Index = 0; Index < Shown; ++Index)
				{
					Specs.Add(DescribeSpec(Hitbox.Hitboxes[Index]).ToString());
				}
				const FText More = Hitbox.Hitboxes.Num() > Shown ? FText::Format(LOCTEXT("HitboxMore", " +{0}"), FText::AsNumber(Hitbox.Hitboxes.Num() - Shown)) : FText::GetEmpty();
				return FText::Format(LOCTEXT("HitboxLabel", "{0}: {1}{2}"), Part, FText::FromString(FString::Join(Specs, TEXT(", "))), More);
			}

		private:
			/** "G0" for a hitbox in root space, "G1 @hand_r" for a baked socket, with the zone when it is not the plain one */
			static FText DescribeSpec(const FGSTN_HitboxSpec& Spec)
			{
				FString Text = FString::Printf(TEXT("G%d"), static_cast<int32>(Spec.HitGroup));
				switch (Spec.Space)
				{
				case EGSTN_HitboxSpace::BakedSocket: Text += Spec.Socket.IsNone() ? FString(TEXT(" @?")) : FString(TEXT(" @")) + Spec.Socket.ToString(); break;
				case EGSTN_HitboxSpace::WorldAtMoveStart: Text += TEXT(" world"); break;
				case EGSTN_HitboxSpace::Projectile: Text += TEXT(" projectile"); break;
				default: break;
				}
				if (Spec.Zone == EGSTN_HitboxZone::Sweet)
				{
					Text += TEXT(" sweet");
				}
				else if (Spec.Zone == EGSTN_HitboxZone::Sour)
				{
					Text += TEXT(" sour");
				}
				return FText::FromString(Text);
			}
		};

		/** HurtboxModifier: a window coloured by the state it gives, labelled with the state and the hurtboxes it addresses */
		class FHurtboxModifierTrackEditor final : public IGSTN_TrackEditor
		{
		public:
			virtual const UScriptStruct* GetTrackStruct() const override { return StructOf<FGSTN_TimelineTrack_HurtboxModifier>(); }
			virtual FText GetDisplayName() const override { return LOCTEXT("HurtboxModifierName", "Hurtbox Modifier"); }
			virtual FName GetIconBrushName() const override { return TEXT("Icons.Lock"); }
			virtual int32 GetDefaultLength() const override { return 8; }

			virtual FLinearColor GetTrackColor(const FGSTN_TimelineTrack& Track) const override
			{
				return GSTNTimelineColors::HurtStateColor(static_cast<const FGSTN_TimelineTrack_HurtboxModifier&>(Track).Kind);
			}

			virtual FText GetRowLabel(const FGSTN_TimelineTrack& Track) const override
			{
				const FGSTN_TimelineTrack_HurtboxModifier& Modifier = static_cast<const FGSTN_TimelineTrack_HurtboxModifier&>(Track);
				FText State;
				switch (Modifier.Kind)
				{
				case EGSTN_HurtboxModifierKind::Intangible: State = LOCTEXT("StateIntangible", "Intangible"); break;
				case EGSTN_HurtboxModifierKind::Armor:
					State = Modifier.ArmorThreshold.bInfinite ? LOCTEXT("StateSuperArmor", "Super armor")
						: Modifier.ArmorThreshold.IsSet() ? FText::Format(LOCTEXT("StateArmor", "Armor >= {0}"), FText::AsNumber(Modifier.ArmorThreshold.Value))
						: LOCTEXT("StateArmorUnset", "Armor (no threshold)");
					break;
				default: State = LOCTEXT("StateInvincible", "Invincible"); break;
				}

				// The hurtboxes it addresses: the selector, the names and the leaves of the tags, a few of them
				TArray<FString> Targets;
				if (!Modifier.Part.IsEmpty())
				{
					Targets.Add(DescribeSelector(Modifier.Part).ToString());
				}
				for (const FName Name : Modifier.AffectedHurtboxes)
				{
					Targets.Add(Name.ToString());
				}
				for (const FGameplayTag& Tag : Modifier.AffectedTags)
				{
					Targets.Add(LeafOf(Tag));
				}
				if (Targets.IsEmpty())
				{
					return FText::Format(LOCTEXT("HurtboxModifierNoTarget", "{0}: (no hurtbox)"), State);
				}
				const int32 Shown = FMath::Min(Targets.Num(), MaxLeavesShown);
				const FString Joined = FString::Join(TArrayView<const FString>(Targets.GetData(), Shown), TEXT(", "));
				return Targets.Num() > Shown
					? FText::Format(LOCTEXT("HurtboxModifierMore", "{0}: {1} +{2}"), State, FText::FromString(Joined), FText::AsNumber(Targets.Num() - Shown))
					: FText::Format(LOCTEXT("HurtboxModifierLabel", "{0}: {1}"), State, FText::FromString(Joined));
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

		// The tracks of M4 (E5b). The hitbox track keeps its keys in lanes, one per hitbox: the model reaches them through this adapter
		RegisterOne(MakeShared<FHitboxTrackEditor>());
		RegisterOne(MakeShared<FHurtboxModifierTrackEditor>());
		FGSTN_KeyedTrackAccessRegistry::Register(FGSTN_TimelineTrack_Hitbox::StaticStruct(), MakeShared<FGSTN_HitboxKeyedAccess>());
	}

	void UnregisterBuiltIns()
	{
		FGSTN_KeyedTrackAccessRegistry::Unregister(FGSTN_TimelineTrack_Hitbox::StaticStruct());
		for (const UScriptStruct* Struct : RegisteredByBuiltIns())
		{
			FGSTN_TrackEditorRegistry::Get().Unregister(Struct);
		}
		RegisteredByBuiltIns().Reset();
	}
}

#undef LOCTEXT_NAMESPACE
