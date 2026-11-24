// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class GASP_Giustino2 : ModuleRules
{
	public GASP_Giustino2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Mover", "GameplayTags", "NetCore", "GameplayAbilities", "AbilitySystemSimulation", "GSTNCombatNP", "GSTNCombatCore", "GSTNInputCore", "GSTNInputNP", "GSTNAnimationCore" });

		// GAS + Network Prediction (GAS-NPP fork) test harness
		PrivateDependencyModuleNames.AddRange(new string[] { "GSTNCombatGAS", "GSTNPickups", "GSTNCameraCore", "GSTNCameraRuntime", "GSTNSurfaceCore", "GSTNLevelObjects", "GameplayTasks", "NetworkPrediction", "EnhancedInput", "Json" });

		// Traversal surface data contract: the wall surface filter resolves the surface of a hit (traversal-surfaces.md section 5.2)
		PrivateDependencyModuleNames.Add("GSTNSurfaceCore");

		// Launch readability (owner 2026-11-24, 3A): project settings, the off-screen arrow layer, the launch class of GSTNPresentation
		PrivateDependencyModuleNames.AddRange(new string[] { "DeveloperSettings", "UMG", "Slate", "SlateCore", "GSTNPresentationCore" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
