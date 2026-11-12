// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GASToolsets : ModuleRules
{
	public GASToolsets(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.Add("Core");

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayAbilitiesEditor",
			"GameplayTags",
			"GameplayTagsEditor",
			"Kismet",
			"BlueprintGraph",
			"ToolsetRegistry",
			"ToolsetRegistryEditor",
			"UnrealEd",
		});
	}
}
