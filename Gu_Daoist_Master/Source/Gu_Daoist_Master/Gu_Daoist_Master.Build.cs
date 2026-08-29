// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Gu_Daoist_Master : ModuleRules
{
	public Gu_Daoist_Master(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks"
        });

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Gu_Daoist_Master",
			"Gu_Daoist_Master/Variant_Horror",
			"Gu_Daoist_Master/Variant_Horror/UI",
			"Gu_Daoist_Master/Variant_Shooter",
			"Gu_Daoist_Master/Variant_Shooter/AI",
			"Gu_Daoist_Master/Variant_Shooter/UI",
			"Gu_Daoist_Master/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
