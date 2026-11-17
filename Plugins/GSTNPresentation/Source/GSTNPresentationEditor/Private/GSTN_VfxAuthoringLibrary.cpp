// Editor-only authoring helpers for the presentation VFX kit (polish plan M2).

#include "GSTN_VfxAuthoringLibrary.h"

#include "EdGraphSchema_Niagara.h"
#include "NiagaraCommon.h"
#include "NiagaraComponent.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_VfxAuthoringLibrary)

namespace GSTNVfxAuthoring
{
	static FNiagaraVariable UserVariable(const FNiagaraTypeDefinition& Type, FName Name)
	{
		FString Full = Name.ToString();
		if (!Full.StartsWith(TEXT("User.")))
		{
			Full = TEXT("User.") + Full;
		}
		return FNiagaraVariable(Type, FName(*Full));
	}

	template <typename T>
	static bool AddUser(UNiagaraSystem* System, const FNiagaraTypeDefinition& Type, FName Name, const T& Default)
	{
		if (!System)
		{
			return false;
		}
		System->Modify();
		const FNiagaraVariable Variable = UserVariable(Type, Name);
		FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
		Store.AddParameter(Variable, true, true);  // no-op when it is already there
		Store.SetParameterValue<T>(Default, Variable, true);
		System->MarkPackageDirty();
		return true;
	}

	static FNiagaraEmitterHandle* FindHandle(UNiagaraSystem* System, const FString& EmitterName)
	{
		if (!System)
		{
			return nullptr;
		}
		for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			if (Handle.GetName().ToString() == EmitterName)
			{
				return &Handle;
			}
		}
		return nullptr;
	}

	static UNiagaraGraph* EmitterGraph(FVersionedNiagaraEmitterData* Data)
	{
		UNiagaraScriptSource* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
		return Source ? Source->NodeGraph : nullptr;
	}

	static FString StackName(ENiagaraScriptUsage Usage)
	{
		switch (Usage)
		{
		case ENiagaraScriptUsage::EmitterSpawnScript: return TEXT("EmitterSpawn");
		case ENiagaraScriptUsage::EmitterUpdateScript: return TEXT("EmitterUpdate");
		case ENiagaraScriptUsage::ParticleSpawnScript:
		case ENiagaraScriptUsage::ParticleSpawnScriptInterpolated: return TEXT("ParticleSpawn");
		case ENiagaraScriptUsage::ParticleUpdateScript: return TEXT("ParticleUpdate");
		default: return TEXT("Other");
		}
	}

	/** The parameter map output of a node (modules chain through it) */
	static UEdGraphPin* MapOutput(UEdGraphNode* Node)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output
				&& UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
			{
				return Pin;
			}
		}
		return nullptr;
	}

	/** The output node a stack module feeds (null for a dynamic input, whose output is a value, not the map) */
	static UNiagaraNodeOutput* StackOutputOf(UNiagaraNodeFunctionCall* Call)
	{
		UEdGraphNode* Node = Call;
		for (int32 Guard = 0; Node && Guard < 256; ++Guard)
		{
			if (UNiagaraNodeOutput* Output = Cast<UNiagaraNodeOutput>(Node))
			{
				return Output;
			}
			UEdGraphPin* Out = MapOutput(Node);
			Node = Out && Out->LinkedTo.Num() > 0 && Out->LinkedTo[0] ? Out->LinkedTo[0]->GetOwningNode() : nullptr;
		}
		return nullptr;
	}

	struct FModuleRef
	{
		FNiagaraEmitterHandle* Handle = nullptr;
		FVersionedNiagaraEmitterData* Data = nullptr;
		UNiagaraGraph* Graph = nullptr;
		UNiagaraNodeFunctionCall* Module = nullptr;
		ENiagaraScriptUsage Usage = ENiagaraScriptUsage::ParticleUpdateScript;
	};

	static void OrderedModules(UNiagaraNodeOutput* Output, TArray<UNiagaraNodeFunctionCall*>& Out);

	static FString FindModule(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleName, FModuleRef& Out)
	{
		Out.Handle = FindHandle(System, EmitterName);
		if (!Out.Handle)
		{
			return FString::Printf(TEXT("no emitter %s"), *EmitterName);
		}
		Out.Data = Out.Handle->GetEmitterData();
		Out.Graph = EmitterGraph(Out.Data);
		if (!Out.Graph)
		{
			return TEXT("emitter has no graph");
		}
		TArray<UNiagaraNodeFunctionCall*> Calls;
		Out.Graph->GetNodesOfClass(Calls);
		UNiagaraNodeFunctionCall* Partial = nullptr;
		ENiagaraScriptUsage PartialUsage = ENiagaraScriptUsage::ParticleUpdateScript;
		// Stack membership from the backward walk of every output: the forward walk (StackOutputOf) loses a template's force modules
		// (review r2/r3: 'no module Drag', so the template's Drag kept its default and no particle ever slowed down)
		TMap<UNiagaraNodeFunctionCall*, UNiagaraNodeOutput*> OutputOf;
		{
			TArray<UNiagaraNodeOutput*> Outputs;
			Out.Graph->GetNodesOfClass(Outputs);
			for (UNiagaraNodeOutput* Output : Outputs)
			{
				TArray<UNiagaraNodeFunctionCall*> Ordered;
				OrderedModules(Output, Ordered);
				for (UNiagaraNodeFunctionCall* Call : Ordered)
				{
					OutputOf.FindOrAdd(Call, Output);
				}
			}
		}
		for (UNiagaraNodeFunctionCall* Call : Calls)
		{
			UNiagaraNodeOutput* const* Found = OutputOf.Find(Call);
			UNiagaraNodeOutput* Output = Found ? *Found : (Call ? StackOutputOf(Call) : nullptr);
			if (!Output || (!Stack.IsEmpty() && StackName(Output->GetUsage()) != Stack))
			{
				continue;
			}
			if (Call->GetFunctionName() == ModuleName)
			{
				Out.Module = Call;
				Out.Usage = Output->GetUsage();
				return FString();
			}
			if (!Partial && Call->GetFunctionName().Contains(ModuleName))
			{
				Partial = Call;
				PartialUsage = Output->GetUsage();
			}
		}
		if (Partial)
		{
			Out.Module = Partial;
			Out.Usage = PartialUsage;
			return FString();
		}
		return FString::Printf(TEXT("no module %s in %s/%s"), *ModuleName, *EmitterName, *Stack);
	}

	static void ScriptsRunning(UNiagaraSystem* System, const FModuleRef& Ref, TArray<UNiagaraScript*>& Out)
	{
		switch (Ref.Usage)
		{
		// Emitter stacks: the emitter's own script holds the value; the system script gets it merged at compile (writing only there is undone)
		case ENiagaraScriptUsage::EmitterSpawnScript:
			Out.Add(Ref.Data->EmitterSpawnScriptProps.Script);
			Out.Add(System->GetSystemSpawnScript());
			break;
		case ENiagaraScriptUsage::EmitterUpdateScript:
			Out.Add(Ref.Data->EmitterUpdateScriptProps.Script);
			Out.Add(System->GetSystemUpdateScript());
			break;
		case ENiagaraScriptUsage::ParticleSpawnScript:
		case ENiagaraScriptUsage::ParticleSpawnScriptInterpolated:
			Out.Add(Ref.Data->SpawnScriptProps.Script);
			Out.Add(Ref.Data->UpdateScriptProps.Script);  // interpolated spawn runs the spawn modules in the update script
			break;
		default: Out.Add(Ref.Data->UpdateScriptProps.Script); break;
		}
		Out.RemoveAll([](const UNiagaraScript* Script) { return Script == nullptr; });
	}

	static TArray<float> Numbers(const FString& Text)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), true);
		TArray<float> Out;
		for (const FString& Part : Parts)
		{
			Out.Add(FCString::Atof(*Part.TrimStartAndEnd()));
		}
		return Out;
	}

	/** Fills Variable's data from text; false = unsupported type or too few numbers */
	static bool FillValue(FNiagaraVariable& Variable, const FString& Text)
	{
		const FNiagaraTypeDefinition& Type = Variable.GetType();
		const TArray<float> N = Numbers(Text);
		auto At = [&N](int32 I) { return N.IsValidIndex(I) ? N[I] : 0.f; };
		Variable.AllocateData();
		if (Type == FNiagaraTypeDefinition::GetFloatDef())
		{
			Variable.SetValue<float>(At(0));
		}
		else if (Type == FNiagaraTypeDefinition::GetIntDef() || Type.IsEnum())
		{
			FNiagaraInt32 Value;
			Value.Value = FCString::Atoi(*Text);
			Variable.SetValue<FNiagaraInt32>(Value);
		}
		else if (Type == FNiagaraTypeDefinition::GetBoolDef())
		{
			FNiagaraBool Value;
			Value.SetValue(Text.TrimStartAndEnd().ToBool());
			Variable.SetValue<FNiagaraBool>(Value);
		}
		else if (Type == FNiagaraTypeDefinition::GetVec2Def())
		{
			Variable.SetValue<FVector2f>(FVector2f(At(0), At(1)));
		}
		else if (Type == FNiagaraTypeDefinition::GetVec3Def() || Type == FNiagaraTypeDefinition::GetPositionDef())
		{
			Variable.SetValue<FVector3f>(FVector3f(At(0), At(1), At(2)));
		}
		else if (Type == FNiagaraTypeDefinition::GetVec4Def())
		{
			Variable.SetValue<FVector4f>(FVector4f(At(0), At(1), At(2), At(3)));
		}
		else if (Type == FNiagaraTypeDefinition::GetColorDef())
		{
			Variable.SetValue<FLinearColor>(FLinearColor(At(0), At(1), At(2), N.Num() > 3 ? At(3) : 1.f));
		}
		else
		{
			return false;
		}
		return true;
	}

	/** A static switch value as the pin stores it: bools "true"/"false", ints as digits, enums by entry name */
	static FString SwitchValue(const UEdGraphPin* Pin, const FString& Text)
	{
		const FNiagaraTypeDefinition Type = UEdGraphSchema_Niagara::PinToTypeDefinition(Pin);
		if (const UEnum* Enum = Type.GetEnum())
		{
			for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
			{
				if (Enum->GetNameStringByIndex(Index) == Text || Enum->GetDisplayNameTextByIndex(Index).ToString() == Text
					|| FString::FromInt(Index) == Text)
				{
					return Enum->GetNameStringByIndex(Index);
				}
			}
		}
		return Text;
	}

	static FString DescribeEnum(const UEdGraphPin* Pin)
	{
		const UEnum* Enum = UEdGraphSchema_Niagara::PinToTypeDefinition(Pin).GetEnum();
		if (!Enum)
		{
			return FString();
		}
		FString Out = TEXT("{");
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
		{
			Out += Enum->GetDisplayNameTextByIndex(Index).ToString() + (Index + 2 < Enum->NumEnums() ? TEXT("|") : TEXT(""));
		}
		return Out + TEXT("}");
	}

	/** The stack's modules in execution order: walks the parameter map chain back from the output node (the engine's ordered walk is not
	 *  exported) */
	static void OrderedModules(UNiagaraNodeOutput* Output, TArray<UNiagaraNodeFunctionCall*>& Out)
	{
		UNiagaraNode* Current = Output;
		while (Current)
		{
			UEdGraphPin* MapIn = nullptr;
			for (UEdGraphPin* Pin : Current->Pins)
			{
				if (Pin && Pin->Direction == EGPD_Input && UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef())
				{
					MapIn = Pin;
					break;
				}
			}
			if (!MapIn || MapIn->LinkedTo.IsEmpty())
			{
				return;
			}
			UNiagaraNode* Previous = Cast<UNiagaraNode>(MapIn->LinkedTo[0]->GetOwningNode());
			if (UNiagaraNodeFunctionCall* Call = Cast<UNiagaraNodeFunctionCall>(Previous))
			{
				Out.Insert(Call, 0);
			}
			Current = Previous;
		}
	}

	static UNiagaraNodeOutput* OutputFor(UNiagaraGraph* Graph, const FString& Stack)
	{
		TArray<UNiagaraNodeOutput*> Outputs;
		Graph->GetNodesOfClass(Outputs);
		for (UNiagaraNodeOutput* Output : Outputs)
		{
			if (Output && StackName(Output->GetUsage()) == Stack && Output->GetUsage() != ENiagaraScriptUsage::ParticleSpawnScriptInterpolated)
			{
				return Output;
			}
		}
		for (UNiagaraNodeOutput* Output : Outputs)
		{
			if (Output && StackName(Output->GetUsage()) == Stack)
			{
				return Output;
			}
		}
		return nullptr;
	}
}

bool UGSTN_VfxAuthoringLibrary::AddUserFloat(UNiagaraSystem* System, FName Name, float Default)
{
	return GSTNVfxAuthoring::AddUser<float>(System, FNiagaraTypeDefinition::GetFloatDef(), Name, Default);
}

bool UGSTN_VfxAuthoringLibrary::AddUserColor(UNiagaraSystem* System, FName Name, FLinearColor Default)
{
	return GSTNVfxAuthoring::AddUser<FLinearColor>(System, FNiagaraTypeDefinition::GetColorDef(), Name, Default);
}

bool UGSTN_VfxAuthoringLibrary::AddUserVector2(UNiagaraSystem* System, FName Name, FVector2D Default)
{
	// Niagara's vec2 is float, the Blueprint-facing FVector2D is double
	return GSTNVfxAuthoring::AddUser<FVector2f>(System, FNiagaraTypeDefinition::GetVec2Def(), Name, FVector2f(Default));
}

bool UGSTN_VfxAuthoringLibrary::AddUserInt(UNiagaraSystem* System, FName Name, int32 Default)
{
	return GSTNVfxAuthoring::AddUser<int32>(System, FNiagaraTypeDefinition::GetIntDef(), Name, Default);
}

FString UGSTN_VfxAuthoringLibrary::AddEmitter(UNiagaraSystem* System, const FString& EmitterAssetPath, const FString& NewName)
{
	UNiagaraEmitter* Source = LoadObject<UNiagaraEmitter>(nullptr, *EmitterAssetPath);
	if (!System || !Source)
	{
		return FString::Printf(TEXT("!cannot load %s"), *EmitterAssetPath);
	}
	System->Modify();
	const FGuid Id = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Source, Source->GetExposedVersion().VersionGuid);
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (Handle.GetId() == Id)
		{
			if (!NewName.IsEmpty())
			{
				Handle.SetName(FName(*NewName), *System);
			}
			System->MarkPackageDirty();
			return Handle.GetName().ToString();
		}
	}
	return TEXT("!emitter not added");
}

void UGSTN_VfxAuthoringLibrary::ClearEmitters(UNiagaraSystem* System)
{
	if (!System)
	{
		return;
	}
	System->Modify();
	TSet<FGuid> Ids;
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		Ids.Add(Handle.GetId());
	}
	System->RemoveEmitterHandlesById(Ids);
	System->MarkPackageDirty();
}

FString UGSTN_VfxAuthoringLibrary::DescribeEmitter(UNiagaraSystem* System, const FString& EmitterName)
{
	using namespace GSTNVfxAuthoring;
	FNiagaraEmitterHandle* Handle = FindHandle(System, EmitterName);
	FVersionedNiagaraEmitterData* Data = Handle ? Handle->GetEmitterData() : nullptr;
	UNiagaraGraph* Graph = EmitterGraph(Data);
	if (!Graph)
	{
		return FString::Printf(TEXT("!no emitter %s"), *EmitterName);
	}
	FString Out;
	TArray<UNiagaraNodeFunctionCall*> Calls;
	Graph->GetNodesOfClass(Calls);
	for (UNiagaraNodeFunctionCall* Call : Calls)
	{
		UNiagaraNodeOutput* Output = Call ? StackOutputOf(Call) : nullptr;
		if (!Output)
		{
			continue;
		}
		const TOptional<bool> Enabled = FNiagaraStackGraphUtilities::GetModuleIsEnabled(*Call);
		Out += FString::Printf(TEXT("%s %s%s |"), *StackName(Output->GetUsage()), *Call->GetFunctionName(),
			Enabled.IsSet() && !Enabled.GetValue() ? TEXT(" (off)") : TEXT(""));
		TArray<FNiagaraVariable> Inputs;
		FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Call, Inputs, FCompileConstantResolver(),
			FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly);
		for (const FNiagaraVariable& Input : Inputs)
		{
			Out += TEXT(" ") + Input.GetName().ToString().Replace(TEXT("Module."), TEXT("")) + TEXT(":") + Input.GetType().GetName();
		}
		TArray<UEdGraphPin*> Switches;
		TSet<UEdGraphPin*> Hidden;
		FCompileConstantResolver Resolver;
		FNiagaraStackGraphUtilities::GetStackFunctionStaticSwitchPins(*Call, Switches, Hidden, Resolver);
		Out += TEXT(" | SW");
		for (const UEdGraphPin* Pin : Switches)
		{
			if (!Hidden.Contains(Pin))
			{
				Out += FString::Printf(TEXT(" %s=%s%s"), *Pin->PinName.ToString(), *Pin->DefaultValue, *DescribeEnum(Pin));
			}
		}
		Out += TEXT("\n");
	}
	for (int32 Index = 0; Index < Data->GetRenderers().Num(); ++Index)
	{
		Out += FString::Printf(TEXT("Renderer %d %s\n"), Index, *GetNameSafe(Data->GetRenderers()[Index] ? Data->GetRenderers()[Index]->GetClass() : nullptr));
	}
	return Out;
}

FString UGSTN_VfxAuthoringLibrary::DescribeInput(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleName,
	const FString& InputName)
{
	using namespace GSTNVfxAuthoring;
	FModuleRef Ref;
	const FString Error = FindModule(System, EmitterName, Stack, ModuleName, Ref);
	if (!Error.IsEmpty())
	{
		return Error;
	}
	const FName FullInput(*(TEXT("Module.") + InputName));
	const FNiagaraParameterHandle Aliased = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(FullInput), Ref.Module);
	FString Out = FString::Printf(TEXT("module %s usage %s aliased %s emitter %s\n"), *Ref.Module->GetFunctionName(), *StackName(Ref.Usage),
		*Aliased.GetParameterHandleString().ToString(), *Ref.Handle->GetInstance().Emitter->GetUniqueEmitterName());
	if (UEdGraphPin* Override = FNiagaraStackGraphUtilities::GetStackFunctionInputOverridePin(*Ref.Module, Aliased))
	{
		Out += FString::Printf(TEXT("override pin default '%s' links %d"), *Override->DefaultValue, Override->LinkedTo.Num());
		for (const UEdGraphPin* Linked : Override->LinkedTo)
		{
			Out += TEXT(" <- ") + GetNameSafe(Linked ? Linked->GetOwningNode() : nullptr);
		}
		Out += TEXT("\n");
	}
	else
	{
		Out += TEXT("no override pin\n");
	}
	TArray<UNiagaraScript*> Scripts;
	ScriptsRunning(System, Ref, Scripts);
	for (UNiagaraScript* Script : Scripts)
	{
		TArray<FNiagaraVariable> Params;
		Script->RapidIterationParameters.GetParameters(Params);
		for (const FNiagaraVariable& Param : Params)
		{
			if (Param.GetName().ToString().Contains(InputName))
			{
				const uint8* Data = Script->RapidIterationParameters.GetParameterData(Param);
				FString Value = TEXT("?");
				if (Data && Param.GetType() == FNiagaraTypeDefinition::GetIntDef())
				{
					Value = FString::FromInt(*reinterpret_cast<const int32*>(Data));
				}
				else if (Data && Param.GetType() == FNiagaraTypeDefinition::GetFloatDef())
				{
					Value = FString::SanitizeFloat(*reinterpret_cast<const float*>(Data));
				}
				Out += FString::Printf(TEXT("RI %s: %s = %s\n"), *GetNameSafe(Script), *Param.GetName().ToString(), *Value);
			}
		}
	}
	return Out;
}

FString UGSTN_VfxAuthoringLibrary::AddModule(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleScriptPath)
{
	using namespace GSTNVfxAuthoring;
	FNiagaraEmitterHandle* Handle = FindHandle(System, EmitterName);
	UNiagaraGraph* Graph = EmitterGraph(Handle ? Handle->GetEmitterData() : nullptr);
	UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModuleScriptPath);
	if (!Graph || !ModuleScript)
	{
		return FString::Printf(TEXT("cannot add %s to %s"), *ModuleScriptPath, *EmitterName);
	}
	UNiagaraNodeOutput* Output = OutputFor(Graph, Stack);
	if (!Output)
	{
		return FString::Printf(TEXT("no stack %s"), *Stack);
	}
	Graph->Modify();
	UNiagaraNodeFunctionCall* Added = FNiagaraStackGraphUtilities::AddScriptModuleToStack(ModuleScript, *Output);
	System->MarkPackageDirty();
	return Added ? FString() : TEXT("AddScriptModuleToStack failed");
}

FString UGSTN_VfxAuthoringLibrary::AddModuleBefore(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack,
	const FString& ModuleScriptPath, const FString& BeforeModule)
{
	using namespace GSTNVfxAuthoring;
	FNiagaraEmitterHandle* Handle = FindHandle(System, EmitterName);
	UNiagaraGraph* Graph = EmitterGraph(Handle ? Handle->GetEmitterData() : nullptr);
	UNiagaraScript* ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ModuleScriptPath);
	UNiagaraNodeOutput* Output = Graph ? OutputFor(Graph, Stack) : nullptr;
	if (!ModuleScript || !Output)
	{
		return FString::Printf(TEXT("cannot add %s to %s %s"), *ModuleScriptPath, *EmitterName, *Stack);
	}
	TArray<UNiagaraNodeFunctionCall*> Modules;
	OrderedModules(Output, Modules);
	const int32 Index = Modules.IndexOfByPredicate([&BeforeModule](const UNiagaraNodeFunctionCall* Call) { return Call->GetFunctionName() == BeforeModule; });
	if (Index == INDEX_NONE)
	{
		return FString::Printf(TEXT("no module %s in %s (%d modules)"), *BeforeModule, *Stack, Modules.Num());
	}
	Graph->Modify();
	UNiagaraNodeFunctionCall* Added = FNiagaraStackGraphUtilities::AddScriptModuleToStack(ModuleScript, *Output, Index);
	System->MarkPackageDirty();
	return Added ? FString() : TEXT("AddScriptModuleToStack failed");
}

FString UGSTN_VfxAuthoringLibrary::ListModules(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack)
{
	using namespace GSTNVfxAuthoring;
	FNiagaraEmitterHandle* Handle = FindHandle(System, EmitterName);
	UNiagaraGraph* Graph = EmitterGraph(Handle ? Handle->GetEmitterData() : nullptr);
	UNiagaraNodeOutput* Output = Graph ? OutputFor(Graph, Stack) : nullptr;
	if (!Output)
	{
		return FString::Printf(TEXT("no stack %s on %s"), *Stack, *EmitterName);
	}
	TArray<UNiagaraNodeFunctionCall*> Modules;
	OrderedModules(Output, Modules);
	TArray<FString> Names;
	for (const UNiagaraNodeFunctionCall* Call : Modules)
	{
		Names.Add(Call->GetFunctionName() + (Call->IsNodeEnabled() ? TEXT("") : TEXT("(off)")));
	}
	return FString::Join(Names, TEXT(" > "));
}

FString UGSTN_VfxAuthoringLibrary::SetModuleEnabled(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleName, bool bEnabled)
{
	GSTNVfxAuthoring::FModuleRef Ref;
	const FString Error = GSTNVfxAuthoring::FindModule(System, EmitterName, Stack, ModuleName, Ref);
	if (!Error.IsEmpty())
	{
		return Error;
	}
	Ref.Graph->Modify();
	FNiagaraStackGraphUtilities::SetModuleIsEnabled(*Ref.Module, bEnabled);
	System->MarkPackageDirty();
	return FString();
}

FString UGSTN_VfxAuthoringLibrary::SetInput(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleName,
	const FString& InputName, const FString& Value)
{
	using namespace GSTNVfxAuthoring;
	FModuleRef Ref;
	const FString Error = FindModule(System, EmitterName, Stack, ModuleName, Ref);
	if (!Error.IsEmpty())
	{
		return Error;
	}

	// A static switch is a pin of the module node
	TArray<UEdGraphPin*> Switches;
	TSet<UEdGraphPin*> Hidden;
	FCompileConstantResolver Resolver;
	FNiagaraStackGraphUtilities::GetStackFunctionStaticSwitchPins(*Ref.Module, Switches, Hidden, Resolver);
	for (UEdGraphPin* Pin : Switches)
	{
		if (Pin && Pin->PinName.ToString() == InputName)
		{
			Ref.Graph->Modify();
			Ref.Module->Modify();
			GetDefault<UEdGraphSchema_Niagara>()->TrySetDefaultValue(*Pin, SwitchValue(Pin, Value));
			Ref.Graph->NotifyGraphChanged();
			System->MarkPackageDirty();
			return FString();
		}
	}

	TArray<FNiagaraVariable> Inputs;
	FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Ref.Module, Inputs, FCompileConstantResolver(),
		FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly);
	const FName FullInput(*(TEXT("Module.") + InputName));
	const FNiagaraVariable* Input = Inputs.FindByPredicate([&FullInput](const FNiagaraVariable& V) { return V.GetName() == FullInput; });
	if (!Input)
	{
		return FString::Printf(TEXT("no input %s on %s (see DescribeEmitter)"), *InputName, *Ref.Module->GetFunctionName());
	}

	Ref.Graph->Modify();
	const FNiagaraParameterHandle Aliased = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(FullInput), Ref.Module);
	// A local value only counts when nothing overrides the input in the graph: drop a link, a dynamic input or a data value there
	if (UEdGraphPin* Override = FNiagaraStackGraphUtilities::GetStackFunctionInputOverridePin(*Ref.Module, Aliased))
	{
		Override->BreakAllPinLinks(true);
		if (UEdGraphNode* Owner = Override->GetOwningNode())
		{
			Owner->Modify();
			Owner->RemovePin(Override);
		}
	}

	FNiagaraVariable Constant = FNiagaraUtilities::ConvertVariableToRapidIterationConstantName(
		FNiagaraVariable(Input->GetType(), Aliased.GetParameterHandleString()), *Ref.Handle->GetInstance().Emitter->GetUniqueEmitterName(), Ref.Usage);
	if (!FillValue(Constant, Value))
	{
		return FString::Printf(TEXT("type %s of %s is not settable from text"), *Input->GetType().GetName(), *InputName);
	}
	TArray<UNiagaraScript*> Scripts;
	ScriptsRunning(System, Ref, Scripts);
	for (UNiagaraScript* Script : Scripts)
	{
		Script->Modify();
		Script->RapidIterationParameters.AddParameter(Constant, true, false);
		Script->RapidIterationParameters.SetParameterData(Constant.GetData(), Constant, true);
	}
	Ref.Graph->NotifyGraphChanged();
	System->MarkPackageDirty();
	return Scripts.IsEmpty() ? TEXT("no script runs this module") : FString();
}

FString UGSTN_VfxAuthoringLibrary::LinkModuleInput(UNiagaraSystem* System, const FString& EmitterName, const FString& Stack, const FString& ModuleName,
	const FString& InputName, FName Parameter)
{
	using namespace GSTNVfxAuthoring;
	FModuleRef Ref;
	const FString Error = FindModule(System, EmitterName, Stack, ModuleName, Ref);
	if (!Error.IsEmpty())
	{
		return Error;
	}
	TArray<FNiagaraVariable> Inputs;
	FNiagaraStackGraphUtilities::GetStackFunctionInputs(*Ref.Module, Inputs, FCompileConstantResolver(),
		FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions::ModuleInputsOnly);
	const FName FullInput(*(TEXT("Module.") + InputName));
	const FNiagaraVariable* Input = Inputs.FindByPredicate([&FullInput](const FNiagaraVariable& V) { return V.GetName() == FullInput; });
	if (!Input)
	{
		return FString::Printf(TEXT("no input %s on %s"), *InputName, *Ref.Module->GetFunctionName());
	}
	Ref.Graph->Modify();
	const FNiagaraParameterHandle Aliased = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(FullInput), Ref.Module);
	UEdGraphPin& OverridePin = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Ref.Module, Aliased, Input->GetType(), FGuid(), FGuid());
	if (OverridePin.LinkedTo.Num() > 0)
	{
		OverridePin.BreakAllPinLinks(true);  // an old dynamic input node stays unreferenced: the compiler ignores it
	}
	FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(OverridePin, FNiagaraVariableBase(Input->GetType(), Parameter),
		TSet<FNiagaraVariableBase>(), ENiagaraDefaultMode::FailIfPreviouslyNotSet);
	Ref.Graph->NotifyGraphChanged();
	System->MarkPackageDirty();
	return FString();
}

UNiagaraRendererProperties* UGSTN_VfxAuthoringLibrary::GetRenderer(UNiagaraSystem* System, const FString& EmitterName, int32 Index)
{
	FNiagaraEmitterHandle* Handle = GSTNVfxAuthoring::FindHandle(System, EmitterName);
	FVersionedNiagaraEmitterData* Data = Handle ? Handle->GetEmitterData() : nullptr;
	return Data && Data->GetRenderers().IsValidIndex(Index) ? Data->GetRenderers()[Index] : nullptr;
}

UNiagaraRendererProperties* UGSTN_VfxAuthoringLibrary::SetRibbonRenderer(UNiagaraSystem* System, const FString& EmitterName)
{
	FNiagaraEmitterHandle* Handle = GSTNVfxAuthoring::FindHandle(System, EmitterName);
	FVersionedNiagaraEmitterData* Data = Handle ? Handle->GetEmitterData() : nullptr;
	if (!Data)
	{
		return nullptr;
	}
	const FVersionedNiagaraEmitter Instance = Handle->GetInstance();
	UNiagaraEmitter* Emitter = Instance.Emitter;
	Emitter->Modify();
	const TArray<UNiagaraRendererProperties*> Old = Data->GetRenderers();
	for (UNiagaraRendererProperties* Renderer : Old)
	{
		Emitter->RemoveRenderer(Renderer, Instance.Version);
	}
	UNiagaraRibbonRendererProperties* Ribbon = NewObject<UNiagaraRibbonRendererProperties>(Emitter, NAME_None, RF_Transactional);
	Emitter->AddRenderer(Ribbon, Instance.Version);
	System->MarkPackageDirty();
	return Ribbon;
}

FString UGSTN_VfxAuthoringLibrary::SetEmitterSettings(UNiagaraSystem* System, const FString& EmitterName, bool bLocalSpace, float FixedBoundsHalfExtent)
{
	FNiagaraEmitterHandle* Handle = GSTNVfxAuthoring::FindHandle(System, EmitterName);
	FVersionedNiagaraEmitterData* Data = Handle ? Handle->GetEmitterData() : nullptr;
	if (!Data)
	{
		return FString::Printf(TEXT("no emitter %s"), *EmitterName);
	}
	Handle->GetInstance().Emitter->Modify();
	Data->SimTarget = ENiagaraSimTarget::CPUSim;
	Data->bLocalSpace = bLocalSpace;
	if (FixedBoundsHalfExtent > 0.f)
	{
		Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
		Data->FixedBounds = FBox(FVector(-FixedBoundsHalfExtent), FVector(FixedBoundsHalfExtent));
	}
	else
	{
		Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Dynamic;
	}
	System->MarkPackageDirty();
	return FString();
}

void UGSTN_VfxAuthoringLibrary::SetSystemFixedBounds(UNiagaraSystem* System, float HalfExtent)
{
	if (!System)
	{
		return;
	}
	System->Modify();
	if (FBoolProperty* Flag = FindFProperty<FBoolProperty>(UNiagaraSystem::StaticClass(), TEXT("bFixedBounds")))
	{
		Flag->SetPropertyValue_InContainer(System, HalfExtent > 0.f);
	}
	System->SetFixedBounds(FBox(FVector(-HalfExtent), FVector(HalfExtent)));
	System->MarkPackageDirty();
}

void UGSTN_VfxAuthoringLibrary::CompileAndWait(UNiagaraSystem* System)
{
	if (!System)
	{
		return;
	}
	System->RequestCompile(false);
	System->WaitForCompilationComplete(false, false);
}

FString UGSTN_VfxAuthoringLibrary::DescribeParticles(UNiagaraComponent* Component, const FString& EmitterName, int32 Max)
{
	FNiagaraSystemInstanceControllerPtr Controller = Component ? Component->GetSystemInstanceController() : nullptr;
	FNiagaraSystemInstance* Instance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
	if (!Instance)
	{
		return TEXT("no instance");
	}
	for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetEmitters())
	{
		if (Emitter->GetEmitterHandle().GetName() != FName(*EmitterName))
		{
			continue;
		}
		const FNiagaraDataSet& Data = Emitter->GetParticleData();
		const FNiagaraDataBuffer* Buffer = Data.GetCurrentData();
		if (!Buffer)
		{
			return TEXT("n=0 (no data)");
		}
		const auto Position = FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(Data, TEXT("Position"));
		const auto Velocity = FNiagaraDataSetAccessor<FVector3f>::CreateReader(Data, TEXT("Velocity"));
		const int32 Count = Buffer->GetNumInstances();
		FString Out = FString::Printf(TEXT("n=%d"), Count);
		for (int32 Index = 0; Index < FMath::Min(Count, Max); ++Index)
		{
			const FVector3f P = Position.GetSafe(Index, FNiagaraPosition(FVector3f::ZeroVector));
			const FVector3f V = Velocity.GetSafe(Index, FVector3f::ZeroVector);
			Out += FString::Printf(TEXT(" %.0f,%.0f,%.0f/%.0f,%.0f,%.0f"), P.X, P.Y, P.Z, V.X, V.Y, V.Z);
		}
		return Out;
	}
	return TEXT("no emitter");
}
