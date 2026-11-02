// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#include "GSTN_MountLinkType.h"

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTNPiggybackCoreModule.h"
#include "GSTN_CombatCoreTags.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkRule_Mount.h"
#include "GSTN_PiggybackTags.h"
#include "GSTN_Placer_Mount.h"
#include "Rules/GSTN_LinkRule_Team.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	/** The registered definition, rooted for the process: the registry holds code-added definitions by raw pointer */
	TStrongObjectPtr<UGSTN_LinkTypeDefinition> GRegistered;

	FGSTN_RoleHitBreak MakeHitBreak(const FGameplayTag& Role, float MinKnockback)
	{
		FGSTN_RoleHitBreak Break;
		Break.RoleTag = Role;
		Break.MinKnockbackToBreak = MinKnockback;
		Break.bPartnerHitsBreak = false;
		return Break;
	}
}

UGSTN_LinkTypeDefinition* FGSTN_MountLinkType::Build(UObject* Outer, const FGSTN_MountRules& Rules)
{
	UGSTN_LinkTypeDefinition* Definition = NewObject<UGSTN_LinkTypeDefinition>(Outer ? Outer : GetTransientPackage(),
		MakeUniqueObjectName(Outer ? Outer : GetTransientPackage(), UGSTN_LinkTypeDefinition::StaticClass(), TEXT("DA_LinkType_Mount")));
	Definition->LinkType = GSTNPiggybackTags::Type_Mount;
	Definition->Channel = EGSTN_LinkChannel::Body;
	Definition->DebugColor = FColor::Cyan;

	FGSTN_LinkRoleDef& Rider = Definition->InitiatorRole;
	Rider.RoleTag = GSTNPiggybackTags::Role_Rider;
	Rider.bParked = true;
	Rider.bActsWhileLinked = true;
	Rider.bCollisionWithPartner = false;
	Rider.SocketOnPartner = NAME_None;
	Rider.SocketOffset = RiderOffset();
	Rider.ReleaseBreakReason = GSTNPiggybackTags::BreakReason_Dismount;
	Rider.YawLimitDeg = 360.f;

	FGSTN_LinkRoleDef& Carrier = Definition->TargetRole;
	Carrier.RoleTag = GSTNPiggybackTags::Role_Carrier;
	Carrier.bActsWhileLinked = true;
	Carrier.ReleaseBreakReason = GSTNPiggybackTags::BreakReason_Drop;

	Definition->bInitiatorPredicts = true;
	Definition->PendingFrames = FMath::Max(Rules.PendingFrames, 1);

	// Hit breaks by role and thresholds; the reason is GSTNLink's generic Hit until G7's mount hit step names CarrierHit / RiderHit
	Definition->Break.MaxDistanceCm = LeashCm;
	Definition->Break.HitBreaks.Add(MakeHitBreak(GSTNPiggybackTags::Role_Carrier, Rules.CarrierBreakKnockback));
	Definition->Break.HitBreaks.Add(MakeHitBreak(GSTNPiggybackTags::Role_Rider, Rules.RiderBreakKnockback));
	Definition->Break.bBreakOnOwnDeath = true;
	Definition->Break.bBreakOnPartnerDeath = true;

	// A carrier in hitstun refuses (spec 6.2). Naming the tag here also puts it in the snapshot's request tag mask, so the rule can read it
	Definition->RequestRequirements.IgnoreTags.AddTag(GSTNCombatTags::State_Hitstun);

	if (Rules.bAllyOnly)
	{
		UGSTN_LinkRule_Team* Team = NewObject<UGSTN_LinkRule_Team>(Definition, TEXT("TeamRule"));
		Team->Requirement = EGSTN_LinkTeamRequirement::AlliesOnly;
		Definition->Rules.Add(Team);
	}
	Definition->Rules.Add(NewObject<UGSTN_LinkRule_Mount>(Definition, TEXT("MountRule")));

	UGSTN_Placer_Mount* Placer = NewObject<UGSTN_Placer_Mount>(Definition, TEXT("MountPlacer"));
	Placer->Mode = EGSTN_ExitMode::BehindCarrier;
	Definition->ExitPlacer = Placer;

	Definition->InteractOffer.Meaning = GSTNPiggybackTags::Interact_Target_Mount;
	Definition->InteractOffer.Priority = OfferPriority;
	Definition->InteractOffer.ReachCm = OfferReachCm;
	Definition->InteractOffer.RequiredKitExtension = FGSTN_MountKitExtension::StaticStruct();

	if (Rules.bPredictRiderExits)
	{
		UE_LOG(LogGSTNPiggyback, Warning, TEXT("FGSTN_MountRules::bPredictRiderExits is not built yet (G5 ships server-given rider exits only): treated as false"));
	}
	return Definition;
}

void FGSTN_MountLinkType::Register()
{
	UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!Registry || GRegistered.IsValid())
	{
		return;
	}
	GRegistered.Reset(Build(GetTransientPackage(), FGSTN_MountRules()));
	Registry->AddDefinition(GRegistered.Get());
	UE_LOG(LogGSTNPiggyback, Log, TEXT("Mount link type registered at index %d"), Registry->GetTypeIndex(GSTNPiggybackTags::Type_Mount));
}

void FGSTN_MountLinkType::Unregister()
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

const UGSTN_LinkTypeDefinition* FGSTN_MountLinkType::Get()
{
	return GRegistered.Get();
}

TArray<FName> FGSTN_MountLinkType::GetBreakReasonNames()
{
	// By name: this runs at module startup, before the native tags of GSTNPiggybackCore are guaranteed to be in the tag tree. Tag name order
	return { TEXT("Combat.Link.BreakReason.CarrierHit"), TEXT("Combat.Link.BreakReason.Dismount"), TEXT("Combat.Link.BreakReason.Drop"),
		TEXT("Combat.Link.BreakReason.Outcome"), TEXT("Combat.Link.BreakReason.RiderHit") };
}
