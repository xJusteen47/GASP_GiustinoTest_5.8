// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Hurtbox/GSTN_HurtboxGizmoTarget.h"

#include "Hurtbox/GSTN_HurtboxEditModel.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FGSTN_HurtboxGizmoTarget::FGSTN_HurtboxGizmoTarget(const TSharedRef<FGSTN_HurtboxEditModel>& InModel)
	: Model(InModel)
{
}

bool FGSTN_HurtboxGizmoTarget::GetWidgetFrame(FVector& OutLocation, FQuat& OutLocalRotation) const
{
	const TArray<FGSTN_Hurtbox>* Hurtboxes = Model->GetHurtboxes();
	const int32 Index = Model->GetSingleSelection();
	if (Hurtboxes == nullptr || !Hurtboxes->IsValidIndex(Index))
	{
		return false;
	}
	const FTransform& Local = (*Hurtboxes)[Index].Shape.Local;
	OutLocation = Local.GetLocation();
	OutLocalRotation = Local.GetRotation();
	return true;
}

void FGSTN_HurtboxGizmoTarget::CollectPickables(TArray<GSTNGizmo::FPickCandidate>& Out) const
{
	const TArray<FGSTN_Hurtbox>* Hurtboxes = Model->GetHurtboxes();
	if (Hurtboxes == nullptr)
	{
		return;
	}
	for (int32 Index = 0; Index < Hurtboxes->Num(); ++Index)
	{
		GSTNGizmo::FPickCandidate& Candidate = Out.AddDefaulted_GetRef();
		Candidate.Track = 0;
		Candidate.Lane = Index;
		Candidate.Shape = (*Hurtboxes)[Index].Shape;
	}
}

bool FGSTN_HurtboxGizmoTarget::Pick(int32 Track, int32 Lane)
{
	if (Track != 0 || Lane < 0 || Lane >= Model->Num())
	{
		return false;
	}
	const int32 One[] = { Lane };
	Model->SetSelection(One);
	return true;
}

bool FGSTN_HurtboxGizmoTarget::BeginDrag(bool bOffsetAllKeys)
{
	const TArray<FGSTN_Hurtbox>* Hurtboxes = Model->GetHurtboxes();
	const int32 Index = Model->GetSingleSelection();
	if (bDragOpen || Hurtboxes == nullptr || !Hurtboxes->IsValidIndex(Index) || !Model->BeginEdit(LOCTEXT("EditHurtboxGizmo", "Edit hurtbox with gizmo")))
	{
		return false;
	}
	bDragOpen = true;
	DragIndex = Index;
	StartShape = (*Hurtboxes)[Index].Shape;
	return true;
}

bool FGSTN_HurtboxGizmoTarget::ApplyDelta(const GSTNGizmo::FDragDelta& Delta)
{
	if (!bDragOpen)
	{
		return false;
	}
	FGSTN_HitShape Shape = StartShape;
	// The root is the world and the shape's Local is its placement in it: the identity is the space, and the key maths is the shape's own
	Shape.Local = GSTNGizmo::ApplyDragToKey(StartShape.Local, FTransform::Identity, Delta);
	if (!Delta.Scale.IsNearlyZero(1e-9))
	{
		Shape.Extent = GSTNGizmo::ScaleExtent(StartShape, Delta.Scale);
	}
	return Model->SetShape(DragIndex, Shape);
}

void FGSTN_HurtboxGizmoTarget::EndDrag()
{
	if (bDragOpen)
	{
		bDragOpen = false;
		Model->EndEdit();
	}
}

void FGSTN_HurtboxGizmoTarget::CancelDrag()
{
	if (bDragOpen)
	{
		bDragOpen = false;
		Model->CancelEdit();
	}
}

FText FGSTN_HurtboxGizmoTarget::GetStatusText() const
{
	if (Model->GetSingleSelection() == INDEX_NONE)
	{
		return LOCTEXT("HurtboxGizmoNone", "Select one hurtbox (click it, or pick it in the list) to move it with the gizmo.");
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
