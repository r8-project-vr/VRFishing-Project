// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Lee/device/FishingWiredDeviceTypes.h"
#include "ArmTrackerBridgeComponent.generated.h"

class UHandHeightDetectorComponent;
class UFishingWiredDeviceSubsystem;

/** @brief アームトラッカーのオイラー角のうち上下運動判定に使う軸 */
UENUM(BlueprintType)
enum class EArmTrackerAxis : uint8
{
	/** X軸（= Roll） */
	AxisX	UMETA(DisplayName = "X軸(Roll)"),
	/** Y軸（= Pitch）。既定: 手首/前腕装着時の上下運動に対応 */
	AxisY	UMETA(DisplayName = "Y軸(Pitch)"),
	/** Z軸（= Yaw） */
	AxisZ	UMETA(DisplayName = "Z軸(Yaw)")
};

/**
 * @brief アームトラッカー（ASerial 0x02）のオイラー角を HandHeightDetectorComponent の外部データへ橋渡しするブリッジ。
 * @note データ経路:
 *       UFishingWiredDeviceSubsystem（シリアル受信/模擬）
 *         → GetLatestArmEuler()（角度）
 *         → 軸選択 + キャリブレーション二点マッピング + 平滑化
 *         → SetExternalHandData(0.0〜1.0, 速度cm/s)
 *       下流の Ready 状態・上下運動状態・Widget は一切変更不要（抽象層の設計どおり）。
 *       BP_XRPawn へ手動追加して使用する（FishingLoadApplierComponent と同じ運用）。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UArmTrackerBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArmTrackerBridgeComponent();

	// --- UActorComponent overrides ---
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * @brief 現在の生角度をキャリブレーション下限（CalAngleLow）として記録する。
	 * @note 腕を最も下げた姿勢で呼ぶ。デバイス側 0x20 とは独立したゲーム側二点校准。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice|Calibration")
	void CaptureCalibrationLow();

	/**
	 * @brief 現在の生角度をキャリブレーション上限（CalAngleHigh）として記録する。
	 * @note 腕を最も上げた姿勢で呼ぶ。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice|Calibration")
	void CaptureCalibrationHigh();

	/**
	 * @brief デバイス側キャリブレーション(0x20)の送信を要求する。
	 * @note 返答なしコマンドのため送信要求のみ。完了待ちの仕組みはない。
	 */
	UFUNCTION(BlueprintCallable, Category = "Fishing|WiredDevice|Calibration")
	void SendDeviceCalibrationRequest();

	// ==================== 設定パラメータ ====================

	/** @brief 上下運動判定に使うオイラー軸（装着方向により変更。実機で要確認） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice")
	EArmTrackerAxis SourceAxis = EArmTrackerAxis::AxisY;

	/** @brief キャリブレーション下限角度[deg]（= HandHeightPercent 0.0 に対応） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice")
	float CalAngleLow = -80.0f;

	/** @brief キャリブレーション上限角度[deg]（= HandHeightPercent 1.0 に対応） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice")
	float CalAngleHigh = 40.0f;

	/** @brief 平滑化係数（0〜1。1=平滑化なし, 小さいほど遅れが大きいが滑らか） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SmoothingAlpha = 0.5f;

	/**
	 * @brief 端点スナップ閾値（percent が 0/1 からこの割合以内なら端点に固定する）。
	 * @note Ready 状態の「50cm 下げ」判定は VirtualZOffset == BottomOffset（= percent が正確に 0.0）でのみ成立する。
	 *       IMU ノイズや平滑化の漸近残差で percent が微小正値に留まると Ready が完了しなくなるため、
	 *       端点近傍を 0/1 にスナップして確実に判定できるようにする（実機ノイズ対策でもある）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice", meta = (ClampMin = "0.0", ClampMax = "0.1"))
	float EndpointSnapPercent = 0.01f;

	/** @brief 軸の向きを反転する（装着方向が逆のとき） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice")
	bool bInvertAxis = false;

	/** @brief この秒数データが更新されなかったら OpenXR へフォールバックする [秒] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice", meta = (ClampMin = "0.1"))
	float DataStaleTimeoutSeconds = 1.0f;

	/** @brief Debug 表示を有効にするか */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice|Debug")
	bool bShowDebug = true;

private:
	/** @brief オーナーからサブシステムを取得する（毎フレーム軽量取得） */
	UFishingWiredDeviceSubsystem* FindSubsystem() const;

	/** @brief センサへの弱参照（無効なら FindComponentByClass で再取得） */
	TWeakObjectPtr<UHandHeightDetectorComponent> Detector;

	/** @brief 直近フレームの生角度（キャリブレーション記録・表示用） */
	float LastRawAngleDeg = 0.0f;
	bool bHasRawAngle = false;

	/** @brief 平滑化後の HandHeightPercent（速度算出に使う前回値） */
	float SmoothedPercent = 0.5f;
	bool bHasSmoothedPercent = false;
};
