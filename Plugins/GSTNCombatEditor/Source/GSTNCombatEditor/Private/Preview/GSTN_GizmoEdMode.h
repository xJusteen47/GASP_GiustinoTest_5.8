// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "EdMode.h"

/**
 * The edit mode that lets the preview viewport draw and use the transform widget (spike S4, combat-m5-editor 15 risk 3). FWidget draws nothing unless an
 * active mode of the viewport's mode tools says it uses the transform widget; a viewport client with no mode shows no gizmo whatever it returns from
 * GetWidgetLocation. This mode does exactly that and nothing else: location, axes, deltas, clicks and keys all stay in FGSTN_TimelineViewportClient.
 *
 * Registered with the editor mode registry by the module, activated by the viewport client on its own mode tools.
 */
class FGSTN_GizmoEdMode : public FEdMode
{
public:
	static const FEditorModeID ModeId;

	/** Registers the mode (module startup) and removes it (shutdown) */
	static void Register();
	static void Unregister();

	//~ FEdMode
	virtual bool UsesTransformWidget() const override { return true; }
	virtual bool UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override { return true; }
};
