// Where the input simulation gets its movement context from (GAS-NPP fork).

#include "Context/GSTN_InputContextProvider.h"

#include "GSTN_InputLog.h"
#include "GameFramework/Actor.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Input/GSTN_InputTags.h"
#include "Mover/GSTN_MoverSimStateTracker.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

void UGSTN_BlueprintInputContextProvider::Initialize(AActor* Owner)
{
	ReceiveInitialize(Owner);
}

float UGSTN_BlueprintInputContextProvider::GetFacingYaw() const
{
	return GetBlueprintFacingYaw();
}

void UGSTN_BlueprintInputContextProvider::AppendContextTags(FGameplayTagContainer& Out) const
{
	Out.AppendTags(GetBlueprintContextTags());
}

uint8 UGSTN_BlueprintInputContextProvider::GetContextFingerprint() const
{
	return GetBlueprintContextFingerprint();
}

void UGSTN_BlueprintInputContextProvider::AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const
{
	if (PreviousFingerprint != CurrentFingerprint)
	{
		Events.EventTags.AppendTags(GetBlueprintFrameEvents(PreviousFingerprint, CurrentFingerprint));
	}
}

UGSTN_MoverInputContextProvider::UGSTN_MoverInputContextProvider()
{
	GroundedModes = { DefaultModeNames::Walking };
	AirborneModes = { DefaultModeNames::Falling, DefaultModeNames::Flying };

	MoverTags.AddTag(Mover_IsOnGround);
	MoverTags.AddTag(Mover_IsInAir);
	MoverTags.AddTag(Mover_IsFalling);
	MoverTags.AddTag(Mover_IsFlying);
	MoverTags.AddTag(Mover_IsCrouching);
}

void UGSTN_MoverInputContextProvider::Initialize(AActor* Owner)
{
	Tracker = nullptr;

	// The facing and tags have to be the Mover's simulated ones at the end of the previous frame, not the actor's
	if (UMoverComponent* Mover = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr)
	{
		Tracker = UGSTN_MoverSimStateTracker::FindOrAdd(Mover);
	}
	else
	{
		UE_LOG(LogGSTNInput, Warning, TEXT("%s: no Mover component on %s, the character is taken to face yaw 0 on the ground"), *GetPathName(), *GetNameSafe(Owner));
	}
}

float UGSTN_MoverInputContextProvider::GetFacingYaw() const
{
	if (Tracker)
	{
		if (const FMoverDefaultSyncState* DefaultSync = Tracker->GetSyncState().SyncStateCollection.FindDataByType<FMoverDefaultSyncState>())
		{
			return static_cast<float>(DefaultSync->GetOrientation_WorldSpace().Yaw);
		}
	}
	return 0.f;
}

UGSTN_MoverInputContextProvider::EContext UGSTN_MoverInputContextProvider::Classify() const
{
	if (!Tracker)
	{
		return EContext::Unknown;
	}

	// Wall first: a wall mode may also be listed as airborne by mistake, and the wall class is the more specific
	const FName Mode = Tracker->GetMovementModeName();
	if (WallModes.Contains(Mode))
	{
		return EContext::OnWall;
	}
	if (GroundedModes.Contains(Mode))
	{
		return EContext::Grounded;
	}
	if (AirborneModes.Contains(Mode))
	{
		return EContext::Airborne;
	}
	return EContext::Unknown;
}

void UGSTN_MoverInputContextProvider::AppendContextTags(FGameplayTagContainer& Out) const
{
	switch (Classify())
	{
	case EContext::Grounded:
		Out.AddTag(GSTNInputTags::Context_Grounded);
		break;
	case EContext::Airborne:
		Out.AddTag(GSTNInputTags::Context_Airborne);
		break;
	case EContext::OnWall:
		Out.AddTag(GSTNInputTags::Context_Airborne);
		Out.AddTag(GSTNInputTags::Context_OnWall);
		break;
	default:
		break;
	}

	if (Tracker)
	{
		for (const FGameplayTag& Tag : MoverTags)
		{
			if (Tracker->HasGameplayTag(Tag, true))
			{
				Out.AddTag(Tag);
			}
		}
	}
}

uint8 UGSTN_MoverInputContextProvider::GetContextFingerprint() const
{
	return static_cast<uint8>(Classify());
}

void UGSTN_MoverInputContextProvider::AppendFrameEvents(uint8 PreviousFingerprint, uint8 CurrentFingerprint, FGSTN_BufferFrameEvents& Events) const
{
	// A landing is falling to the ground; getting off a wall onto the ground is not (it would flush the buffer of a wall jump)
	if (PreviousFingerprint == static_cast<uint8>(EContext::Airborne) && CurrentFingerprint == static_cast<uint8>(EContext::Grounded))
	{
		Events.EventTags.AddTag(GSTNInputTags::BufferEvent_Landed);
	}
}
