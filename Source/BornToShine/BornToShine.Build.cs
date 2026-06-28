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
			"Niagara",
			"RenderCore"  // GWhiteTexture for the HUD's untextured triangle fills
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
