// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Loadout/GSTN_LoadoutComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Effects/GSTN_GE_WeaponStats.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Input/GSTN_InputSettings.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_SkinDefinition.h"
#include "Kits/GSTN_SuperDefinition.h"
#include "Kits/GSTN_SuperInputScheme.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLoadoutComponent, Log, All);

UGSTN_LoadoutComponent::UGSTN_LoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGSTN_LoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Everyone gets the loadout: remote fighters' tables and skins are built from it
	DOREPLIFETIME(UGSTN_LoadoutComponent, Revision);
	DOREPLIFETIME(UGSTN_LoadoutComponent, Applied);
}

void UGSTN_LoadoutComponent::BeginPlay()
{
	Super::BeginPlay();

	// Grants are the server's: it re-applies when the PlayerState gets a new pawn (respawn)
	if (IsServerAuthority())
	{
		if (APlayerState* PlayerState = Cast<APlayerState>(GetOwner()))
		{
			PlayerState->OnPawnSet.AddDynamic(this, &UGSTN_LoadoutComponent::HandlePawnSet);
		}
	}
}

void UGSTN_LoadoutComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerState* PlayerState = Cast<APlayerState>(GetOwner()))
	{
		PlayerState->OnPawnSet.RemoveDynamic(this, &UGSTN_LoadoutComponent::HandlePawnSet);
	}
	Super::EndPlay(EndPlayReason);
}

bool UGSTN_LoadoutComponent::IsServerAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

const UGSTN_LoadoutRules& UGSTN_LoadoutComponent::GetRules() const
{
	const UGSTN_LoadoutRules* Custom = Rules.IsNull() ? nullptr : Rules.LoadSynchronous();
	return Custom ? *Custom : *GetDefault<UGSTN_LoadoutRules>();
}

UGSTN_PrimaryKitDefinition* UGSTN_LoadoutComponent::GetResolvedKit() const
{
	return const_cast<UGSTN_PrimaryKitDefinition*>(Resolved.Kit);
}

UGSTN_SkinDefinition* UGSTN_LoadoutComponent::GetResolvedSkin() const
{
	return const_cast<UGSTN_SkinDefinition*>(Resolved.Skin);
}

UObject* UGSTN_LoadoutComponent::ResolveAsset(const FPrimaryAssetId& Id) const
{
	if (!Id.IsValid() || !UAssetManager::IsInitialized())
	{
		return nullptr;
	}
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Id);
	return Path.IsValid() ? Path.TryLoad() : nullptr;
}

UAbilitySystemComponent* UGSTN_LoadoutComponent::GetTargetAbilitySystem() const
{
	const AActor* Owner = GetOwner();
	if (const APlayerState* PlayerState = Cast<APlayerState>(Owner))
	{
		const APawn* Pawn = PlayerState->GetPawn();
		return Pawn ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) : nullptr;
	}
	return Owner ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner) : nullptr;
}

void UGSTN_LoadoutComponent::ResolveLoadout(const FGSTN_LoadoutData& Data, FGSTN_ResolvedLoadout& OutResolved, TArray<TStrongObjectPtr<UObject>>& OutHold) const
{
	OutResolved = FGSTN_ResolvedLoadout();
	OutResolved.Loadout = Data;
	OutResolved.WorldContext = this;

	auto Hold = [&OutHold](UObject* Object) -> UObject*
	{
		if (Object)
		{
			OutHold.Emplace(Object);
		}
		return Object;
	};

	const UGSTN_PrimaryKitDefinition* Kit = Cast<UGSTN_PrimaryKitDefinition>(Hold(ResolveAsset(Data.PrimaryKit)));
	OutResolved.Kit = Kit;

	// Weapon 0 is the kit's own; the loadout's additional weapons follow, an unresolved one keeps its place as null
	OutResolved.Weapons.Add(Kit ? Cast<UGSTN_WeaponDefinition>(Hold(Kit->PrimaryWeapon.LoadSynchronous())) : nullptr);
	for (const FPrimaryAssetId& WeaponId : Data.AdditionalWeapons)
	{
		OutResolved.Weapons.Add(Cast<UGSTN_WeaponDefinition>(Hold(ResolveAsset(WeaponId))));
	}

	for (const FPrimaryAssetId& SuperId : Data.Supers)
	{
		OutResolved.Supers.Add(Cast<UGSTN_SuperDefinition>(Hold(ResolveAsset(SuperId))));
	}

	OutResolved.SuperScheme = Kit ? Cast<UGSTN_SuperInputScheme>(Hold(Kit->SuperInputScheme.LoadSynchronous())) : nullptr;
	OutResolved.Skin = Cast<UGSTN_SkinDefinition>(Hold(ResolveAsset(Data.Skin)));
}

void UGSTN_LoadoutComponent::ServerRequestLoadout_Implementation(const FGSTN_LoadoutData& Requested)
{
	TArray<FString> Errors;
	if (!AcceptLoadout(Requested, Errors))
	{
		ClientLoadoutRejected(Errors);
	}
}

void UGSTN_LoadoutComponent::ClientLoadoutRejected_Implementation(const TArray<FString>& Errors)
{
	OnLoadoutRejected.Broadcast(this, Errors);
}

bool UGSTN_LoadoutComponent::ValidateLoadout(const FGSTN_LoadoutData& Requested, TArray<FString>& OutErrors) const
{
	OutErrors.Reset();

	// Judged exactly as it will be applied: same resolution, same table, with the revision it would get
	TArray<TStrongObjectPtr<UObject>> Hold;
	FGSTN_ResolvedLoadout Candidate;
	ResolveLoadout(Requested, Candidate, Hold);

	FGSTN_CompiledBindingTable Table;
	FGSTN_LoadoutRuleResult Result;
	if (!FGSTN_LoadoutAssembly::CompileAndValidate(Candidate, Revision + 1, UGSTN_InputSettings::Get(), GetRules(), Table, Result))
	{
		for (const FText& Error : Result.Errors)
		{
			OutErrors.Add(Error.ToString());
		}
		return false;
	}
	return true;
}

bool UGSTN_LoadoutComponent::AcceptLoadout(const FGSTN_LoadoutData& Requested, TArray<FString>& OutErrors)
{
	OutErrors.Reset();
	if (!IsServerAuthority())
	{
		OutErrors.Add(TEXT("A loadout is accepted on the server only."));
		return false;
	}
	if (HasLoadout() && Requested == Applied)
	{
		return true;
	}

	if (!ValidateLoadout(Requested, OutErrors))
	{
		return false;
	}

	Applied = Requested;
	++Revision;
	// Called here and not left to OnRep_Applied: the server (a listen host included) never receives the OnRep of its own write
	ApplyAppliedLoadout();
	return true;
}

void UGSTN_LoadoutComponent::OnRep_Applied()
{
	ApplyAppliedLoadout();
}

void UGSTN_LoadoutComponent::ApplyAppliedLoadout()
{
	// Every id is looked up here, once; the simulation and the per-frame code only ever see the results
	TArray<TStrongObjectPtr<UObject>> Hold;
	FGSTN_ResolvedLoadout NewResolved;
	ResolveLoadout(Applied, NewResolved, Hold);

	KeptAssets.Reset();
	for (const TStrongObjectPtr<UObject>& Object : Hold)
	{
		KeptAssets.Add(Object.Get());
	}
	Resolved = NewResolved;

	bool bValid = false;
	if (HasLoadout())
	{
		FGSTN_LoadoutRuleResult Result;
		bValid = FGSTN_LoadoutAssembly::CompileAndValidate(Resolved, Revision, UGSTN_InputSettings::Get(), GetRules(), CompiledTable, Result);
		if (!bValid)
		{
			// The server accepted it, so this is content that differs between machines or changed since (a respawn after a patch)
			UE_LOG(LogGSTNLoadoutComponent, Warning, TEXT("%s: the applied loadout (revision %d) does not validate here: %s"), *GetNameSafe(GetOwner()), Revision,
				Result.Errors.IsEmpty() ? TEXT("no finding") : *Result.Errors[0].ToString());
		}
	}
	else
	{
		CompiledTable = FGSTN_CompiledBindingTable();
	}

	if (IsServerAuthority())
	{
		if (!bValid)
		{
			ClearGranted();
			bGrantPending = false;
		}
		else if (UAbilitySystemComponent* ASC = GetTargetAbilitySystem(); ASC && ASC->AbilityActorInfo.IsValid())
		{
			GrantToAbilitySystem(*ASC);
			bGrantPending = false;
		}
		else
		{
			// No pawn yet: HandlePawnSet applies again when the PlayerState gets one
			bGrantPending = true;
			UE_LOG(LogGSTNLoadoutComponent, Verbose, TEXT("%s: loadout revision %d waits for a pawn to grant on."), *GetNameSafe(GetOwner()), Revision);
		}
	}

	OnLoadoutApplied.Broadcast(this, Revision);
}

void UGSTN_LoadoutComponent::HandlePawnSet(APlayerState* Player, APawn* NewPawn, APawn* OldPawn)
{
	if (!NewPawn || !HasLoadout())
	{
		return;
	}
	// One tick later, so the pawn's ability system has its actor info by the time abilities are given
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UGSTN_LoadoutComponent::ApplyAppliedLoadout);
	}
}

void UGSTN_LoadoutComponent::ClearGranted()
{
	if (UAbilitySystemComponent* Old = GrantedOn.Get())
	{
		// The extensions undo their grants first, while the abilities they may rely on are still there
		FGSTN_LoadoutExtensionRegistry::RunOnCleared(*Old);
		for (const FGameplayAbilitySpecHandle& Handle : GrantedHandles)
		{
			Old->ClearAbility(Handle);
		}
		if (AppliedWeaponStatsClass)
		{
			UGSTN_GE_WeaponStats::Remove(*Old, AppliedWeaponStatsClass);
		}
	}
	GrantedHandles.Reset();
	AppliedWeaponStatsClass = nullptr;
	GrantedOn.Reset();
}

void UGSTN_LoadoutComponent::GrantToAbilitySystem(UAbilitySystemComponent& ASC)
{
	// The fork carries abilities and effects through the simulation state and refuses a grant that is not the authority's
	if (!ASC.GetOwner() || !ASC.GetOwner()->HasAuthority())
	{
		return;
	}

	ClearGranted();
	GrantedOn = &ASC;

	const UGSTN_PrimaryKitDefinition* Kit = Resolved.Kit;
	if (Kit && !Kit->InitStatsEffect.IsNull())
	{
		// The instant effect overwrites the starting values; the weapon's infinite effect below stacks on top of it
		Kit->Stats.ApplyTo(ASC, Kit->InitStatsEffect.LoadSynchronous());
	}

	TArray<FSoftObjectPath> AbilityPaths = CompiledTable.GrantedAbilities;
	const UGSTN_WeaponDefinition* Primary = Resolved.Weapons.IsValidIndex(0) ? Resolved.Weapons[0] : nullptr;
	if (Primary)
	{
		// The primary weapon is active from spawn; the others bring theirs when they become active
		if (!Primary->StatsEffect.IsNull())
		{
			const TSubclassOf<UGSTN_GE_WeaponStats> StatsClass = Primary->StatsEffect.LoadSynchronous();
			if (UGSTN_GE_WeaponStats::Apply(ASC, StatsClass, Primary->StatModifiers).IsValid())
			{
				AppliedWeaponStatsClass = StatsClass;
			}
		}
		for (const TSoftClassPtr<UGameplayAbility>& Passive : Primary->PassivesWhileActive)
		{
			AbilityPaths.AddUnique(Passive.ToSoftObjectPath());
		}
	}

	for (const FSoftObjectPath& Path : AbilityPaths)
	{
		const TSubclassOf<UGameplayAbility> AbilityClass = TSoftClassPtr<UGameplayAbility>(Path).LoadSynchronous();
		if (!AbilityClass)
		{
			UE_LOG(LogGSTNLoadoutComponent, Warning, TEXT("%s: the ability %s of the loadout does not load, it is not granted."), *GetNameSafe(GetOwner()), *Path.ToString());
			continue;
		}
		if (ASC.FindAbilitySpecFromClass(AbilityClass))
		{
			// Already there (a default ability of the pawn): not ours, so never removed with the loadout
			continue;
		}
		// No source object: a transient one would differ between machines and keep the simulation reconciling
		const FGameplayAbilitySpecHandle Handle = ASC.GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
		if (Handle.IsValid())
		{
			GrantedHandles.Add(Handle);
		}
	}

	// The extensions grant last (an attribute set, a passive effect), after the abilities of the table they may read
	FGSTN_LoadoutExtensionContext Context;
	Context.Resolved = &Resolved;
	Context.WorldContext = this;
	FGSTN_LoadoutExtensionRegistry::RunOnGranted(Context, ASC);
}
