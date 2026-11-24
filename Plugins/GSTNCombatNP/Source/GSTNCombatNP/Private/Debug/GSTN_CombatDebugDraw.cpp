// Combat simulation (GAS-NPP fork): debug draw of the hit geometry.

#include "Debug/GSTN_CombatDebugDraw.h"

#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Hits/GSTN_HitboxTypes.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "MoverComponent.h"
#include "World/GSTN_StepSnapshot.h"

static TAutoConsoleVariable<int32> CVarCombatDebugHitboxes(TEXT("gstn.Combat.Debug.Hitboxes"), 0,
	TEXT("1 draws the world shape of every open hitbox of the combat timelines, one colour per space."));
static TAutoConsoleVariable<int32> CVarCombatDebugHurtboxes(TEXT("gstn.Combat.Debug.Hurtboxes"), 0,
	TEXT("1 draws the hurtboxes of the combat entities: green normal, grey intangible, yellow invincible, orange armor."));
static TAutoConsoleVariable<int32> CVarCombatDebugHitLog(TEXT("gstn.Combat.Debug.HitLog"), 0,
	TEXT("1 makes the attacker step keep a log of its hits and a trace of its hitbox centres (UGSTN_CombatSimComponent::GetHitLog / GetHitboxTrace)."));

static TAutoConsoleVariable<float> CVarCombatDebugHitboxLinger(TEXT("gstn.Combat.Debug.HitboxLinger"), 0.35f,
	TEXT("Seconds the last open hitboxes stay drawn (darker) after they close, so a 2-3 frame active window can be seen. 0 = only while open."));

bool GSTNCombatDebug::IsHitboxDrawOn() { return CVarCombatDebugHitboxes.GetValueOnAnyThread() != 0; }
float GSTNCombatDebug::GetHitboxLingerSeconds() { return FMath::Max(0.f, CVarCombatDebugHitboxLinger.GetValueOnAnyThread()); }
static TAutoConsoleVariable<int32> CVarCombatDebugBody(TEXT("gstn.Combat.Debug.Body"), 0,
	TEXT("1 draws the body of each fighter: the real movement capsule (white), the pushbox cylinder (blue) and its rigid core (orange)."));

bool GSTNCombatDebug::IsBodyDrawOn() { return CVarCombatDebugBody.GetValueOnAnyThread() != 0; }

static TAutoConsoleVariable<int32> CVarCombatDebugFollowMesh(TEXT("gstn.Combat.Debug.FollowMesh"), 1,
	TEXT("Where the hitbox/hurtbox/body debug shapes are drawn. 1 (default) on the presented, smoothed mesh: they stay on the body. ")
	TEXT("0 on the simulated root (fixed tick): where the hits really are, up to a tick apart from the mesh."));

FVector GSTNCombatDebug::GetPresentationOffset(const AActor* Owner)
{
	if (CVarCombatDebugFollowMesh.GetValueOnGameThread() == 0 || !Owner)
	{
		return FVector::ZeroVector;
	}
	const UMoverComponent* Mover = Owner->FindComponentByClass<UMoverComponent>();
	const USceneComponent* Root = Mover ? Mover->GetUpdatedComponent() : nullptr;
	const USceneComponent* Visual = Mover ? Mover->GetPrimaryVisualComponent() : nullptr;
	if (!Root || !Visual || Visual == Root || Mover->SmoothingMode != EMoverSmoothingMode::VisualComponentOffset)
	{
		return FVector::ZeroVector;
	}
	// Mover's own smoothed root (2026-11-24): the shapes follow the smoothing, not the hitstop shake or the body capsule's mesh correction
	if (Mover->HasSmoothedPresentation())
	{
		return Mover->GetPresentedTransform().GetLocation() - Root->GetComponentLocation();
	}
	// Mover writes Visual = Base * SmoothedRoot (FinalizeSmoothingFrame), so SmoothedRoot = Base^-1 * Visual
	const FTransform Presented = Mover->GetBaseVisualComponentTransform().Inverse() * Visual->GetComponentTransform();
	return Presented.GetLocation() - Root->GetComponentLocation();
}
bool GSTNCombatDebug::IsHurtboxDrawOn() { return CVarCombatDebugHurtboxes.GetValueOnAnyThread() != 0; }
bool GSTNCombatDebug::IsHitLogOn() { return CVarCombatDebugHitLog.GetValueOnAnyThread() != 0; }

namespace
{
	void DrawShape(const UWorld* World, const FGSTN_HitShape& Shape, const FTransform& Carrier, const FColor& Color, float Thickness = 1.f)
	{
#if ENABLE_DRAW_DEBUG
		const FTransform World_ = Shape.Local * Carrier;
		const FVector Center = World_.GetLocation();
		const FQuat Rotation = World_.GetRotation();
		const FVector Scale = World_.GetScale3D().GetAbs();
		switch (Shape.Type)
		{
		case EGSTN_ShapeType::Sphere:
			DrawDebugSphere(World, Center, Shape.Extent.X * Scale.GetMax(), 12, Color, false, 0.f, 0, Thickness);
			break;
		case EGSTN_ShapeType::Capsule:
			DrawDebugCapsule(World, Center, Shape.Extent.Z * Scale.Z, Shape.Extent.X * FMath::Max(Scale.X, Scale.Y), Rotation, Color, false, 0.f, 0, Thickness);
			break;
		case EGSTN_ShapeType::Box:
			DrawDebugBox(World, Center, Shape.Extent * Scale, Rotation, Color, false, 0.f, 0, Thickness);
			break;
		}
#endif
	}
}

void GSTNCombatDebug::DrawHitboxes(const UWorld* World, const TArray<FGSTN_AttackerDebugShape>& Shapes, bool bFaded, const FVector& Offset)
{
	static const FColor SpaceColors[] = { FColor::Red, FColor::Magenta, FColor::Cyan, FColor::Purple };
	for (const FGSTN_AttackerDebugShape& Debug : Shapes)
	{
		FColor Color = SpaceColors[FMath::Min<int32>(Debug.Space, 3)];
		if (bFaded)
		{
			Color = FColor(Color.R / 2, Color.G / 2, Color.B / 2);
		}
		FTransform Carrier = Debug.World;
		// Shapes that ride the attacker follow its presented mesh; a hitbox left in the world where it was thrown stays there
		if (Debug.Space == static_cast<uint8>(EGSTN_HitboxSpace::Root) || Debug.Space == static_cast<uint8>(EGSTN_HitboxSpace::BakedSocket))
		{
			Carrier.AddToTranslation(Offset);
		}
		DrawShape(World, Debug.Shape, Carrier, Color, bFaded ? 1.f : 2.5f);
	}
}

void GSTNCombatDebug::DrawHurtboxes(const UWorld* World, const FGSTN_SnapshotEntry& Entry, const FVector& Offset)
{
	if (!Entry.Hurtboxes)
	{
		return;
	}
	FTransform Carrier = Entry.Transform;
	Carrier.AddToTranslation(Offset);
	for (int32 Index = 0; Index < Entry.Hurtboxes->Hurtboxes.Num(); ++Index)
	{
		FColor Color = FColor::Green;
		if (Entry.HurtStates.IsValidIndex(Index))
		{
			switch (Entry.HurtStates[Index].State)
			{
			case EGSTN_HurtState::Intangible: Color = FColor(128, 128, 128); break;
			case EGSTN_HurtState::Invincible: Color = FColor::Yellow; break;
			case EGSTN_HurtState::Armor: Color = FColor::Orange; break;
			default: break;
			}
		}
		// The posed shape (bones, detail level); a hurtbox the level leaves out is not drawn
		if (Entry.IsHurtboxPresent(Index))
		{
			DrawShape(World, Entry.GetHurtShape(Index), Carrier, Color);
		}
	}
	// The move's and the kit's own hurtbox shapes, in cyan: they count as the hurtbox they name
	for (const FGSTN_PosedHurtboxExtension& Extension : Entry.HurtPose.Extensions)
	{
		DrawShape(World, Extension.Shape, Carrier, FColor::Cyan);
	}
}

void GSTNCombatDebug::DrawBody(const UWorld* World, const FGSTN_SnapshotEntry* Entry, const UCapsuleComponent* MovementCapsule, const FVector& Offset)
{
#if ENABLE_DRAW_DEBUG
	if (MovementCapsule)
	{
		DrawDebugCapsule(World, MovementCapsule->GetComponentLocation() + Offset, MovementCapsule->GetScaledCapsuleHalfHeight(),
			MovementCapsule->GetScaledCapsuleRadius(), MovementCapsule->GetComponentQuat(), FColor::White, false, 0.f, SDPG_Foreground, 1.5f);
	}
	if (Entry && Entry->Pushbox.IsActive())
	{
		const FGSTN_PushboxCylinder& Push = Entry->Pushbox;
		const FVector Half(0.0, 0.0, Push.HalfHeight);
		const FVector Center = Push.Center + Offset;
		DrawDebugCylinder(World, Center - Half, Center + Half, Push.Radius, 20, FColor(60, 140, 255), false, 0.f, SDPG_Foreground, 1.5f);
		if (Push.CoreRadius > 0.f)
		{
			DrawDebugCylinder(World, Center - Half, Center + Half, Push.CoreRadius, 16, FColor(255, 140, 0), false, 0.f, SDPG_Foreground, 2.5f);
		}
	}
#endif
}
