// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "RpmGaugeWidget.generated.h"

class UFishingReelStateComponent;
class UFishingStateManagerComponent;
class UFishingStateComponentBase;
class UImage;

/**
 * @brief 満量程（目盛りの上限）の決め方。
 * @note 黄/緑/赤の 3 色帯が盤面のどこに来るかを決める唯一のスイッチ。既定は絶対目盛り。
 */
UENUM(BlueprintType)
enum class ERpmGaugeRangeMode : uint8
{
	/** 全プリセット中の最大速すぎ閾値（デバイス解決済み）× RangeHeadroom を満量程にする（絶対目盛り）。
	 *  プリセットを切り替えても目盛りが伸縮しないため、3 色帯が実際に盤面を移動する */
	FixedFullScale,

	/** 現在の速すぎ閾値 × RangeHeadroom を満量程にする（相対目盛り・旧挙動）。
	 *  速すぎ閾値が変わっても危険帯が常に最後尾を占めるため、色帯が盤面で動かない */
	FitToCurrentSafeMax,

	/** 適正区間 [Min, Max] を盤面の中央へ固定配置する（12 時方向を中心に SafeWindowShareRatio の割合。既定）。
	 *  遅すぎ／速すぎは残りの割合を均等に分けるため、3 色帯の角度は閾値に依存せず一定になる。
	 *  @note RPM→角度は区間ごとの折れ線写像:
	 *        [0, Min] → [0, Side] ／ [Min, Max] → [Side, Side+S] ／ [Max, Min+Max] → [Side+S, 1]。
	 *        これで「針が上向き＝適正／左下＝遅すぎ／右下＝速すぎ」が常に成立し、入力デバイスの
	 *        切り替わりで満量程（＝Min + Max）が変わっても色帯の角度は動かない（ラベルと針は動く）。
	 *        このモードでは RangeHeadroom と切り上げ（bRoundRangeMaxToNiceValue）は適用しない。 */
	FitToSafeWindow
};

/**
 * @brief リール RPM を速度計風の針式ゲージで表示する Widget（表示専用）。
 * @note ゲームプレイ判定には一切関与しない。ReelState の OnRPMCalculated（1 回転ごとに発火）を
 *       購読し、判定閾値（MinAllowedRPM / WheelMaxAllowedRPM / StickMaxAllowedRPM）は
 *       FishFightMeterWidget と同一の共通ユーティリティで反射読み取りするため、判定表示の基準が一致する。
 *
 *       描画は OnPaint による自前描画（目盛り・安全/危険弧・針）が基本。子 Widget として
 *       Image_DialFace / Image_Needle（BindWidgetOptional）を置いた場合は貼り付け側を優先する。
 *       ・Image_DialFace が非 null → 目盛り・弧・自前針をすべて描かない（子 Image は親の OnPaint より
 *         後に描かれるため、文字盤テクスチャ＝全面差し替えになる）
 *       ・Image_Needle が非 null → 自前線分針の代わりに SetRenderTransformAngle で回転表示する
 *
 *       針は FInterpTo で平滑追従し、リールフェーズ中は最後に受け取った RPM を保持する
 *       （RPM は 1 回転ごとの更新のため、無入力で 0 に戻すと低速回転時に針が往復してしまう）。
 *       リールフェーズを抜けたときだけ 0 へ緩やかに戻す（セットの区切りとして）。
 *
 *       盤面の 3 色帯（遅すぎ [0, SafeMin] / 適正 [SafeMin, SafeMax] / 速すぎ [SafeMax, 満量程]）は、
 *       反射読み取りした閾値と満量程から毎回算出する。満量程の決め方は RangeMode が持つ:
 *       ・FitToSafeWindow（既定）＝ 適正区間を盤面中央（12 時方向を中心に SafeWindowShareRatio の割合。
 *         既定 1/3）へ固定配置する。RPM→角度は 3 区間の折れ線写像のため、入力デバイスの切り替わりで
 *         満量程（＝Min + Max）が変わっても 3 色帯の角度は一定＝「針が上向き＝適正／左下＝遅すぎ／
 *         右下＝速すぎ」が崩れない（変わるのは目盛りラベルと針の位置だけ）
 *       ・FixedFullScale ＝ 全プリセット最大の速すぎ閾値 × RangeHeadroom（絶対目盛り。プリセットを
 *         変えると色帯が盤面を移動するが、低プリセットでは適正区間が盤面の左寄りになる）
 *       ・FitToCurrentSafeMax ＝ 現在の速すぎ閾値 × RangeHeadroom（旧挙動。危険帯が常に最後尾を占める）
 *       ⚠️ Image_DialFace を設定した場合は自前描画ごと省略されるため、3 色帯も描かれない。
 */
UCLASS(BlueprintType, Blueprintable)
class VRFISHING_API URpmGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URpmGaugeWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// ==================== 貼り付けスロット（省略可・名前一致が必須） ====================

	/**
	 * @brief 文字盤テクスチャ用スロット。非 null の場合は C++ の自前描画（目盛り・弧・自前針）をすべて省略する。
	 * @note 子 Image は親 Widget の OnPaint より後描きされるため、テクスチャ設置＝全面差し替え。
	 *       「テクスチャ底 + C++ 目盛り」の重ね描きは不可能なので注意。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauge|Slots", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_DialFace;

	/**
	 * @brief 針テクスチャ用スロット。非 null の場合は自前線分針の代わりに回転で使用する。
	 * @note テクスチャは「0°＝真上向き」で描くこと。Pivot は (0.5, 1.0)（底辺中央＝回転軸）推奨。
	 *       未設定でも C++ 側で実行時にフォールバック設定する。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gauge|Slots", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Needle;

	// ==================== 表示設定（ゲージ形状） ====================

	/** @brief 0 RPM 側の針角度（度）。0=真上・正=時計回り（例：-120=左下 8時方向） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Shape", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float GaugeStartAngleDeg = -120.0f;

	/** @brief 満タン側の針角度（度）。GaugeStartAngleDeg より大きい値（例：120=右下 4時方向） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Shape", meta = (ClampMin = "-180.0", ClampMax = "360.0"))
	float GaugeEndAngleDeg = 120.0f;

	/** @brief ゲージ中心のローカルサイズ比（0-1）。Y をやや下げると扇形が上に寄る */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D GaugeCenterRatio = FVector2D(0.5f, 0.62f);

	/** @brief ゲージ半径のローカル短辺比（min(X,Y) × この値） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Shape", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float GaugeRadiusRatio = 0.40f;

	/** @brief 表示レンジ上限 ＝ 満量程の基準 × この倍率（レンジ下限は常に 0）。基準は RangeMode が決める。
	 *  FitToSafeWindow モードでは使用しない（掛けると適正区間の中心が 12 時方向からずれるため） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Shape", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float RangeHeadroom = 1.25f;

	/** @brief 満量程の決め方。既定は「適正区間を盤面中央へ固定配置する」 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Shape")
	ERpmGaugeRangeMode RangeMode = ERpmGaugeRangeMode::FitToSafeWindow;

	/** @brief 適正区間が盤面に占める角度の割合（FitToSafeWindow モードのみ。既定 1/3）。
	 *  残りは遅すぎ／速すぎで均等に分ける。主目盛りを 7 本（6 分割）にすると 1/3・2/3 の境界が
	 *  ちょうど目盛りに乗り、その位置のラベルが閾値そのもの（Min / Max）になる */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Shape", meta = (ClampMin = "0.1", ClampMax = "0.8"))
	float SafeWindowShareRatio = 0.3333f;

	/** @brief 満量程の手動指定（RPM）。0 なら Project Settings の全プリセットから自動算出する（RangeMode が FixedFullScale のときのみ有効） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Shape", meta = (ClampMin = "0.0"))
	float FixedRangeMaxRPM = 0.0f;

	/** @brief 閾値の反射読み取りに失敗した場合のフォールバック用速すぎ閾値 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Shape", meta = (ClampMin = "1.0"))
	float FallbackSafeMaxRPM = 60.0f;

	// ==================== 表示設定（目盛り・弧） ====================

	/** @brief 自前描画を行うか。文字盤テクスチャを使う場合は false（Image_DialFace があればこの値に関係なく省略される） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial")
	bool bSelfDrawDial = true;

	/** @brief 主目盛りの本数（両端を含む）。7 本＝6 分割にすると 1/3・2/3 が目盛りに乗る */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial", meta = (ClampMin = "2", ClampMax = "21"))
	int32 MajorTickCount = 7;

	/** @brief 主目盛り間の小目盛りの本数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial", meta = (ClampMin = "0", ClampMax = "9"))
	int32 MinorTicksPerMajor = 1;

	/** @brief 主目盛りの長さ（半径比） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MajorTickLengthRatio = 0.16f;

	/** @brief 小目盛りの長さ（半径比） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float MinorTickLengthRatio = 0.08f;

	/** @brief 主目盛りの線幅（px） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial", meta = (ClampMin = "1.0"))
	float MajorTickThickness = 3.0f;

	/** @brief 小目盛りの線幅（px） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial", meta = (ClampMin = "1.0"))
	float MinorTickThickness = 1.5f;

	/** @brief 数字ラベルを描くか（数字のみで言語ルールを回避） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial")
	bool bDrawScaleLabels = true;

	/** @brief 遅すぎ区間 [0, SafeMin] の弧を描くか */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial")
	bool bDrawTooSlowZone = true;

	/** @brief 満量程を目盛り刻みが整数になる値へ切り上げるか（例：112.5 → 120）。
	 *  切り上げないと最大ラベルが「113」のような半端な数字になる。
	 *  @note RangeMode が FixedFullScale のときのみ有効（FitToSafeWindow では中心がずれるため適用しない） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial")
	bool bRoundRangeMaxToNiceValue = true;

	/** @brief 数字ラベルのフォントサイズ */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial", meta = (ClampMin = "6", ClampMax = "64"))
	int32 ScaleLabelFontSize = 12;

	/** @brief 安全/危険弧の帯幅（半径比）。帯は目盛りリングの内側に描く */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float ZoneBandRatio = 0.09f;

	/** @brief 弧の分割数（帯は折れ線近似。多いほど滑らか） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Dial", meta = (ClampMin = "2", ClampMax = "64"))
	int32 ArcSegmentCount = 24;

	// ==================== 表示設定（針の挙動） ====================

	/** @brief 針の追従速度（FInterpTo の InterpSpeed）。大きいほど速く追う */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Needle", meta = (ClampMin = "0.1"))
	float NeedleSmoothingSpeed = 8.0f;

	/** @brief リールフェーズ外へ抜けたときに 0 へ戻る速度（FInterpTo の InterpSpeed） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Needle", meta = (ClampMin = "0.1"))
	float NeedleDecaySpeed = 3.0f;

	/** @brief リールフェーズ以外では表示を更新しない（フェーズ外の RPM 入力を無視し、針は 0 へ落ちる） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Needle")
	bool bDisplayOnlyInReelPhase = true;

	/** @brief 自前針の長さ（半径比） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Needle", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float NeedleLengthRatio = 0.92f;

	/** @brief 自前針の尻尾の長さ（半径比） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Needle", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float NeedleTailRatio = 0.18f;

	/** @brief 自前針の線幅（px） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Needle", meta = (ClampMin = "1.0"))
	float NeedleThickness = 4.0f;

	// ==================== 表示設定（色） ====================

	/** @brief 目盛りの色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Color")
	FLinearColor TickColor = FLinearColor(0.85f, 0.85f, 0.85f, 0.85f);

	/** @brief 遅すぎ区間 [0, SafeMin] の弧の色（針の遅すぎ色と同系の黄） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Color")
	FLinearColor TooSlowZoneColor = FLinearColor(1.0f, 0.85f, 0.2f, 0.45f);

	/** @brief 適正区間 [SafeMin, SafeMax] の弧の色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Color")
	FLinearColor SafeZoneColor = FLinearColor(0.15f, 0.85f, 0.25f, 0.45f);

	/** @brief 速すぎ区間 [SafeMax, RangeMax] の弧の色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Color")
	FLinearColor DangerZoneColor = FLinearColor(0.95f, 0.20f, 0.20f, 0.45f);

	/** @brief 判定未確定（RPM 未受信）時の針の色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Color")
	FLinearColor NeedleColor = FLinearColor::White;

	/** @brief 数字ラベルの色 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Color")
	FLinearColor ScaleLabelColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.8f);

	/** @brief 適正判定時の針の色（見た目のみ。ReelState の判定とは独立） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Color")
	FLinearColor StateGoodColor = FLinearColor(0.2f, 0.9f, 0.3f, 1.0f);

	/** @brief 遅すぎ判定時の針の色（見た目のみ） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Color")
	FLinearColor StateTooSlowColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	/** @brief 速すぎ判定時の針の色（見た目のみ） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gauge|Color")
	FLinearColor StateTooFastColor = FLinearColor(1.0f, 0.25f, 0.25f, 1.0f);

	// ==================== 出力（読み取り専用） ====================

	/** @brief 針が今指している表示値（平滑化済み） */
	UPROPERTY(BlueprintReadOnly, Category = "Gauge|RPM")
	float DisplayedRPM = 0.0f;

	/** @brief 最新の遅すぎ閾値（反射読み取り失敗時はフォールバック値） */
	UPROPERTY(BlueprintReadOnly, Category = "Gauge|RPM")
	float SafeMinRPM = 0.0f;

	/** @brief 最新の速すぎ閾値。実判定が使用した上限を最優先（未入力時のみデバイス予測値） */
	UPROPERTY(BlueprintReadOnly, Category = "Gauge|RPM")
	float SafeMaxRPM = 0.0f;

	/** @brief 表示レンジ上限（満量程の基準 × RangeHeadroom。FixedFullScale 時は切り上げ後の値） */
	UPROPERTY(BlueprintReadOnly, Category = "Gauge|RPM")
	float DisplayRangeMaxRPM = 0.0f;

	/** @brief 満量程の算出に使った基準値（全プリセット最大の速すぎ閾値。手動指定時はその値） */
	UPROPERTY(BlueprintReadOnly, Category = "Gauge|RPM")
	float RangeBaseRPM = 0.0f;

	/** @brief 最新 RPM の判定状態（表示のみ） */
	UPROPERTY(BlueprintReadOnly, Category = "Gauge|RPM")
	EHandSpeedState GaugeState = EHandSpeedState::TooSlow;

	/** @brief リールフェーズ中か（フェーズゲートの現在値） */
	UPROPERTY(BlueprintReadOnly, Category = "Gauge|Phase")
	bool bGaugeActive = false;

	// ==================== BP イベント ====================

	/**
	 * @brief 表示値が更新された際の BP 通知（表示専用）。
	 * @note RPM は 1 回転ごとにしか届かないため発火頻度は低い。平仮名の応援メッセージ等は
	 *       BP 側で実装する（子ども向け表現ルールに従うこと）。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gauge|RPM")
	void OnGaugeRPMChanged(float InDisplayedRPM, EHandSpeedState State);

private:
	// ==================== デリゲート ハンドラ ====================

	/** @brief ReelState::OnRPMCalculated の受信（1 回転ごと） */
	UFUNCTION()
	void HandleRPMCalculated(float NewRPM);

	/** @brief StateManager::OnFishingStateChanged の受信（フェーズゲート更新） */
	UFUNCTION()
	void HandleFishingStateChanged(UFishingStateComponentBase* NewState);

	// ==================== 内部処理 ====================

	/** @brief オーナー Pawn から ReelState／StateManager を検索して購読する（Pawn が未就位なら Tick で再試行） */
	void TryInitializeComponents();

	/** @brief キャッシュ済みプロパティポインタで閾値を読み取り、SafeMin/SafeMax/レンジを更新する */
	void RefreshThresholds();

	/**
	 * @brief 満量程の基準（全プリセット中の最大速すぎ閾値・デバイス解決済み）を Project Settings から解決する。
	 * @param OutTableMaxAnyDeviceRPM 表全体の最大値（デバイス非依存・手動指定の影響を受けない）。
	 *                                実効閾値が表の想定を超えていないかの判定に使う。
	 * @return 満量程の基準値（FixedRangeMaxRPM の指定があればそれ）
	 */
	float ResolveRangeBaseRPM(float& OutTableMaxAnyDeviceRPM) const;

	/** @brief 満量程を「主目盛りの分割数で割り切れる切り良い値」へ切り上げる（数字ラベルの整数化） */
	float SnapRangeMaxToNiceValue(float RawRangeMax) const;

	/** @brief 描画キャッシュ（中心・半径・目盛り頂点・弧頂点・ラベル）を再構築する */
	void RebuildDrawCache();

	/** @brief 描画パラメータの変化検出用ハッシュ（閾値・レンジ・形状パラメータ） */
	uint32 ComputeDrawParamsHash() const;

	/** @brief RPM 値をゲージ角度（度）へ変換する */
	float RPMToAngleDeg(float RPM) const;

	/** @brief 折れ線写像（FitToSafeWindow）が成立するか。閾値未取得（0）や Min >= Max の誤設定では false */
	bool IsSafeWindowScaleValid() const;

	/** @brief 適正区間が盤面に占める角度割合（SafeWindowShareRatio をクランプした値） */
	float GetSafeWindowShare() const;

	/** @brief 遅すぎ／速すぎ区間それぞれの角度割合（残りを均等に分割した値） */
	float GetSafeWindowSide() const;

	/** @brief RPM → 盤面の正規化位置。FitToSafeWindow では区間ごとの折れ線写像、他モードでは 0 起点の線形 */
	float RPMToNormValue(float RPM) const;

	/** @brief 正規化位置 → RPM（目盛りラベルの逆写像。RPMToNormValue と対で使う） */
	float NormToRPM(float Norm) const;

	/** @brief 現在の判定状態に応じた針の色を返す（未受信時は NeedleColor） */
	FLinearColor ResolveNeedleColor() const;

	// ==================== 参照 ====================

	/** @brief RPM データと閾値の読み取り元（所有は Pawn） */
	UPROPERTY()
	TObjectPtr<UFishingReelStateComponent> ReelState;

	/** @brief フェーズゲートの取得元（OnFishingStateChanged を購読） */
	UPROPERTY()
	TObjectPtr<UFishingStateManagerComponent> StateManager;

	/** @brief コンポーネント初期化済みか（世界空間 WidgetComponent では Pawn が遅れて就位することがある） */
	bool bComponentsInitialized = false;

	// ==================== 針の状態 ====================

	/** @brief 針が向かうべき RPM（最新 RPM またはフェーズ外減衰時 0） */
	float TargetRPMForNeedle = 0.0f;

	/** @brief 減衰モード中か（フェーズ外へ抜けたときに 1 回だけ立てる） */
	bool bDecaying = false;

	/** @brief 一度でも RPM を受信したか */
	bool bHasEverReceivedRPM = false;

	/** @brief OnGaugeRPMChanged の発火制御用（前回発火時の値） */
	float LastNotifiedRPM = -1.0f;
	/** @brief OnGaugeRPMChanged の発火制御用（前回発火時の判定状態） */
	EHandSpeedState LastNotifiedState = EHandSpeedState::TooSlow;

	/** @brief Image_Needle へ最後に適用した角度（0.1° 未満の更新を抑える） */
	float LastAppliedNeedleImageAngle = 0.0f;

	// ==================== 閾値読み取り ====================

	/** @brief 閾値読み取り失敗の警告を出力済みか（読み取りは共通ユーティリティ側で毎回行うため、
	 *  ここでは警告の多重度防止だけを持つ） */
	bool bWarnedThresholdFallback = false;

	/** @brief Project Settings の表全体の最大速すぎ閾値（デバイス非依存）。実効閾値の妥当性判定用 */
	float TableMaxAnyDeviceRPM = 0.0f;

	/** 閾値を Tick で再取得する間隔（秒）。LoadApplier は Pawn BeginPlay で書き込むため
	 *  NativeConstruct のタイミングとは順序が保証されない → 一度きりの読み取りは禁止 */
	float ThresholdRefreshInterval = 0.25f;
	/** 前回の閾値再取得時刻（FPlatformTime::Seconds() 基準。初期値は実質「未実行」） */
	double LastThresholdRefreshTime = -1.0e9;

	/** 前回ログ出力時の満量程／閾値（値が動いた瞬間だけログるための多重度防止。HMD 内での確認用） */
	float LastLoggedRangeMaxRPM = -1.0f;
	float LastLoggedSafeMinRPM = -1.0f;
	float LastLoggedSafeMaxRPM = -1.0f;

	// ==================== 描画キャッシュ（Tick 側で構築 / NativePaint は const のため読むだけ） ====================

	/** @brief 主目盛りの頂点（2 点 1 組のフラット配列） */
	TArray<FVector2f> MajorTickPoints;

	/** @brief 小目盛りの頂点（2 点 1 組のフラット配列） */
	TArray<FVector2f> MinorTickPoints;

	/** @brief 遅すぎ弧（[0, SafeMin]）の折れ線頂点 */
	TArray<FVector2f> TooSlowArcPoints;

	/** @brief 安全弧の折れ線頂点 */
	TArray<FVector2f> SafeArcPoints;

	/** @brief 危険弧の折れ線頂点 */
	TArray<FVector2f> DangerArcPoints;

	/** @brief 数字ラベルの文字列（Tick 側で構築し Paint では文字列化しない） */
	TArray<FString> ScaleLabels;

	/** @brief 数字ラベルの配置座標（ローカル空間・ScaleLabels と 1:1 対応） */
	TArray<FVector2f> ScaleLabelPositions;

	/** ゲージ中心（ローカル空間。RebuildDrawCache で算出） */
	FVector2f CachedCenter = FVector2f::ZeroVector;
	/** ゲージ半径（px。RebuildDrawCache で算出） */
	float CachedRadius = 0.0f;

	/** @brief 判定区間の弧の帯の太さ（RebuildDrawCache で算出し Paint で参照） */
	float CachedBandThickness = 1.0f;

	/** @brief ラベルフォント構築済みのサイズ（ScaleLabelFontSize 変更検出用） */
	int32 CachedLabelFontSize = 0;

	/** ジオメトリ変更検出用（Widget サイズが変わったらキャッシュ再構築） */
	FVector2f CachedGeometrySize = FVector2f::ZeroVector;
	/** 描画パラメータ変更検出用（ComputeDrawParamsHash の前回値） */
	uint32 CachedDrawParamsHash = 0;
	/** キャッシュ無効フラグ（初回は必ず再構築） */
	bool bDrawCacheDirty = true;

	/** @brief ラベル用フォント（Construct で構築し Paint では参照のみ） */
	FSlateFontInfo LabelFontInfo;

	/** @brief 表示中の針角度（度） */
	float NeedleAngleDeg = 0.0f;
};
