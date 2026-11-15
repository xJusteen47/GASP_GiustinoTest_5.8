// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_CommandBindings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_CombatMoveAuthoring.generated.h"

class UAnimMontage;
class UBlueprint;
class UGameplayAbility;
class UGSTN_GA_BaseCombat;
class UGSTN_PrimaryKitDefinition;
class UGSTN_WeaponDefinition;
struct FGSTN_AbilityBinding;

/** What a new combat move starts as. Every choice is plain timeline data the designer retunes afterwards: nothing is locked in */
UENUM(BlueprintType)
enum class EGSTN_CombatMoveTemplate : uint8
{
	/** A melee hit: one sphere hitbox swept forward over the active frames (the jab of the examples, as a starting point) */
	Strike,
	/**
	 * No hitbox: one Event key on the first active frame, for a move made of actions (add a Fire Projectile, Send Event or Apply Effect action
	 * to the key) or of a Blueprint graph (On Move Key)
	 */
	Action,
	/** Frames only: no track at all (a stance, a taunt, a move the designer lays out track by track) */
	Empty
};

/**
 * Where a new move is bound, so it is playable without opening the kit: the kit or weapon gets a binding of Command (with Requirements) to the
 * move. A binding with the same command and the same requirements is replaced, any other binding is left alone. Nothing here names an input:
 * the command is a tag of the kit's command set, and which button enters it is the input plugin's data
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATEDITOR_API FGSTN_MoveKitHookup
{
	GENERATED_BODY()

	/** The kit whose own bindings get the move (KitBindings). Empty = not bound to a kit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	TSoftObjectPtr<UGSTN_PrimaryKitDefinition> Kit;

	/** The weapon whose bindings get the move (a move that changes with the weapon). Empty = not bound to a weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	TSoftObjectPtr<UGSTN_WeaponDefinition> Weapon;

	/** The command the move answers (Command.Attack.Neutral, Command.Special.Directional...). Needed when a kit or a weapon is set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit", meta = (Categories = "Command"))
	FGameplayTag Command;

	/** The context the binding needs (a grounded or an airborne version of the same command, a chain tag). Empty = any context */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	FGSTN_TagRequirements Requirements;

	/** Whether a kit or a weapon is named */
	bool WantsBinding() const { return !Kit.IsNull() || !Weapon.IsNull(); }
};

/**
 * The choices of the "Combat Move" entry of the Content Browser (and of CreateCombatMove for a script). Frames are simulation frames (62.5 Hz).
 * Out-of-order frames are put in order, never refused (SanitizeOptions says what it changed)
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATEDITOR_API FGSTN_NewCombatMoveOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	EGSTN_CombatMoveTemplate Template = EGSTN_CombatMoveTemplate::Strike;

	/** The class the move Blueprint derives from: the combat ability, or a game's own child of it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move", meta = (AllowAbstract = "true"))
	TSubclassOf<UGSTN_GA_BaseCombat> ParentClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frames", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 TotalFrames = 30;

	/** First active frame: the hitbox comes out (Strike), the Event key fires (Action) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frames", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ActiveStart = 6;

	/** Last active frame (inclusive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frames", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ActiveEnd = 9;

	/** First frame the character is free again; kept after the active frames and inside the move */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Frames", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 FirstActionableFrame = 24;

	/** Optional: the animation. Empty = the move plays without one until the designer picks it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.f;

	/** Action template: the tag of the Event key, what On Move Key compares (Example.Key.Fire). Empty = an untagged key */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	FGameplayTag KeyTag;

	/**
	 * Off: the timeline lives inside the move Blueprint (Class Defaults > Timeline, with the Open Timeline Editor row). On: a TL_ asset is made next
	 * to the move and the move points to it, for a timeline several moves or weapons share
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	bool bTimelineAsAsset = false;

	/**
	 * Lays down the move events in the event graph (On Move Started, On Move Key, On Move Hit Confirmed, On Move Ended), unwired, for a move that
	 * will have a Blueprint graph. Off = a data-only move (most moves)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Move")
	bool bAddMoveEvents = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kit")
	FGSTN_MoveKitHookup Hookup;

	FGSTN_NewCombatMoveOptions();
};

/**
 * Makes combat moves from the editor (CP5 of the Blueprint seams): the "Combat Move" factory of the Content Browser and its "Add to Kit or
 * Weapon" action call this, and so can a Python script or an editor agent (every function is BlueprintCallable), so a move is never wired by
 * hand. Static, stateless, editor only. Nothing here saves: the Content Browser shows the changed assets as unsaved, a script saves them.
 */
UCLASS()
class GSTNCOMBATEDITOR_API UGSTN_CombatMoveAuthoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Puts the frames in order (1 <= total, active start <= active end < total, active end < actionable <= total). One line per change */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Move Authoring", meta = (DevelopmentOnly))
	static TArray<FString> SanitizeOptions(UPARAM(ref) FGSTN_NewCombatMoveOptions& Options);

	/** The starting timeline of the options (sanitized first): frames, montage and the template's track */
	UFUNCTION(BlueprintPure, Category = "GSTN Combat|Move Authoring", meta = (DevelopmentOnly))
	static FGSTN_CombatTimelineData MakeStarterTimeline(const FGSTN_NewCombatMoveOptions& Options);

	/**
	 * Makes the move Blueprint FolderPath/AssetName ("/Game/Moves", "GA_Uppercut") with its starter timeline, its optional TL_ asset, move events
	 * and kit or weapon binding. Null and OutError when the name is taken, the parent class is not a combat ability or the Blueprint does not
	 * compile. A failed binding does not undo the move: OutReport says what was bound and what was not
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Move Authoring", meta = (DevelopmentOnly))
	static UBlueprint* CreateCombatMove(const FString& FolderPath, const FString& AssetName, FGSTN_NewCombatMoveOptions Options, FString& OutReport,
		FString& OutError);

	/** CreateCombatMove in an existing package (the factory's InParent, a test's temporary package) */
	static UBlueprint* CreateCombatMoveIn(UObject* Outer, FName Name, EObjectFlags Flags, FGSTN_NewCombatMoveOptions Options, FString& OutReport,
		FString& OutError);

	/** Whether the Blueprint makes a combat move (its class derives from the combat ability) */
	UFUNCTION(BlueprintPure, Category = "GSTN Combat|Move Authoring", meta = (DevelopmentOnly))
	static bool IsCombatMoveBlueprint(const UBlueprint* Blueprint);

	/**
	 * Binds MoveClass to the kit and/or weapon of the hookup (loading them). Returns false when nothing could be bound (no command, no kit or
	 * weapon, an asset that does not load); OutReport has one line per asset
	 */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Move Authoring", meta = (DevelopmentOnly))
	static bool AddMoveToKit(TSubclassOf<UGameplayAbility> MoveClass, const FGSTN_MoveKitHookup& Hookup, FString& OutReport);

	/** The same with the assets in hand (either may be null). Marks what it changed dirty and transactional (Ctrl+Z undoes it) */
	UFUNCTION(BlueprintCallable, Category = "GSTN Combat|Move Authoring", meta = (DevelopmentOnly))
	static bool AddMoveToKitAssets(TSubclassOf<UGameplayAbility> MoveClass, UGSTN_PrimaryKitDefinition* Kit, UGSTN_WeaponDefinition* Weapon,
		FGameplayTag Command, const FGSTN_TagRequirements& Requirements, FString& OutReport);

	/**
	 * Puts Binding in Bindings: in place of the entry with the same command and the same requirements (the key inheritance uses), else at the end.
	 * Returns true when it replaced one
	 */
	static bool UpsertBinding(TArray<FGSTN_AbilityBinding>& Bindings, const FGSTN_AbilityBinding& Binding);

	/** The name the TL_ asset of a move gets: GA_Uppercut -> TL_Uppercut, Uppercut -> TL_Uppercut */
	static FString TimelineAssetNameFor(const FString& MoveName);
};
