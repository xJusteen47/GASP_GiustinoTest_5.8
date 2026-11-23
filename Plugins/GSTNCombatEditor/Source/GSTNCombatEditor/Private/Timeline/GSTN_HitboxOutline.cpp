// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_HitboxOutline.h"

#include "Preview/GSTN_GizmoMath.h"
#include "Timeline/GSTN_HitboxKeyedAccess.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineTypes.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	const FGSTN_TimelineTrack_Hitbox* GetHitboxTrack(const FGSTN_CombatTimelineData& Data, int32 Track)
	{
		return Data.GetHitboxTrack(Track);
	}

	/** 1-based position of a Hitbox track among the Hitbox tracks */
	int32 GetGroupNumber(const FGSTN_CombatTimelineData& Data, int32 Track)
	{
		int32 Number = 0;
		for (int32 Index = 0; Index <= Track && Index < Data.Tracks.Num(); ++Index)
		{
			Number += Data.GetHitboxTrack(Index) != nullptr ? 1 : 0;
		}
		return Number;
	}

	FText DescribeShape(const FGSTN_HitboxSpec& Spec)
	{
		const FVector& Extent = Spec.Shape.Extent;
		switch (Spec.Shape.Type)
		{
		case EGSTN_ShapeType::Sphere:
			return FText::Format(LOCTEXT("OutlineSphere", "Sphere r {0} cm"), FText::AsNumber(FMath::RoundToInt(Extent.X)));
		case EGSTN_ShapeType::Capsule:
			return FText::Format(LOCTEXT("OutlineCapsule", "Capsule r {0}, half height {1} cm"), FText::AsNumber(FMath::RoundToInt(Extent.X)), FText::AsNumber(FMath::RoundToInt(Extent.Z)));
		default:
			return FText::Format(LOCTEXT("OutlineBox", "Box {0} x {1} x {2} cm"), FText::AsNumber(FMath::RoundToInt(Extent.X * 2.0)), FText::AsNumber(FMath::RoundToInt(Extent.Y * 2.0)), FText::AsNumber(FMath::RoundToInt(Extent.Z * 2.0)));
		}
	}

	FText DescribeSpace(const FGSTN_TimelineTrack_Hitbox& Track, const FGSTN_HitboxSpec& Spec)
	{
		// A child of the group says so: its space is the group's
		if (Track.IsParentedToGroup(Spec))
		{
			return LOCTEXT("OutlineChildOfGroup", "child of group");
		}
		switch (Spec.Space)
		{
		case EGSTN_HitboxSpace::BakedSocket:
			return FText::Format(LOCTEXT("OutlineSocket", "socket {0}"), FText::FromName(Spec.Socket));
		case EGSTN_HitboxSpace::WorldAtMoveStart:
			return LOCTEXT("OutlineWorld", "stays where thrown");
		case EGSTN_HitboxSpace::Projectile:
			return LOCTEXT("OutlineProjectile", "projectile");
		default:
			return LOCTEXT("OutlineRoot", "root");
		}
	}
}

TArray<FGSTN_HitboxOutlineItem> GSTNHitboxOutline::Build(const FGSTN_CombatTimelineData& Data)
{
	TArray<FGSTN_HitboxOutlineItem> Items;
	for (int32 Track = 0; Track < Data.Tracks.Num(); ++Track)
	{
		if (const FGSTN_TimelineTrack_Hitbox* Hitbox = GetHitboxTrack(Data, Track))
		{
			Items.Emplace(Track, INDEX_NONE);
			for (int32 Spec = 0; Spec < Hitbox->Hitboxes.Num(); ++Spec)
			{
				Items.Emplace(Track, Spec);
			}
		}
	}
	return Items;
}

FText GSTNHitboxOutline::GetName(const FGSTN_CombatTimelineData& Data, const FGSTN_HitboxOutlineItem& Item)
{
	const FGSTN_TimelineTrack_Hitbox* Hitbox = GetHitboxTrack(Data, Item.Track);
	if (Hitbox == nullptr)
	{
		return FText::GetEmpty();
	}
	if (Item.IsGroup())
	{
		// The designer's name when the group has one (owner 2026-11-16), else its number
		return Hitbox->GroupName.IsNone()
			? FText::Format(LOCTEXT("OutlineGroupName", "Group {0}"), FText::AsNumber(GetGroupNumber(Data, Item.Track)))
			: FText::FromName(Hitbox->GroupName);
	}
	if (!Hitbox->Hitboxes.IsValidIndex(Item.Spec))
	{
		return FText::GetEmpty();
	}
	const FGSTN_HitboxSpec& Spec = Hitbox->Hitboxes[Item.Spec];
	const FText Name = Spec.Name.IsNone() ? FText::Format(LOCTEXT("OutlineHitboxName", "Hitbox {0}"), FText::AsNumber(Item.Spec + 1)) : FText::FromName(Spec.Name);
	switch (Spec.Zone)
	{
	case EGSTN_HitboxZone::Sweet:
		return FText::Format(LOCTEXT("OutlineSweet", "{0} (sweet)"), Name);
	case EGSTN_HitboxZone::Sour:
		return FText::Format(LOCTEXT("OutlineSour", "{0} (sour)"), Name);
	default:
		return Name;
	}
}

FText GSTNHitboxOutline::GetSummary(const FGSTN_CombatTimelineData& Data, const FGSTN_HitboxOutlineItem& Item)
{
	const FGSTN_TimelineTrack_Hitbox* Hitbox = GetHitboxTrack(Data, Item.Track);
	if (Hitbox == nullptr)
	{
		return FText::GetEmpty();
	}
	if (Item.IsGroup())
	{
		// The last open frame, as the timeline shows it (EndFrame is exclusive)
		FText Summary = FText::Format(LOCTEXT("OutlineGroupSummary", "frames {0}-{1}, {2} {2}|plural(one=hitbox,other=hitboxes)"),
			FText::AsNumber(Hitbox->StartFrame), FText::AsNumber(FMath::Max(Hitbox->EndFrame - 1, Hitbox->StartFrame)), FText::AsNumber(Hitbox->Hitboxes.Num()));
		if (!Hitbox->Part.PartName.IsNone())
		{
			Summary = FText::Format(LOCTEXT("OutlineGroupPart", "{0}, part {1}"), Summary, FText::FromName(Hitbox->Part.PartName));
		}
		if (Hitbox->bUseGroupTransform)
		{
			Summary = Hitbox->GroupSpace == EGSTN_HitboxSpace::BakedSocket
				? FText::Format(LOCTEXT("OutlineGroupOnSocket", "{0}, on socket {1}"), Summary, FText::FromName(Hitbox->GroupSocket))
				: FText::Format(LOCTEXT("OutlineGroupTransform", "{0}, group transform"), Summary);
		}
		return Summary;
	}
	if (!Hitbox->Hitboxes.IsValidIndex(Item.Spec))
	{
		return FText::GetEmpty();
	}
	const FGSTN_HitboxSpec& Spec = Hitbox->Hitboxes[Item.Spec];
	return FText::Format(LOCTEXT("OutlineHitboxSummary", "{0}, {1}, hit group {2}, {3} {3}|plural(one=key,other=keys)"),
		DescribeShape(Spec), DescribeSpace(*Hitbox, Spec), FText::AsNumber(static_cast<int32>(Spec.HitGroup)), FText::AsNumber(Spec.Keys.Num()));
}

bool GSTNHitboxOutline::FindSelected(const FGSTN_TimelineEditorModel& Model, FGSTN_HitboxOutlineItem& OutItem)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	if (Data == nullptr)
	{
		return false;
	}
	for (const FGSTN_Selection& Selected : Model.GetSelection())
	{
		const FGSTN_TimelineTrack_Hitbox* Hitbox = GetHitboxTrack(*Data, Selected.TrackIndex);
		if (Hitbox == nullptr)
		{
			continue;
		}
		if (!Selected.IsKey())
		{
			OutItem = FGSTN_HitboxOutlineItem(Selected.TrackIndex, INDEX_NONE);
			return true;
		}
		int32 Spec = INDEX_NONE;
		int32 KeyInSpec = INDEX_NONE;
		if (FGSTN_HitboxKeyedAccess::Locate(*Hitbox, Selected.KeyIndex, Spec, KeyInSpec))
		{
			OutItem = FGSTN_HitboxOutlineItem(Selected.TrackIndex, Spec);
			return true;
		}
	}
	return false;
}

bool GSTNHitboxOutline::Focus(FGSTN_TimelineEditorModel& Model, const FGSTN_HitboxOutlineItem& Item)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	const FGSTN_TimelineTrack_Hitbox* Hitbox = Data != nullptr ? GetHitboxTrack(*Data, Item.Track) : nullptr;
	if (Hitbox == nullptr || (!Item.IsGroup() && !Hitbox->Hitboxes.IsValidIndex(Item.Spec)))
	{
		return false;
	}

	// Copies: the selection and playhead calls below broadcast, and a listener may touch the data
	const int32 Start = Hitbox->StartFrame;
	const int32 LastOpen = FMath::Max(Hitbox->EndFrame - 1, Start);
	const TArray<FGSTN_HitboxKey> Keys = Item.IsGroup() ? TArray<FGSTN_HitboxKey>() : Hitbox->Hitboxes[Item.Spec].Keys;

	// Outside the window nothing is drawn: go to the first key of the hitbox inside the window, else to the window's first frame
	const int32 Playhead = Model.GetPlayhead();
	if (Playhead < Start || Playhead > LastOpen)
	{
		const int32 FirstKey = Keys.IsEmpty() ? 0 : Keys[0].Frame;
		Model.SetPlayhead(FMath::Clamp(Start + FirstKey, Start, LastOpen));
	}

	if (Item.IsGroup())
	{
		const FGSTN_Selection One[] = { FGSTN_Selection::Window(Item.Track) };
		Model.SetSelection(One);
		return true;
	}

	// As a click in the viewport: the key on the playhead, else the nearest, so the gizmo sits on the hitbox
	const int32 KeyInSpec = GSTNGizmo::FindKeyForPick(Keys, Model.GetPlayhead() - Start);
	const FGSTN_TimelineTrack_Hitbox* Current = Model.GetData() != nullptr ? GetHitboxTrack(*Model.GetData(), Item.Track) : nullptr;
	if (KeyInSpec != INDEX_NONE && Current != nullptr)
	{
		Model.SelectKey(Item.Track, FGSTN_HitboxKeyedAccess::ToFlat(*Current, Item.Spec, KeyInSpec));
	}
	else
	{
		const FGSTN_Selection One[] = { FGSTN_Selection::Window(Item.Track) };
		Model.SetSelection(One);
	}
	return true;
}

FText GSTNHitboxOutline::GetEditableName(const FGSTN_CombatTimelineData& Data, const FGSTN_HitboxOutlineItem& Item)
{
	const FGSTN_TimelineTrack_Hitbox* Hitbox = GetHitboxTrack(Data, Item.Track);
	if (Hitbox == nullptr)
	{
		return FText::GetEmpty();
	}
	if (Item.IsGroup())
	{
		return Hitbox->GroupName.IsNone() ? FText::GetEmpty() : FText::FromName(Hitbox->GroupName);
	}
	return Hitbox->Hitboxes.IsValidIndex(Item.Spec) && !Hitbox->Hitboxes[Item.Spec].Name.IsNone() ? FText::FromName(Hitbox->Hitboxes[Item.Spec].Name) : FText::GetEmpty();
}

bool GSTNHitboxOutline::IsValidName(const FText& NewName, FText& OutError)
{
	if (NewName.ToString().TrimStartAndEnd().Len() >= NAME_SIZE)
	{
		OutError = LOCTEXT("OutlineNameTooLong", "The name is too long.");
		return false;
	}
	return true;
}

bool GSTNHitboxOutline::Rename(FGSTN_TimelineEditorModel& Model, const FGSTN_HitboxOutlineItem& Item, const FText& NewName)
{
	FText Error;
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	const FGSTN_TimelineTrack_Hitbox* Hitbox = Data != nullptr ? GetHitboxTrack(*Data, Item.Track) : nullptr;
	if (Hitbox == nullptr || (!Item.IsGroup() && !Hitbox->Hitboxes.IsValidIndex(Item.Spec)) || !IsValidName(NewName, Error))
	{
		return false;
	}
	const FString Trimmed = NewName.ToString().TrimStartAndEnd();
	const FName Name = Trimmed.IsEmpty() ? NAME_None : FName(*Trimmed);
	const FName Current = Item.IsGroup() ? Hitbox->GroupName : Hitbox->Hitboxes[Item.Spec].Name;
	// FName compares without case: a change of case only is still a rename the designer wants to see
	if (Current.IsEqual(Name, ENameCase::CaseSensitive))
	{
		return false;
	}
	const int32 Spec = Item.Spec;
	const bool bGroup = Item.IsGroup();
	return Model.ApplyTrackEdit(Item.Track, [Spec, bGroup, Name](FGSTN_TimelineTrack& Edited)
	{
		FGSTN_TimelineTrack_Hitbox& Track = static_cast<FGSTN_TimelineTrack_Hitbox&>(Edited);
		if (bGroup)
		{
			Track.GroupName = Name;
		}
		else if (Track.Hitboxes.IsValidIndex(Spec))
		{
			Track.Hitboxes[Spec].Name = Name;
		}
	}, bGroup ? LOCTEXT("RenameHitboxGroup", "Rename hitbox group") : LOCTEXT("RenameHitbox", "Rename hitbox"));
}

FGSTN_HitboxOutlineItem GSTNHitboxOutline::AddHitbox(FGSTN_TimelineEditorModel& Model, int32 TrackIndex)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	const FGSTN_TimelineTrack_Hitbox* Hitbox = Data != nullptr ? GetHitboxTrack(*Data, TrackIndex) : nullptr;
	if (Hitbox == nullptr)
	{
		return FGSTN_HitboxOutlineItem();
	}
	const int32 NewSpec = Hitbox->Hitboxes.Num();
	// Appended last: the flat indices of the keys already selected do not move
	if (!Model.ApplyTrackEdit(TrackIndex, [](FGSTN_TimelineTrack& Edited)
		{
			FGSTN_TimelineTrack_Hitbox& Track = static_cast<FGSTN_TimelineTrack_Hitbox&>(Edited);
			FGSTN_HitboxSpec Spec = FGSTN_HitboxKeyedAccess::MakeDefaultSpec();
			// A child of a group with a transform starts on the group itself, not in front of the root
			if (Track.bUseGroupTransform)
			{
				for (FGSTN_HitboxKey& Key : Spec.Keys)
				{
					Key.Offset = FVector::ZeroVector;
				}
			}
			Track.Hitboxes.Add(MoveTemp(Spec));
		}, LOCTEXT("AddHitboxToGroup", "Add hitbox")))
	{
		return FGSTN_HitboxOutlineItem();
	}
	const FGSTN_HitboxOutlineItem Added(TrackIndex, NewSpec);
	Focus(Model, Added);
	return Added;
}

FGSTN_HitboxOutlineItem GSTNHitboxOutline::DuplicateHitbox(FGSTN_TimelineEditorModel& Model, const FGSTN_HitboxOutlineItem& Item)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	const FGSTN_TimelineTrack_Hitbox* Hitbox = Data != nullptr ? GetHitboxTrack(*Data, Item.Track) : nullptr;
	if (Hitbox == nullptr || Item.IsGroup() || !Hitbox->Hitboxes.IsValidIndex(Item.Spec))
	{
		return FGSTN_HitboxOutlineItem();
	}
	const int32 NewSpec = Hitbox->Hitboxes.Num();
	const int32 Source = Item.Spec;
	if (!Model.ApplyTrackEdit(Item.Track, [Source](FGSTN_TimelineTrack& Edited)
		{
			FGSTN_TimelineTrack_Hitbox& Track = static_cast<FGSTN_TimelineTrack_Hitbox&>(Edited);
			FGSTN_HitboxSpec Copy = Track.Hitboxes[Source];
			if (!Copy.Name.IsNone())
			{
				Copy.Name = FName(*(Copy.Name.ToString() + TEXT(" copy")));
			}
			Track.Hitboxes.Add(MoveTemp(Copy));
		}, LOCTEXT("DuplicateHitbox", "Duplicate hitbox")))
	{
		return FGSTN_HitboxOutlineItem();
	}
	const FGSTN_HitboxOutlineItem Added(Item.Track, NewSpec);
	Focus(Model, Added);
	return Added;
}

bool GSTNHitboxOutline::DeleteHitbox(FGSTN_TimelineEditorModel& Model, const FGSTN_HitboxOutlineItem& Item)
{
	const FGSTN_CombatTimelineData* Data = Model.GetData();
	const FGSTN_TimelineTrack_Hitbox* Hitbox = Data != nullptr ? GetHitboxTrack(*Data, Item.Track) : nullptr;
	if (Hitbox == nullptr || Item.IsGroup() || !Hitbox->Hitboxes.IsValidIndex(Item.Spec) || Hitbox->Hitboxes.Num() <= 1)
	{
		return false;
	}
	// The keys after the removed hitbox change their flat index: a selection kept across the edit would point at other keys
	Model.ClearSelection();
	const int32 Spec = Item.Spec;
	const bool bRemoved = Model.ApplyTrackEdit(Item.Track, [Spec](FGSTN_TimelineTrack& Edited)
	{
		FGSTN_TimelineTrack_Hitbox& Track = static_cast<FGSTN_TimelineTrack_Hitbox&>(Edited);
		if (Track.Hitboxes.IsValidIndex(Spec))
		{
			Track.Hitboxes.RemoveAt(Spec);
		}
	}, LOCTEXT("DeleteHitbox", "Delete hitbox"));
	if (bRemoved)
	{
		Focus(Model, FGSTN_HitboxOutlineItem(Item.Track, INDEX_NONE));
	}
	return bRemoved;
}

#undef LOCTEXT_NAMESPACE
