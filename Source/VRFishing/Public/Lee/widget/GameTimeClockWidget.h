// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameTimeClockWidget.generated.h"

class AFishingGameModeBase;
class UImage;
class UTextBlock;

/**
 * @brief ゲーム全体の残り制限時間をリング型カウントダウン（円弧ゲージ）で表示する Widget（表示専用）。
 * @details 緑の円弧の長さ＝残り時間の割合。12 時方向（RingStartAngleDeg）から時計回りに時間の経過と
 *          ともに短くなっていく。下には半透明のトラック円（満円の軌跡）を敷く。ポインタ・目盛り・
 *          外周ラベルは持たない（2026.09.11 の針式時計から円環方式へ改稿）。
 *          中心の残り分数は Text_Remaining（省略可スロット）へ毎フレーム反映する（フォントや
 *          配置は WBP 側で調整する。未束縛なら数字なしで円弧のみ）。残分数は RoundToInt(残り秒/60)。
 *          データ源は AFishingGameModeBase の公開プロパティのみ。満量程は CurrentGameTime + RemainingTime
 *          として毎回算出する（GetRemainingTime が Max(0) でクランプするため時間切れ時も等式が成立。
 *          キャッシュ不要＝LoadApplier が Pawn BeginPlay で TotalGameTime を書き替える順序問題とも無縁）。
 *          計時対象ステート（手上下／リール）以外では RemainingTime が止まるため弧も自然に停止する。
 *          制限時間切れ（GameMode の bIsTimeUp）でも最終セットの完了までは続行するため、その期間は
 *          ロスタイム表示になる：弧は満円の黄（LossTimeColor）になり、中心は数字の代わりに
 *          LossTimeText（既定「ロスタイム」）を表示する。
 *          GameMode の無いレベル（タイトル等）では警告を出さず現在の表示を保持する。
 *          Image_DialFace を設定した場合は自前描画をすべて省略する（全面差し替え。RpmGauge と同一規則）。
 * @note 時間の再取得は 0.25 秒間隔（LoadApplier の BeginPlay 書き込み順序に依存しない設計）。
 *       動的デリゲートを一切購読しないため NativeDestruct は不要。前景弧の頂点は長さが毎フレーム
 *       変わるため Tick 側で再構築し（UpdateForegroundArc）、描画キャッシュ（トラック円）のみ
 *       ハッシュ比較で再構築する。
 */
UCLASS(BlueprintType, Blueprintable)
class VRFISHING_API UGameTimeClockWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UGameTimeClockWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// ==================== 貼り付けスロット（省略可・名前一致が必須） ====================

	/** @brief 文字盤テクスチャ用スロット。非 null の場合は自前描画（トラック円・残り時間弧）をすべて省略する */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clock|Slots", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_DialFace;

	/** @brief 残り時間を「M:SS」形式で表示するテキスト（円環の中心に置くこと）。C++ は SetText のみ行い、
	 *  フォント・色・位置は WBP 側で調整する。未束縛でもエラーにはならない */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Remaining = nullptr;

	// ==================== 表示設定（リングの形状） ====================

	/** @brief リング中心のローカルサイズ比（0-1）。時計は円盤のため中央既定 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Ring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D GaugeCenterRatio = FVector2D(0.5f, 0.5f);

	/** @brief リング半径のローカル短辺比（min(X,Y) × この値） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Ring", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float GaugeRadiusRatio = 0.42f;

	/** @brief リング帯の太さ（半径比） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Ring", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float RingThicknessRatio = 0.12f;

	/** @brief 残り満タン側の円弧の起点角度（度）。0=真上・正=時計回り。既定 0＝12 時方向から時計回りに消退する */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Ring", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float RingStartAngleDeg = 0.0f;

	/** @brief 円弧の分割数（折れ線近似。全円 360°を描くため多いほど滑らか） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Ring", meta = (ClampMin = "2", ClampMax = "128"))
	int32 ArcSegmentCount = 64;

	// ==================== 表示設定（色・トラック円） ====================

	/** @brief 自前描画を行うか。文字盤テクスチャを使う場合は false（Image_DialFace があればこの値に関係なく省略される） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Style")
	bool bSelfDrawDial = true;

	/** @brief 残り時間の円弧の色（既定＝緑） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Style")
	FLinearColor RingColor = FLinearColor(0.2f, 0.85f, 0.3f, 1.0f);

	/** @brief ロスタイム中の円弧の色（既定＝黄。ロスタイム中は弧を満円で表示する） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Style")
	FLinearColor LossTimeColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	/** @brief ロスタイム中に中心へ表示するテキスト（数字の代わりに表示する） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Style")
	FText LossTimeText = FText::FromString(TEXT("ロスタイム"));

	/** @brief 満円の軌跡として下に敷くトラック円を描くか */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Style")
	bool bDrawTrack = true;

	/** @brief トラック円の色（半透明の暗色推奨） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Style")
	FLinearColor TrackColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.15f);

	// ==================== 表示設定（弧の動き） ====================

	/** @brief 弧の長さの追従速度（FInterpTo の InterpSpeed）。大きいほど速く追う */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock|Motion", meta = (ClampMin = "0.1"))
	float ArcSmoothingSpeed = 6.0f;

	// ==================== 出力（読み取り専用） ====================

	/** @brief 弧が今表している残り時間（秒・平滑化済み） */
	UPROPERTY(BlueprintReadOnly, Category = "Clock|Time")
	float DisplayedRemainingSeconds = 0.0f;

	/** @brief 現在の満量程（CurrentGameTime + RemainingTime。GameMode 未取得時は 0） */
	UPROPERTY(BlueprintReadOnly, Category = "Clock|Time")
	float DisplayFullScaleSeconds = 0.0f;

	/** @brief GameMode を解決済みか（タイトル等の GameMode 無しレベルでは false のまま） */
	UPROPERTY(BlueprintReadOnly, Category = "Clock|Time")
	bool bTimeSourceResolved = false;

	/** @brief ロスタイム中か（GameMode の bIsTimeUp。制限時間切れ後も最終セットの完了までは
	 *  続行する期間。弧が満円の黄になり、中心は数字の代わりに LossTimeText を表示する） */
	UPROPERTY(BlueprintReadOnly, Category = "Clock|Time")
	bool bLossTime = false;

private:
	/** @brief GameMode を再解決し、残り時間と満量程を読み直す（0.25 秒間隔で Tick から呼ぶ） */
	void RefreshTimeSource();

	/** @brief 残り時間（秒）→ 円弧の長さの割合（0-1）へ変換する */
	float RemainingToRatio(float RemainingSeconds) const;

	/** @brief 前景弧（残り時間）の頂点を Tick で毎フレーム再構築する（長さが毎フレーム変わるためキャッシュ不可） */
	void UpdateForegroundArc();

	/** @brief トラック円（満円の軌跡）など静的な描画キャッシュを再構築する */
	void RebuildDrawCache();

	/** @brief 静的描画パラメータの変化検出用ハッシュ（色は含めない） */
	uint32 ComputeDrawParamsHash() const;

	// ==================== 時間ソース ====================

	/** @brief 残り時間の読み取り元（所有は World。null のレベルでは何もしない） */
	UPROPERTY()
	TObjectPtr<AFishingGameModeBase> CachedGameMode;

	/** @brief 直近の読み取りによる残り時間の目標値（-1＝未取得） */
	float TargetRemainingSeconds = -1.0f;

	/** @brief 一度でも時間を取得したか（初回は補間せず弧をスナップする） */
	bool bHasEverReceivedTime = false;

	/** 時間の再取得間隔（秒）。LoadApplier が Pawn BeginPlay で TotalGameTime を書き替えるため
	 *  NativeConstruct のタイミングとは順序が保証されない → 一度きりの読み取りは禁止 */
	float TimeRefreshInterval = 0.25f;
	/** 前回の時間再取得時刻（FPlatformTime::Seconds() 基準。初期値は実質「未実行」） */
	double LastTimeRefreshTime = -1.0e9;

	/** @brief 前回ログ出力時の満量程（値が動いた瞬間だけログるための多重度防止。HMD 内での確認用） */
	float LastLoggedFullScale = -1.0f;

	/** @brief 前回ログ出力時のロスタイム状態（遷移瞬間だけログるための多重度防止） */
	bool LastLoggedLossTime = false;

	/** @brief Text_Remaining へロスタイム表示へ切り替えた直後か（SetText を 1 回だけ行うため） */
	bool bLossTimeTextShown = false;

	// ==================== 中心テキスト ====================

	/** @brief Text_Remaining へ最後に反映した残り秒数（値が変わった時だけ SetText する） */
	int32 LastDisplayedTotalSeconds = -1;

	// ==================== 描画キャッシュ（Tick 側で構築 / NativePaint は const のため読むだけ） ====================

	/** @brief トラック円（満円）の折れ線頂点。静的なためハッシュ比較で再構築 */
	TArray<FVector2f> TrackArcPoints;

	/** @brief 残り時間の前景弧の折れ線頂点。Tick で毎フレーム再構築 */
	TArray<FVector2f> ForegroundArcPoints;

	/** リング中心（ローカル空間。RebuildDrawCache で算出） */
	FVector2f CachedCenter = FVector2f::ZeroVector;
	/** リング半径（px。RebuildDrawCache で算出） */
	float CachedRadius = 0.0f;

	/** @brief リング帯の太さ（px。RebuildDrawCache で算出し Paint で参照） */
	float CachedBandThickness = 1.0f;

	/** ジオメトリ変更検出用（Widget サイズが変わったらキャッシュ再構築） */
	FVector2f CachedGeometrySize = FVector2f::ZeroVector;
	/** 描画パラメータ変更検出用（ComputeDrawParamsHash の前回値） */
	uint32 CachedDrawParamsHash = 0;
	/** キャッシュ無効フラグ（初回は必ず再構築） */
	bool bDrawCacheDirty = true;
};
