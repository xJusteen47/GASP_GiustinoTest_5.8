// Spin and drill, Network Prediction half.

#include "Sim/GSTN_DrillFragment.h"

namespace GSTNDrillFragment
{
	/** Groups of the wire format, one presence bit each */
	namespace Presence
	{
		constexpr uint8 Phase = 1 << 0;
		constexpr uint8 Air = 1 << 1;
		constexpr uint8 Latch = 1 << 2;
		constexpr uint8 All = Phase | Air | Latch;
	}

	const TCHAR* PhaseName(EGSTN_DrillPhase Phase)
	{
		switch (Phase)
		{
		case EGSTN_DrillPhase::None: return TEXT("None");
		case EGSTN_DrillPhase::Spinning: return TEXT("Spinning");
		case EGSTN_DrillPhase::Diving: return TEXT("Diving");
		case EGSTN_DrillPhase::Seating: return TEXT("Seating");
		case EGSTN_DrillPhase::Boring: return TEXT("Boring");
		case EGSTN_DrillPhase::Launching: return TEXT("Launching");
		case EGSTN_DrillPhase::Lag: return TEXT("Lag");
		default: return TEXT("?");
		}
	}
}

bool FGSTN_DrillFragmentRegistration::Register()
{
	return FGSTN_SyncFragmentRegistry::Register<FGSTN_DrillFragment>(GSTNFeatureIds::SpinDrill);
}

void FGSTN_DrillFragmentRegistration::Unregister()
{
	FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::SpinDrill);
}

bool FGSTN_DrillFragment::IsDefault() const
{
	return *this == FGSTN_DrillFragment();
}

bool FGSTN_DrillFragment::operator==(const FGSTN_DrillFragment& Other) const
{
	return Phase == Other.Phase && Flags == Other.Flags && PhaseStartFrame == Other.PhaseStartFrame
		&& AirSpinsUsed == Other.AirSpinsUsed && LowGravityUntilFrame == Other.LowGravityUntilFrame
		&& LatchFrame == Other.LatchFrame && SeatFrames == Other.SeatFrames && MaxBoreFrames == Other.MaxBoreFrames && HitsLanded == Other.HitsLanded;
}

void FGSTN_DrillFragment::NetSerialize(FArchive& Ar)
{
	using namespace GSTNDrillFragment;

	uint8 Mask = 0;
	if (Ar.IsSaving())
	{
		if (IsDefault())
		{
			return;
		}
		Mask |= (Phase != EGSTN_DrillPhase::None || Flags != 0 || PhaseStartFrame != INDEX_NONE) ? Presence::Phase : 0;
		Mask |= (AirSpinsUsed != 0 || LowGravityUntilFrame != INDEX_NONE) ? Presence::Air : 0;
		Mask |= (LatchFrame != INDEX_NONE || SeatFrames != 0 || MaxBoreFrames != 0 || HitsLanded != 0) ? Presence::Latch : 0;
	}
	else
	{
		*this = FGSTN_DrillFragment();
		// The registry reads into a default instance: a payload of zero bytes is the default fragment
		if (Ar.TotalSize() == 0)
		{
			return;
		}
	}

	Ar << Mask;
	if (Ar.IsLoading() && (Mask == 0 || (Mask & ~Presence::All) != 0))
	{
		// No group at all, or a bit this build does not know: a corrupt or foreign payload, not state to compare
		Ar.SetError();
		*this = FGSTN_DrillFragment();
		return;
	}

	if (Mask & Presence::Phase)
	{
		uint8 PhaseByte = static_cast<uint8>(Phase);
		Ar << PhaseByte << Flags << PhaseStartFrame;
		if (Ar.IsLoading())
		{
			if (PhaseByte >= static_cast<uint8>(EGSTN_DrillPhase::Count) || (Flags & ~GSTNDrillFlags::All) != 0)
			{
				Ar.SetError();
			}
			Phase = static_cast<EGSTN_DrillPhase>(PhaseByte);
		}
	}
	if (Mask & Presence::Air)
	{
		Ar << AirSpinsUsed << LowGravityUntilFrame;
	}
	if (Mask & Presence::Latch)
	{
		Ar << LatchFrame << SeatFrames << MaxBoreFrames << HitsLanded;
	}
	if (Ar.IsLoading() && Ar.IsError())
	{
		*this = FGSTN_DrillFragment();
	}
}

bool FGSTN_DrillFragment::ShouldReconcile(const FGSTN_SyncFragment& Authority) const
{
	return !(*this == static_cast<const FGSTN_DrillFragment&>(Authority));
}

void FGSTN_DrillFragment::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("Drill");
	if (Phase != EGSTN_DrillPhase::None || PhaseStartFrame != INDEX_NONE)
	{
		Out.Appendf(" %s@%d %s", TCHAR_TO_ANSI(GSTNDrillFragment::PhaseName(Phase)), PhaseStartFrame, IsClockwise() ? "cw" : "ccw");
	}
	if (AirSpinsUsed != 0 || LowGravityUntilFrame != INDEX_NONE)
	{
		Out.Appendf(" air %d lowg %d", AirSpinsUsed, LowGravityUntilFrame);
	}
	if (LatchFrame != INDEX_NONE)
	{
		Out.Appendf(" latch %d seat %d max %d hits %d", LatchFrame, SeatFrames, MaxBoreFrames, HitsLanded);
	}
}
