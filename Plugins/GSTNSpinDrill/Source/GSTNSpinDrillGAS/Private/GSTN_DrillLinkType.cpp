// Spin and drill plugin, module GSTNSpinDrillGAS.

#include "GSTN_DrillLinkType.h"

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_SpinDrillTags.h"
#include "Rules/GSTN_LinkRule_Drill.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNDrillLink, Log, All);

namespace
{
	/** The registered definition, rooted for the process: the registry holds code-added definitions by raw pointer */
	TStrongObjectPtr<UGSTN_LinkTypeDefinition> GRegistered;
}

UGSTN_LinkTypeDefinition* FGSTN_DrillLinkType::Build(UObject* Outer)
{
	UObject* const Owner = Outer ? Outer : GetTransientPackage();
	UGSTN_LinkTypeDefinition* Definition = NewObject<UGSTN_LinkTypeDefinition>(Owner,
		MakeUniqueObjectName(Owner, UGSTN_LinkTypeDefinition::StaticClass(), TEXT("DA_LinkType_Drill")));
	Definition->LinkType = GSTNSpinDrillTags::Link_Type_Drill;
	Definition->Channel = EGSTN_LinkChannel::Body;
	Definition->DebugColor = FColor::Yellow;

	FGSTN_LinkRoleDef& Driller = Definition->InitiatorRole;
	Driller.RoleTag = GSTNSpinDrillTags::Link_Role_Driller;
	Driller.bParked = true;
	Driller.bActsWhileLinked = true;
	Driller.bCollisionWithPartner = false;
	Driller.SocketOnPartner = NAME_None;
	Driller.SocketOffset = DrillerOffset();
	Driller.YawLimitDeg = 0.f;

	FGSTN_LinkRoleDef& Drilled = Definition->TargetRole;
	Drilled.RoleTag = GSTNSpinDrillTags::Link_Role_Drilled;
	Drilled.bActsWhileLinked = true;

	Definition->bInitiatorPredicts = true;
	Definition->PendingFrames = 2;

	FGSTN_RoleHitBreak DrillerHit;
	DrillerHit.RoleTag = GSTNSpinDrillTags::Link_Role_Driller;
	DrillerHit.MinKnockbackToBreak = 0.f;
	DrillerHit.bPartnerHitsBreak = false;
	Definition->Break.HitBreaks.Add(DrillerHit);
	Definition->Break.MaxDistanceCm = LeashCm;
	Definition->Break.bBreakOnOwnDeath = true;
	Definition->Break.bBreakOnPartnerDeath = true;

	Definition->RequestRequirements.IgnoreTags.AddTag(GSTNSpinDrillTags::Immunity_Drill);
	Definition->Rules.Add(NewObject<UGSTN_LinkRule_Drill>(Definition, TEXT("DrillRule")));
	return Definition;
}

void FGSTN_DrillLinkType::Register()
{
	UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!Registry || GRegistered.IsValid())
	{
		return;
	}
	GRegistered.Reset(Build(GetTransientPackage()));
	Registry->AddDefinition(GRegistered.Get());
	UE_LOG(LogGSTNDrillLink, Log, TEXT("Drill link type registered at index %d"), Registry->GetTypeIndex(GSTNSpinDrillTags::Link_Type_Drill));
}

void FGSTN_DrillLinkType::Unregister()
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

const UGSTN_LinkTypeDefinition* FGSTN_DrillLinkType::Get()
{
	return GRegistered.Get();
}
