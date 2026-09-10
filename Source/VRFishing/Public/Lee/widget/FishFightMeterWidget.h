// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "Styling/SlateBrush.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "Lee/component/FishingStateHandUpDown.h"
#include "FishFightMeterWidget.generated.h"

class UImage;
class UTexture2D;
class UFishingReelStateComponent;
class UFishingStateManagerComponent;
class UFishingStateComponentBase;
class UFishingReadyStateComponent;
class UFishingCatchingStateComponent;
class UFishingResultStateComponent;

/**
 * @brief 釣りの進行フェーズ（ステップバー表示用）を定義する列挙型。
 * @note 値の順序＝ステップバーの表示順（左→右）。各ステートコンポーネントと 1:1 対応。
 *       DisplayName は子ども向けのひらがな励まし表現（2026.08.26 Lee、GetStateDisplayName() と同期）。
 */
UENUM(BlueprintType)
enum class EFishingPhase : uint8
{
	Ready      UMETA(DisplayName = "よーい！"),
	HandUpDown UMETA(DisplayName = "うでをあげさげ！"),
	Reel       UMETA(DisplayName = "ぐるぐるまわして！"),
	Catching   UMETA(DisplayName = "うんとひっぱって！"),
	Result     UMETA(DisplayName = "つれたかな？")
};

/**
 * @brief 釣りアトラクト／リールフェーズの表示専用 Widget。
 * @note ゲームプレイロジック（矢印ガイド・スコアリング）は FishingStateHandUpDown が管理し、
 *       本 Widget は表示データの読み取りと BP イベント発火のみを行う。
 */
UCLASS(BlueprintType, Blueprintable)
class VRFISHING_API UFishFightMeterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFishFightMeterWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// ==================== 表示設定（RPM） ====================

	/** 表示用の目標 RPM。実行時は ReelState の判定閾値から算出された値で上書きされる（閾値が読み取れない場合のみこの設定値を使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|RPM")
	float TargetRPM = 30.0f;

	/** 表示用の許容誤差。実行時は ReelState の判定閾値から算出された値で上書きされる（閾値が読み取れない場合のみこの設定値を使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|RPM")
	float RPMTolerance = 10.0f;

	// 2026.09.07 Lee startーーー フェーズ連動表示切替 ーーー

	/** @brief ステップバーを表示してから自動隠蔽するまでの秒数（0 以下＝自動隠蔽せず常時表示。レイアウト調整用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase", meta = (ClampMin = "0.0"))
	float StepBarDisplaySeconds = 5.0f;

	// 2026.09.07 Lee endーーー

	// 2026.09.09 Lee startーーー ステップバー 3 状態表示＋魚アイコン指示器 ーーー

	/** @brief 現在フェーズ位置を示す魚アイコンのテクスチャ。未設定時は魚アイコンを非表示にし、現在ドットを完了色のまま代替表示する（WBP の Class Defaults で割り当てる） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase")
	TObjectPtr<UTexture2D> PhaseFishTexture = nullptr;

	/** @brief 魚アイコンの表示基準サイズ（px）。高さ基準で適用し、幅はテクスチャのアスペクト比から自動算出する */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase", meta = (ClampMin = "8.0"))
	float PhaseFishSize = 36.0f;

	/** @brief 魚アイコンが目標位置へ追従する補間速度（大きいほど速く滑る） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase", meta = (ClampMin = "0.1"))
	float PhaseFishInterpSpeed = 10.0f;

	/** @brief 完了ドットの色（デザイナー設定ブラシ＝四角にこの色を重ねて表示する） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase")
	FLinearColor StepCompletedColor = FLinearColor::Green;

	/** @brief 未到達ドットの色（小円で表示） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase")
	FLinearColor StepUnreachedColor = FLinearColor::Gray;

	/** @brief 到達済み接続線（完了〜現在を結ぶ線を含む）の色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase")
	FLinearColor StepLineActiveColor = FLinearColor::Green;

	/** @brief 未到達接続線の色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase")
	FLinearColor StepLineInactiveColor = FLinearColor(0.25f, 0.25f, 0.25f, 1.0f);

	/** @brief 未到達ドットのブラシ（形状のみ使用。既定＝円形。色は StepUnreachedColor で上書き表示する） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase")
	FSlateBrush UnreachedDotBrush;

	/** @brief 未到達ドットの縮小率（1.0 で完了ドットと同寸法。RenderScale の原点はコントロール中心） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Phase", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float UnreachedDotScale = 0.5f;

	// ---- WBP ウィジェット束縛（名前一致必須。無い場合は該当機能を静かに無効化する） ----

	/** @brief ステップドット 1〜4（左から よーい/うで/リール/つりあげ。Result は表示しない） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Dot_1 = nullptr;

	/** @brief ステップドット 2（手上下） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Dot_2 = nullptr;

	/** @brief ステップドット 3（リール） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Dot_3 = nullptr;

	/** @brief ステップドット 4（釣り上げ） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Dot_4 = nullptr;

	/** @brief 接続線 1〜3（Dot i と Dot i+1 の間。到達済みなら StepLineActiveColor） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Line_1 = nullptr;

	/** @brief 接続線 2（Dot_2〜Dot_3 間） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Line_2 = nullptr;

	/** @brief 接続線 3（Dot_3〜Dot_4 間） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Line_3 = nullptr;

	/** @brief 現在フェーズを示す魚アイコン（CanvasPanel_24 直下・PhasePanel より手前に配置） */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_PhaseFish = nullptr;

	// 2026.09.09 Lee endーーー

	// 2026.09.07 Lee startーーー 推奨範囲表示 ーーー

	/** @brief 手が推奨範囲内のときの ProgressBar フィル色（通常色。WBP の既定フィル色＝青 (0, 0.5, 1) と同一値で復帰させる） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Arrow")
	FLinearColor HandRangeNormalColor = FLinearColor(0.0f, 0.5f, 1.0f, 1.0f);

	/** @brief 手が推奨範囲外のときの ProgressBar フィル色（警告色） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meter|Arrow")
	FLinearColor HandRangeWarningColor = FLinearColor::Red;

	// 2026.09.07 Lee endーーー

	// ==================== 出力（読み取り専用） ====================

	/** @brief 矢印ガイドの縦位置（0.0=下端 〜 1.0=上端。センサの HandHeightPercent と同じ刻み） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	float ArrowPosition = 0.0f;

	/** @brief 矢印の現在状態（上昇中/下降中/リール解放など） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	EFishArrowState ArrowState = EFishArrowState::MovingUp;

	/** @brief 完了した上下往復回数（TargetUpAndDownCount までの進捗表示用） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	int32 CycleCount = 0;

	/** @brief 上下運動が完了しリールフェーズへ移行済みか */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Arrow")
	bool bReelUnlocked = false;

	/** @brief 現在のリール回転速度（RPM） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|RPM")
	float CurrentRPM = 0.0f;

	/** @brief 現在 RPM の判定結果（遅すぎ/適速/速すぎ） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|RPM")
	EHandSpeedState RPMState = EHandSpeedState::Good;

	/** @brief 現在のスコア（リールフェーズ中のフレーム平均） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Scoring")
	float CurrentScore = 0.0f;

	/**
	 * @brief 全回数終了後の総合得点（全フレーム平均 × 100）
	 * @note FishingStateHandUpDown で計算され、本 Widget は表示のみ。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Scoring")
	float FinalScore = 0.0f;

	// ==================== BP イベント ====================

	/** @brief 矢印ガイド更新時に発火（矢印 Image の位置・状態を BP 側で反映する） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Arrow")
	void OnArrowUpdated(float Position, EFishArrowState State);

	// 2026.09.07 Lee startーーー 推奨範囲表示 ーーー

	/**
	 * @brief 矢印周辺の推奨範囲（評点閾値と同源）更新時に発火。各値は 0.0～1.0 の軌道スケールでクランプ済み。
	 * @param GoodBottom    有得帯（減点なし側の限界）の下端
	 * @param GoodTop       有得帯の上端
	 * @param PerfectBottom 満点帯の下端
	 * @param PerfectTop    満点帯の上端
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Arrow")
	void OnArrowRangeUpdated(float GoodBottom, float GoodTop, float PerfectBottom, float PerfectTop);

	/** @brief 手が推奨範囲を出たり戻ったりした時に発火。ProgressBar のフィル色を BP 側で反映する */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Arrow")
	void OnHandRangeColorChanged(FLinearColor NewColor);

	// 2026.09.07 Lee endーーー

	/** @brief RPM 更新時に発火（数値表示と速度判定色を BP 側で反映する） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|RPM")
	void OnRPMChanged(float RPM, EHandSpeedState State);

	/** @brief スコア更新時に発火 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Scoring")
	void OnScoreChanged(float Score);

	// ==================== 出力（フェーズ／ステップバー） ====================

	/** @brief 現在の釣りフェーズ（ステップバーの強調位置） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	EFishingPhase CurrentPhase = EFishingPhase::Ready;

	/** @brief 現在フェーズのインデックス（0～4）。ステップバー描画の便宜用 */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	int32 CurrentPhaseIndex = 0;

	/** @brief 現在フェーズの表示名（GetStateDisplayName() の戻り値をそのまま表示） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	FString CurrentPhaseName;

	/** @brief 直前の遷移で中間フェーズを飛ばしたか（例：リール失敗→釣り上げ結果） */
	UPROPERTY(BlueprintReadOnly, Category = "Meter|Phase")
	bool bPhaseSkipped = false;

	/**
	 * @brief フェーズ変化時に発火する BP イベント。
	 * @param NewPhase  新しいフェーズ
	 * @param PhaseName 新フェーズの表示名
	 * @param bSkipped  遷移で中間フェーズを飛ばした場合 true
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Phase")
	void OnPhaseChanged(EFishingPhase NewPhase, const FString& PhaseName, bool bSkipped);

	// 2026.09.07 Lee startーーー フェーズ連動表示切替イベント ーーー

	/** @brief 矢印ガイド（手上下フェーズ専用の UI 群）の表示切替時に発火。BP 側で SetVisibility を行う */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Phase")
	void OnArrowGuideVisibilityChanged(bool bVisible);

	/** @brief RPM ゲージ一式（リールフェーズ専用の UI 群）の表示切替時に発火。BP 側で SetVisibility を行う */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Phase")
	void OnRpmGaugeVisibilityChanged(bool bVisible);

	/** @brief ステップバーの表示切替時に発火（切替直後に true、StepBarDisplaySeconds 経過後に false） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Meter|Phase")
	void OnStepBarVisibilityChanged(bool bVisible);

	// 2026.09.07 Lee endーーー

private:
	/** @brief ReelState の OnRPMCalculated 受信ハンドラ（RPM 表示の更新） */
	UFUNCTION()
	void OnRPMUpdated(float NewRPM);

	/** @brief HandUpDown 完了受信ハンドラ（bReelUnlocked の更新） */
	UFUNCTION()
	void OnHandUpDownCompleted(bool bIsSuccess);

	/** @brief StateManager の状態変更通知ハンドラ（OnFishingStateChanged 受信） */
	UFUNCTION()
	void HandleFishingStateChanged(UFishingStateComponentBase* NewState);

	/** @brief ステートコンポーネントを対応フェーズへ変換（ポインタ比較） */
	EFishingPhase ResolvePhase(const UFishingStateComponentBase* State) const;

	/** @brief フェーズを適用して BP イベントを発火（イベント駆動と初回同期で共用） */
	void ApplyPhase(EFishingPhase NewPhase, const FString& PhaseName, bool bSkipped);

	// 2026.09.07 Lee startーーー フェーズ連動表示切替 ーーー

	/** @brief フェーズから導出した表示状態を BP へ通知する（変化時のみ発火。初回適用は強制発火） */
	void ApplyPhaseVisibility(bool bNewArrowGuideVisible, bool bNewRpmGaugeVisible);

	/** @brief ステップバー自動隠蔽タイマーのコールバック（表示中なら非表示へ切替通知） */
	UFUNCTION()
	void HideStepBar();

	/** @brief ステップバー自動隠蔽タイマーのハンドル */
	FTimerHandle StepBarHideTimerHandle;

	/** @brief 前回 BP へ通知した矢印ガイド表示状態（変化検出用キャッシュ） */
	bool bArrowGuideVisible = false;

	/** @brief 前回 BP へ通知した RPM ゲージ表示状態（変化検出用キャッシュ） */
	bool bRpmGaugeVisible = false;

	/** @brief 前回 BP へ通知したステップバー表示状態（変化検出用キャッシュ） */
	bool bStepBarVisible = false;

	/** @brief フェーズ由来可視性の初回適用済みフラグ（途中生成でもデザイナー既定値に依存せず強制同期する） */
	bool bPhaseVisibilityApplied = false;

	// 2026.09.07 Lee endーーー

	// 2026.09.07 Lee startーーー 推奨範囲表示 ーーー

	/** @brief 矢印周辺の推奨範囲（評点閾値と同源）を算出して BP へ通知する */
	void PushArrowRange();

	/** @brief 手の帯内外判定を更新してフィル色を通知する（変化時のみ。bForce=true で強制通知） */
	void UpdateHandRangeColor(bool bForce);

	/** @brief 前回の帯内外判定キャッシュ（変化検出用） */
	bool bHandInRange = true;

	// 2026.09.07 Lee endーーー

	// 2026.09.09 Lee startーーー ステップバー 3 状態表示＋魚アイコン指示器 ーーー

	/**
	 * @brief 現在フェーズに応じて全ドット／接続線／魚アイコンへ 3 状態（完了・現在・未到達）を適用する
	 * @note 表示は 4 段構成（Result は最終段に丸め込み、ステップバーには出さない）。
	 *       現在ドットは魚アイコンと置き換わるため完全透明で表示する（Hidden にすると
	 *       描画経路から外れて GetCachedGeometry が更新されなくなるため）
	 */
	void UpdateStepBarVisuals();

	/**
	 * @brief 魚アイコンを現在ドットの中心へ補間移動する（NativeTick から毎フレーム呼ぶ）
	 * @note ドット中心は GetCachedGeometry の実寸から毎フレーム再計算する（デザイナー座標の
	 *       定数は持たない。推奨範囲帯と同じ方式）。初回のみスナップし、以降は補間で滑らかに追従
	 */
	void UpdateStepFishPosition(float DeltaTime);

	/** @brief 初回適用時に退避したドットのデザイナー設定ブラシ（未到達态で差し替えた後、完了态で復元するため） */
	FSlateBrush OriginalDotBrushes[4];

	/** @brief デザイナー設定ブラシの退避済みフラグ */
	bool bOriginalBrushCaptured = false;

	/** @brief 魚アイコンの現在描画位置（RenderTranslation。補間状態の保持用） */
	FVector2D StepFishCurrentPos = FVector2D::ZeroVector;

	/** @brief 魚アイコンの目標位置（現在ドット中心の左上基準位置） */
	FVector2D StepFishTargetPos = FVector2D::ZeroVector;

	/** @brief 魚アイコン位置の初回確定済みフラグ（初回は補間せずスナップする） */
	bool bStepFishPosInitialized = false;

	// 2026.09.09 Lee endーーー

	/** @brief 常駐センサ（HandHeightPercent 表示用。所有は Pawn、Widget は参照のみ） */
	UPROPERTY()
	TObjectPtr<UHandHeightDetectorComponent> HandHeightDetector;

	/** @brief 上下運動ステート（矢印・スコア表示データの読み取り元） */
	UPROPERTY()
	TObjectPtr<UFishingStateHandUpDown> HandUpDownState;

	/** @brief リールステート（RPM 表示データと閾値の読み取り元） */
	UPROPERTY()
	TObjectPtr<UFishingReelStateComponent> ReelSimulator;

	/** @brief ステートマネージャ（購読と現在ステート取得用） */
	UPROPERTY()
	TObjectPtr<UFishingStateManagerComponent> StateManager;

	/** @brief 各フェーズのステートコンポーネント参照（HandUpDown／Reel は既存メンバを流用） */
	UPROPERTY()
	TObjectPtr<UFishingReadyStateComponent> ReadyState;

	/** @brief 釣り上げステート（ResolvePhase のポインタ比較用） */
	UPROPERTY()
	TObjectPtr<UFishingCatchingStateComponent> CatchingState;

	/** @brief 結果ステート（ResolvePhase のポインタ比較用） */
	UPROPERTY()
	TObjectPtr<UFishingResultStateComponent> ResultState;

	/** @brief コンポーネント参照の解決済みフラグ（見つかるまで Tick で再試行する） */
	bool bComponentsInitialized = false;

	/** @brief 一つ前のフェーズ（スキップ判定用） */
	EFishingPhase PreviousPhase = EFishingPhase::Ready;
};
