// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BornToShine : ModuleRules
{
	public BornToShine(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"UMG",
			"Slate",
			"SlateCore",
			"Json",
			"ProceduralMeshComponent",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
