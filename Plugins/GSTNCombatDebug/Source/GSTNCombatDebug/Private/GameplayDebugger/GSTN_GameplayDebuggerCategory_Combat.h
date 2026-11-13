// The combat category of the Gameplay Debugger (combat-m5-editor.md 10.2, 10.2b; plan E15).
#pragma once

#include "CoreMinimal.h"

#if WITH_GAMEPLAY_DEBUGGER_MENU

#include "GameplayDebuggerCategory.h"

/**
 * Shows the combat state of the selected pawn: action state, timeline and frame, hitstop and hitstun remaining, weapon and revisions,
 * the input history in notation with the last request, the last 8 hits dealt and received, and in the world a facing arrow and the
 * timeline frame above the head.
 *
 * Collects on the authority (the host of a listen server, or the dedicated server) and replicates through the category's data pack,
 * so every machine shows the authority's view of the pawn. It reads the combat component through its const accessors only, outside
 * the simulation. Registered by the module at slot 6 ("GSTNCombat"), enabled by default in game and simulate.
 */
class FGSTN_GameplayDebuggerCategory_Combat : public FGameplayDebuggerCategory
{
public:
	FGSTN_GameplayDebuggerCategory_Combat();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;

	/** The category's name in the debugger and in the ini, and its default slot (key 6 on the numpad row) */
	static const FName CategoryName;
	static constexpr int32 DefaultSlot = 6;
};

#endif // WITH_GAMEPLAY_DEBUGGER_MENU
