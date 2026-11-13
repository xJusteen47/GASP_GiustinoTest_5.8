// Interaction runtime: shared types (interaction.md §3.4, §5, §9). Game thread only; never simulated.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InteractTypes.h"
#include "Engine/Texture2D.h"
#include "GSTN_InteractRuntimeTypes.generated.h"

GSTNINTERACTRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogGSTNInteract, Log, All);

/** What the player does with the focus */
UENUM(BlueprintType)
enum class EGSTN_InteractVerb : uint8
{
	/** A press and release */
	Tap,
	/** A press held for the meaning's HoldFrames */
	Hold
};

/** What the prompt shows about a target (§5). Empty verbs are not drawn: a pickup shows only the hold */
USTRUCT(BlueprintType)
struct GSTNINTERACTRUNTIME_API FGSTN_InteractDisplayInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FText TapVerb;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FText HoldVerb;
};

/** What a presenter is fed each frame for the local focus (§5). A ghost or unresolvable focus is never presented */
USTRUCT(BlueprintType)
struct GSTNINTERACTRUNTIME_API FGSTN_InteractPresentationView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	FGSTN_InteractCandidateId FocusId;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	TWeakObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	FGSTN_InteractDisplayInfo Display;

	/** World position of the highlight and prompt */
	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	FVector Anchor = FVector::ZeroVector;

	/** Reach to the surface, for scaling a ring */
	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	float RadiusCm = 0.f;

	/** 0..1 progress of the hold being predicted locally; 0 when nothing is held */
	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	float HoldProgress = 0.f;

	/** Focused but not actionable ("needs key"): drawn greyed */
	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	bool bUnactionable = false;
};

/**
 * The replicated part of a spawned record (§3.2): the server's handle and stamps. Level-placed actors replicate nothing: both
 * machines derive the same handle from the actor's stable key and the record exists from frame 0. Frames are server frames.
 */
USTRUCT()
struct GSTNINTERACTRUNTIME_API FGSTN_InteractRecordStamp
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Handle = 0;

	UPROPERTY()
	int32 FromFrame = 0;

	UPROPERTY()
	int32 ActionableFrom = 0;

	bool IsSet() const { return Handle != 0; }
};

/** Console variables of the runtime (all local, presentation-side or debug) */
namespace GSTNInteractCVars
{
	/** gstn.Interact.Enable: 0 stops every interactor from probing (a debugging switch) */
	GSTNINTERACTRUNTIME_API bool IsEnabled();
	/** gstn.Interact.Presentation: 0 hides the prompt and the highlight, the focus still runs */
	GSTNINTERACTRUNTIME_API bool IsPresentationEnabled();
	/** gstn.Interact.Debug: 1 also draws every record near each local interactor */
	GSTNINTERACTRUNTIME_API bool IsDebugDrawEnabled();
}
