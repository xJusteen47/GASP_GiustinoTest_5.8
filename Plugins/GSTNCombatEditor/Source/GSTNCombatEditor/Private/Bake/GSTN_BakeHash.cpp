// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "Bake/GSTN_SocketBaker.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Misc/Crc.h"
#include "Preview/GSTN_PreviewPose.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	/** A real number as a whole number of millionths: the text of the hash must not depend on how a float prints */
	int64 Micro(double Value)
	{
		return FMath::RoundToInt64(Value * 1000000.0);
	}

	void AppendSegments(const UAnimMontage& Montage, double RangeStart, double RangeEnd, bool bWholeMontage, FString& Out)
	{
		// The animation the section plays: every segment of every slot track that overlaps the section's time range. A segment's own timing is in
		// the montage, not in the sequence, so it is part of the inputs together with the sequence's data
		int32 Index = 0;
		for (const FSlotAnimationTrack& Slot : Montage.SlotAnimTracks)
		{
			for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
			{
				const double SegStart = Segment.StartPos;
				const double SegEnd = Segment.GetEndPos();
				const bool bOverlaps = bWholeMontage || (SegEnd > RangeStart && SegStart < RangeEnd);
				if (!bOverlaps)
				{
					continue;
				}
				const UAnimSequenceBase* Sequence = Segment.GetAnimReference();
				FString DataGuid = TEXT("none");
				if (Sequence != nullptr)
				{
					if (const IAnimationDataModel* Model = Sequence->GetDataModel())
					{
						DataGuid = Model->GenerateGuid().ToString(EGuidFormats::Digits);
					}
				}
				Out += FString::Printf(TEXT("segment%d=%s|data=%s|pos=%lld|start=%lld|end=%lld|rate=%lld|loops=%d\n"), Index++,
					Sequence ? *Sequence->GetPathName() : TEXT("none"), *DataGuid,
					Micro(Segment.StartPos), Micro(Segment.AnimStartTime), Micro(Segment.AnimEndTime), Micro(Segment.AnimPlayRate), Segment.LoopingCount);
			}
		}
	}
}

FString GSTNBake::DescribeInputs(const FGSTN_CombatTimelineData& Data, const FGSTN_BakeInputs& Inputs)
{
	FString Out;
	Out += FString::Printf(TEXT("totalFrames=%d\n"), Data.TotalFrames);
	Out += FString::Printf(TEXT("tickRate=%lld\n"), Micro(Inputs.TickRateHz));
	Out += FString::Printf(TEXT("playRate=%lld\n"), Micro(Data.MontagePlayRate));
	Out += FString::Printf(TEXT("section=%s\n"), *Data.MontageSection.ToString());

	if (Inputs.Montage != nullptr)
	{
		const UAnimMontage& Montage = *Inputs.Montage;
		Out += FString::Printf(TEXT("montage=%s\n"), *Montage.GetPathName());
		Out += FString::Printf(TEXT("montageLength=%lld\n"), Micro(Montage.GetPlayLength()));
		double SectionStart = 0.0;
		double SectionEnd = Montage.GetPlayLength();
		bool bWhole = true;
		if (!Data.MontageSection.IsNone())
		{
			const int32 SectionIndex = Montage.GetSectionIndex(Data.MontageSection);
			if (SectionIndex != INDEX_NONE)
			{
				float Start = 0.f;
				float End = 0.f;
				Montage.GetSectionStartAndEndTime(SectionIndex, Start, End);
				SectionStart = Start;
				SectionEnd = End;
				bWhole = false;
			}
		}
		Out += FString::Printf(TEXT("sectionStart=%lld\nsectionEnd=%lld\n"), Micro(SectionStart), Micro(SectionEnd));
		AppendSegments(Montage, SectionStart, SectionEnd, bWhole, Out);
	}
	else
	{
		Out += TEXT("montage=none\n");
	}

	if (Inputs.Mesh != nullptr)
	{
		const USkeletalMesh& Mesh = *Inputs.Mesh;
		Out += FString::Printf(TEXT("mesh=%s\n"), *Mesh.GetPathName());
		const USkeleton* Skeleton = Mesh.GetSkeleton();
		Out += FString::Printf(TEXT("skeleton=%s\n"), Skeleton ? *Skeleton->GetGuid().ToString(EGuidFormats::Digits) : TEXT("none"));
	}
	else
	{
		Out += TEXT("mesh=none\n");
	}

	TArray<FGSTN_BakeSocketUse> Uses;
	FGSTN_SocketBaker::CollectUses(Data, Uses);
	for (const FName Socket : FGSTN_SocketBaker::GetDistinctSockets(Uses))
	{
		FString Line = FString::Printf(TEXT("socket=%s"), *Socket.ToString());
		// A mesh socket has its own bone and local transform, which move a hitbox as surely as the animation does. A bone has neither
		if (Inputs.Mesh != nullptr)
		{
			if (const USkeletalMeshSocket* MeshSocket = Inputs.Mesh->FindSocket(Socket))
			{
				Line += FString::Printf(TEXT("|bone=%s|loc=%lld,%lld,%lld|rot=%lld,%lld,%lld|scale=%lld,%lld,%lld"), *MeshSocket->BoneName.ToString(),
					Micro(MeshSocket->RelativeLocation.X), Micro(MeshSocket->RelativeLocation.Y), Micro(MeshSocket->RelativeLocation.Z),
					Micro(MeshSocket->RelativeRotation.Pitch), Micro(MeshSocket->RelativeRotation.Yaw), Micro(MeshSocket->RelativeRotation.Roll),
					Micro(MeshSocket->RelativeScale.X), Micro(MeshSocket->RelativeScale.Y), Micro(MeshSocket->RelativeScale.Z));
			}
		}
		Out += Line + TEXT("\n");
	}
	return Out;
}

uint32 GSTNBake::ComputeInputHash(const FGSTN_CombatTimelineData& Data, const FGSTN_BakeInputs& Inputs)
{
	return FCrc::StrCrc32(*DescribeInputs(Data, Inputs));
}

FGSTN_BakeStatusResult GSTNBake::GetStatus(const FGSTN_CombatTimelineData& Data, const FGSTN_BakeInputs& Inputs)
{
	FGSTN_BakeStatusResult Result;
	TArray<FGSTN_BakeSocketUse> Uses;
	FGSTN_SocketBaker::CollectUses(Data, Uses);
	const TArray<FName> Sockets = FGSTN_SocketBaker::GetDistinctSockets(Uses);
	if (Sockets.IsEmpty())
	{
		Result.Status = EGSTN_BakeStatus::NotNeeded;
		Result.Reason = LOCTEXT("StatusNotNeeded", "No hitbox uses a baked socket, so there is nothing to bake.");
		return Result;
	}

	bool bAllFramesThere = true;
	for (const FName Socket : Sockets)
	{
		const FGSTN_BakedSocketFrames* Baked = Data.BakedSockets.Find(Socket);
		if (Baked == nullptr || Baked->Frames.IsEmpty())
		{
			bAllFramesThere = false;
			break;
		}
	}
	if (!bAllFramesThere)
	{
		Result.Status = EGSTN_BakeStatus::NeverBaked;
		Result.Reason = LOCTEXT("StatusNeverBaked", "A hitbox uses a baked socket that has no baked frames. Bake the sockets.");
		return Result;
	}

	if (!Data.BakeInfo.IsSet())
	{
		Result.Status = EGSTN_BakeStatus::BakedElsewhere;
		Result.Reason = LOCTEXT("StatusElsewhere", "The frames were baked by another tool, so the editor cannot tell whether they are current. Bake the sockets once here to track them.");
		return Result;
	}

	// From here the bake is the editor's and has a record. The cheap facts first, each with its own words
	for (const FName Socket : Sockets)
	{
		const int32 BakedFrames = Data.BakedSockets.FindChecked(Socket).Frames.Num();
		if (BakedFrames != Data.TotalFrames)
		{
			Result.Status = EGSTN_BakeStatus::Stale;
			Result.Reason = FText::Format(LOCTEXT("StatusLength", "The socket '{0}' is baked for {1} frames and the timeline has {2}. Bake the sockets again."),
				FText::FromName(Socket), FText::AsNumber(BakedFrames), FText::AsNumber(Data.TotalFrames));
			return Result;
		}
	}
	if (!FMath::IsNearlyEqual(Data.BakeInfo.TickRateHz, Inputs.TickRateHz, 1e-3f))
	{
		Result.Status = EGSTN_BakeStatus::Stale;
		Result.Reason = FText::Format(LOCTEXT("StatusTickRate", "The sockets were baked at {0} Hz and the simulation now runs at {1} Hz. Bake the sockets again."),
			FText::AsNumber(Data.BakeInfo.TickRateHz), FText::AsNumber(Inputs.TickRateHz));
		return Result;
	}
	if (Inputs.Mesh == nullptr)
	{
		Result.Status = EGSTN_BakeStatus::Stale;
		Result.Reason = FText::Format(LOCTEXT("StatusMeshGone", "The mesh the sockets were baked from ({0}) could not be loaded."), FText::FromString(Data.BakeInfo.Mesh.ToString()));
		return Result;
	}
	if (FSoftObjectPath(Inputs.Mesh) != Data.BakeInfo.Mesh)
	{
		Result.Status = EGSTN_BakeStatus::Stale;
		Result.Reason = FText::Format(LOCTEXT("StatusOtherMesh", "The sockets were baked from {0} and the preview shows {1}. Bake the sockets again to bake the one you see."),
			FText::FromString(Data.BakeInfo.Mesh.GetAssetName()), FText::FromString(Inputs.Mesh->GetName()));
		return Result;
	}
	if (ComputeInputHash(Data, Inputs) != Data.BakeInfo.InputHash)
	{
		Result.Status = EGSTN_BakeStatus::Stale;
		Result.Reason = LOCTEXT("StatusHash", "The montage, its section or play rate, the length, the list of sockets or the mesh changed since the sockets were baked. Bake the sockets again.");
		return Result;
	}

	Result.Status = EGSTN_BakeStatus::Fresh;
	Result.Reason = FText::Format(LOCTEXT("StatusFresh", "The baked sockets match their sources (baked {0})."), FText::AsDateTime(Data.BakeInfo.When));
	return Result;
}

#undef LOCTEXT_NAMESPACE
