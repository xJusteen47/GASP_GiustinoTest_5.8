// Piggyback, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTN_MountUtils.h"

namespace GSTNMountUtils
{
	/** A unit fraction quantised to a byte, round half up; NaN and anything below 0 are 0, anything above 1 is 255 */
	uint8 QuantiseUnit(double Fraction)
	{
		if (!(Fraction > 0.0))
		{
			return 0;
		}
		if (Fraction >= 1.0)
		{
			return 255;
		}
		return static_cast<uint8>(FMath::FloorToInt(Fraction * 255.0 + 0.5));
	}

	/** A candidate on the carrier's plane along one of its local axes, facing the carrier's way */
	FTransform AroundCarrier(const FTransform& Carrier, const FVector& LocalAxis, double DistanceCm)
	{
		const FVector Location = Carrier.GetLocation() + Carrier.TransformVectorNoScale(LocalAxis) * DistanceCm;
		return FTransform(Carrier.GetRotation(), Location);
	}
}

EGSTN_MountVerdict UGSTN_MountUtils::EvaluateEligibility(const FGSTN_MountEligibilityInput& Input)
{
	if (!Input.bCarrierCanBeMounted)
	{
		return EGSTN_MountVerdict::NotAllowedKit;
	}
	if (!Input.bRiderCanRide)
	{
		return EGSTN_MountVerdict::RiderIneligible;
	}
	if (Input.bCarrierLinked || Input.bRiderLinked)
	{
		return EGSTN_MountVerdict::AlreadyLinked;
	}
	if (Input.Frame < Input.RiderCooldownEndFrame)
	{
		return EGSTN_MountVerdict::Cooldown;
	}
	if (Input.bCarrierInHitstun)
	{
		return EGSTN_MountVerdict::CarrierHitstun;
	}
	if (Input.bCarrierBusyOrLocked)
	{
		return EGSTN_MountVerdict::CarrierBusyLock;
	}
	// Written so a NaN distance or weight refuses (every comparison with NaN is false)
	if (!(Input.DistanceCm <= Input.MountMaxDistanceCm))
	{
		return EGSTN_MountVerdict::OutOfRange;
	}
	if (!(Input.RiderWeight <= Input.CarrierMaxCarryWeight))
	{
		return EGSTN_MountVerdict::WeightTooHigh;
	}
	return EGSTN_MountVerdict::Accept;
}

uint8 UGSTN_MountUtils::ComputeCarryRatio(double RiderWeight, double MaxCarryWeight)
{
	if (!(MaxCarryWeight > 0.0))
	{
		return RiderWeight > 0.0 ? 255 : 0;
	}
	return GSTNMountUtils::QuantiseUnit(RiderWeight / MaxCarryWeight);
}

double UGSTN_MountUtils::CarryRatioToUnit(uint8 RatioQ)
{
	return static_cast<double>(RatioQ) / 255.0;
}

FGSTN_CarryTuning UGSTN_MountUtils::ScaleCarryTuning(const FGSTN_CarryTuning& AtFullLoad, uint8 RatioQ)
{
	const double Ratio = CarryRatioToUnit(RatioQ);
	FGSTN_CarryTuning Scaled;
	Scaled.SpeedPenalty = static_cast<float>(AtFullLoad.SpeedPenalty * Ratio);
	Scaled.AccelPenalty = static_cast<float>(AtFullLoad.AccelPenalty * Ratio);
	Scaled.JumpPenalty = static_cast<float>(AtFullLoad.JumpPenalty * Ratio);
	// Integer math so every machine removes the same number of jumps
	Scaled.AirJumpsRemoved = FMath::Max(AtFullLoad.AirJumpsRemoved, 0) * static_cast<int32>(RatioQ) / 255;
	return Scaled;
}

FTransform UGSTN_MountUtils::ComposeAnchor(const FTransform& CarrierTransform, const FTransform& SocketLocal, const FTransform& MountOffset, double LocalYawDeg)
{
	// FTransform products apply left to right: offset in socket space, socket in carrier space, carrier in the world
	const FTransform Placed = MountOffset * SocketLocal * CarrierTransform;
	if (LocalYawDeg == 0.0)
	{
		return Placed;
	}
	const FTransform Turn(FQuat(FVector::UpVector, FMath::DegreesToRadians(LocalYawDeg)));
	return Turn * Placed;
}

TArray<FTransform> UGSTN_MountUtils::ExitCandidates(const FTransform& Anchor, const FTransform& CarrierTransform, EGSTN_ExitMode Mode, double DistanceCm)
{
	const double Distance = FMath::Max(DistanceCm, 0.0);
	const FTransform Back = GSTNMountUtils::AroundCarrier(CarrierTransform, -FVector::ForwardVector, Distance);
	const FTransform Left = GSTNMountUtils::AroundCarrier(CarrierTransform, -FVector::RightVector, Distance);
	const FTransform Right = GSTNMountUtils::AroundCarrier(CarrierTransform, FVector::RightVector, Distance);
	const FTransform Front = GSTNMountUtils::AroundCarrier(CarrierTransform, FVector::ForwardVector, Distance);

	TArray<FTransform> Candidates;
	switch (Mode)
	{
	case EGSTN_ExitMode::SideOfCarrier:
		Candidates = { Right, Left, Back, Front };
		break;
	case EGSTN_ExitMode::AtAnchor:
		Candidates = { Anchor, Back, Left, Right, Front };
		break;
	case EGSTN_ExitMode::BehindCarrier:
	default:
		Candidates = { Back, Left, Right, Front };
		break;
	}
	return Candidates;
}

uint8 UGSTN_MountUtils::MakePartnerShare(double Fraction)
{
	return GSTNMountUtils::QuantiseUnit(Fraction);
}

FGSTN_CreditSplit UGSTN_MountUtils::SplitCredit(uint8 PartnerShare, double Amount)
{
	FGSTN_CreditSplit Split;
	if (PartnerShare == 255)
	{
		Split.PartnerAmount = Amount;
		return Split;
	}
	Split.PartnerAmount = Amount * static_cast<double>(PartnerShare) / 255.0;
	Split.OwnerAmount = Amount - Split.PartnerAmount;
	return Split;
}
