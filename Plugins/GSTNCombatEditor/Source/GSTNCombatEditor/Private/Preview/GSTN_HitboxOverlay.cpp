// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_HitboxOverlay.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "Hits/GSTN_HurtboxPose.h"
#include "Preview/GSTN_GizmoMath.h"
#include "Preview/GSTN_HitboxGizmoTarget.h"
#include "Preview/GSTN_HitboxHitProxy.h"
#include "Preview/GSTN_PreviewDrawing.h"
#include "Preview/GSTN_TimelinePreviewScene.h"
#include "SceneManagement.h"
#include "Timeline/GSTN_TimelineEditorModel.h"

namespace
{
	/** One colour per HitGroup (a hitbox group is at most a handful in a move); more groups wrap around */
	/** The colour of a move's own hurtbox shapes: the green of the hurtbox set editor, not in the hitbox group palette */
	const FLinearColor& HurtboxTrackColor()
	{
		static const FLinearColor Color(0.2f, 1.f, 0.45f);
		return Color;
	}

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
				// A Hurtbox track (the move's own hurtboxes, owner 2026-11-16) is edited like a hitbox group but drawn in the hurtbox green, so the two never read alike
				const FGSTN_TimelineTrack* Owner = Data->Tracks.IsValidIndex(Hitbox.Track) ? Data->Tracks[Hitbox.Track].GetPtr<FGSTN_TimelineTrack>() : nullptr;
				const bool bHurtbox = Owner != nullptr && Owner->GetKind() == EGSTN_TimelineTrackKind::Hurtbox;
				GSTNPreviewDrawing::DrawHitShape(PDI, Hitbox.Shape, Hitbox.Space, bHurtbox ? HurtboxTrackColor() : GroupColor(Hitbox.HitGroup), bSelected ? 2.5f : 0.f);
				if (!bHurtbox && !bHitTesting)
				{
					DrawLaunchArrow(PDI, *Data, Hitbox, bSelected);
				}
				if (bHitTesting)
				{
					PDI->SetHitProxy(nullptr);
				}
			}
		}

	private:
		/**
		 * The knockback preview in the viewport: an arrow from the hitbox centre along the launch its hit data gives a victim in front of the
		 * attacker (root frame: +X facing, +Z up), elevation AngleDeg and the sideways YawOffsetDeg, the same UGSTN_HitMathUtils::LaunchDirection the
		 * knockback step calls. Hitbox-relative yaw sources (away from the hitbox, along its motion) read a victim straight ahead and no motion here
		 */
		static void DrawLaunchArrow(FPrimitiveDrawInterface* PDI, const FGSTN_CombatTimelineData& Data, const GSTNGizmo::FActiveHitbox& Hitbox, bool bSelected)
		{
			const FGSTN_TimelineTrack_Hitbox* Track = Data.Tracks.IsValidIndex(Hitbox.Track) ? Data.Tracks[Hitbox.Track].GetPtr<FGSTN_TimelineTrack_Hitbox>() : nullptr;
			if (Track == nullptr || !Track->Hitboxes.IsValidIndex(Hitbox.Lane))
			{
				return;
			}
			const FGSTN_HitData& Hit = Track->Hitboxes[Hitbox.Lane].Data;
			const FVector Facing = FVector::ForwardVector;
			const FVector Direction = UGSTN_HitMathUtils::LaunchDirection(Hit.YawSource, Hit.AngleDeg, Hit.SpecialAngle, FVector::UpVector, Facing, Facing,
				FVector::ZeroVector, /*bVictimGrounded*/ true, Hit.BaseKnockback, FGSTN_SakuraiParams(), Hit.YawOffsetDeg);
			if (Direction.IsNearlyZero())
			{
				return;
			}
			const FVector From = (Hitbox.Shape.Local * Hitbox.Space).GetLocation();
			const float Length = bSelected ? 90.f : 60.f;
			const FMatrix Frame = FRotationTranslationMatrix(Direction.GetSafeNormal().Rotation(), From);
			DrawDirectionalArrow(PDI, Frame, FLinearColor(1.f, 0.55f, 0.f), Length, bSelected ? 10.f : 7.f, SDPG_Foreground, bSelected ? 2.5f : 1.5f);
		}

		mutable TArray<GSTNGizmo::FActiveHitbox> Active;
	};
}

namespace
{
	/**
	 * The hurtboxes of the previewed character at the playhead (owner 2026-11-16): the set made for the preview mesh (its PreviewMesh), posed by the
	 * same FGSTN_HurtboxPose the simulation uses, so following bones, the move's detail level and its Hurtbox tracks show as in game. The bones come
	 * from the timeline's bake: an unbaked timeline shows the reference pose (save or bake it). Drawn with the Hitboxes toggle; no set found = nothing
	 */
	class FHurtboxOverlay final : public IGSTN_PreviewOverlay
	{
	public:
		virtual FName GetOverlayName() const override { return TEXT("GSTN.Hurtboxes"); }

		virtual void Draw(const FGSTN_PreviewDrawContext& Context, FPrimitiveDrawInterface* PDI) const override
		{
			const FGSTN_CombatTimelineData* Data = Context.Model != nullptr ? Context.Model->GetData() : nullptr;
			const UDebugSkelMeshComponent* MeshComponent = Context.Scene != nullptr ? Context.Scene->GetMeshComponent() : nullptr;
			const USkeletalMesh* Mesh = MeshComponent != nullptr ? MeshComponent->GetSkeletalMeshAsset() : nullptr;
			if (PDI == nullptr || Data == nullptr || Mesh == nullptr || !Context.bShowHitboxes || PDI->IsHitTesting())
			{
				return;
			}
			const UGSTN_HurtboxSetDefinition* Set = FindSetFor(*Mesh);
			if (Set == nullptr)
			{
				return;
			}
			FGSTN_HurtboxPoseInput Input;
			Input.Set = Set;
			Input.Timeline = Data;
			Input.TimelineFrame = Context.Frame;
			FGSTN_HurtboxPose::Build(Input, Pose);
			const FTransform Root = FGSTN_HitboxGizmoTarget::GetRootTransform();
			static const FLinearColor Body(0.2f, 0.85f, 0.35f, 1.f);
			for (int32 Index = 0; Index < Set->Hurtboxes.Num(); ++Index)
			{
				if (Pose.IsPresent(Index))
				{
					GSTNPreviewDrawing::DrawHitShape(PDI, Pose.GetShape(*Set, Index), Root, Body);
				}
			}
			// The move's own shapes are drawn by the hitbox overlay (selectable, editable); only the kit's would land here, and kits are not previewed
		}

	private:
		/** The set whose PreviewMesh is Mesh, looked up once per mesh (the asset registry is not asked on every redraw) */
		const UGSTN_HurtboxSetDefinition* FindSetFor(const USkeletalMesh& Mesh) const
		{
			if (CachedMesh.Get() != &Mesh)
			{
				CachedMesh = &Mesh;
				CachedSet.Reset();
				IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
				TArray<FAssetData> Assets;
				Registry.GetAssetsByClass(UGSTN_HurtboxSetDefinition::StaticClass()->GetClassPathName(), Assets, true);
				// Sorted by path, so two sets for one mesh always resolve to the same one
				Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });
				const FSoftObjectPath MeshPath(&Mesh);
				for (const FAssetData& Asset : Assets)
				{
					const UGSTN_HurtboxSetDefinition* Candidate = Cast<UGSTN_HurtboxSetDefinition>(Asset.GetAsset());
					if (Candidate != nullptr && Candidate->PreviewMesh.ToSoftObjectPath() == MeshPath)
					{
						CachedSet = Candidate;
						break;
					}
				}
			}
			return CachedSet.Get();
		}

		mutable FGSTN_HurtboxPose Pose;
		mutable TWeakObjectPtr<const USkeletalMesh> CachedMesh;
		mutable TWeakObjectPtr<const UGSTN_HurtboxSetDefinition> CachedSet;
	};
}

void GSTNHitboxOverlay::Register()
{
	FGSTN_PreviewOverlayRegistry::Register(MakeShared<FHitboxOverlay>());
	FGSTN_PreviewOverlayRegistry::Register(MakeShared<FHurtboxOverlay>());
}

void GSTNHitboxOverlay::Unregister()
{
	FGSTN_PreviewOverlayRegistry::Unregister(TEXT("GSTN.Hitboxes"));
	FGSTN_PreviewOverlayRegistry::Unregister(TEXT("GSTN.Hurtboxes"));
}
