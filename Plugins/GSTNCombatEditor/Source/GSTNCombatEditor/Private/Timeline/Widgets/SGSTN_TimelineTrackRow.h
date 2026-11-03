// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STableRow.h"
#include "Timeline/GSTN_FindingsStore.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "Timeline/Widgets/GSTN_TimelineWidgetCommon.h"

/** One row of the list: which track it shows. The list is rebuilt when tracks are added, removed or replaced, never to follow a move */
struct FGSTN_TimelineRowData
{
	int32 TrackIndex = INDEX_NONE;

	/** The struct of the track when the row was made: the panel compares it to see that the rows still match the data */
	TWeakObjectPtr<const UScriptStruct> TrackStruct;
};

/**
 * A row of the timeline: the fixed-width name column (colour swatch, track name, a "+" that adds a key on a track that has them)
 * and the lane to its right. Clicking the name selects the track. Built by the panel's list view, one per track.
 */
class SGSTN_TimelineTrackRow : public STableRow<TSharedPtr<FGSTN_TimelineRowData>>
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelineTrackRow) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineGesture>, Gesture)
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelinePaintStats>, PaintStats)
		SLATE_ARGUMENT(TSharedPtr<FGSTN_FindingsStore>, Findings)
		SLATE_ARGUMENT(TWeakPtr<SWidget>, FocusTarget)
		SLATE_ARGUMENT(int32, TrackIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable);
};
