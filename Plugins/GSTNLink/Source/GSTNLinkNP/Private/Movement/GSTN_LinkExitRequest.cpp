// Links, Network Prediction half.

#include "Movement/GSTN_LinkExitRequest.h"

#include "AbilitySystemComponent.h"
#include "GSTN_LinkWatchdog.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

bool FGSTN_LinkExitEffect::ApplyMovementEffect(FApplyMovementEffectParams& ApplyEffectParams, FMoverSyncState& OutputState)
{
	if (!FTeleportEffect::ApplyMovementEffect(ApplyEffectParams, OutputState))
	{
		return false;
	}
	FMoverDefaultSyncState& Sync = OutputState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	Sync.SetTransforms_WorldSpace(Sync.GetLocation_WorldSpace(), Sync.GetOrientation_WorldSpace(), ExitVelocity, Sync.GetAngularVelocityDegrees_WorldSpace(), nullptr);
	return true;
}

FInstantMovementEffect* FGSTN_LinkExitEffect::Clone() const
{
	return new FGSTN_LinkExitEffect(*this);
}

void FGSTN_LinkExitEffect::NetSerialize(FArchive& Ar)
{
	FTeleportEffect::NetSerialize(Ar);
	Ar << ExitVelocity;
}

UScriptStruct* FGSTN_LinkExitEffect::GetScriptStruct() const
{
	return FGSTN_LinkExitEffect::StaticStruct();
}

FString FGSTN_LinkExitEffect::ToSimpleString() const
{
	return FString::Printf(TEXT("GSTN link exit to %s, velocity %s"), *TargetLocation.ToCompactString(), *ExitVelocity.ToCompactString());
}

FEffectMoveContainer UGSTN_LinkExitMoveCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent, UAbilitySystemComponent* TargetAbilitySystemComponent,
	const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	FGSTN_LinkExitEffect Effect;
	if (EffectSpec.IsValid() && EffectSpec.Data.IsValid())
	{
		const FGameplayEffectSpec& Spec = *EffectSpec.Data.Get();
		auto Read = [&Spec](FName Name) { return static_cast<double>(Spec.GetSetByCallerMagnitude(Name, false, 0.f)); };
		Effect.TargetLocation = FVector(Read(FGSTN_LinkExitRequest::PositionX()), Read(FGSTN_LinkExitRequest::PositionY()), Read(FGSTN_LinkExitRequest::PositionZ()));
		Effect.bUseActorRotation = false;
		Effect.TargetRotation = FRotator(0.0, Read(FGSTN_LinkExitRequest::Yaw()), 0.0);
		Effect.ExitVelocity = FVector(Read(FGSTN_LinkExitRequest::VelocityX()), Read(FGSTN_LinkExitRequest::VelocityY()), Read(FGSTN_LinkExitRequest::VelocityZ()));
	}
	return FEffectMoveContainer(&Effect);
}

UGSTN_GE_LinkExit::UGSTN_GE_LinkExit()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	UApplyMoveGameplayEffectComponent* ApplyMove = CreateDefaultSubobject<UApplyMoveGameplayEffectComponent>(TEXT("ApplyMove"));
	ApplyMove->MoveProcessor = CreateDefaultSubobject<UGSTN_LinkExitMoveCreator>(TEXT("ExitMoveCreator"));
	GEComponents.Add(ApplyMove);
}

FName FGSTN_LinkExitRequest::PositionX() { static const FName Name(TEXT("GSTN.LinkExit.PositionX")); return Name; }
FName FGSTN_LinkExitRequest::PositionY() { static const FName Name(TEXT("GSTN.LinkExit.PositionY")); return Name; }
FName FGSTN_LinkExitRequest::PositionZ() { static const FName Name(TEXT("GSTN.LinkExit.PositionZ")); return Name; }
FName FGSTN_LinkExitRequest::Yaw() { static const FName Name(TEXT("GSTN.LinkExit.Yaw")); return Name; }
FName FGSTN_LinkExitRequest::VelocityX() { static const FName Name(TEXT("GSTN.LinkExit.VelocityX")); return Name; }
FName FGSTN_LinkExitRequest::VelocityY() { static const FName Name(TEXT("GSTN.LinkExit.VelocityY")); return Name; }
FName FGSTN_LinkExitRequest::VelocityZ() { static const FName Name(TEXT("GSTN.LinkExit.VelocityZ")); return Name; }

bool FGSTN_LinkExitRequest::IsDue(const FGSTN_LinkActions::FExit& Exit, int32 Frame)
{
	return Exit.Frame != 0 && Exit.Frame == Frame && (Exit.Flags & GSTNLinkExitFlags::Teleport) != 0;
}

bool FGSTN_LinkExitRequest::Apply(UAbilitySystemComponent* AbilitySystem, const FGSTN_LinkActions::FExit& Exit)
{
	if (!AbilitySystem)
	{
		return false;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(UGSTN_GE_LinkExit::StaticClass(), 1.f, AbilitySystem->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return false;
	}
	FGameplayEffectSpec& Data = *Spec.Data.Get();
	// Floats: the command carried the exit quantised to floats already, so nothing is lost here
	Data.SetSetByCallerMagnitude(PositionX(), static_cast<float>(Exit.Position.X));
	Data.SetSetByCallerMagnitude(PositionY(), static_cast<float>(Exit.Position.Y));
	Data.SetSetByCallerMagnitude(PositionZ(), static_cast<float>(Exit.Position.Z));
	Data.SetSetByCallerMagnitude(Yaw(), Exit.YawDeg);
	Data.SetSetByCallerMagnitude(VelocityX(), static_cast<float>(Exit.Velocity.X));
	Data.SetSetByCallerMagnitude(VelocityY(), static_cast<float>(Exit.Velocity.Y));
	Data.SetSetByCallerMagnitude(VelocityZ(), static_cast<float>(Exit.Velocity.Z));
	AbilitySystem->ApplyGameplayEffectSpecToSelf(Data);
	return true;
}
