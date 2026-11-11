// Held objects, simulation side (needs the GAS-NPP fork).

#include "GSTN_HeldSim.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_HeldLoadEffect.h"
#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSubsystem.h"
#include "GSTN_HeldTags.h"
#include "GSTN_HeldTypes.h"
#include "GSTN_LinkRegistry.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "State/GSTN_LinkState.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	/** The registered definition, rooted for the process: the registry holds code-added definitions by raw pointer */
	TStrongObjectPtr<UGSTN_LinkTypeDefinition> GRegistered;
}

UGSTN_LinkTypeDefinition* FGSTN_HeldLinkType::Build(UObject* Outer)
{
	UObject* Package = Outer ? Outer : GetTransientPackage();
	UGSTN_LinkTypeDefinition* Definition = NewObject<UGSTN_LinkTypeDefinition>(Package,
		MakeUniqueObjectName(Package, UGSTN_LinkTypeDefinition::StaticClass(), TEXT("DA_LinkType_Held")));
	Definition->LinkType = GSTNHeldTags::Link_Type_Held;
	Definition->Channel = EGSTN_LinkChannel::Hand;
	Definition->DebugColor = FColor::Yellow;

	FGSTN_LinkRoleDef& Holder = Definition->InitiatorRole;
	Holder.RoleTag = GSTNHeldTags::Link_Role_Holder;
	Holder.bActsWhileLinked = true;
	Holder.bCollisionWithPartner = true;
	// M10 G6: the carry load, applied and removed by GSTNLink's link tick with the hold (payload = size class and load ratio)
	Holder.LinkEffect.Effect = FSoftClassPath(UGSTN_GE_HoldLoad::StaticClass());
	Holder.LinkEffect.SetByCallerTag = GSTNHeldTags::SetByCaller_Held_Load;

	FGSTN_LinkRoleDef& Object = Definition->TargetRole;
	Object.RoleTag = GSTNHeldTags::Link_Role_HeldObject;

	// The host's held break rules end a hold (they read the object's definition); GSTNLink's own rules stay off so a break has one author
	Definition->Break.bBreakOnOwnDeath = false;
	Definition->Break.bBreakOnPartnerDeath = false;
	Definition->Break.MaxDistanceCm = 0.f;
	return Definition;
}

void FGSTN_HeldLinkType::Register()
{
	UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!Registry || GRegistered.IsValid())
	{
		return;
	}
	GRegistered.Reset(Build(GetTransientPackage()));
	Registry->AddDefinition(GRegistered.Get());
}

void FGSTN_HeldLinkType::Unregister()
{
	if (!GRegistered.IsValid())
	{
		return;
	}
	if (UObjectInitialized())
	{
		if (UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get())
		{
			Registry->RemoveDefinition(GRegistered.Get());
		}
	}
	GRegistered.Reset();
}

const UGSTN_LinkTypeDefinition* FGSTN_HeldLinkType::Get()
{
	return GRegistered.Get();
}

uint8 FGSTN_HeldLinkType::GetTypeIndex(const UGSTN_LinkRegistry* Registry)
{
	const UGSTN_LinkRegistry* Used = Registry ? Registry : UGSTN_LinkRegistry::Get();
	return Used ? static_cast<uint8>(Used->GetTypeIndex(GSTNHeldTags::Link_Type_Held)) : 0;
}

bool FGSTN_HeldLinkType::IsHolding(const FGSTN_LinkState& Hand, uint8 HeldTypeIndex)
{
	return HeldTypeIndex != 0 && Hand.IsLinked() && Hand.Role == 1 && Hand.LinkTypeIndex == HeldTypeIndex;
}

uint8 FGSTN_HeldLinkType::ToLinkReason(EGSTN_HeldBreakReason Reason)
{
	switch (Reason)
	{
	case EGSTN_HeldBreakReason::Dropped:
		return static_cast<uint8>(EGSTN_LinkBreakReason::Release);
	case EGSTN_HeldBreakReason::Thrown:
		return static_cast<uint8>(EGSTN_LinkBreakReason::Throw);
	case EGSTN_HeldBreakReason::HolderState:
		return static_cast<uint8>(EGSTN_LinkBreakReason::Hit);
	case EGSTN_HeldBreakReason::HolderEvent:
		return static_cast<uint8>(EGSTN_LinkBreakReason::OwnDeath);
	default:
		return static_cast<uint8>(EGSTN_LinkBreakReason::Forced);
	}
}

FGSTN_InboxEntry FGSTN_HeldLinkType::MakeHold(uint8 TypeIndex, uint8 Serial, int32 ObjectId, uint8 ObjectSerial, uint8 LoadPayload)
{
	FGSTN_LinkCommandEntry Command;
	Command.Kind = EGSTN_LinkCommandKind::Form;
	Command.Channel = EGSTN_LinkChannel::Hand;
	Command.Serial = Serial;
	Command.LinkTypeIndex = TypeIndex;
	Command.Role = 1;
	Command.PartnerId = ObjectId;
	Command.PartnerKind = GSTNEntityKinds::Object;
	Command.PartnerSerial = ObjectSerial;
	Command.Payload = LoadPayload;
	return FGSTN_LinkInboxHandlers::MakeCommand(Command);
}

FGSTN_InboxEntry FGSTN_HeldLinkType::MakeRelease(uint8 Serial, uint8 LinkReason)
{
	FGSTN_LinkCommandEntry Command;
	Command.Kind = EGSTN_LinkCommandKind::Break;
	Command.Channel = EGSTN_LinkChannel::Hand;
	Command.Serial = Serial;
	Command.Reason = LinkReason;
	return FGSTN_LinkInboxHandlers::MakeCommand(Command);
}

void FGSTN_HeldTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	if (Point != EGSTN_CombatHookPoint::BeforeTagProjection || !Context.AbilitySystem)
	{
		return;
	}
	const FGSTN_LinkFragment* Links = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments);
	const bool bHolding = Links && FGSTN_HeldLinkType::IsHolding(Links->Links.Hand, FGSTN_HeldLinkType::GetTypeIndex());
	Context.AbilitySystem->SetLooseGameplayTagCount(GSTNHeldTags::Held_Carrying, bHolding ? 1 : 0);
	// M10 G9: a grabbed enemy the holder can lift (static data of the two fighters, the frame's own Body link): the enemy throw binding requires it
	const bool bLifting = !bHolding && Links && UGSTN_HeldSubsystem::FindLiftedEnemy(Context.Owner, Links->Links.Body) != nullptr;
	Context.AbilitySystem->SetLooseGameplayTagCount(GSTNHeldTags::Held_CarryingEnemy, bLifting ? 1 : 0);
	// M10 G6: the size class of the hold from the link's payload (sync state), never from the object
	int32 SizeIndex = INDEX_NONE;
	if (bHolding)
	{
		float Ratio = 0.f;
		FGSTN_HeldLoad::DecodePayload(Links->Links.Hand.Payload, SizeIndex, Ratio);
	}
	const TArray<FGSTN_HeldSizeClass>& SizeClasses = UGSTN_HeldSettings::Get().SizeClasses;
	for (int32 Index = 0; Index < FMath::Min(SizeClasses.Num(), static_cast<int32>(FGSTN_HeldLoad::MaxSizeClasses)); ++Index)
	{
		if (SizeClasses[Index].CarryingTag.IsValid())
		{
			Context.AbilitySystem->SetLooseGameplayTagCount(SizeClasses[Index].CarryingTag, Index == SizeIndex ? 1 : 0);
		}
	}
	// Interact.Target.Pickup is not projected here since M10 G2: an object at rest is a stamped GSTNInteract record and GSTNLink's interact
	// probe projects its meaning, the one Interact.Target tag of the frame (UGSTN_HeldObjectComponent::ApplyRecord)
}
