// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_GizmoEdMode.h"

#include "EditorModeRegistry.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

const FEditorModeID FGSTN_GizmoEdMode::ModeId(TEXT("GSTNCombatGizmoEdMode"));

void FGSTN_GizmoEdMode::Register()
{
	FEditorModeRegistry::Get().RegisterMode<FGSTN_GizmoEdMode>(ModeId, LOCTEXT("GizmoModeName", "GSTN Combat Gizmo"), FSlateIcon(), false);
}

void FGSTN_GizmoEdMode::Unregister()
{
	FEditorModeRegistry::Get().UnregisterMode(ModeId);
}

bool FGSTN_GizmoEdMode::UsesTransformWidget(UE::Widget::EWidgetMode CheckMode) const
{
	return CheckMode == UE::Widget::WM_Translate || CheckMode == UE::Widget::WM_Rotate || CheckMode == UE::Widget::WM_Scale;
}

#undef LOCTEXT_NAMESPACE
