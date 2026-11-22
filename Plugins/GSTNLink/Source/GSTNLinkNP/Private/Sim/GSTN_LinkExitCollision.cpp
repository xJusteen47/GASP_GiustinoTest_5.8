// Links, Network Prediction half.

#include "Sim/GSTN_LinkExitCollision.h"

#include "Data/GSTN_LinkTypeDefinition.h"
#include "GameFramework/Actor.h"
#include "GSTN_LinkPresentationComponent.h"
#include "GSTN_LinkRegistry.h"
#include "Sim/GSTN_LinkFragment.h"
#include "State/GSTN_LinkState.h"

bool FGSTN_LinkExitCollision::Begin(FGSTN_LinkFragment& Fragment, const FGSTN_LinkState& Ending, const UGSTN_LinkTypeDefinition* Definition, int32 ExitFrame)
{
	if (!Definition || Definition->Channel != EGSTN_LinkChannel::Body || !Definition->IgnoresPartnerCollision() || Ending.PartnerId.IsNull()
		|| Definition->ExitCollisionIgnoreFrames <= 0)
	{
		return false;
	}
	Fragment.ExitIgnorePartner = Ending.PartnerId;
	Fragment.ExitIgnoreEndFrame = ExitFrame + Definition->ExitCollisionIgnoreFrames;
	return true;
}

FGSTN_EntityRef FGSTN_LinkExitCollision::GetIgnoredPartner(const FGSTN_LinkFragment* Fragment, const UGSTN_LinkRegistry* Registry, int32 Frame)
{
	if (!Fragment)
	{
		return FGSTN_EntityRef::Null();
	}
	const FGSTN_LinkState& Body = Fragment->Links.Body;
	if (Body.Role != 0 && (Body.Phase == EGSTN_LinkPhase::Active || Body.Phase == EGSTN_LinkPhase::Exiting) && !Body.PartnerId.IsNull())
	{
		const UGSTN_LinkTypeDefinition* Definition = Registry ? Registry->GetDefinition(Body.LinkTypeIndex) : nullptr;
		if (Definition && Definition->Channel == EGSTN_LinkChannel::Body && Definition->IgnoresPartnerCollision())
		{
			return Body.PartnerId;
		}
	}
	if (Frame < Fragment->ExitIgnoreEndFrame && !Fragment->ExitIgnorePartner.IsNull())
	{
		return Fragment->ExitIgnorePartner;
	}
	return FGSTN_EntityRef::Null();
}

bool FGSTN_LinkExitCollision::DropExpired(FGSTN_LinkFragment& Fragment, int32 Frame)
{
	if (Fragment.ExitIgnoreEndFrame == 0 && Fragment.ExitIgnorePartner.IsNull())
	{
		return false;
	}
	if (Frame < Fragment.ExitIgnoreEndFrame)
	{
		return false;
	}
	Fragment.ExitIgnorePartner = FGSTN_EntityRef::Null();
	Fragment.ExitIgnoreEndFrame = 0;
	return true;
}

void FGSTN_LinkExitCollision::Apply(AActor* Owner, AActor* Partner)
{
	if (UGSTN_LinkPresentationComponent* Component = Owner ? Owner->FindComponentByClass<UGSTN_LinkPresentationComponent>() : nullptr)
	{
		Component->SetCollisionIgnoredPartner(Partner);
	}
}
