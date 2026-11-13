// Backpacks, Network Prediction half (M11 L7b).

#include "GSTN_GA_BackpackDome.h"
#include "GSTN_BackpackSim.h"

UGSTN_GA_BackpackDome::UGSTN_GA_BackpackDome(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Held: the button going up ends the move, and with it a WhileHeld dome (the tick extension sees its timeline stop)
	bEndOnTriggerRelease = true;
	Timeline = MakeDefaultTimeline();
}

FGSTN_CombatTimelineData UGSTN_GA_BackpackDome::MakeDefaultTimeline(int32 TotalFrames, int32 Slot)
{
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = FMath::Max(TotalFrames, 1);
	// Locked while the dome is up: the carrier holds it (the release is the way out)
	Data.FirstActionableFrame = Data.TotalFrames;

	FGSTN_BackpackChargeTrack Charge;
	Charge.StartFrame = 0;
	Charge.EndFrame = 1;
	Charge.Slot = Slot;
	Charge.Cost = 1;
	Charge.Charges = 2;
	Charge.Refill = EGSTN_BackpackRefill::OverTime;
	Charge.RefillFrames = 1250;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_BackpackChargeTrack>(Charge));

	FGSTN_BackpackDomeTrack Dome;
	Dome.StartFrame = 0;
	Dome.EndFrame = 1;
	Dome.RadiusCm = 400;
	Dome.Lifetime = EGSTN_DomeLifetime::WhileHeld;
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_BackpackDomeTrack>(Dome));
	return Data;
}
