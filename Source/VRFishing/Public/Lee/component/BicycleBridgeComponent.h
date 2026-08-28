// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BicycleBridgeComponent.generated.h"

class AVRPawn;
class UFishingWiredDeviceSubsystem;

/**
 * @brief 自転車デバイス（ASerial 0x03）の RPS を Reel 状態の仮想スティック入力へ橋渡しするブリッジ。
 * @note データ経路:
 *       UFishingWiredDeviceSubsystem（シリアル受信/模擬）
 *         → GetLatestBicycleSample()（RPS・回転方向）
 *         → 仮想角度 θ を RPS 分だけ毎フレーム進める
 *         → AVRPawn::InjectReelStickInput((cosθ, sinθ))
 *       InjectReelStickInput は前後フレームのベクトル角差 → Δangle → RPM 計算・速すぎ/遅すぎ判定
 *       （CalculateRPM/JudgeRPM）へそのまま流れるため、実リールと同じ判定プレイが成立する。
 *       Reel 状態が非アクティブのときは Pawn 側で入力が無視されるため本ブリッジの個別ゲートは不要。
 *       BP_XRPawn へ手動追加して使用する。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UBicycleBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBicycleBridgeComponent();

	// --- UActorComponent overrides ---
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==================== 設定パラメータ ====================

	/** @brief デバイスの回転方向データが 0（逆転）のときに入力方向を反転するか（既定: 反転する） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice")
	bool bRespectDeviceDirection = true;

	/** @brief 仮想スティックの回転向きを反転する（回転角度の進む向き調整） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice")
	bool bInvertVirtualRotation = false;

	/** @brief RPS に掛ける係数（装置のギヤ比などで実効速度を調整する場合に使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice", meta = (ClampMin = "0.0"))
	float RpsScale = 1.0f;

	/** @brief この秒数データが更新されなかったら注入を停止する [秒] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice", meta = (ClampMin = "0.1"))
	float DataStaleTimeoutSeconds = 1.0f;

	/** @brief Debug 表示を有効にするか */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice|Debug")
	bool bShowDebug = true;

private:
	/** @brief オーナーからサブシステムを取得する（毎フレーム軽量取得） */
	UFishingWiredDeviceSubsystem* FindSubsystem() const;

	/** @brief 仮想スティックの現在角度[rad]（RPS に応じて毎フレーム進める） */
	float VirtualAngleRad = 0.0f;
};
