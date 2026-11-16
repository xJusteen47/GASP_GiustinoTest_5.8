// Pure combat logic (no GAS, no Network Prediction).

#include "Rules/GSTN_GroundCrouch.h"
#include "Input/GSTN_CombatButtonTags.h"

namespace GSTNCombatTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Crouch, "Combat.State.Crouch", "Crouched on the ground: grounded with the kit's crouch button held (FGSTN_GroundCrouchKitExtension). Projected by the combat tick, never granted");
}

FGSTN_GroundCrouchKitExtension::FGSTN_GroundCrouchKitExtension()
{
	FGSTN_MovementHurtboxShape& Body = CrouchHurtboxShapes.AddDefaulted_GetRef();
	Body.Name = TEXT("CrouchBody");
	Body.Shape = MakeDefaultCrouchBody();
}

FGSTN_HitShape FGSTN_GroundCrouchKitExtension::MakeDefaultCrouchBody()
{
	// Root = the crouched capsule centre, 55 above the feet with the default capsule: -55 .. +45 is feet .. 100 cm
	FGSTN_HitShape Body = FGSTN_MovementHurtboxShape::MakeDefaultBody();
	Body.Extent = FVector(38.0, 38.0, 50.0);
	Body.Local = FTransform(FVector(0.0, 0.0, -5.0));
	return Body;
}

FGameplayTag FGSTN_GroundCrouchKitExtension::ResolveHoldButton() const
{
	return HoldButton.IsValid() ? HoldButton : GSTNCombatButtonTags::Button_Crouch.GetTag();
}

bool FGSTN_GroundCrouchKitExtension::HoldsStance(bool bGrounded, bool bHeld, bool bInMove, bool bActionLocked, bool bWasCrouched, const FGameplayTagContainer& OwnerTags) const
{
	if (!bEnabled || !bGrounded || !bHeld || bActionLocked)
	{
		return false;
	}
	// A move keeps a crouch it started in, never starts one
	if (bInMove && (!bKeepStanceDuringMoves || !bWasCrouched))
	{
		return false;
	}
	return BlockedTags.IsEmpty() || !OwnerTags.HasAny(BlockedTags);
}

bool FGSTN_GroundCrouchKitExtension::HoldsTag(bool bGrounded, bool bHeld, bool bInMove, bool bActionLocked, bool bWasCrouched, const FGameplayTagContainer& OwnerTags) const
{
	if (bInMove && !bKeepTagDuringMoves)
	{
		return false;
	}
	return HoldsStance(bGrounded, bHeld, bInMove, bActionLocked, bWasCrouched, OwnerTags);
}

FGSTN_BodyCapsuleShape FGSTN_GroundCrouchKitExtension::MakeCapsule(float HalfHeight, float Radius, const FGSTN_BodyCapsuleShape& Standing)
{
	FGSTN_BodyCapsuleShape Shape;
	Shape.Radius = Radius > 0.f ? Radius : Standing.Radius;
	// Never taller than standing (a crouch that grows would be a bug in the data), never under the radius (Unreal clamps it there)
	const float StandingHalf = Standing.HalfHeight > 0.f ? Standing.HalfHeight : HalfHeight;
	Shape.HalfHeight = FMath::Max(FMath::Min(HalfHeight, StandingHalf), Shape.Radius);
	// Bottom on the mesh root (between the feet), as the standing capsule's (0, 0, its half height)
	Shape.Offset = FVector(0.0, 0.0, Shape.HalfHeight);
	return Shape;
}

FGSTN_MovementHurtboxExtension FGSTN_GroundCrouchKitExtension::MakeHurtbox(const FGSTN_MovementHurtboxExtension* KitMovement) const
{
	FGSTN_MovementHurtboxExtension Out = KitMovement ? *KitMovement : FGSTN_MovementHurtboxExtension();
	if (!CrouchHurtboxShapes.IsEmpty())
	{
		Out.Shapes = CrouchHurtboxShapes;
	}
	return Out;
}
