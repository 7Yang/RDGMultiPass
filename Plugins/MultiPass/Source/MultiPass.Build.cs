// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MultiPass : ModuleRules
{
	public MultiPass(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths .AddRange( new string[] { } );
		PrivateIncludePaths.AddRange( new string[] { } );
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
				"Core",
				"CoreUObject",
				"Engine",
				"RHI",
				"Engine",
				"RenderCore",
                "AssetTools",
				"Slate",
                "SlateCore",
				"UMG",
                "MaterialShaderQualitySettings",
		});
			
		DynamicallyLoadedModuleNames.AddRange(new string[] { } );
		PrivateDependencyModuleNames.AddRange(new string[] { "Projects", });
	}
}
