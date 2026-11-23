// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Hits/GSTN_HitData.h"

class IPropertyHandle;

/**
 * The details row of an FGSTN_HitData (combat-m5-editor 5.1, plan E12). Everything the struct has is still here, in groups instead of one flat list:
 *
 *  - Damage and shield, Knockback, Hitstun, Hitstop, Stagger, Hit (priority, type, effects, cue, custom data), Impact, Extensions;
 *  - in Knockback, an angle dial for AngleDeg (drag the needle; one undo step per drag) with the elevation and the direction it makes on the movement
 *    plane written under it, and a sentence under SpecialAngle and YawSource saying what each value does;
 *  - the hit type as a gameplay tag combo limited to Combat.HitType;
 *  - the extension data through the standard struct array editor, so a plugin's own customization of its struct (the parts editor, S 12b) shows inside it;
 *  - a Preview knockback line: this hit on a test victim (percent, weight, grounded), computed by the knockback model of the default combat rules.
 *
 * The data is never changed except through the property handles, so every edit is the usual transaction and undo. The header line is a summary of
 * the hit for the collapsed row. Registered for the struct by the module, so it applies wherever a hit is edited: the details of a hitbox in the
 * timeline editor, the values of a relationship variant, a hit stored in any asset.
 */
class FGSTN_HitDataCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** The struct the module registers this for */
	static FName GetStructName();

	/**
	 * Every property of FGSTN_HitData the row lays out, in the order of its groups. A test compares it with the struct's own properties, so a field
	 * added to the hit data later cannot be silently missing from the row (the whole point of keeping every field editable here)
	 */
	static TArray<FName> GetLaidOutProperties();

	//~ IPropertyTypeCustomization
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	/**
	 * The value of the struct a handle shows, false when there is none or several (several objects selected). Static and taking the handle, so
	 * the widgets of the row capture the handle and not this object, which the details view may drop before them
	 */
	static bool GetHit(const TSharedPtr<IPropertyHandle>& SourceHandle, FGSTN_HitData& OutHit);

	/** The struct's child by name */
	TSharedPtr<IPropertyHandle> Child(FName Name) const;

	/** The dial: reads and writes AngleDeg through the handle, a drag is one transaction */
	TSharedRef<SWidget> MakeAngleWidget() const;

	/** The enum a handle holds, as its raw byte */
	static uint8 GetEnumValue(const TSharedPtr<IPropertyHandle>& EnumHandle);

	/** The Preview knockback row: test victim fields and the line */
	TSharedRef<SWidget> MakePreviewWidget() const;

	TSharedPtr<IPropertyHandle> Handle;
};
