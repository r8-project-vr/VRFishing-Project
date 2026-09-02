// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

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
	/** @brief コンストラクタ。常駐センサとして毎フレーム自律 Tick を有効化する */
	UHandHeightDetectorComponent();

	// --- UActorComponent overrides ---
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** @brief 開始処理。CameraRef が未設定ならオーナーから自動検索する */
	virtual void BeginPlay() override;

public:
	// ==================== 設定パラメータ ====================

	/** @brief 正規化 0%（下限）となる「頭より下」の距離 [cm]。カメラ Z − BottomOffset が下端 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Detection")
	float BottomOffset = 50.0f;

	/** @brief 正規化 100%（上限）となる「頭より上」の距離 [cm]。カメラ Z + TopOffset が上端 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height Detection")
	float TopOffset = 30.0f;

	/** @brief Debug表示を有効にするかどうか */
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

	/** @brief 手の高さ（0.0=下限 〜 1.0=上限）。カメラを基準に BottomOffset/TopOffset で正規化 */
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

	/**
	 * @brief 頭(カメラ)を基準にした手の下がり量(cm)。正=手が頭より下にある。
	 * @note Ready 状態などが手の「下げ判定」に再利用するための cm 単位インターフェース。
	 *       値は直近フレームのキャッシュ（CachedHeadZ/CachedHandZ）から即時取得できる。
	 */
	UFUNCTION(BlueprintCallable, Category = "Height Detection")
	float GetHandHeightBelowHeadCm() const;

	/**
	 * @brief 直近フレームの実効的な手のワールド Z 高さを取得する。
	 * @param OutHandZCm 手の Z 座標 [cm]。通常モード = HandRef の実測値、
	 *        外部データモード = 注入率から逆算した仮想 Z（BottomOffset/TopOffset 準拠）
	 * @return Tick で一度でも手 Z を計算済みなら true
	 * @note 釣り竿ビジュアライザなど「センサの実効値に追従したい」表示系が利用する。
	 *       両モードで同一規約の値を返すため、呼び出し側はデータ経路を意識しなくてよい。
	 */
	UFUNCTION(BlueprintCallable, Category = "Height Detection")
	bool GetEffectiveHandZCm(float& OutHandZCm) const;

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

	/** @brief 外部データモードを解除し、HandRef ベースの通常動作へ戻す */
	UFUNCTION(BlueprintCallable, Category = "Height Detection|External")
	void ClearExternalHandData();

	/** @brief 現在外部データモードかどうか */
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

	/** @brief 直近フレームの頭(カメラ)Z座標（GetHandHeightBelowHeadCm 用にキャッシュ） */
	float CachedHeadZ = 0.0f;

	/** @brief 直近フレームの手のZ座標（GetHandHeightBelowHeadCm 用にキャッシュ） */
	float CachedHandZ = 0.0f;

	/** @brief CachedHandZ を Tick で一度でも計算済みか（GetEffectiveHandZCm の有効判定） */
	bool bHasCachedHandZ = false;

	// ==================== 外部データソース用 内部状態 ====================

	/** @brief 外部データモードフラグ（true 時は HandRef の代わりに External* 値を使用） */
	bool bUseExternalData = false;

	/** @brief 外部から注入された手の高さ（0.0〜1.0） */
	float ExternalHeightPercent = 0.0f;

	/** @brief 外部から注入された手の速度（cm/s） */
	float ExternalSpeed = 0.0f;
};
