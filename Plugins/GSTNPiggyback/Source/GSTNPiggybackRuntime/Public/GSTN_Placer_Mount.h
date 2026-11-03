// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_MountTypes.h"
#include "Placement/GSTN_LinkExitPlacer.h"
#include "GSTN_Placer_Mount.generated.h"

/**
 * Where a rider is put down when its mount ends without a launch (combat-m7-piggyback.md 6.4): the candidates of UGSTN_MountUtils::ExitCandidates
 * for Mode, in order, the first free one wins; none free = GSTNLink's fallback above the carrier, reported invalid. The anchor is the query's
 * SocketTransform (the carrier's socket with the role's offset, where the rider was parked), the carrier the query's PartnerTransform.
 *
 * Instanced on the mount type's definition, so the mode and distance are the definition's data. The query carries no kit, so the carrier
 * profile's DefaultExit and ExitDistance are not read per carrier yet (m7-doc-deltas G5, OPEN): the placer's own Mode and DistanceCm decide.
 * Deterministic for equal inputs; the world is asked only through the query's collision interface.
 */
UCLASS(meta = (DisplayName = "Mount Exit Placer"))
class GSTNPIGGYBACKRUNTIME_API UGSTN_Placer_Mount : public UGSTN_LinkExitPlacer
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit")
	EGSTN_ExitMode Mode = EGSTN_ExitMode::BehindCarrier;

	/** Distance of the ring candidates from the carrier's root */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float DistanceCm = 90.f;

	virtual FGSTN_ExitResult Place_Implementation(const FGSTN_ExitQuery& Query) const override;
};
