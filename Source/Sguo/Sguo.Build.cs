// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sguo : ModuleRules
{
	public Sguo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate",
			"GameplayTasks",
			"GameplayTags",
			"SlateCore",
			"GameplayAbilities",
			// 添加 MVVM 模块以使用 ViewModel API
			"ModelViewViewModel",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"NetCore"
		});

		PublicIncludePaths.AddRange(new string[] {
			"Sguo",
			"Sguo/Variant_Strategy",
			"Sguo/Variant_Strategy/UI",
			"Sguo/Variant_TwinStick",
			"Sguo/Variant_TwinStick/AI",
			"Sguo/Variant_TwinStick/Gameplay",
			"Sguo/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
