// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "GSTN_CombatEditorUtils.h"

#include "Bake/GSTN_BakeAll.h"
#include "Bake/GSTN_SocketBaker.h"
#include "Engine/SkeletalMesh.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hurtbox/GSTN_HurtboxGenerator.h"
#include "Preview/GSTN_PreviewSubject.h"
#include "Preview/GSTN_TimelinePreviewScene.h"
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

int32 UGSTN_CombatEditorUtils::PrepareHurtboxSet(UGSTN_HurtboxSetDefinition* Set, USkeletalMesh* Mesh, bool bAddSimpleBody, FString& OutMessage)
{
	if (Set == nullptr || Mesh == nullptr)
	{
		OutMessage = TEXT("PrepareHurtboxSet: a hurtbox set and a skeletal mesh are needed.");
		return 0;
	}
	// The same root frame the socket baker and the preview use
	FGSTN_TimelinePreviewScene Scene;
	FGSTN_ResolvedSubject Subject;
	Subject.Mesh = Mesh;
	Scene.SetSubject(Subject);

	TArray<FGSTN_Hurtbox> Next = Set->Hurtboxes;
	TArray<FName> Missing;
	const int32 Following = GSTNHurtboxGenerator::FollowBones(Next, Mesh->GetRefSkeleton(), Scene.GetMeshToRoot(), TConstArrayView<int32>(), &Missing);
	bool bBody = false;
	if (bAddSimpleBody)
	{
		bBody = GSTNHurtboxGenerator::ApplySimpleBody(Next, GSTNHurtboxGenerator::SuggestZone(UGSTN_CombatEditorSettings::Get().HurtboxZoneRules, TEXT("spine")));
	}
	Set->Modify();
	Set->Hurtboxes = MoveTemp(Next);
#if WITH_EDITORONLY_DATA
	if (Set->PreviewMesh.IsNull())
	{
		// The timeline editor finds the set to draw over a mesh by this field
		Set->PreviewMesh = Mesh;
	}
#endif
	Set->MarkPackageDirty();
	OutMessage = FString::Printf(TEXT("%s: %d hurtboxes follow their bones%s%s"), *Set->GetName(), Following, bBody ? TEXT(", simple body added") : TEXT(""),
		Missing.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", %d without a bone on the mesh"), Missing.Num()));
	return Following;
}
