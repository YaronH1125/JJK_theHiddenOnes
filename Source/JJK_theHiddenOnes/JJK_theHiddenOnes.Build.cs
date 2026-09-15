// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class JJK_theHiddenOnes : ModuleRules
{
	public JJK_theHiddenOnes(ReadOnlyTargetRules Target) : base(Target)
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
			"JJK_theHiddenOnes",
			"JJK_theHiddenOnes/Training",
			"JJK_theHiddenOnes/Variant_Platforming",
			"JJK_theHiddenOnes/Variant_Platforming/Animation",
			"JJK_theHiddenOnes/Variant_Combat",
			"JJK_theHiddenOnes/Variant_Combat/AI",
			"JJK_theHiddenOnes/Variant_Combat/Animation",
			"JJK_theHiddenOnes/Variant_Combat/Gameplay",
			"JJK_theHiddenOnes/Variant_Combat/Interfaces",
			"JJK_theHiddenOnes/Variant_Combat/UI",
			"JJK_theHiddenOnes/Variant_SideScrolling",
			"JJK_theHiddenOnes/Variant_SideScrolling/AI",
			"JJK_theHiddenOnes/Variant_SideScrolling/Gameplay",
			"JJK_theHiddenOnes/Variant_SideScrolling/Interfaces",
			"JJK_theHiddenOnes/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
