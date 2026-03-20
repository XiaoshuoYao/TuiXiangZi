// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TuiXiangZi : ModuleRules
{
	public TuiXiangZi(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[] { ModuleDirectory });

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem", "AIModule", "Niagara", "EnhancedInput", "Json", "JsonUtilities", "ProceduralMeshComponent", "UMG", "Slate", "SlateCore", "RenderCore" });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
