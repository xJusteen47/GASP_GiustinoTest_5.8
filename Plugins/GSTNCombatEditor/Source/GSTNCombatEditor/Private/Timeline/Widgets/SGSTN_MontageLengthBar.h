// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The warning bar above the timeline when the montage and the timeline disagree about their length (combat-m5-editor 2.1 item 6).
 * It offers Fit Total Frames to Montage, which is an explicit, undoable model command: the bar never changes TotalFrames by itself,
 * and it appears only when the lengths differ. It loads the montage once per path, as the fit command does when asked.
 */
class SGSTN_MontageLengthBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_MontageLengthBar) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGSTN_MontageLengthBar() override;

	/** The montage length in frames the bar compares with, INDEX_NONE for no montage */
	int32 GetMontageFrames() const { return MontageFrames; }

	/** Is the warning showing? */
	bool IsMismatched() const;

private:
	/** Re-reads the montage length after the data changed */
	void Recompute();

	TSharedPtr<FGSTN_TimelineEditorModel> Model;
	FDelegateHandle DataHandle;

	/** The montage the length was computed for, so a repaint does not load it again */
	FSoftObjectPath CachedPath;
	TWeakObjectPtr<class UAnimMontage> CachedMontage;
	int32 MontageFrames = INDEX_NONE;
};
