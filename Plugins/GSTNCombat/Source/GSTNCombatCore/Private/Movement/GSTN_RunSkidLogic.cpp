// Pure combat logic (no GAS, no Network Prediction).

#include "Movement/GSTN_RunSkidTypes.h"

bool FGSTN_RunSkidMath::ShouldStart(const FGSTN_RunSkidTuning& Tuning, int32 Stage, const FVector& GroundVelocity, const FVector& StickWorld, float Factor)
{
	if (!Tuning.bEnabled || Stage < Tuning.MinStage || Factor <= 0.f)
	{
		return false;
	}
	const double Speed = GroundVelocity.Size();
	const double Stick = StickWorld.Size();
	if (Speed < Tuning.MinSpeed * Factor || Speed <= KINDA_SMALL_NUMBER || Stick < Tuning.MinStickMagnitude || Stick <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	return (StickWorld / Stick).Dot(GroundVelocity / Speed) <= Tuning.ReverseStickCos;
}

float FGSTN_RunSkidMath::SpeedAt(float StartSpeed, float Brake, int32 LiveFrame, float DeltaSeconds)
{
	return FMath::Max(0.f, StartSpeed - Brake * DeltaSeconds * static_cast<float>(FMath::Max(LiveFrame, 0)));
}

int32 FGSTN_RunSkidMath::FramesToRest(float StartSpeed, float Brake, float DeltaSeconds)
{
	const float PerFrame = Brake * DeltaSeconds;
	return PerFrame > 0.f ? FMath::CeilToInt32(StartSpeed / PerFrame) : MAX_int32;
}

EGSTN_RunSkidPhase FGSTN_RunSkidMath::PhaseAt(const FGSTN_RunSkidTuning& Tuning, int32 LiveFrame, int32 RestFrame)
{
	if (LiveFrame < 0)
	{
		return EGSTN_RunSkidPhase::None;
	}
	if (LiveFrame < RestFrame)
	{
		return LiveFrame < Tuning.EnterFrames ? EGSTN_RunSkidPhase::Enter : EGSTN_RunSkidPhase::Active;
	}
	return LiveFrame < RestFrame + Tuning.ExitFrames ? EGSTN_RunSkidPhase::Exit : EGSTN_RunSkidPhase::None;
}

bool FGSTN_RunSkidMath::InJumpWindow(const FGSTN_RunSkidTuning& Tuning, int32 LiveFrame)
{
	// The abilities read this frame's tag on the next frame: the window is shifted one frame early so it opens on the authored frame for them
	const int32 Seen = LiveFrame + 1;
	return Seen >= Tuning.JumpWindowStart && (Tuning.JumpWindowEnd < 0 || Seen <= Tuning.JumpWindowEnd);
}

float FGSTN_RunSkidMath::StopDistance(float StartSpeed, float Brake)
{
	return Brake > 0.f ? StartSpeed * StartSpeed / (2.f * Brake) : 0.f;
}

float FGSTN_RunSkidMath::TrackClipTime(const FGSTN_RunSkidRootTrack& Track, int32 LiveFrame, float DeltaSeconds)
{
	return static_cast<float>(FMath::Max(LiveFrame, 0)) * DeltaSeconds * Track.PlayRate;
}

void FGSTN_RunSkidMath::SampleTrack(const FGSTN_RunSkidRootTrack& Track, float ClipTime, float& OutSpeed, float& OutTurn)
{
	OutSpeed = 0.f;
	OutTurn = 0.f;
	const int32 Num = FMath::Min(Track.Speed.Num(), Track.Turn.Num());
	if (Num <= 0 || Track.SampleRate <= 0.f)
	{
		return;
	}
	const float Position = FMath::Clamp(ClipTime * Track.SampleRate, 0.f, static_cast<float>(Num - 1));
	const int32 Index = FMath::Min(FMath::FloorToInt32(Position), Num - 1);
	const int32 Next = FMath::Min(Index + 1, Num - 1);
	const float Alpha = Position - static_cast<float>(Index);
	OutSpeed = FMath::Lerp(Track.Speed[Index], Track.Speed[Next], Alpha);
	OutTurn = FMath::Clamp(FMath::Lerp(Track.Turn[Index], Track.Turn[Next], Alpha), 0.f, 1.f);
}

int32 FGSTN_RunSkidMath::TrackFrames(const FGSTN_RunSkidRootTrack& Track, float DeltaSeconds)
{
	const float PerFrame = DeltaSeconds * Track.PlayRate;
	// A small tolerance so an end time that is a whole number of frames does not get one more
	return PerFrame > 0.f ? FMath::Max(FMath::CeilToInt32(Track.GetEndTime() / PerFrame - 1.e-3f), 1) : 1;
}

int32 FGSTN_RunSkidMath::TrackRestFrame(const FGSTN_RunSkidRootTrack& Track, float DeltaSeconds)
{
	const int32 Frames = TrackFrames(Track, DeltaSeconds);
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		float Speed, Turn;
		SampleTrack(Track, TrackClipTime(Track, Frame, DeltaSeconds), Speed, Turn);
		if (Speed <= 0.f)
		{
			return Frame;
		}
	}
	return Frames;
}

float FGSTN_RunSkidMath::TrackGroundSpeed(const FGSTN_RunSkidRootTrack& Track, int32 LiveFrame, float DeltaSeconds, float EntrySpeed)
{
	float Speed, Turn;
	SampleTrack(Track, TrackClipTime(Track, LiveFrame, DeltaSeconds), Speed, Turn);
	Speed *= Track.PlayRate;
	if (Speed <= 0.f)
	{
		return Speed * Track.ReverseSpeedScale;
	}
	// Braking: from the run's own speed to the clip's over EntryBlendSeconds, scaling the clip's curve so its shape is kept
	const float ClipEntry = Track.Speed.Num() > 0 ? Track.Speed[0] * Track.PlayRate : 0.f;
	if (Track.EntryBlendSeconds <= 0.f || ClipEntry <= KINDA_SMALL_NUMBER || EntrySpeed <= 0.f)
	{
		return Speed;
	}
	const float Alpha = FMath::Clamp(static_cast<float>(FMath::Max(LiveFrame, 0)) * DeltaSeconds / Track.EntryBlendSeconds, 0.f, 1.f);
	return Speed * FMath::Lerp(EntrySpeed / ClipEntry, 1.f, Alpha);
}

float FGSTN_RunSkidMath::TrackTurnAt(const FGSTN_RunSkidRootTrack& Track, int32 LiveFrame, float DeltaSeconds)
{
	if (LiveFrame + 1 >= TrackFrames(Track, DeltaSeconds))
	{
		return 1.f;
	}
	float Speed, Turn, EndSpeed, EndTurn;
	SampleTrack(Track, TrackClipTime(Track, LiveFrame + 1, DeltaSeconds), Speed, Turn);
	// Normalised to the turn reached at the end time, so a skid cut before the clip's end still faces the new direction when it ends
	SampleTrack(Track, Track.GetEndTime(), EndSpeed, EndTurn);
	return EndTurn > KINDA_SMALL_NUMBER ? FMath::Clamp(Turn / EndTurn, 0.f, 1.f) : 1.f;
}
