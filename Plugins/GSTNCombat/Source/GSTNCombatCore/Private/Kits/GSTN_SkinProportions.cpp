// Pure combat logic (no GAS, no Network Prediction).

#include "Kits/GSTN_SkinProportions.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"

namespace
{
	float Distance(const TMap<FName, FVector>& Positions, FName A, FName B)
	{
		const FVector* PA = Positions.Find(A);
		const FVector* PB = Positions.Find(B);
		return PA && PB ? static_cast<float>(FVector::Distance(*PA, *PB)) : 0.f;
	}

	float Ratio(float Skin, float Base)
	{
		return Skin > UE_KINDA_SMALL_NUMBER && Base > UE_KINDA_SMALL_NUMBER ? Skin / Base : 0.f;
	}

	void CheckRange(const TCHAR* Label, float Value, float Min, float Max, TArray<FString>& Errors, TArray<FString>& Unmeasured)
	{
		if (Value <= 0.f)
		{
			Unmeasured.Add(Label);
			return;
		}
		// A tolerance of a tenth of a percent: a mesh exported at the limit must not fail on rounding
		constexpr float Tolerance = 0.001f;
		if (Value < Min - Tolerance || Value > Max + Tolerance)
		{
			Errors.Add(FString::Printf(TEXT("%s %.3f, allowed %.2f-%.2f"), Label, Value, Min, Max));
		}
	}
}

TMap<FName, FVector> FGSTN_SkinProportionUtils::ComposeComponentSpace(const TArray<FName>& Names, const TArray<FTransform>& Local, const TArray<int32>& Parents)
{
	TMap<FName, FVector> Positions;
	const int32 Count = FMath::Min3(Names.Num(), Local.Num(), Parents.Num());
	TArray<FTransform> Component;
	Component.SetNum(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Parent = Parents[Index];
		// A parent always comes before its child in a reference skeleton; anything else is treated as a root
		Component[Index] = Parent >= 0 && Parent < Index ? Local[Index] * Component[Parent] : Local[Index];
		Positions.Add(Names[Index], Component[Index].GetLocation());
	}
	return Positions;
}

TMap<FName, FVector> FGSTN_SkinProportionUtils::ComposeComponentSpace(const FReferenceSkeleton& Skeleton)
{
	const TArray<FMeshBoneInfo>& Info = Skeleton.GetRawRefBoneInfo();
	TArray<FName> Names;
	TArray<int32> Parents;
	Names.Reserve(Info.Num());
	Parents.Reserve(Info.Num());
	for (const FMeshBoneInfo& Bone : Info)
	{
		Names.Add(Bone.Name);
		Parents.Add(Bone.ParentIndex);
	}
	return ComposeComponentSpace(Names, Skeleton.GetRawRefBonePose(), Parents);
}

FGSTN_SkinBodyLengths FGSTN_SkinProportionUtils::Measure(const TMap<FName, FVector>& Positions, float BoundsMinZ, float BoundsMaxZ, const FGSTN_SkinMeasureBones& Bones)
{
	FGSTN_SkinBodyLengths Lengths;
	Lengths.Height = FMath::Max(0.f, BoundsMaxZ - BoundsMinZ);
	Lengths.Legs = Distance(Positions, Bones.Thigh, Bones.Foot);
	Lengths.Arms = Distance(Positions, Bones.UpperArmLeft, Bones.Hand);
	Lengths.Shoulders = Distance(Positions, Bones.UpperArmLeft, Bones.UpperArmRight);
	if (const FVector* Neck = Positions.Find(Bones.Neck))
	{
		Lengths.Head = FMath::Max(0.f, BoundsMaxZ - static_cast<float>(Neck->Z));
	}
	return Lengths;
}

FGSTN_SkinProportions FGSTN_SkinProportionUtils::Compare(const FGSTN_SkinBodyLengths& Base, const FGSTN_SkinBodyLengths& Skin)
{
	FGSTN_SkinProportions Ratios;
	Ratios.Height = Ratio(Skin.Height, Base.Height);
	Ratios.Legs = Ratio(Skin.Legs, Base.Legs);
	Ratios.Arms = Ratio(Skin.Arms, Base.Arms);
	Ratios.Shoulders = Ratio(Skin.Shoulders, Base.Shoulders);
	Ratios.Head = Ratio(Skin.Head, Base.Head);
	return Ratios;
}

void FGSTN_SkinProportionUtils::Check(const FGSTN_SkinProportions& Ratios, const FGSTN_SkinProportionLimits& Limits, TArray<FString>& Errors, TArray<FString>& Unmeasured)
{
	CheckRange(TEXT("height"), Ratios.Height, Limits.MinHeight, Limits.MaxHeight, Errors, Unmeasured);
	CheckRange(TEXT("legs"), Ratios.Legs, Limits.MinLegs, Limits.MaxLegs, Errors, Unmeasured);
	CheckRange(TEXT("arms"), Ratios.Arms, Limits.MinArms, Limits.MaxArms, Errors, Unmeasured);
	CheckRange(TEXT("shoulders"), Ratios.Shoulders, Limits.MinShoulders, Limits.MaxShoulders, Errors, Unmeasured);
	// The head has no lower bound worth enforcing: a smaller head never puts a limb somewhere the hitbox is not
	CheckRange(TEXT("head"), Ratios.Head, 0.f, Limits.MaxHead, Errors, Unmeasured);
}

void FGSTN_SkinProportionUtils::CheckMeshes(const USkeletalMesh& Base, const USkeletalMesh& Skin, const FGSTN_SkinProportionLimits& Limits, const FGSTN_SkinMeasureBones& Bones,
	FGSTN_SkinProportions& OutRatios, TArray<FString>& Errors, TArray<FString>& Unmeasured)
{
	if (Base.GetSkeleton() != Skin.GetSkeleton())
	{
		Errors.Add(FString::Printf(TEXT("mesh %s is not on the game skeleton of %s: every skin shares the one skeleton, no retarget"), *Skin.GetName(), *Base.GetName()));
	}
	const FBox BaseBox = Base.GetImportedBounds().GetBox();
	const FBox SkinBox = Skin.GetImportedBounds().GetBox();
	const FGSTN_SkinBodyLengths BaseLengths = Measure(ComposeComponentSpace(Base.GetRefSkeleton()), BaseBox.Min.Z, BaseBox.Max.Z, Bones);
	const FGSTN_SkinBodyLengths SkinLengths = Measure(ComposeComponentSpace(Skin.GetRefSkeleton()), SkinBox.Min.Z, SkinBox.Max.Z, Bones);
	OutRatios = Compare(BaseLengths, SkinLengths);
	Check(OutRatios, Limits, Errors, Unmeasured);
}
