// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_MountLoadout.h"
#include "GSTN_PairMoveDefinition.h"
#include "GSTN_PairMoveRegistry.h"

#include "GSTN_LinkTags.h"
#include "GSTN_MountAbilities.h"
#include "GSTN_MountTypes.h"
#include "GSTN_PiggybackTags.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_LinkSlots.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"

namespace
{
	using FCondition = TInstancedStruct<FGSTN_InputCondition>;

	FCondition MakePress(const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Press Condition;
		Condition.Buttons.AddTag(Button);
		return FCondition::Make<FGSTN_InputCondition_Press>(Condition);
	}

	FCondition MakeHold(const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Hold Condition;
		Condition.Button = Button;
		Condition.MinFrames = 0;
		return FCondition::Make<FGSTN_InputCondition_Hold>(Condition);
	}

	FGSTN_BindingInput MakeBinding(const FGameplayTag& Command, const FGameplayTag& Required, std::initializer_list<FCondition> AllOf, int32 PriorityOffset,
		const FSoftClassPath& Override, const UClass* Default)
	{
		FGSTN_BindingInput Binding;
		Binding.Command = Command;
		Binding.Requirements.RequireTags.AddTag(Required);
		Binding.Ability = Override.IsValid() ? FSoftObjectPath(Override.ToString()) : FSoftObjectPath(Default);
		Binding.bHasSlotOverride = true;
		Binding.SlotOverride.Command = Command;
		Binding.SlotOverride.PriorityOffset = PriorityOffset;
		// A meaning of the interact button outside its context must leave the press to the slots below it (the grab), as GSTNLink's link slots do
		Binding.SlotOverride.bFallThroughWhenUnavailable = true;
		FGSTN_InputStep Step;
		for (const FCondition& Condition : AllOf)
		{
			Step.AllOf.Add(Condition);
		}
		Binding.SlotOverride.Pattern.Steps.Add(MoveTemp(Step));
		return Binding;
	}

	int32 IgnoreCarrierRole(const FGameplayTagContainer& Disallowed, const FGameplayTag& CarrierRole, TArray<FGSTN_BindingInput>& Bindings)
	{
		int32 Changed = 0;
		for (FGSTN_BindingInput& Binding : Bindings)
		{
			if (Binding.Command.IsValid() && Disallowed.HasTagExact(Binding.Command) && !Binding.Requirements.RequireTags.HasTagExact(CarrierRole)
				&& !Binding.Requirements.IgnoreTags.HasTagExact(CarrierRole))
			{
				Binding.Requirements.IgnoreTags.AddTag(CarrierRole);
				++Changed;
			}
		}
		return Changed;
	}
}

FName FGSTN_MountLoadout::GetLoadoutExtensionId()
{
	return TEXT("GSTNPiggyback.Mount");
}

FGameplayTag FGSTN_MountLoadout::GetMountCommand()
{
	return FGameplayTag::RequestGameplayTag(TEXT("Command.Link.Mount"), false);
}

FGameplayTag FGSTN_MountLoadout::GetDismountCommand()
{
	return FGameplayTag::RequestGameplayTag(TEXT("Command.Link.Dismount"), false);
}

FGameplayTag FGSTN_MountLoadout::GetDropCommand()
{
	return FGameplayTag::RequestGameplayTag(TEXT("Command.Link.Drop"), false);
}

TArray<FGSTN_BindingInput> FGSTN_MountLoadout::MakeDefaultBindings(const FGSTN_MountKitExtension& MountData)
{
	TArray<FGSTN_BindingInput> Bindings;
	const FGameplayTag Interact = GSTNCombatButtonTags::Button_Interact.GetTag();
	const FGameplayTag Crouch = GSTNCombatButtonTags::Button_Crouch.GetTag();
	const int32 RoleOffset = UGSTN_LinkSlots::RoleSlotPriorityOffset;

	const FGameplayTag MountCommand = GetMountCommand();
	const FGameplayTag DismountCommand = GetDismountCommand();
	const FGameplayTag DropCommand = GetDropCommand();
	if (MountData.Rider.bCanRide && MountCommand.IsValid() && DismountCommand.IsValid())
	{
		// One below the grab's slot of the same press: the loadout validator refuses two slots with one input at one priority, whatever their
		// requirements. The probe projects one Interact.Target tag anyway; ignoring the grab's meaning says what it does (a grab target, the grab)
		FGSTN_BindingInput Mount = MakeBinding(MountCommand, GSTNPiggybackTags::Interact_Target_Mount, { MakePress(Interact) }, -1,
			MountData.MountAbility, UGSTN_GA_Mount::StaticClass());
		Mount.Requirements.IgnoreTags.AddTag(GSTNLinkTags::Interact_Target_Grab);
		Bindings.Add(Mount);
		// One above GSTNLink's Release slot of the same press (role offset 100; the validator refuses equal priorities), so above the grab too (a
		// rider that acts keeps interact candidates, so a grab target may be projected while riding); exclusive of the Release bindings by role
		FGSTN_BindingInput Dismount = MakeBinding(DismountCommand, GSTNPiggybackTags::Role_Rider, { MakePress(Interact) }, RoleOffset + 1,
			MountData.DismountAbility, UGSTN_GA_Dismount::StaticClass());
		Dismount.Requirements.IgnoreTags.AddTag(GSTNLinkTags::Role_Grabber);
		Dismount.Requirements.IgnoreTags.AddTag(GSTNLinkTags::Role_Latcher);
		Dismount.Requirements.IgnoreTags.AddTag(GSTNLinkTags::Role_Latched);
		Bindings.Add(Dismount);
	}
	if (MountData.Carrier.bCanBeMounted && DropCommand.IsValid())
	{
		// Crouch held is the more specific shape: one above the Dismount press
		Bindings.Add(MakeBinding(DropCommand, GSTNPiggybackTags::Role_Carrier, { MakePress(Interact), MakeHold(Crouch) }, RoleOffset + 2,
			MountData.DropAbility, UGSTN_GA_Drop::StaticClass()));
	}
	return Bindings;
}

int32 FGSTN_MountLoadout::ApplyDisallowedCommands(const FGameplayTagContainer& Disallowed, FGSTN_CompileInput& Input)
{
	if (Disallowed.IsEmpty())
	{
		return 0;
	}
	const FGameplayTag CarrierRole = GSTNPiggybackTags::Role_Carrier.GetTag();
	int32 Changed = IgnoreCarrierRole(Disallowed, CarrierRole, Input.Kit.Bindings);
	for (FGSTN_WeaponCompileInput& Weapon : Input.Weapons)
	{
		Changed += IgnoreCarrierRole(Disallowed, CarrierRole, Weapon.Bindings);
	}
	return Changed;
}

void FGSTN_MountLoadout::ContributeFromKitData(const FGSTN_MountKitExtension& MountData, FGSTN_CompileInput& Input)
{
	if (MountData.bAddDefaultBindings)
	{
		Input.Kit.Bindings.Append(MakeDefaultBindings(MountData));
	}
	// After the bindings are added: a kit that disallows its own drop gets what it asked for
	ApplyDisallowedCommands(MountData.Carrier.DisallowedCommands, Input);
}

void FGSTN_MountLoadout::Contribute(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
{
	const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved ? Context.Resolved->Kit : nullptr;
	if (const FGSTN_MountKitExtension* MountData = Kit ? Kit->Extensions.Find<FGSTN_MountKitExtension>() : nullptr)
	{
		ContributeFromKitData(*MountData, Input);
		// Content for the mount type's action provider: the same set on every machine that compiles this kit (ids are authored, G8)
		if (UGSTN_PairMoveRegistry* PairMoves = MountData->PairMoves.IsNull() ? nullptr : UGSTN_PairMoveRegistry::Get())
		{
			PairMoves->AddSet(Cast<UGSTN_PairMoveSet>(MountData->PairMoves.TryLoad()));
		}
	}
}
