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
 *       デバイスの回転方向(0x23)が 0（逆転）でも踏み込み強度は絶対値で正方向へ反映される
 *       （ゲーム判定は正角差のみ受理のため。方向はログ/Debug 表示専用）。
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

	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 旧パラメータ bRespectDeviceDirection / bInvertVirtualRotation は削除。
	// ゲーム側 SimulateReelByStick は「正方向の角差」のみ受理する（DeltaAngle>0 フィルタ）ため、
	// 方向 0x23=0 (逆転) を負の RPS として注入すると負角差が全てフィルタされ「完全無反応」になる。
	// → 有効 RPS は絶対値へ正規化して常に正方向へ進める方式へ変更（回転方向はログ/表示専用）。
	//   なお旧 bInvertVirtualRotation は毎フレーム θ の符号を反転する実装であり、
	//   反転ではなく ±振動（巨大な角差ジャンプ）を生むバグだったため、一緒に撤去した。
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

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
