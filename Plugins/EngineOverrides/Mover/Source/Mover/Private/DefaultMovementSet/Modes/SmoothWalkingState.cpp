// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmoothWalkingState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmoothWalkingState)

namespace SmoothWalkingStateErrorTolerance
{
	constexpr float AngularVelocityErrorTolerance = 10.f;
	constexpr float FacingDegreeErrorTolerance = 10.0f;

	// GAS-NPP fork: wider tolerances for the spring velocities and acceleration (were 10 cm/s and 50 cm/s^2).
	// FMoverDefaultSyncState only reconciles on a 5cm position error and accepts small velocity differences (quantized
	// replicated state, floor checks redone after a rollback since the floor cache is not rolled back), but the springs
	// amplify them: every server update after a layered move (dash), or on stairs after any rollback, triggered a reconcile,
	// in a chain. SpringVelocity is a copy of the real velocity, overwritten every frame. A spring drift that matters shows up
	// as a position error and is reconciled through the default sync state, and a real divergence (e.g. a spring reset on one
	// side only) is still far above these values.
	constexpr float VelocityErrorTolerance = 50.f;
	constexpr float AccelerationErrorTolerance = 500.f;
	// The acceleration reaches several thousand cm/s^2 during layered moves: also accept this fraction of its magnitude
	constexpr float AccelerationRelativeErrorTolerance = 0.1f;
}

UScriptStruct* FSmoothWalkingState::GetScriptStruct() const
{ 
	return StaticStruct(); 
}

FMoverDataStructBase* FSmoothWalkingState::Clone() const
{ 
	return new FSmoothWalkingState(*this); 
}

bool FSmoothWalkingState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bool bSuccess = Super::NetSerialize(Ar, Map, bOutSuccess);

	// Could be quantized to save bandwidth
	Ar << SpringVelocity;
	Ar << SpringAcceleration;
	Ar << IntermediateVelocity;
	Ar << IntermediateFacing;
	Ar << IntermediateAngularVelocity;

	return bSuccess;
}

void FSmoothWalkingState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("SpringVelocity=%s SpringAcceleration=%s IntVel=%s IntFac=%s IntAng=%s\n",
		TCHAR_TO_ANSI(*SpringVelocity.ToCompactString()),
		TCHAR_TO_ANSI(*SpringAcceleration.ToCompactString()),
		TCHAR_TO_ANSI(*IntermediateVelocity.ToCompactString()),
		TCHAR_TO_ANSI(*IntermediateFacing.ToString()),
		TCHAR_TO_ANSI(*IntermediateAngularVelocity.ToString()));
}

bool FSmoothWalkingState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FSmoothWalkingState* AuthoritySpringState = static_cast<const FSmoothWalkingState*>(&AuthorityState);

	const float AccelerationErrorTolerance = FMath::Max(SmoothWalkingStateErrorTolerance::AccelerationErrorTolerance,
		SmoothWalkingStateErrorTolerance::AccelerationRelativeErrorTolerance * AuthoritySpringState->SpringAcceleration.GetAbsMax());

	return (!(SpringVelocity - AuthoritySpringState->SpringVelocity).IsNearlyZero(SmoothWalkingStateErrorTolerance::VelocityErrorTolerance) ||
			!(SpringAcceleration - AuthoritySpringState->SpringAcceleration).IsNearlyZero(AccelerationErrorTolerance) ||
			!(IntermediateVelocity - AuthoritySpringState->IntermediateVelocity).IsNearlyZero(SmoothWalkingStateErrorTolerance::VelocityErrorTolerance) ||
			 (FMath::RadiansToDegrees(IntermediateFacing.AngularDistance(AuthoritySpringState->IntermediateFacing)) > SmoothWalkingStateErrorTolerance::FacingDegreeErrorTolerance ||
			!(IntermediateAngularVelocity - AuthoritySpringState->IntermediateAngularVelocity).IsNearlyZero(SmoothWalkingStateErrorTolerance::AngularVelocityErrorTolerance)));
}


void FSmoothWalkingState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FSmoothWalkingState* FromState = static_cast<const FSmoothWalkingState*>(&From);
	const FSmoothWalkingState* ToState   = static_cast<const FSmoothWalkingState*>(&To);

	SpringVelocity = FMath::Lerp(FromState->SpringVelocity, ToState->SpringVelocity, Pct);
	SpringAcceleration = FMath::Lerp(FromState->SpringAcceleration, ToState->SpringAcceleration, Pct);
	IntermediateVelocity = FMath::Lerp(FromState->IntermediateVelocity, ToState->IntermediateVelocity, Pct);
	IntermediateFacing = FQuat::Slerp(FromState->IntermediateFacing, ToState->IntermediateFacing, Pct);
	IntermediateAngularVelocity = FMath::Lerp(FromState->IntermediateAngularVelocity, ToState->IntermediateAngularVelocity, Pct);
}
