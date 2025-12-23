// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class LMMTest : ModuleRules
{
	public LMMTest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] 
        { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore", 
            "EnhancedInput", 
            "ControlRig", 
            "AnimGraphRuntime", 
            "RigVM"
        });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "../ThirdParty/onnxruntime/include"));
        PublicAdditionalLibraries.Add(Path.Combine(ModuleDirectory, "../ThirdParty/onnxruntime/lib/onnxruntime.lib"));
        RuntimeDependencies.Add("$(TargetOutputDir)/onnxruntime.dll", Path.Combine(ModuleDirectory, "../ThirdParty/onnxruntime/lib/onnxruntime.dll"));

        //PublicSystemLibraries.Add("msvcp140.lib");
        bUseRTTI = true;
        bEnableExceptions = true;
    }
}
