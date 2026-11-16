// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IConsoleVariable;
class SVerticalBox;

/**
 * The Debug Options window: the editor tab that gathers every debug switch of the GSTN plugins, in the spirit of the options window of the
 * Game Animation Sample (an editor tab whose buttons set console variables, next to the viewport instead of over the game).
 *
 * Nothing is listed by hand. The panel asks the console manager for every variable whose name starts with "gstn." and contains "Debug",
 * groups them by the second part of the name (gstn.<Group>.Debug...), and builds one row per variable from its type: a check box for a bool,
 * a check box (0 or 1) plus the exact value for an int, a number box for a float, a text box for a string. The help text of the variable is
 * the tooltip. Every row reads the variable each frame, so a change made in the console, a config file or a script shows here at once, and
 * every edit goes through IConsoleVariable::Set with the console priority, exactly as if typed: the variables stay the only source of truth.
 */
class SGSTN_DebugOptionsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_DebugOptionsPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The variables the panel lists, sorted by name: what the console has registered now (a test reads it) */
	static TArray<FString> CollectVariableNames();

	/** The group a name belongs to: the second part of gstn.<Group>.<...>, or "Other" */
	static FString GroupOf(const FString& VariableName);

private:
	/** Builds the rows again from the console (a plugin loaded later registers its variables later) */
	void Rebuild();

	TSharedRef<SWidget> MakeRow(const FString& Name, IConsoleVariable* Variable);

	FReply OnRefresh();
	FReply OnToggleGameplayDebugger();
	bool HasPlayWorld() const;
	void OnFilterChanged(const FText& Text);

	TSharedPtr<SVerticalBox> Rows;
	FString Filter;
};
