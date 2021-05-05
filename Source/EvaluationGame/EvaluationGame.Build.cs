// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EvaluationGame : ModuleRules
{
	public EvaluationGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay" });
	}
}
