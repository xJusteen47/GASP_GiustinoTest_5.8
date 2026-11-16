// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "GSTN_CombatEditorUtils.h"

#include "Bake/GSTN_BakeAll.h"
#include "Bake/GSTN_SocketBaker.h"
#include "Engine/SkeletalMesh.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/GSTN_CombatTimeline.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

float UGSTN_CombatEditorUtils::GetTickRateHz()
{
	return UGSTN_CombatEditorSettings::GetSimulationTickRate();
}

bool UGSTN_CombatEditorUtils::BakeTimeline(UGSTN_CombatTimeline* Timeline, USkeletalMesh* Mesh, FString& OutMessage)
{
	OutMessage.Reset();
	if (Timeline == nullptr)
	{
		OutMessage = LOCTEXT("UtilsNoTimeline", "No timeline.").ToString();
		return false;
	}
	const FGSTN_BakeResult Result = FGSTN_SocketBaker::BakeStandalone(Timeline->Data, Mesh, GetTickRateHz());
	if (!Result.bBaked)
	{
		OutMessage = Result.DescribeProblems().ToString();
		return false;
	}
	Timeline->Modify();
	FGSTN_SocketBaker::Apply(Timeline->Data, Result);
	Timeline->MarkPackageDirty();
	return true;
}

bool UGSTN_CombatEditorUtils::IsBakeStale(UGSTN_CombatTimeline* Timeline, USkeletalMesh* Mesh)
{
	if (Timeline == nullptr)
	{
		return false;
	}
	FGSTN_BakeInputs Inputs;
	Inputs.Mesh = Mesh != nullptr ? Mesh : GSTNBakeAll::ResolveMesh(*Timeline);
	Inputs.TickRateHz = GetTickRateHz();
	if (!Timeline->Data.Montage.IsNull())
	{
		Inputs.Montage = Timeline->Data.Montage.LoadSynchronous();
	}
	return GSTNBake::GetStatus(Timeline->Data, Inputs).NeedsBake();
}

#undef LOCTEXT_NAMESPACE
