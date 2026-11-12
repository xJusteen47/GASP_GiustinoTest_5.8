// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class GASP_Giustino2Target : TargetRules
{
	public GASP_Giustino2Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// UE 5.9: V8 drops Tests folders from the game target; with V7 the installed engine refuses the
		// shared build environment (IncludeTestsFolder differs from UnrealGame). Tests run in the editor.
		DefaultBuildSettings = BuildSettingsVersion.V8;

		ExtraModuleNames.AddRange( new string[] { "GASP_Giustino2" } );
	}
}
