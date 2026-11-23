// Respawn, server side.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSTN_Checkpoint.generated.h"

class UBoxComponent;

/**
 * A checkpoint of a level (game-modes 5): shared by the team, the first player whose server-side position enters the box activates it for everyone,
 * and fallen players come back at its spawn points. Not a collision volume: the respawn subsystem tests player positions against the box at its
 * poll rate, outside the simulation, so nothing here runs on rollback. Blueprint children add the look (flag, light) on OnActivatedChanged.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GSTN Checkpoint"))
class GSTNRESPAWNRUNTIME_API AGSTN_Checkpoint : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_Checkpoint();

	/** The touch area (its shape only; no collision) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
	TObjectPtr<UBoxComponent> Area;

	/** Position along the level: with bCheckpointsOnlyForward a lower order never replaces a higher one */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint", meta = (ClampMin = "0"))
	int32 Order = 0;

	/** Where players come back, relative to the actor; player N takes point N, more players than points stand behind them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint", meta = (MakeEditWidget = true))
	TArray<FTransform> SpawnPoints;

	/** Distance between rows when there are more players than spawn points */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint", meta = (ClampMin = "0", ForceUnits = "cm"))
	float RowSpacing = 150.f;

	/** The team's active checkpoint (replicated for the look; the server's respawn subsystem owns the decision) */
	UPROPERTY(ReplicatedUsing = OnRep_Active, BlueprintReadOnly, Category = "Checkpoint")
	bool bActive = false;

	/** World transform of the Index-th returning player */
	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	FTransform GetSpawnTransform(int32 Index) const;

	/** Whether a world position is inside the area */
	UFUNCTION(BlueprintCallable, Category = "Checkpoint")
	bool ContainsLocation(const FVector& Location) const;

	/** Server: set by the respawn subsystem */
	void SetActive(bool bInActive);

	/** Look of the activation (every machine) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Checkpoint")
	void OnActivatedChanged(bool bIsActive);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void OnRep_Active();
};
