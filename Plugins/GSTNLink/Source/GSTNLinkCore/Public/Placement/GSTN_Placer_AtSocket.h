// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Placement/GSTN_LinkExitPlacer.h"
#include "GSTN_Placer_AtSocket.generated.h"

/**
 * The exit of a throw (m6 spec 7.7): the entity leaves from the partner's socket, and its velocity is the throw hit's knockback, not
 * anything the placer invents. If the capsule does not fit at the socket the placer tries a few positions raised along the local up
 * before it gives up and reports the fallback next to the partner as invalid (the velocity is kept: the throw still launches).
 */
UCLASS(meta = (DisplayName = "At Socket Placer"))
class GSTNLINKCORE_API UGSTN_Placer_AtSocket : public UGSTN_LinkExitPlacer
{
	GENERATED_BODY()

public:
	/** How far each retry raises the capsule above the socket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float NudgeCm = 15.f;

	/** How many raised retries follow the socket position itself */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket", meta = (ClampMin = "0", ClampMax = "16"))
	int32 MaxNudges = 3;

	/** Height of the fallback above the partner's position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FallbackLiftCm = 100.f;

	virtual FGSTN_ExitResult Place_Implementation(const FGSTN_ExitQuery& Query) const override;
};
