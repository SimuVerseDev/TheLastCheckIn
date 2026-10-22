// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TheLastCheckIn : ModuleRules
{
	public TheLastCheckIn(ReadOnlyTargetRules Target) : base(Target)
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
			"ProceduralMeshComponent",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"TheLastCheckIn",
			// Template variant samples kept for reference only
			"TheLastCheckIn/Legacy/Variant_Horror",
			"TheLastCheckIn/Legacy/Variant_Horror/UI",
			"TheLastCheckIn/Legacy/Variant_Shooter",
			"TheLastCheckIn/Legacy/Variant_Shooter/AI",
			"TheLastCheckIn/Legacy/Variant_Shooter/UI",
			"TheLastCheckIn/Legacy/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
