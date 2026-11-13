// Minion Editor module: builds the test brain tree in code (M8 G8) and hands it to the runtime through a factory delegate, so the Runtime
// module has no editor dependency. Validators and wave details join in G12.

#include "Brain/GSTN_MinionBrain.h"
#include "Brain/GSTN_MinionStateTree.h"
#include "GSTNMinionCoreModule.h"
#include "Modules/ModuleManager.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	/**
	 * The "chase and swing" tree: Root > Chase [MoveToTarget, FaceTarget] goes to Swing when the target is in range; Swing [FaceTarget,
	 * RequestAttack] goes back to Chase when the target is out of the (wider) exit range. The numbers come from
	 * FGSTN_MinionChaseSwingParams, the same struct the native brain reads, so spike 6 compares two implementations of one behaviour.
	 * Built once and kept alive by a strong pointer: the brain subsystem only holds weak object pointers to it.
	 */
	UObject* BuildChaseAndSwingTree()
	{
		static TStrongObjectPtr<UStateTree> Cached;
		if (Cached.IsValid())
		{
			return Cached.Get();
		}
		const FGSTN_MinionChaseSwingParams Params;

		UStateTree* Tree = NewObject<UStateTree>(GetTransientPackage(), TEXT("GSTN_TestBrain_ChaseAndSwing"), RF_Transient);
		UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(Tree);
		Tree->EditorData = EditorData;
		EditorData->Schema = NewObject<UGSTN_MinionStateTreeSchema>(EditorData);

		UStateTreeState& Root = EditorData->AddSubTree(FName(TEXT("Root")));
		UStateTreeState& Chase = Root.AddChildState(FName(TEXT("Chase")));
		UStateTreeState& Swing = Root.AddChildState(FName(TEXT("Swing")));

		TStateTreeEditorNode<FGSTN_MoveToTargetTask>& Move = Chase.AddTask<FGSTN_MoveToTargetTask>();
		Move.GetNode().Name = FName(TEXT("MoveToTarget"));
		Move.GetInstanceData().Speed = Params.Speed;
		Move.GetInstanceData().StopRadius = Params.StopRadius;
		Move.GetInstanceData().SlowRadius = Params.SlowRadius;
		Move.GetInstanceData().SeparationRadius = Params.SeparationRadius;
		Move.GetInstanceData().SeparationWeight = Params.SeparationWeight;
		Chase.AddTask<FGSTN_FaceTargetTask>().GetNode().Name = FName(TEXT("FaceTarget"));

		FStateTreeTransition& ToSwing = Chase.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Swing);
		TStateTreeEditorNode<FGSTN_TargetInRangeCondition>& InRange = ToSwing.AddConditionWithOuter<FGSTN_TargetInRangeCondition>(&Chase);
		InRange.GetInstanceData().MinRange = 0.f;
		InRange.GetInstanceData().MaxRange = Params.EnterRange;

		Swing.AddTask<FGSTN_FaceTargetTask>().GetNode().Name = FName(TEXT("FaceTarget"));
		Swing.AddTask<FGSTN_RequestAttackTask>().GetNode().Name = FName(TEXT("RequestAttack"));

		FStateTreeTransition& ToChase = Swing.AddTransition(EStateTreeTransitionTrigger::OnTick, EStateTreeTransitionType::GotoState, &Chase);
		TStateTreeEditorNode<FGSTN_TargetInRangeCondition>& OutOfRange = ToChase.AddConditionWithOuter<FGSTN_TargetInRangeCondition>(&Swing);
		OutOfRange.GetInstanceData().MinRange = 0.f;
		OutOfRange.GetInstanceData().MaxRange = Params.ExitRange;
		OutOfRange.GetInstanceData().bInvert = true;

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);
		if (!Compiler.Compile(*Tree))
		{
			UE_LOG(LogGSTNMinion, Error, TEXT("MinionBrain: the test StateTree did not compile; the native brain runs instead"));
			Log.DumpToLog(LogGSTNMinion);
			return nullptr;
		}
		Cached.Reset(Tree);
		return Tree;
	}

	class FGSTNMinionEditorModule : public IModuleInterface
	{
	public:
		virtual void StartupModule() override
		{
			GSTNMinion::GetTestBrainTreeFactory().BindStatic(&BuildChaseAndSwingTree);
		}

		virtual void ShutdownModule() override
		{
			GSTNMinion::GetTestBrainTreeFactory().Unbind();
		}
	};
}

IMPLEMENT_MODULE(FGSTNMinionEditorModule, GSTNMinionEditor)
