// Links, Network Prediction half.

#include "Sim/GSTN_LinkPartnerWindow.h"

#include "Actions/GSTN_LinkActionProvider.h"
#include "GSTN_LinkRegistry.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Timeline/GSTN_TimelineTypes.h"

bool FGSTN_LinkPartnerWindow::BuildFromInitiator(const FGSTN_LinkFragment& InitiatorFragment, const UGSTN_LinkRegistry* Registry, FTimelineResolver ResolveTimeline,
	FGSTN_LinkPartnerWindow& Out)
{
	Out = FGSTN_LinkPartnerWindow();
	const FGSTN_LinkState& Body = InitiatorFragment.Links.Body;
	const FGSTN_LinkActions::FActive& Active = InitiatorFragment.Actions.Active;
	if (!Registry || Active.Id <= 0 || Active.Side != GSTNLinkActionSide::Initiator || !Body.IsActive() || Body.Role == 0)
	{
		return false;
	}
	FGSTN_LinkActionInfo Info;
	if (!Registry->ResolveLinkAction(Body.LinkTypeIndex, Active.Id, Info))
	{
		return false;
	}

	Out.Serial = Body.Serial;
	Out.MoveId = Active.Id;
	Out.InitiatorFrame = Active.StartFrame;
	// The same clamp as StartPartnerMove: the entry reaches the partner the frame after it is posted at the earliest
	Out.PartnerStartFrame = Active.StartFrame + FMath::Max(Info.PartnerStartOffsetFrames, 1);
	Out.PartnerAbility = Info.PartnerAbility;

	const FGSTN_CombatTimelineData* Timeline = Info.PartnerAbility ? ResolveTimeline(Info.PartnerAbility) : nullptr;
	int32 Length = Timeline ? Timeline->TotalFrames : 0;
	if (Length <= 0)
	{
		// No timeline to read (an ability the caller cannot resolve): the action's own limit, if it has one
		Length = Info.MaxDurationFrames;
	}
	Out.bLengthKnown = Length > 0;
	Out.PartnerEndFrame = Out.PartnerStartFrame + FMath::Max(Length, 0);
	Out.bValid = true;
	return true;
}
