#include "Kits/GSTN_RunSkidTrackLibrary.h"

#include "Animation/AnimSequence.h"
#include "Kits/GSTN_LocomotionProfile.h"

bool UGSTN_RunSkidTrackLibrary::BakeRunSkidRootTrack(const UAnimSequence* Clip, float SampleRate, FGSTN_RunSkidRootTrack& Track)
{
	if (!Clip || SampleRate <= 0.f)
	{
		return false;
	}
	const double Length = Clip->GetPlayLength();
	const int32 Num = FMath::Max(FMath::CeilToInt32(Length * SampleRate - 1.e-4), 1) + 1;

	TArray<FVector> Positions;
	TArray<double> Yaws;
	Positions.SetNum(Num);
	Yaws.SetNum(Num);
	double Unwound = 0.0;
	double PreviousYaw = 0.0;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const double Time = FMath::Min(static_cast<double>(Index) / SampleRate, Length);
		const FTransform Root = Clip->ExtractRootTrackTransform(FAnimExtractContext(Time), nullptr);
		Positions[Index] = FVector(Root.GetLocation().X, Root.GetLocation().Y, 0.0);
		const double Yaw = Root.GetRotation().Rotator().Yaw;
		// Unwrapped, so a 180 turn that crosses +-180 keeps counting the same way
		Unwound = Index == 0 ? 0.0 : Unwound + FRotator::NormalizeAxis(Yaw - PreviousYaw);
		PreviousYaw = Yaw;
		Yaws[Index] = Unwound;
	}

	// The entry direction: where the root goes over its first 5 cm (the run the skid brakes), else the mannequin's forward (+Y)
	FVector Entry = FVector::ZeroVector;
	for (int32 Index = 1; Index < Num && Entry.IsNearlyZero(); ++Index)
	{
		const FVector Delta = Positions[Index] - Positions[0];
		if (Delta.Size() > 5.0)
		{
			Entry = Delta.GetSafeNormal();
		}
	}
	if (Entry.IsNearlyZero())
	{
		Entry = FVector::YAxisVector;
	}

	const double Total = Yaws.Last();
	Track.SampleRate = SampleRate;
	Track.TotalYawDeg = static_cast<float>(Total);
	Track.TurnSign = Total >= 0.0 ? 1.f : -1.f;
	Track.Speed.SetNum(Num);
	Track.Turn.SetNum(Num);
	float Reached = 0.f;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const int32 Prev = FMath::Max(Index - 1, 0);
		const int32 Next = FMath::Min(Index + 1, Num - 1);
		const double Span = static_cast<double>(Next - Prev) / SampleRate;
		Track.Speed[Index] = Span > 0.0 ? static_cast<float>((Positions[Next] - Positions[Prev]).Dot(Entry) / Span) : 0.f;
		const float Fraction = FMath::Abs(Total) > 1.0 ? static_cast<float>(FMath::Clamp(Yaws[Index] / Total, 0.0, 1.0)) : 0.f;
		// Non-decreasing: a wobble of the clip's root yaw stays in the mesh, the capsule only turns forward
		Reached = FMath::Max(Reached, Fraction);
		Track.Turn[Index] = Reached;
	}
	Track.SourceClip = FSoftObjectPath(Clip);
	Track.bEnabled = true;
	return true;
}

bool UGSTN_RunSkidTrackLibrary::BakeProfileSkidRootTrack(UGSTN_LocomotionProfile* Profile)
{
	if (!Profile)
	{
		return false;
	}
	const UAnimSequence* Clip = Profile->SkidRootMotionClip.LoadSynchronous();
	FGSTN_RunSkidRootTrack& Track = Profile->RunStages.Skid.RootTrack;
	if (!BakeRunSkidRootTrack(Clip, Track.SampleRate > 0.f ? Track.SampleRate : 60.f, Track))
	{
		return false;
	}
	Profile->MarkPackageDirty();
	return true;
}

void UGSTN_RunSkidTrackLibrary::SampleRunSkidRootTrack(const FGSTN_RunSkidRootTrack& Track, int32 LiveFrame, float DeltaSeconds, float EntrySpeed, float& GroundSpeed,
	float& TurnFraction)
{
	GroundSpeed = FGSTN_RunSkidMath::TrackGroundSpeed(Track, LiveFrame, DeltaSeconds, EntrySpeed);
	TurnFraction = LiveFrame > 0 ? FGSTN_RunSkidMath::TrackTurnAt(Track, LiveFrame - 1, DeltaSeconds) : 0.f;
}
