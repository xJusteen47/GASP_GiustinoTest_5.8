// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class GASP_Giustino2EditorTarget : TargetRules
{
	public GASP_Giustino2EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// UE 5.9: V8, or the editor asks to upgrade the target at every launch (editor targets keep their Tests folders on V8 too)
		DefaultBuildSettings = BuildSettingsVersion.V8;

		ExtraModuleNames.AddRange( new string[] { "GASP_Giustino2" } );
	}
}
