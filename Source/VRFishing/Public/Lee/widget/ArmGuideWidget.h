// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Lee/component/FishingStateHandUpDown.h"
#include "ArmGuideWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UMaterialInterface;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

/**
 * @brief 原図手臂 UI の表示モード（釣りフェーズとの対応は FishFightMeterWidget 側で解決する）。
 */
UENUM(BlueprintType)
enum class EArmGuideMode : uint8
{
	Hidden     UMETA(DisplayName = "非表示"),
	Ready      UMETA(DisplayName = "準備"),
	HandUpDown UMETA(DisplayName = "上下運動"),
	Catching   UMETA(DisplayName = "つりあげ")
};

/**
 * @brief 原図手臂 UI への最小入力セット（独立デモ/テスト用に全項目 BlueprintReadWrite）。
 * @note ゲームデータ→表示の変換のみを行い、判定ロジックは含まない。
 */
USTRUCT(BlueprintType)
struct FArmGuideInput
{
	GENERATED_BODY()

	/** @brief 表示モード（Reel／Result は Hidden でパネルごと非表示） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide")
	EArmGuideMode Mode = EArmGuideMode::Hidden;

	/** @brief センサの手位置が有効か（HandHeightDetectorComponent::GetEffectiveHandZCm の戻り値） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide")
	bool bHandValid = false;

	/** @brief 現在の手の高さ（0.0〜1.0）。bHandValid=false のときは使用しない（0 を達成扱いにしない） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide")
	float HandPercent = 0.0f;

	/** @brief 目標（破線）位置が有効か（Ready 閾値の反射読み取り失敗時は false） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide")
	bool bTargetValid = false;

	/** @brief 目標の高さ（0.0〜1.0）。bTargetValid=false のときは破線ごと非表示にする */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide")
	float TargetPercent = 0.0f;

	/** @brief 上下運動ステートの矢印状態（文案と矢印方向の決定に使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide")
	EFishArrowState ArrowState = EFishArrowState::MovingUp;

	/** @brief 残り時間表示を出すか（上下運動フェーズのみ true） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide")
	bool bShowTimer = false;

	/** @brief 残り運動時間（秒）。「のこり N びょう」として表示する */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide")
	float RemainingSeconds = 0.0f;
};

/**
 * @brief 原図手臂（ユーザー提供 PNG を未加工のまま使う）を回転表示するガイド子 Widget（表示専用）。
 * @note WidgetTree を自己構築する純 C++ 子 Widget（Blueprint 資産不要）。
 *       FishFightMeterWidget が実行時に生成し、既存ルート CanvasPanel へ挿入する。
 *       単独でも CreateWidget + ApplyGuideInput() でデモ表示できる。
 *       回転は「原図 alpha 下端円弧の最小二乗フィット中心」を pivot とし、
 *       実線（現在手）／破線（目標）の同軸 2 枚で構成する。補間・自前アニメは行わない。
 */
UCLASS(BlueprintType, Blueprintable)
class VRFISHING_API UArmGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UArmGuideWidget(const FObjectInitializer& ObjectInitializer);

protected:
	/** @brief 初期化。WidgetTree にパネル・同軸 2 枚の腕 Image・提示テキスト群を構築する（ネイティブ初期化フック） */
	virtual void NativeOnInitialized() override;

	/** @brief 生成時処理。原図テクスチャ／破線マテリアルを brush へ適用し初回表示を同期する */
	virtual void NativeConstruct() override;

public:
	// ==================== 入力 I/F（独立デモ可能） ====================

	/**
	 * @brief ガイド表示を一括更新する（毎フレーム呼び出し前提・変化検出付きで点滅しない）。
	 * @param InInput 表示入力（モード・手位置・目標位置・矢印状態・残り時間）
	 * @note 純表示処理。ゲーム判定には触れない。デモ／テストは BP から直接呼び出せる。
	 */
	UFUNCTION(BlueprintCallable, Category = "ArmGuide")
	void ApplyGuideInput(const FArmGuideInput& InInput);

	/** @brief 最後に適用した入力のコピー（デバッグ確認用） */
	UFUNCTION(BlueprintPure, Category = "ArmGuide")
	FArmGuideInput GetLastInput() const { return LastInput; }

	/**
	 * @brief WBP デザイナー用テンプレート（ArmGuidePreview／ArmGuidePreviewText）のレイアウトを実
	 *        ウィジェットへ複写する（WBP の現在設定が優先。テンプレート無し時は既定レイアウトを保持）。
	 * @param PreviewPanel プレビュー用 Border。slot レイアウト（アンカー・位置寸法・アライメント）と
	 *                     padding を実パネルへ写す。無指定ならパネルは触らない
	 * @param PreviewText  プレビュー用 TextBlock。slot レイアウトを文字エリア（PromptBox）へ写す。
	 *                     無指定なら文字エリアは触らない。プレビューの文言自体は複写しない
	 * @note CreateWidget 直後に 1 回呼ぶ想定。毎フレームの再計算・上書きは行わない。
	 *       BP からの呼び出しを想定しないため UFUNCTION にはしない。
	 */
	void ApplyDesignerTemplate(const UBorder* PreviewPanel, const UTextBlock* PreviewText);

protected:
	// ==================== 表示設定 ====================

	/**
	 * @brief パネル寸法の既定値（デザインキャンバス px）。
	 * @note WBP に ArmGuidePreview テンプレートがあれば、その slot レイアウトと padding を実行時
	 *       に複写してこちらを上書きする（WBP レイアウト優先・以後は WBP で調整する）。
	 *       テンプレートが無い旧資産のときだけこの既定値を使う。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout")
	FVector2D PanelSize = FVector2D(540.0f, 220.0f);

	/** @brief 画面下端からパネル下端までの余白（デザインキャンバス px） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout")
	float PanelBottomMargin = 40.0f;

	/** @brief パネル背景色（浅色半透明・角丸） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout")
	FLinearColor PanelColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.62f);

	/** @brief パネル背景の角丸半径（デザインキャンバス px） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout", meta = (ClampMin = "0.0"))
	float PanelCornerRadius = 24.0f;

	/**
	 * @brief 文字色（白）。
	 * @note 背景なし表示のため白色へ変更。SetupTextStyle が Roboto 太字＋文字色と同色の
	 *       1px アウトライン（白の同色拡縁・黒縁ではない）で仮名まで太く描画する
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout")
	FLinearColor PanelTextColor = FLinearColor::White;

	/** @brief 提示文案のフォントサイズ（2026-10-28 に約 20% 拡大：34→41） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout", meta = (ClampMin = "8.0"))
	float PromptFontSize = 41.0f;

	/** @brief 方向矢印のフォントサイズ（同約 20% 拡大：44→53） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout", meta = (ClampMin = "8.0"))
	float ArrowFontSize = 53.0f;

	/** @brief 残り時間のフォントサイズ（同約 20% 拡大：24→29） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout", meta = (ClampMin = "8.0"))
	float TimerFontSize = 29.0f;

	/**
	 * @brief 腕の表示高さ（デザインキャンバス px。幅は原図の縦横比 958/1642 から自動算出し引き伸ばさない）。
	 * @note 2026-10-28 に約 20% 拡大（135→162）。回転軸 UV・配置座標・角度写像は不変で、
	 *       実線／破線の 2 枚がこの高さを共用する。拡大後の全回転範囲での見え方（はみ出し等）は
	 *       旧 135 の包囲計算をそのまま適用できないため、PIE での目視確認待ち
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout", meta = (ClampMin = "16.0"))
	float ArmDisplayHeight = 162.0f;

	/** @brief パネル幅のうち左側の腕エリアが占める割合（0.1〜0.9） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Layout", meta = (ClampMin = "0.1", ClampMax = "0.9"))
	float ArmZoneWidthRatio = 0.45f;

	// ==================== 回転設定 ====================

	/** @brief 手角度（度）：手高 0%（最下）のとき。原図直立＝0 度、UMG の描画角（時計回り正）でそのまま使う */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Rotation")
	float ArmAngleAtZeroPercent = 125.0f;

	/** @brief 手角度（度）：手高 100%（最上）のとき。既定 25 度＝「125 - 100 * p」の上端側 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Rotation")
	float ArmAngleAtOnePercent = 25.0f;

	/**
	 * @brief 回転軸の UV（原図 alpha 下端円弧の最小二乗フィット中心）。
	 * @note v3 の pivot は使わず原図から再フィットした値（Saved/Codex/arm-source-acceptance.json）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Rotation")
	FVector2D ArmPivotUV = FVector2D(0.5328015f, 0.9102468f);

	/**
	 * @brief pivot を置くパネル内容領域座標（左上基準・padding 後の GuideCanvas 内 px）。
	 * @note 既定 (32,112) は「表示高 135 で 25〜125 度全角度の alpha 包囲範囲が
	 *       内容 504x192／左側アクション領域に収まる」実測計算に基づく（Codex レビュー 2026-10-21）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Rotation")
	FVector2D ArmPivotPosition = FVector2D(32.0f, 112.0f);

	// ==================== アセット参照（ハード参照でパッケージ収集を保証） ====================

	/** @brief 原図テクスチャ（実線アーム。/Game/UI/ArmGuide/T_ArmOriginal。未導入時は実線のみ縮退） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Assets")
	TObjectPtr<UTexture2D> ArmTexture = nullptr;

	/** @brief 破線アーム用 UI マテリアル（同源 alpha 輪郭。/Game/UI/ArmGuide/M_ArmGuideDashed。未導入時は破線のみ縮退） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArmGuide|Assets")
	TObjectPtr<UMaterialInterface> DashedArmMaterial = nullptr;

	// ==================== 固定文案（子ども向けひらがな・変更容易化のためメンバ化） ====================

	/** @brief センサ無効時の待機文案（手位置データが来るまでの暫定表示） */
	FText WaitingForHandText = FText::FromString(TEXT("じゅんび ちゅう"));

	/** @brief Ready：閾値より手が高いとき */
	FText ReadyLowerText = FText::FromString(TEXT("てを おろしてね"));

	/** @brief Ready：閾値達成（保持タイマーは表示しない） */
	FText ReadyHoldText = FText::FromString(TEXT("そのまま まってね"));

	/** @brief 上下運動：上昇中 */
	FText UpDownRiseText = FText::FromString(TEXT("あげてね"));

	/** @brief 上下運動：下降中 */
	FText UpDownLowerText = FText::FromString(TEXT("おろしてね"));

	/** @brief 上下運動：上端で待機（もう少し上げる） */
	FText UpDownMoreTopText = FText::FromString(TEXT("もうすこし うえへ"));

	/** @brief 上下運動：下端で待機（もう少し下げる） */
	FText UpDownMoreBottomText = FText::FromString(TEXT("もうすこし したへ"));

	/** @brief つりあげ：手を頭上へ */
	FText CatchingLiftText = FText::FromString(TEXT("てを あたまより うえへ"));

	/** @brief 上向き矢印（フォントで表示できる記号のみ使用。v3 素材は使わない） */
	FText ArrowUpGlyph = FText::FromString(TEXT("↑"));

	/** @brief 下向き矢印 */
	FText ArrowDownGlyph = FText::FromString(TEXT("↓"));

private:
	// ==================== 内部処理 ====================

	/** @brief WidgetTree の構築（OnInitialized から 1 回だけ呼ぶ。RootWidget 未設定時のみ） */
	void BuildWidgetTree();

	/** @brief 腕 Image の Canvas スロット設定（同寸・同位置・同 pivot で 2 枚を重ねる） */
	void ApplyArmImageSlot(UImage* ArmImage, int32 ZOrder);

	/** @brief テキストへフォントサイズ・色・中央揃えを適用する */
	void SetupTextStyle(UTextBlock* Text, float FontSize) const;

	/** @brief 手高（0.0〜1.0）から腕の回転角（度）を算出する（「125 - 100 * p」写像） */
	float ComputeArmAngleDeg(float Percent) const;

	/** @brief パネル全体の表示切替（変化時のみ SetVisibility する） */
	void SetPanelVisible(bool bVisible);

	/**
	 * @brief テキストを変化検出付きで適用する（同じ文字列・可視性の再設定を避けて点滅を防ぐ）
	 * @param Text 対象テキスト
	 * @param NewText 新しい文字列
	 * @param bVisible 可視化するか（false で Collapsed）
	 */
	void ApplyTextIfChanged(UTextBlock* Text, const FText& NewText, bool bVisible);

	// ==================== 自己構築ウィジェット参照 ====================

	/** @brief ルート全面キャンバス（FishFightMeterWidget のルートと同寸法になる） */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

	/** @brief 下部の丸角パネル本体 */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder = nullptr;

	/** @brief パネル内容の絶対配置用キャンバス（腕エリア＋提示エリア） */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> GuideCanvas = nullptr;

	/** @brief 提示エリアの縦積み（矢印／文案／残り時間） */
	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PromptBox = nullptr;

	/** @brief 破線アーム（奥・目標。原図と同源 alpha の UI マテリアル） */
	UPROPERTY(Transient)
	TObjectPtr<UImage> DashedArmImage = nullptr;

	/** @brief 実線アーム（手前・現在手。原図テクスチャ） */
	UPROPERTY(Transient)
	TObjectPtr<UImage> SolidArmImage = nullptr;

	/** @brief 方向矢印テキスト（↑／↓のみ。待機中は非表示） */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ArrowText = nullptr;

	/** @brief 行動提示テキスト */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText = nullptr;

	/** @brief 残り時間テキスト（上下運動のみ表示） */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TimerText = nullptr;

	// ==================== 状態キャッシュ ====================

	/** @brief 最後に適用した入力（変化検出と GetLastInput 用） */
	FArmGuideInput LastInput;

	/** @brief 実線アーム brush の適用済みフラグ（アセット未導入時の縮退判定に使用） */
	bool bSolidArmBrushReady = false;

	/** @brief 破線アーム brush の適用済みフラグ */
	bool bDashedArmBrushReady = false;
};
