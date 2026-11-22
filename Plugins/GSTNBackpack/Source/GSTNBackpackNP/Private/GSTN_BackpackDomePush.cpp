// Backpacks, Network Prediction half (M11 L7d: the dome pushes back whoever hits it from outside).

#include "GSTN_BackpackDomePush.h"

#include "AbilitySystemComponent.h"
#include "GSTN_BackpackDomeSnapshot.h"
#include "GSTN_BackpackSim.h"
#include "GSTN_CombatNPTags.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Movement/GSTN_LayeredMove_PushBack.h"
#include "Sync/GSTN_SyncFragment.h"
#include "World/GSTN_StepSnapshot.h"

namespace GSTNBackpackDomePush
{
	const FGSTN_BackpackSnapshotExt* DomeOf(const FGSTN_SnapshotEntry* Entry)
	{
		const FGSTN_BackpackSnapshotExt* Ext = Entry ? Entry->Extension.Get<FGSTN_BackpackSnapshotExt>(GSTNFeatureIds::Backpack) : nullptr;
		return Ext && Ext->Type == FGSTN_BackpackSnapshotExt::TypeWord && Ext->HasDome() ? Ext : nullptr;
	}
}

uint8 FGSTN_BackpackDomePush::GetTypeId() const
{
	return GSTNFeatureIds::Backpack;
}

const FGSTN_BackpackSnapshotExt* FGSTN_BackpackDomePush::FindBlockingDome(const FGSTN_AttackerPushQuery& Query, int32& OutOwnerId)
{
	OutOwnerId = INDEX_NONE;
	const FGSTN_SnapshotEntry* Victim = Query.Snapshot.Find(Query.VictimId);
	if (!Victim)
	{
		return nullptr;
	}
	// The shell: the carrier's own dome
	if (GSTNHurtboxIndex::IsExtra(Query.HurtboxIndex))
	{
		const int32 Position = GSTNHurtboxIndex::ToExtraPosition(Query.HurtboxIndex);
		if (Victim->ExtraHurtboxes.IsValidIndex(Position) && Victim->ExtraHurtboxes[Position].Source == GSTNFeatureIds::Backpack)
		{
			OutOwnerId = Victim->EntityId;
			return GSTNBackpackDomePush::DomeOf(Victim);
		}
		return nullptr;
	}
	// A body inside domes: the first one (owner id order, the snapshot's) that does not hold the attacker's root, the BackpackDome step's rule
	const FGSTN_BackpackSnapshotExt* VictimExt = Victim->Extension.Get<FGSTN_BackpackSnapshotExt>(GSTNFeatureIds::Backpack);
	if (!VictimExt || VictimExt->Type != FGSTN_BackpackSnapshotExt::TypeWord)
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < FMath::Min<int32>(VictimExt->NumInside, FGSTN_BackpackSnapshotExt::MaxInsideDomes); ++Index)
	{
		const int32 OwnerId = VictimExt->InsideDomes[Index];
		const FGSTN_BackpackSnapshotExt* Dome = GSTNBackpackDomePush::DomeOf(Query.Snapshot.Find(OwnerId));
		if (Dome && FVector::DistSquared(Query.AttackerRoot, Dome->GetDomeCenter()) > FMath::Square(static_cast<double>(Dome->DomeRadiusCm)))
		{
			OutOwnerId = OwnerId;
			return Dome;
		}
	}
	return nullptr;
}

bool FGSTN_BackpackDomePush::ComputePush(const FGSTN_BackpackSnapshotExt& Dome, const FGSTN_AttackerPushQuery& Query, FVector& OutVelocity, int32& OutFrames)
{
	const int32 Frames = Dome.PushPacked & 0x7F;
	const float Speed = static_cast<float>(Dome.PushSpeedQ) * FGSTN_BackpackDomeTrack::PushSpeedStep;
	if (Frames <= 0 || Speed <= 0.f)
	{
		return false;
	}
	const FVector Up = Query.Up.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	auto OnPlane = [&Up](const FVector& V) { return V - Up * V.Dot(Up); };
	const FVector Back = OnPlane(-Query.AttackerFacing).GetSafeNormal(UE_SMALL_NUMBER, -Query.AttackerFacing);
	FVector Direction = Back;
	if ((Dome.PushPacked & 0x80) == 0)
	{
		// Out of the dome through the attacker; an attacker right above the centre falls back to its own back
		Direction = OnPlane(Query.AttackerRoot - Dome.GetDomeCenter()).GetSafeNormal(UE_SMALL_NUMBER, Back);
	}
	OutVelocity = Direction * Speed;
	OutFrames = Frames;
	return true;
}

bool FGSTN_BackpackDomePush::GetPush(const FGSTN_AttackerPushQuery& Query, FVector& OutVelocity, int32& OutFrames) const
{
	int32 OwnerId = INDEX_NONE;
	const FGSTN_BackpackSnapshotExt* Dome = FindBlockingDome(Query, OwnerId);
	return Dome && ComputePush(*Dome, Query, OutVelocity, OutFrames);
}

void FGSTN_BackpackDomePush::ApplyToFighter(UAbilitySystemComponent* AbilitySystem, const FVector& Velocity, int32 Frames)
{
	if (!AbilitySystem || Frames <= 0)
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UGSTN_GE_PushBack::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityX, static_cast<float>(Velocity.X));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityY, static_cast<float>(Velocity.Y));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveVelocityZ, static_cast<float>(Velocity.Z));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveDecay, 1.f / static_cast<float>(Frames));
	Spec.Data->SetSetByCallerMagnitude(GSTNCombatNPTags::SetByCaller_MoveSeconds, static_cast<float>(Frames) * FrameSeconds);
	AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}
