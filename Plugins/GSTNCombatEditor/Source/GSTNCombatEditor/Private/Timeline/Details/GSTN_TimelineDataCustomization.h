// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Timeline/GSTN_TimelineEditContext.h"

class IPropertyHandle;

/**
 * The details row of an FGSTN_CombatTimelineData (combat-m5-editor 2.1 item 3): the frame data on one line and an Open Timeline Editor
 * button, over the same child rows as before, so the data stays editable in the panel as it always was. Used in the Class Defaults of an
 * ability Blueprint (the inline Timeline property, which opens the editor in inline mode) and on a timeline asset (Data).
 *
 * The button needs a target it can resolve: a timeline asset, or the live default object of an ability Blueprint. Anywhere else (an
 * instance, a native class, a struct shown on its own, several objects selected) the row shows the frame data line and no button,
 * so the customization is harmless on every FGSTN_CombatTimelineData in the editor.
 */
class FGSTN_TimelineDataCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** The struct the module registers this for */
	static FName GetStructName();

	/**
	 * What the row opens: the context of the single object the property belongs to, invalid when there is none the editor can open.
	 * Only the property the ability and the asset keep their data in counts (Timeline, Data), not any other struct of the same type.
	 */
	static FGSTN_TimelineEditContext ResolveContext(const IPropertyHandle& PropertyHandle);

	//~ IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	/** The frame data line of the value the row shows, empty for several objects */
	FText MakeSummary() const;

	/** The button shows while the row has a target to open */
	EVisibility GetOpenVisibility() const;

	FReply OnOpenClicked() const;

	TSharedPtr<IPropertyHandle> Handle;
};
