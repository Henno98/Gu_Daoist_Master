// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Gu_Daoist_Master : ModuleRules
{
    public Gu_Daoist_Master(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "AIModule",
            "StateTreeModule",
            "GameplayStateTreeModule",
            "UMG",
            "Slate",
            "SlateCore",
            "GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "Json",
            "JsonUtilities"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "NetCore"
        });

        PublicIncludePaths.AddRange(new string[] {
            "Gu_Daoist_Master",
            "Gu_Daoist_Master/Variant_Horror",
            "Gu_Daoist_Master/Variant_Horror/UI",
            "Gu_Daoist_Master/Variant_Shooter",
            "Gu_Daoist_Master/Variant_Shooter/AI",
            "Gu_Daoist_Master/Variant_Shooter/UI",
            "Gu_Daoist_Master/Variant_Shooter/Weapons"
        });
    }
}
