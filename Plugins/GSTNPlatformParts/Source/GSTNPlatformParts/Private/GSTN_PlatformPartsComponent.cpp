// Platform parts bridge: part state -> platform shape mask (traversal-surfaces.md 4.4, plan T6).

#include "GSTN_PlatformPartsComponent.h"

#include "Data/GSTN_PartSetDefinition.h"
#include "GameFramework/Actor.h"
#include "GSTN_EnemyPlatformComponent.h"
#include "GSTN_PlatformSpec.h"
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "Sim/GSTN_PartsAttach.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "State/GSTN_PartsFragment.h"
#include "Sync/GSTN_SyncFragment.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformPartsComponent)

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNPlatformParts)

DEFINE_LOG_CATEGORY_STATIC(LogGSTNPlatformParts, Log, All);

#define LOCTEXT_NAMESPACE "GSTNPlatformParts"

UGSTN_PlatformPartsComponent::UGSTN_PlatformPartsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

FGSTN_PlatformPartGates UGSTN_PlatformPartsComponent::ResolveGates(const FGSTN_PlatformSpec& Spec, const UGSTN_PartSetDefinition* Set, TArray<TPair<int32, FName>>* OutUnknown)
{
	FGSTN_PlatformPartGates Result;
	if (!Set)
	{
		return Result;
	}
	const TConstArrayView<FGSTN_PartDefinition> Parts = Set->GetParts();
	TArray<int32, TInlineAllocator<16>> Parents;
	for (const FGSTN_PartDefinition& Part : Parts)
	{
		Parents.Add(Part.ParentIndex);
	}
	for (int32 Index = 0; Index < Spec.Shapes.Num(); ++Index)
	{
		const FGSTN_PlatformShape& Shape = Spec.Shapes[Index];
		if (Shape.PartName.IsNone() || Shape.ShapeId >= GSTNPlatform::MaxShapes)
		{
			continue;
		}
		const int32 PartIndex = Set->FindPartByName(Shape.PartName);
		if (PartIndex == INDEX_NONE || PartIndex >= Parts.Num())
		{
			if (OutUnknown)
			{
				OutUnknown->Emplace(Index, Shape.PartName);
			}
			continue;
		}
		Result.Gates[Shape.ShapeId] = FGSTN_PlatformPartGates::MakeGate(PartIndex, Parents);
	}
	return Result;
}

void UGSTN_PlatformPartsComponent::OnRegister()
{
	Super::OnRegister();
	if (const UWorld* World = GetWorld(); World && World->IsGameWorld())
	{
		BindPlatform();
	}
}

void UGSTN_PlatformPartsComponent::OnUnregister()
{
	UnbindPlatform();
	Super::OnUnregister();
}

void UGSTN_PlatformPartsComponent::BeginPlay()
{
	Super::BeginPlay();
	// The platform may register after this component (component order of the Blueprint)
	BindPlatform();
}

void UGSTN_PlatformPartsComponent::BindPlatform()
{
	if (PrePoseHandle.IsValid())
	{
		return;
	}
	AActor* Owner = GetOwner();
	UGSTN_EnemyPlatformComponent* Found = Owner ? Owner->FindComponentByClass<UGSTN_EnemyPlatformComponent>() : nullptr;
	if (!Found)
	{
		return;
	}
	Platform = Found;
	PrePoseHandle = Found->OnPrePose.AddUObject(this, &UGSTN_PlatformPartsComponent::HandlePrePose);
}

void UGSTN_PlatformPartsComponent::UnbindPlatform()
{
	if (UGSTN_EnemyPlatformComponent* Bound = Platform.Get(); Bound && PrePoseHandle.IsValid())
	{
		Bound->OnPrePose.Remove(PrePoseHandle);
	}
	PrePoseHandle.Reset();
	Platform.Reset();
}

uint16 UGSTN_PlatformPartsComponent::ReadBrokenMask()
{
	if (BrokenMaskOverride.IsSet())
	{
		return BrokenMaskOverride.GetValue();
	}
	if (!CombatSim.IsValid())
	{
		AActor* Owner = GetOwner();
		CombatSim = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	}
	const UGSTN_CombatSimComponent* Combat = CombatSim.Get();
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	const FGSTN_PartsFragment* Fragment = Sync ? FGSTN_SyncFragmentList::Find<FGSTN_PartsFragment>(Sync->Fragments) : nullptr;
	return Fragment ? Fragment->BrokenMask : 0;
}

void UGSTN_PlatformPartsComponent::HandlePrePose(UGSTN_EnemyPlatformComponent& InPlatform, const FGSTN_PlatformPoseContext& Context)
{
	const UGSTN_PartSetDefinition* Set = FGSTN_PartsAttach::FindSet(GetOwner());
	if (!bResolved || ResolvedSet.Get() != Set)
	{
		TArray<TPair<int32, FName>> Unknown;
		Gates = ResolveGates(InPlatform.GetSpec(), Set, &Unknown);
		ResolvedSet = Set;
		bResolved = true;
		for (const TPair<int32, FName>& Entry : Unknown)
		{
			UE_LOG(LogGSTNPlatformParts, Error, TEXT("%s: shape %d names part '%s', which part set %s lacks: the shape behaves as body"),
				*GetPathName(), Entry.Key, *Entry.Value.ToString(), *GetNameSafe(Set));
		}
	}

	// The authority reads the confirmed mask and records the frame it takes effect at; a client takes both from the replicated state
	int32 ChangeFrame = INDEX_NONE;
	const AActor* Owner = GetOwner();
	if (!Owner || Owner->HasAuthority())
	{
		const uint16 Mask = ReadBrokenMask();
		if (Mask != NetState.BrokenMask)
		{
			NetState.BrokenMask = Mask;
			NetState.Frame = Context.Frame != INDEX_NONE ? Context.Frame : InPlatform.GetLatestPoseFrame() + 1;
			ChangeFrame = Context.Frame;
		}
	}
	else if (static_cast<uint16>(NetState.BrokenMask) != LastBrokenMask)
	{
		ChangeFrame = NetState.Frame;
	}
	LastBrokenMask = static_cast<uint16>(NetState.BrokenMask);

	// A gate change alone (another set) takes effect at the next frame; a mask change at its frame
	const uint8 Desired = Gates.ComputeEnabledShapes(LastBrokenMask);
	if (Desired != AppliedMask)
	{
		AppliedMask = Desired;
		InPlatform.SetShapeMaskAtFrame(Desired, bTelegraphBreak ? EGSTN_PlatformShapeState::Breaking : EGSTN_PlatformShapeState::Inactive, ChangeFrame);
	}
}

void UGSTN_PlatformPartsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGSTN_PlatformPartsComponent, NetState);
}

const UGSTN_PartSetDefinition* UGSTN_PlatformPartsComponent::FindValidationSet() const
{
	if (!ValidationPartSet.IsNull())
	{
		return ValidationPartSet.LoadSynchronous();
	}
	const AActor* Owner = GetOwner();
	const UGSTN_PartsAttachComponent* Attach = Owner ? Owner->FindComponentByClass<UGSTN_PartsAttachComponent>() : nullptr;
	return Attach && !Attach->DefaultPartSet.IsNull() ? Attach->DefaultPartSet.LoadSynchronous() : nullptr;
}

bool UGSTN_PlatformPartsComponent::CollectFindings(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	const AActor* Owner = GetOwner();
	const UGSTN_EnemyPlatformComponent* Found = Owner ? Owner->FindComponentByClass<UGSTN_EnemyPlatformComponent>() : nullptr;
	if (!Found)
	{
		OutWarnings.Add(LOCTEXT("NoPlatform", "No GSTN Enemy Platform component next to the parts bridge: it does nothing"));
		return true;
	}
	const FGSTN_PlatformSpec& Spec = Found->GetSpec();
	const bool bAnyName = Spec.Shapes.ContainsByPredicate([](const FGSTN_PlatformShape& Shape) { return !Shape.PartName.IsNone(); });
	if (!bAnyName)
	{
		return true;
	}
	const UGSTN_PartSetDefinition* Set = FindValidationSet();
	if (!Set)
	{
		OutWarnings.Add(LOCTEXT("NoSet", "The shapes name parts but there is no part set to check them against: set ValidationPartSet, or a DefaultPartSet on the GSTN Parts Attach component"));
		return true;
	}
	TArray<TPair<int32, FName>> Unknown;
	ResolveGates(Spec, Set, &Unknown);
	for (const TPair<int32, FName>& Entry : Unknown)
	{
		OutErrors.Add(FText::Format(LOCTEXT("UnknownPart", "Shape {0} names part {1}, which part set {2} does not have"),
			Entry.Key, FText::FromName(Entry.Value), FText::FromString(Set->GetName())));
	}
	return Unknown.Num() == 0;
}

#if WITH_EDITOR
EDataValidationResult UGSTN_PlatformPartsComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FText> Errors, Warnings;
	CollectFindings(Errors, Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
	}
	return Errors.Num() > 0 ? EDataValidationResult::Invalid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
