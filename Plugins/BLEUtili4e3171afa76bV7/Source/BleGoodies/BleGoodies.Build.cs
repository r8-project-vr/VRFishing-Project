// Copyright (c) 2020 Nineva Studios

using System.IO;
using UnrealBuildTool;

public class BleGoodies : ModuleRules
{
	public BleGoodies(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine"
			}
		);

		string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);


		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Android"));
			PrivateDependencyModuleNames.Add("Launch");

			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "BleGoodies_Android_UPL.xml"));
		}

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/IOS"));
			PrivateDependencyModuleNames.Add("Launch");

			AdditionalPropertiesForReceipt.Add("IOSPlugin", Path.Combine(PluginPath, "BleGoodies_IOS_UPL.xml"));

			PublicFrameworks.Add("CoreBluetooth");
		}
	}
}
