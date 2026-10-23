// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_PreviewDrawing.h"

#include "PrimitiveDrawingUtils.h"
#include "SceneManagement.h"

namespace
{
	TArray<TSharedRef<IGSTN_PreviewOverlay>>& Overlays()
	{
		static TArray<TSharedRef<IGSTN_PreviewOverlay>> Registered;
		return Registered;
	}
}

void FGSTN_PreviewOverlayRegistry::Register(const TSharedRef<IGSTN_PreviewOverlay>& Overlay)
{
	Unregister(Overlay->GetOverlayName());
	Overlays().Add(Overlay);
}

void FGSTN_PreviewOverlayRegistry::Unregister(FName OverlayName)
{
	Overlays().RemoveAll([OverlayName](const TSharedRef<IGSTN_PreviewOverlay>& Overlay) { return Overlay->GetOverlayName() == OverlayName; });
}

int32 FGSTN_PreviewOverlayRegistry::Num()
{
	return Overlays().Num();
}

void FGSTN_PreviewOverlayRegistry::DrawAll(const FGSTN_PreviewDrawContext& Context, FPrimitiveDrawInterface* PDI)
{
	for (const TSharedRef<IGSTN_PreviewOverlay>& Overlay : Overlays())
	{
		Overlay->Draw(Context, PDI);
	}
}

void GSTNPreviewDrawing::DrawGrid(FPrimitiveDrawInterface* PDI, float FloorZ, float Spacing, int32 HalfCells)
{
	const FLinearColor Minor(0.32f, 0.32f, 0.34f);
	const FLinearColor Major(0.5f, 0.5f, 0.54f);
	const float Extent = Spacing * HalfCells;
	for (int32 I = -HalfCells; I <= HalfCells; ++I)
	{
		const FLinearColor& Color = (I == 0) ? Major : Minor;
		const float Offset = I * Spacing;
		PDI->DrawLine(FVector(-Extent, Offset, FloorZ), FVector(Extent, Offset, FloorZ), Color, SDPG_World, 0.f, 0.f);
		PDI->DrawLine(FVector(Offset, -Extent, FloorZ), FVector(Offset, Extent, FloorZ), Color, SDPG_World, 0.f, 0.f);
	}
}

void GSTNPreviewDrawing::DrawRootCapsule(FPrimitiveDrawInterface* PDI, const FGSTN_PreviewCapsule& Capsule)
{
	DrawWireCapsule(PDI, Capsule.Center, FVector::ForwardVector, FVector::RightVector, FVector::UpVector, FLinearColor(0.2f, 0.85f, 0.35f),
		Capsule.Radius, Capsule.HalfHeight, 16, SDPG_World, 1.f);
}

void GSTNPreviewDrawing::DrawFacingArrow(FPrimitiveDrawInterface* PDI, const FGSTN_PreviewCapsule& Capsule)
{
	const FVector From = Capsule.Center;
	const FVector To = From + FVector::ForwardVector * (Capsule.Radius + Capsule.HalfHeight);
	DrawDirectionalArrow(PDI, FTranslationMatrix(From), FLinearColor(1.f, 0.85f, 0.1f), (To - From).Size(), 6.f, SDPG_World, 1.5f);
}
