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

	FText DescribeSpace(const FGSTN_HitboxSpec& Spec)
	{
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
		return FText::Format(LOCTEXT("OutlineGroupName", "Group {0}"), FText::AsNumber(GetGroupNumber(Data, Item.Track)));
	}
	if (!Hitbox->Hitboxes.IsValidIndex(Item.Spec))
	{
		return FText::GetEmpty();
	}
	const FGSTN_HitboxSpec& Spec = Hitbox->Hitboxes[Item.Spec];
	const FText Name = FText::Format(LOCTEXT("OutlineHitboxName", "Hitbox {0}"), FText::AsNumber(Item.Spec + 1));
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
		return Summary;
	}
	if (!Hitbox->Hitboxes.IsValidIndex(Item.Spec))
	{
		return FText::GetEmpty();
	}
	const FGSTN_HitboxSpec& Spec = Hitbox->Hitboxes[Item.Spec];
	return FText::Format(LOCTEXT("OutlineHitboxSummary", "{0}, {1}, hit group {2}, {3} {3}|plural(one=key,other=keys)"),
		DescribeShape(Spec), DescribeSpace(Spec), FText::AsNumber(static_cast<int32>(Spec.HitGroup)), FText::AsNumber(Spec.Keys.Num()));
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

#undef LOCTEXT_NAMESPACE
