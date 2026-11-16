// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_HitboxOverlay.h"

#include "Preview/GSTN_GizmoMath.h"
#include "Preview/GSTN_HitboxGizmoTarget.h"
#include "Preview/GSTN_HitboxHitProxy.h"
#include "Preview/GSTN_PreviewDrawing.h"
#include "SceneManagement.h"
#include "Timeline/GSTN_TimelineEditorModel.h"

namespace
{
	/** One colour per HitGroup (a hitbox group is at most a handful in a move); more groups wrap around */
	const FLinearColor& GroupColor(uint8 Group)
	{
		static const FLinearColor Palette[] = {
			FLinearColor(1.f, 0.35f, 0.25f), FLinearColor(1.f, 0.65f, 0.1f), FLinearColor(0.95f, 0.9f, 0.2f), FLinearColor(0.6f, 0.9f, 0.25f),
			FLinearColor(0.2f, 0.85f, 0.85f), FLinearColor(0.35f, 0.55f, 1.f), FLinearColor(0.75f, 0.45f, 1.f), FLinearColor(1.f, 0.45f, 0.8f)
		};
		return Palette[Group % UE_ARRAY_COUNT(Palette)];
	}

	class FHitboxOverlay final : public IGSTN_PreviewOverlay
	{
	public:
		virtual FName GetOverlayName() const override { return TEXT("GSTN.Hitboxes"); }

		virtual void Draw(const FGSTN_PreviewDrawContext& Context, FPrimitiveDrawInterface* PDI) const override
		{
			const FGSTN_CombatTimelineData* Data = Context.Model != nullptr ? Context.Model->GetData() : nullptr;
			if (PDI == nullptr || Data == nullptr || !Context.bShowHitboxes)
			{
				return;
			}
			// The scratch array keeps its capacity between redraws: no allocation once it has grown
			Active.Reset();
			const FGSTN_TimelinePreviewScene* Scene = Context.Scene;
			GSTNGizmo::CollectActiveHitboxes(*Data, Context.Frame, FGSTN_HitboxGizmoTarget::GetRootTransform(),
				[Scene](FName Socket, FTransform& OutTransform) { return FGSTN_HitboxGizmoTarget::ResolveLiveSocket(Scene, Socket, OutTransform); }, Active);

			FGSTN_HitboxGizmoTarget::FLane Selected;
			const bool bHasSelection = FGSTN_HitboxGizmoTarget::FindSelectedLane(*Context.Model, Selected);
			const bool bHitTesting = PDI->IsHitTesting();
			for (const GSTNGizmo::FActiveHitbox& Hitbox : Active)
			{
				const bool bSelected = bHasSelection && Selected.Track == Hitbox.Track && Selected.Spec == Hitbox.Lane;
				if (bHitTesting)
				{
					PDI->SetHitProxy(new HGSTN_HitboxProxy(Hitbox.Track, Hitbox.Lane));
				}
				GSTNPreviewDrawing::DrawHitShape(PDI, Hitbox.Shape, Hitbox.Space, GroupColor(Hitbox.HitGroup), bSelected ? 2.5f : 0.f);
				if (bHitTesting)
				{
					PDI->SetHitProxy(nullptr);
				}
			}
		}

	private:
		mutable TArray<GSTNGizmo::FActiveHitbox> Active;
	};
}

void GSTNHitboxOverlay::Register()
{
	FGSTN_PreviewOverlayRegistry::Register(MakeShared<FHitboxOverlay>());
}

void GSTNHitboxOverlay::Unregister()
{
	FGSTN_PreviewOverlayRegistry::Unregister(TEXT("GSTN.Hitboxes"));
}
