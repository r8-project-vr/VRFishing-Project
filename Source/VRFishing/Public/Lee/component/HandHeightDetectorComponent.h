// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HandHeightDetectorComponent.generated.h"

class UCameraComponent;
class USceneComponent;

/** 手の移動速度の判定状態 */
UENUM(BlueprintType)
enum class EHandSpeedState : uint8
{
	Good		UMETA(DisplayName = "適正"),
	TooSlow		UMETA(DisplayName = "遅すぎ"),
	TooFast		UMETA(DisplayName = "速すぎ")
};

/**
 * @brief 手部運動センササービス（常駐 Tick）。
 * @note かつてはステートマシンの1状態だったが、感知機能をステートから分離して常駐化。
 *       上下運動のカウント等のプレイロジックは UFishingStateHandUpDown へ移管。
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VRFISHING_API UHandHeightDetectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UHandHeightDetectorComponent();

	// --- UActorComponent overrides ---
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// ==================== 設定パラメータ ====================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Detection")
	float BottomOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Detection")
	float TopOffset = 30.0f;

	/// @brief Debug表示を有効にするかどうか
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Detection|Debug")
	bool bShowDebug = true;

	// ==================== 移動速度 設定 ====================

	/**
	 * @brief 適正と判定する最小速度 (cm/s).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Detection")
	float MinGoodSpeed = 5.0f;

	/** 適正と判定する最大速度 (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Detection")
	float MaxGoodSpeed = 30.0f;

	// ==================== 出力変数 ====================

	UPROPERTY(BlueprintReadOnly, Category = "Height Detection")
	float HandHeightPercent = 0.0f;

	/** 手の現在の移動速度 (cm/s) */
	UPROPERTY(BlueprintReadOnly, Category = "Speed Detection")
	float CurrentHandSpeed = 0.0f;

	/**
	 * @brief 手の垂直移動速度（符号付き・正規化空間 1.0/s、正=上昇）。
	 * @note HandHeightPercent の毎フレーム差分から算出。速度スコアリングのデータ源。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Speed Detection")
	float HandPercentSpeed = 0.0f;

	/** 手の移動速度の判定状態 */
	UPROPERTY(BlueprintReadOnly, Category = "Speed Detection")
	EHandSpeedState HandSpeedState = EHandSpeedState::Good;

	// ==================== コンポーネント参照 ====================

	UPROPERTY(BlueprintReadWrite, BlueprintReadWrite, Category = "Height Detection|References")
	TWeakObjectPtr<UCameraComponent> CameraRef;

	UPROPERTY(BlueprintReadWrite, BlueprintReadWrite, Category = "Height Detection|References")
	TWeakObjectPtr<USceneComponent> HandRef;

	// ==================== 公開API ====================

	/// @brief 頭(カメラ)を基準にした手の下がり量(cm)。正=手が頭より下にある。
	/// @note Wait状態などが手の「下げ判定」に再利用するための cm 语义インターフェース。
	UFUNCTION(BlueprintCallable, Category = "Height Detection")
	float GetHandHeightBelowHeadCm() const;

	// ==================== 外部データソース用 I/F ====================

	/**
	 * @brief 外部デバイス（BLE IMU等）のデータで HandRef を上書きする。
	 * @param InHeightPercent 正規化された手の高さ（0.0=下, 1.0=上）
	 * @param InSpeed 手の移動速度(cm/s)。-1.0f で速度計算をスキップ
	 * @note 呼び出し後は毎フレームこの値を出力に使用する。
	 *       ClearExternalHandData() で解除し HandRef ベースへ戻る。
	 */
	UFUNCTION(BlueprintCallable, Category = "Height Detection|External")
	void SetExternalHandData(float InHeightPercent, float InSpeed = -1.0f);

	/// @brief 外部データモードを解除し、HandRef ベースの通常動作へ戻す
	UFUNCTION(BlueprintCallable, Category = "Height Detection|External")
	void ClearExternalHandData();

	/// @brief 現在外部データモードかどうか
	UFUNCTION(BlueprintCallable, Category = "Height Detection|External")
	bool IsUsingExternalData() const { return bUseExternalData; }

private:
	/** 前フレームの手のワールド位置（速度計算用） */
	FVector PreviousHandLocation = FVector::ZeroVector;

	/** PreviousHandLocation が有効か（最初のフレームは無効） */
	bool bHasPreviousLocation = false;

	/** 前フレームの HandHeightPercent（正規化垂直速度計算用） */
	float PreviousHandPercent = 0.0f;

	/** PreviousHandPercent が有効か（最初のフレームは無効） */
	bool bHasPreviousPercent = false;

	/// @brief 直近フレームの頭(カメラ)Z座標（GetHandHeightBelowHeadCm 用にキャッシュ）
	float CachedHeadZ = 0.0f;

	/// @brief 直近フレームの手のZ座標（GetHandHeightBelowHeadCm 用にキャッシュ）
	float CachedHandZ = 0.0f;

	// ==================== 外部データソース用 内部状態 ====================

	/** @brief 外部データモードフラグ（true 時は HandRef の代わりに External* 値を使用） */
	bool bUseExternalData = false;

	/** @brief 外部から注入された手の高さ（0.0〜1.0） */
	float ExternalHeightPercent = 0.0f;

	/** @brief 外部から注入された手の速度（cm/s） */
	float ExternalSpeed = 0.0f;
};
