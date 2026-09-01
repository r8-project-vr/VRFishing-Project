// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class VRFishing : ModuleRules
{
	public VRFishing(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG" });
		// 2026.08.19 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// FishingMenuNavigatorComponent の C++ 拡張入力バインド用に追加
		PublicDependencyModuleNames.AddRange(new string[] { "EnhancedInput" });
		// 2026.08.19 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

		// 2026.08.24 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// FishingLoadSettingsDeveloperSettings（Project Settings パネル）の基底クラスがあるモジュール
		PublicDependencyModuleNames.AddRange(new string[] { "DeveloperSettings" });

		// 2026.08.26 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// 有線デバイス（ASerial/UART）連携用。ASerialCom は Win64 専用プラグイン（PlatformAllowList）のため、
		// Win64 のときのみ依存に追加する。参照箇所は PLATFORM_WINDOWS ガードで括ること。
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDependencyModuleNames.AddRange(new string[] { "ASerialCom" });
		}
		// 2026.08.26 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// 2026.08.24 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// 2026.08.31 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// RpmGaugeWidget の OnPaint 自前描画（FSlateDrawElement）に使用
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		// 2026.08.31 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

		// 2026.08.31 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		// AVRMenuPawn 用。UMotionControllerComponent は HeadMountedDisplay、
		// UHeadMountedDisplayFunctionLibrary は XRBase プラグイン（UE5.8 ではモジュール分割済み）
		PublicDependencyModuleNames.AddRange(new string[] { "HeadMountedDisplay", "XRBase" });
		// 2026.08.31 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
