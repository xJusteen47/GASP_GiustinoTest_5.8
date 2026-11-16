// Pre-mission loadout screen: one per local player.

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutDraft.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "GSTN_LoadoutMenuSubsystem.generated.h"

class APawn;
class SGSTN_LoadoutMenu;
class UGSTN_InputConfig;
class UGSTN_LoadoutComponent;
class UInputAction;
class UInputMappingContext;

/**
 * The loadout screen of one local player (M11): kit, specials, mods and backpack chosen with the pad before the match, for every player
 * of a listen server on their own machine. It holds an FGSTN_LoadoutDraft and a Slate widget on that player's part of the viewport;
 * confirming sends the draft with UGSTN_LoadoutComponent::ServerRequestLoadout (the PlayerState's component: the server validates it,
 * it replicates, and it is granted on every pawn the player spawns) and saves it as the player's last loadout for the next session.
 *
 * Input. No key is named here (owner rule): navigation reads the player's own Move action and two functions of the button layout,
 * "Menu.Confirm" and "Menu.Back" (UGSTN_ButtonLayout, defaults in DefaultGSTNInput.ini), through the pawn's input config
 * (IGSTN_InputConfigSource). While open, a transient mapping context copies every key the player has mapped (their remaps included)
 * onto the menu's own actions at a high priority and consumes them, so the character neither moves nor attacks and the simulation
 * keeps getting a neutral command. The copy follows the player's contexts while the screen is open: a context added or removed after it opened
 * (the pawn binding its ability input a few frames after spawning, the input config replacing the base game's context) is copied again, so
 * no key is left unmapped (the first version copied once, at the first frame of the pawn, when the player had no keys yet: the screen got
 * nothing and the character kept moving). Nothing here runs in the simulation; it is local presentation and one RPC.
 *
 * Debug mode (OpenMenu(true), the loadout debug pad AGSTN_LoadoutMenuPad): the same draft and input, drawn as a bare text list with every
 * row's index and option count, the applied kit and the rules' verdict.
 */
UCLASS()
class GSTNLOADOUTMENU_API UGSTN_LoadoutMenuSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/**
	 * Opens the screen for this player (bDebug: the bare debug list). False while there is no pawn, loadout component or input config yet;
	 * GetOpenProblem says which
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	bool OpenMenu(bool bDebug = false);

	/** Why the last OpenMenu returned false, empty after one that opened */
	const FString& GetOpenProblem() const { return OpenProblem; }

	/** Closes it without sending anything */
	UFUNCTION(BlueprintCallable, Category = "Combat|Loadout")
	void CloseMenu();

	UFUNCTION(BlueprintPure, Category = "Combat|Loadout")
	bool IsMenuOpen() const { return bOpen; }

	const FGSTN_LoadoutDraft& GetDraft() const { return Draft; }

	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual TStatId GetStatId() const override;

private:
	UFUNCTION()
	void HandleRejected(UGSTN_LoadoutComponent* Component, const TArray<FString>& Errors);

	APawn* GetPawn() const;
	UGSTN_LoadoutComponent* FindLoadoutComponent() const;
	bool ShouldAutoOpen(const UWorld& World) const;

	/** Builds the menu's mapping context from the player's active mappings and adds it */
	bool SetUpInput(const UGSTN_InputConfig& Config);
	void TearDownInput();

	/** (Re)fills MenuContext from the player's mappings without it and applies it at once */
	void CopyPlayerKeys();

	/** True when a mapping of the player's own contexts reaches the player past the menu's copy (a context that changed after the copy) */
	bool HasUncopiedKeys() const;

	/** Stick and buttons, polled each frame while open */
	void HandleInput(float DeltaTime);
	void Confirm();
	void RefreshWidget();

	/** The keys mapped to a button function of the layout, for the on-screen hints */
	FText DescribeKeys(const UInputAction* Action) const;

	FGSTN_LoadoutDraft Draft;
	TSharedPtr<SGSTN_LoadoutMenu> Widget;
	TWeakObjectPtr<UGSTN_LoadoutComponent> Component;

	/** The world the automatic opening last ran for: a new level opens it again */
	TWeakObjectPtr<UWorld> AutoOpenWorld;
	bool bAutoOpenDone = false;
	bool bOpen = false;
	bool bDebug = false;
	FString OpenProblem;

	/** The player's own actions the menu actions stand for (resolved from the input config at open) */
	TWeakObjectPtr<const UInputAction> SourceMove;
	TWeakObjectPtr<const UInputAction> SourceConfirm;
	TWeakObjectPtr<const UInputAction> SourceBack;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MenuContext;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ConfirmAction;
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BackAction;
	/** Every other key the player has mapped, so it does not reach the character */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SwallowAction;

	bool bConfirmHeld = true;
	bool bBackHeld = true;
	FIntPoint HeldDirection = FIntPoint::ZeroValue;
	float RepeatTimer = 0.f;

	FText ConfirmHint;
	FText BackHint;
	FString Feedback;
};
