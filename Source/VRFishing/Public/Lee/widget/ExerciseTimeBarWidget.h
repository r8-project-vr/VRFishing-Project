// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExerciseTimeBarWidget.generated.h"

class UCanvasPanelSlot;
class UFishingReelStateComponent;
class UFishingStateComponentBase;
class UFishingStateHandUpDown;
class UFishingStateManagerComponent;
class UImage;
class UProgressBar;
class UTexture2D;

/**
 * @brief 運動（手上下／リール）ステートの残り時間を「空→満」の進捗バーで表示する Widget（表示専用）。
 * @details 進捗 = 1 - RemainingExerciseSeconds / GetCurrentExerciseSeconds()（左→右へ満ちる）。
 *          分子は ExerciseSecondsReader（LeeFishingTime）経由で読む（HandUpDown は public 直読み、
 *          Reel は protected のため反射）。リールは最初の 1 回転検知まで減算しないため、その間は
 *          進捗 0 のまま静止する（＝「まだ計時が始まっていない」の誠実な表現になる）。
 *          分母の GameMode 未取得時フォールバック（FallbackTotalSeconds = 20 秒）は、両ステート
 *          EnterState 内のハードコード値（20.0f）と同一源である。本人側がその値を変えたら同期必須。
 *          進捗先端を FishIconTexture の魚アイコンが滑走する（FishFightMeterWidget の PhaseFish 方式。
 *          RenderTranslation で動かし Slot Position は触らない。テクスチャ未設定なら魚は Collapsed）。
 *          表示ゲート：bDisplayOnlyInExerciseStates = true（既定）では OnFishingStateChanged を購読し、
 *          HandUpDown／Reel（ポインタ比較）以外のステートでは自 Widget を Collapsed する。
 *          運動ステートへ入るたび進捗と魚位置を 0 へスナップし直す。
 * @note ⚠️ ProgressBar_Exercise は常時 Visible で使うこと（Hidden/Collapsed にすると描画経路から外れて
 *       GetCachedGeometry が更新されず、魚アイコンの位置計算が破綻する——FishFightMeter の現在ドットと
 *       同一の制約）。自 Widget 全体を Collapsed する分には魚も一緒に隠れるため問題ない。
 *       Image_Fish は ProgressBar と同じ CanvasPanel 直下・初期位置 (0,0) に置くこと。
 */
UCLASS(BlueprintType, Blueprintable)
class VRFISHING_API UExerciseTimeBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UExerciseTimeBarWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// ==================== 貼り付けスロット（名前一致が必須） ====================

	/** @brief 進捗バー本体。Percent は C++ が毎フレーム SetPercent する（Fill の色／スタイルは WBP 側で設定） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar_Exercise = nullptr;

	/** @brief 進捗先端を滑走する魚アイコン。ProgressBar と同じ CanvasPanel 直下・初期位置 (0,0) に置くこと */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Fish = nullptr;

	// ==================== 表示設定 ====================

	/** @brief 魚アイコンのテクスチャ。未設定（既定）なら魚アイコンを非表示にしてバーのみ表示する */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExerciseBar|Fish")
	TObjectPtr<UTexture2D> FishIconTexture = nullptr;

	/** @brief 魚アイコンの表示基準サイズ（px）。高さ基準で適用し、幅はテクスチャのアスペクト比から自動算出する */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExerciseBar|Fish", meta = (ClampMin = "8.0"))
	float FishIconSize = 28.0f;

	/** @brief 魚アイコンが目標位置へ追従する補間速度（大きいほど速く滑る） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExerciseBar|Fish", meta = (ClampMin = "0.1"))
	float FishInterpSpeed = 10.0f;

	/** @brief 運動ステート（手上下／リール）中のみ表示する。false＝常時表示（レイアウト調整用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ExerciseBar|Phase")
	bool bDisplayOnlyInExerciseStates = true;

	// ==================== 出力（読み取り専用） ====================

	/** @brief 表示中の進捗（0=運動開始直後 〜 1=時間満了） */
	UPROPERTY(BlueprintReadOnly, Category = "ExerciseBar|State")
	float DisplayedProgress = 0.0f;

	/** @brief 現在の残り運動時間（秒。読み取り失敗中は前回値を保持） */
	UPROPERTY(BlueprintReadOnly, Category = "ExerciseBar|State")
	float DisplayedRemainingSeconds = 0.0f;

	/** @brief 現在レベルの運動時間（分母。GameMode 未取得時は FallbackTotalSeconds） */
	UPROPERTY(BlueprintReadOnly, Category = "ExerciseBar|State")
	float CurrentTotalSeconds = 0.0f;

	/** @brief 運動ステート中か（表示ゲートの現在値） */
	UPROPERTY(BlueprintReadOnly, Category = "ExerciseBar|State")
	bool bExerciseActive = false;

private:
	/** @brief StateManager::OnFishingStateChanged の受信（表示ゲート更新と進捗リセット） */
	UFUNCTION()
	void HandleFishingStateChanged(UFishingStateComponentBase* NewState);

	/** @brief オーナー Pawn から HandUpDown／Reel／StateManager を検索して購読する（未就位なら Tick で再試行） */
	void TryInitializeComponents();

	/** @brief 分母（GetCurrentExerciseSeconds）を 0.25 秒間隔で再取得する（GameMode 未取得時はフォールバック値） */
	void RefreshTotalSeconds();

	/** @brief 表示ゲート（bExerciseActive）を自 Widget の Visibility へ適用する */
	void ApplyVisibility();

	/** @brief 魚アイコンを進捗先端へ補間移動する（NativeTick から毎フレーム呼ぶ） */
	void UpdateFishPosition(float DeltaTime);

	// ==================== 参照 ====================

	/** @brief 手上下ステート（残り時間は public フィールドのため直読み） */
	UPROPERTY()
	TObjectPtr<UFishingStateHandUpDown> HandUpDownState;

	/** @brief リールステート（進捗データ源。フィールドは protected のため Reader 経由） */
	UPROPERTY()
	TObjectPtr<UFishingReelStateComponent> ReelState;

	/** @brief OnFishingStateChanged の購読元（表示ゲート） */
	UPROPERTY()
	TObjectPtr<UFishingStateManagerComponent> StateManager;

	/** @brief 進捗の読み取り元になっているアクティブ状態（HandUpDown or Reel。非アクティブ時 null） */
	UPROPERTY()
	TObjectPtr<UFishingStateComponentBase> ActiveState;

	// ==================== 内部状態 ====================

	/** @brief コンポーネント初期化済みか（世界空間 WidgetComponent では Pawn が遅れて就位することがある） */
	bool bComponentsInitialized = false;

	/** @brief 魚位置の初回スナップ用（運動ステートへ入るたび false へ戻す） */
	bool bFishPosInitialized = false;

	/** @brief ProgressBar_Exercise 未束縛の警告を出力済みか（多重度の警告を防ぐ） */
	bool bWarnedMissingBar = false;

	/** @brief 魚アイコンの現在位置／目標位置（自 Widget ローカル座標・RenderTranslation 用） */
	FVector2D FishCurrentPos = FVector2D::ZeroVector;
	FVector2D FishTargetPos = FVector2D::ZeroVector;

	/** 分母の再取得間隔（秒）。ExerciseLevel はセット成功ごとに上がるため 1 回きりの読み取りは禁止 */
	float TotalRefreshInterval = 0.25f;
	/** 前回の分母再取得時刻（FPlatformTime::Seconds() 基準。初期値は実質「未実行」） */
	double LastTotalRefreshTime = -1.0e9;

	/** @brief GameMode 未取得時の分母フォールバック（両ステート EnterState のハードコード 20 秒と同一源） */
	float FallbackTotalSeconds = 20.0f;
};
