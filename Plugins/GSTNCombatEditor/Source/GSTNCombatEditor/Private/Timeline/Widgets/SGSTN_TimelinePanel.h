// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Timeline/GSTN_FindingsStore.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "Timeline/GSTN_TimelineLayout.h"
#include "Timeline/Widgets/GSTN_TimelineWidgetCommon.h"
#include "Timeline/Widgets/SGSTN_TimelineRuler.h"

class SScrollBar;
struct FGSTN_TimelineRowData;

/**
 * The frame timeline (combat-m5-editor 5.2): a toolbar, the ruler, the phase bar, one row per track in a list view, and the frame
 * data line. It is a view of an FGSTN_TimelineEditorModel and nothing else: every gesture goes through the model (one transaction
 * each, the drag helper for drags), and the panel owns only what is not data, the commands, the playback ticker and the gesture
 * shared by its lanes. The toolkit (FGSTN_TimelineEditorToolkit) hosts it in its Timeline tab.
 *
 * Keyboard: the panel takes the focus when a lane, the ruler or the phase bar is clicked and runs the chords of
 * FGSTN_TimelineEditorCommands through its command list, so they follow the user's key bindings. Escape cancels a drag.
 */
class SGSTN_TimelinePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelinePanel) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
		/** The findings the lanes mark at their frame (red for errors, yellow for warnings). Optional: without it the lanes draw no markers */
		SLATE_ARGUMENT(TSharedPtr<FGSTN_FindingsStore>, Findings)
		/** Shows the paint time of the last repaint in the footer (the acceptance readout of E4) */
		SLATE_ARGUMENT(bool, bShowPaintStats)
		/** Controls the host adds to the end of the toolbar (the toolkit: loop range, preview skin and kit, validate). The panel knows nothing of them */
		SLATE_NAMED_SLOT(FArguments, ToolbarExtra)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGSTN_TimelinePanel() override;

	//~ SWidget
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	const TSharedRef<FUICommandList>& GetCommandList() const { return CommandList; }

	bool IsPlaying() const { return bPlaying; }
	void SetPlaying(bool bInPlaying);

	/** Width of the lane area (the ruler's), 0 before the first arrangement */
	double GetLaneWidth() const;

private:
	void BindCommands();

	/** Rebuilds the rows when the tracks no longer match them (added, removed, or replaced by undo) */
	void HandleDataChanged();
	void RebuildRows();

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FGSTN_TimelineRowData> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<SWidget> MakeToolbar(const TSharedRef<SWidget>& ToolbarExtra);
	TSharedRef<SWidget> MakeCommandButton(const TSharedPtr<FUICommandInfo>& Command);
	TSharedRef<SWidget> MakeAddTrackMenu();

	/** The FTSTicker callback of playback: whole frames at the real tick rate, looped over the timeline */
	bool TickPlayback(float DeltaSeconds);

	/** Keeps Slate awake while playing, so the playhead redraws on a machine whose idle editor throttles */
	EActiveTimerReturnType KeepAwake(double CurrentTime, float DeltaTime);

	/** Frames the whole timeline once the lane width is known */
	EActiveTimerReturnType FitInitialView(double CurrentTime, float DeltaTime);

	TSharedPtr<FGSTN_TimelineEditorModel> Model;
	TSharedPtr<FGSTN_TimelineGesture> Gesture;
	TSharedPtr<FGSTN_TimelinePaintStats> PaintStats;
	TSharedPtr<FGSTN_FindingsStore> Findings;
	TSharedRef<FUICommandList> CommandList = MakeShared<FUICommandList>();

	TArray<TSharedPtr<FGSTN_TimelineRowData>> Rows;
	TSharedPtr<SListView<TSharedPtr<FGSTN_TimelineRowData>>> RowList;
	TSharedPtr<SScrollBar> RowScrollBar;
	TSharedPtr<SGSTN_TimelineRuler> Ruler;

	FDelegateHandle DataChangedHandle;
	FTSTicker::FDelegateHandle PlaybackHandle;
	FGSTN_PlaybackClock PlaybackClock;
	bool bPlaying = false;
};
