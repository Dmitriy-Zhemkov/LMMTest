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
            "RigVM"
        });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

        // ONNX Runtime
        string OnnxPath = Path.Combine(ModuleDirectory, "../ThirdParty/onnxruntime");

        if (Directory.Exists(OnnxPath))
        {
            PublicIncludePaths.Add(Path.Combine(OnnxPath, "include"));
            PublicAdditionalLibraries.Add(Path.Combine(OnnxPath, "lib/onnxruntime.lib"));

            // Копируем DLL
            string DllPath = Path.Combine(OnnxPath, "lib/onnxruntime.dll");
            if (File.Exists(DllPath))
            {
                RuntimeDependencies.Add("$(BinaryOutputDir)/onnxruntime.dll", DllPath);
                PublicDelayLoadDLLs.Add("onnxruntime.dll");
            }

            PublicDefinitions.Add("WITH_ONNX=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_ONNX=0");
        }
        bUseRTTI = true;
        bEnableExceptions = true;
    }
}
