// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UObject;

// What a validation rule says (combat-m5-editor 9.1). The rule functions fill a report and know nothing about where it goes: the Data
// Validation validators, the Validate button of the timeline editor (Message Log) and, from E11, the validation panel all read the same
// findings. This header includes no Slate and no widget type, on purpose: a rule, a test or a commandlet must be able to produce a report
// without a UI (test GSTNCombat.Editor.Validate.ReportIsWidgetFree reads this file).

/** How bad a finding is. Errors invalidate the asset; warnings and infos never do (open question Q12 of the M5 spec) */
enum class EGSTN_Severity : uint8
{
	Error,
	Warning,
	Info
};

/**
 * Where in the asset a finding points, for the panel to select it and put the playhead on it. Every part is optional: a finding about
 * the whole timeline sets none of them, one about a track sets TrackIndex, one about a key sets both.
 */
struct GSTNCOMBATEDITOR_API FGSTN_FindingLocation
{
	/** Index in the timeline's Tracks, INDEX_NONE when the finding is not about a track */
	int32 TrackIndex = INDEX_NONE;

	/** Index of the key in the track, INDEX_NONE when it is not about a key */
	int32 KeyIndex = INDEX_NONE;

	/** The frame the finding is about, INDEX_NONE when it has none */
	int32 Frame = INDEX_NONE;

	bool IsSet() const { return TrackIndex != INDEX_NONE || KeyIndex != INDEX_NONE || Frame != INDEX_NONE; }

	static FGSTN_FindingLocation OnTrack(int32 InTrackIndex, int32 InFrame = INDEX_NONE)
	{
		FGSTN_FindingLocation Location;
		Location.TrackIndex = InTrackIndex;
		Location.Frame = InFrame;
		return Location;
	}

	static FGSTN_FindingLocation OnFrame(int32 InFrame)
	{
		FGSTN_FindingLocation Location;
		Location.Frame = InFrame;
		return Location;
	}
};

/** One thing a rule found */
struct GSTNCOMBATEDITOR_API FGSTN_ValidationFinding
{
	EGSTN_Severity Severity = EGSTN_Severity::Error;

	/** The rule, e.g. Timeline.FrameRange, Slot.Ambiguous. Stable: a project can key settings and filters on it */
	FName Code;

	/** One sentence for the designer */
	FText Message;

	/**
	 * More lines than the message holds, shown by the Show action of the Message Log entry. The slot findings put the input patterns
	 * of the slots here, in the notation of M1 section 4. Empty for most findings.
	 */
	FText Detail;

	/** The asset the finding belongs to. Weak: a report outlives a closed editor */
	TWeakObjectPtr<UObject> Asset;

	FGSTN_FindingLocation Where;
};

/** The findings of one run of the rules */
struct GSTNCOMBATEDITOR_API FGSTN_ValidationReport
{
	TArray<FGSTN_ValidationFinding> Findings;

	/** Adds a finding and returns it, so the caller can fill Detail */
	FGSTN_ValidationFinding& Add(EGSTN_Severity Severity, FName Code, FText Message, UObject* Asset = nullptr, const FGSTN_FindingLocation& Where = FGSTN_FindingLocation());

	void Append(const FGSTN_ValidationReport& Other);

	int32 Num() const { return Findings.Num(); }
	bool IsEmpty() const { return Findings.IsEmpty(); }

	int32 CountSeverity(EGSTN_Severity Severity) const;
	int32 CountCode(FName Code) const;
	bool HasCode(FName Code) const { return CountCode(Code) > 0; }

	/** The first finding with the code, null when there is none */
	const FGSTN_ValidationFinding* FindCode(FName Code) const;

	bool HasErrors() const { return CountSeverity(EGSTN_Severity::Error) > 0; }

	/** Errors invalidate an asset, warnings do not */
	bool IsValid() const { return !HasErrors(); }

	/** Sets Asset on every finding that has none, for a rule that does not know the asset it runs on */
	void SetAssetWhereMissing(UObject* Asset);
};
