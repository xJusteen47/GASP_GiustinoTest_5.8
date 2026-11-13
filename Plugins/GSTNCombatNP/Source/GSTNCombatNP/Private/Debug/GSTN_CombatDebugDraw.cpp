// Combat simulation (GAS-NPP fork): debug draw of the hit geometry.

#include "Debug/GSTN_CombatDebugDraw.h"

#include "DrawDebugHelpers.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "World/GSTN_StepSnapshot.h"

static TAutoConsoleVariable<int32> CVarCombatDebugHitboxes(TEXT("gstn.Combat.Debug.Hitboxes"), 0,
	TEXT("1 draws the world shape of every open hitbox of the combat timelines, one colour per space."));
static TAutoConsoleVariable<int32> CVarCombatDebugHurtboxes(TEXT("gstn.Combat.Debug.Hurtboxes"), 0,
	TEXT("1 draws the hurtboxes of the combat entities: green normal, grey intangible, yellow invincible, orange armor."));
static TAutoConsoleVariable<int32> CVarCombatDebugHitLog(TEXT("gstn.Combat.Debug.HitLog"), 0,
	TEXT("1 makes the attacker step keep a log of its hits and a trace of its hitbox centres (UGSTN_CombatSimComponent::GetHitLog / GetHitboxTrace)."));

bool GSTNCombatDebug::IsHitboxDrawOn() { return CVarCombatDebugHitboxes.GetValueOnAnyThread() != 0; }
bool GSTNCombatDebug::IsHurtboxDrawOn() { return CVarCombatDebugHurtboxes.GetValueOnAnyThread() != 0; }
bool GSTNCombatDebug::IsHitLogOn() { return CVarCombatDebugHitLog.GetValueOnAnyThread() != 0; }

namespace
{
	void DrawShape(const UWorld* World, const FGSTN_HitShape& Shape, const FTransform& Carrier, const FColor& Color)
	{
#if ENABLE_DRAW_DEBUG
		const FTransform World_ = Shape.Local * Carrier;
		const FVector Center = World_.GetLocation();
		const FQuat Rotation = World_.GetRotation();
		const FVector Scale = World_.GetScale3D().GetAbs();
		switch (Shape.Type)
		{
		case EGSTN_ShapeType::Sphere:
			DrawDebugSphere(World, Center, Shape.Extent.X * Scale.GetMax(), 12, Color, false, 0.f, 0, 1.f);
			break;
		case EGSTN_ShapeType::Capsule:
			DrawDebugCapsule(World, Center, Shape.Extent.Z * Scale.Z, Shape.Extent.X * FMath::Max(Scale.X, Scale.Y), Rotation, Color, false, 0.f, 0, 1.f);
			break;
		case EGSTN_ShapeType::Box:
			DrawDebugBox(World, Center, Shape.Extent * Scale, Rotation, Color, false, 0.f, 0, 1.f);
			break;
		}
#endif
	}
}

void GSTNCombatDebug::DrawHitboxes(const UWorld* World, const TArray<FGSTN_AttackerDebugShape>& Shapes)
{
	static const FColor SpaceColors[] = { FColor::Red, FColor::Magenta, FColor::Cyan, FColor::Purple };
	for (const FGSTN_AttackerDebugShape& Debug : Shapes)
	{
		DrawShape(World, Debug.Shape, Debug.World, SpaceColors[FMath::Min<int32>(Debug.Space, 3)]);
	}
}

void GSTNCombatDebug::DrawHurtboxes(const UWorld* World, const FGSTN_SnapshotEntry& Entry)
{
	if (!Entry.Hurtboxes)
	{
		return;
	}
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
		DrawShape(World, Entry.Hurtboxes->Hurtboxes[Index].Shape, Entry.Transform, Color);
	}
}
