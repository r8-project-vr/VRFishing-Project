// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/widget/ArmGuideWidget.h"
// 2026.10.21 Lee startーーー 原図腕ガイド（自己構築 UMG） ーーー
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "VRFishingLog.h"
// 2026.10.21 Lee endーーー

UArmGuideWidget::UArmGuideWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 原図テクスチャ／破線マテリアルをハード参照で既定指定する（パッケージ収集を保証するため）。
	// アセット未導入時はここで警告ログのみで null になり、実行時は該当 Image を静かに非表示へ縮退する
	static ConstructorHelpers::FObjectFinder<UTexture2D> TextureFinder(TEXT("/Game/UI/ArmGuide/T_ArmOriginal.T_ArmOriginal"));
	if (TextureFinder.Succeeded())
	{
		ArmTexture = TextureFinder.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Game/UI/ArmGuide/M_ArmGuideDashed.M_ArmGuideDashed"));
	if (MaterialFinder.Succeeded())
	{
		DashedArmMaterial = MaterialFinder.Object;
	}
}

// 2026.10.21 Lee startーーー WidgetTree 自己構築（BP 資産不要の純 C++ 子 Widget） ーーー
void UArmGuideWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 二次構築対策：RootWidget が既にある（再初期化された）場合は壊さず流用する
	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}
}

void UArmGuideWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// ---- 実線アーム：原図テクスチャをそのまま貼る（画素は無加工） ----
	if (SolidArmImage)
	{
		if (ArmTexture)
		{
			SolidArmImage->SetBrushFromTexture(ArmTexture);
			bSolidArmBrushReady = true;
		}
		else
		{
			UE_LOG(LogFishing, Warning, TEXT("[ArmGuide] T_ArmOriginal が未導入のため実線アームを非表示にします（scripts/import_arm_assets.py を実行してください）"));
		}
	}

	// ---- 破線アーム：同源 alpha の UI マテリアルを貼る ----
	if (DashedArmImage)
	{
		if (DashedArmMaterial)
		{
			DashedArmImage->SetBrushFromMaterial(DashedArmMaterial);
			bDashedArmBrushReady = true;
		}
		else
		{
			UE_LOG(LogFishing, Warning, TEXT("[ArmGuide] M_ArmGuideDashed が未生成のため破線アームを非表示にします（scripts/import_arm_assets.py を実行してください）"));
		}
	}

	// 初回同期：途中生成でも現在入力（既定は Hidden）を即時反映する
	ApplyGuideInput(LastInput);
}
// 2026.10.21 Lee endーーー

// 2026.10.21 Lee startーーー 入力適用（変化検出付き・点滅なし・補間なし） ーーー
void UArmGuideWidget::ApplyGuideInput(const FArmGuideInput& InInput)
{
	LastInput = InInput;

	if (!RootCanvas)
	{
		return;
	}

	const bool bShowPanel = (InInput.Mode != EArmGuideMode::Hidden);
	SetPanelVisible(bShowPanel);
	if (!bShowPanel)
	{
		return;
	}

	// ---- モード別の表示内容決定（純表示ロジック。ゲーム判定には触れない） ----
	bool bShowSolid = false;
	bool bShowDashed = false;
	bool bShowArrow = false;
	bool bShowTimer = false;
	FText ArrowGlyph;
	FText PromptString;
	FText TimerString;

	// Ready の達成判定は表示用の比較（本番判定は ReadyState 側）。無効データは 0 と見なさない
	const bool bReadyMeets = (InInput.bHandValid && InInput.bTargetValid && InInput.HandPercent <= InInput.TargetPercent);

	switch (InInput.Mode)
	{
	case EArmGuideMode::Ready:
		bShowSolid = InInput.bHandValid;
		bShowDashed = (InInput.bHandValid && InInput.bTargetValid);
		if (InInput.bHandValid && InInput.bTargetValid)
		{
			// 両方有効：表示用の達成比較（本番判定は ReadyState 側）。保持タイマーはミラー表示しない
			PromptString = bReadyMeets ? ReadyHoldText : ReadyLowerText;
		}
		else
		{
			// センサ無効／閾値の反射読み取り失敗：判定不能のため中性の待機文案のみ出す
			// （「おろして」指示も達成提示も出さない。0 を達成扱いにしないため）
			PromptString = WaitingForHandText;
		}
		break;

	case EArmGuideMode::HandUpDown:
		bShowSolid = InInput.bHandValid;
		bShowDashed = (InInput.bHandValid && InInput.bTargetValid);
		if (InInput.bHandValid)
		{
			switch (InInput.ArrowState)
			{
			case EFishArrowState::MovingUp:
				PromptString = UpDownRiseText;
				ArrowGlyph = ArrowUpGlyph;
				bShowArrow = true;
				break;
			case EFishArrowState::MovingDown:
				PromptString = UpDownLowerText;
				ArrowGlyph = ArrowDownGlyph;
				bShowArrow = true;
				break;
			case EFishArrowState::WaitingAtTop:
				PromptString = UpDownMoreTopText;
				break;
			case EFishArrowState::WaitingAtBottom:
			default:
				PromptString = UpDownMoreBottomText;
				break;
			}
		}
		else
		{
			// センサ無効：目標・矢印を隠し中性の待機文案のみ（既定 0% を有効な動作として扱わない）
			PromptString = WaitingForHandText;
		}
		bShowTimer = InInput.bShowTimer;
		// 残り時間は秒の切り上げで 1 秒単位表示（回数は表示しない）
		TimerString = FText::FromString(FString::Printf(TEXT("のこり %d びょう"), FMath::CeilToInt32(FMath::Max(0.0f, InInput.RemainingSeconds))));
		break;

	case EArmGuideMode::Catching:
		// つりあげ：実線＋上矢印のみ。目標破線なし・計時なし（終了は実状態切替のみ）
		bShowSolid = InInput.bHandValid;
		bShowDashed = false;
		if (InInput.bHandValid)
		{
			PromptString = CatchingLiftText;
			ArrowGlyph = ArrowUpGlyph;
			bShowArrow = true;
		}
		else
		{
			// センサ無効：矢印も隠して中性の待機文案（0% を挙上動作ありと扱わない）
			PromptString = WaitingForHandText;
		}
		break;

	case EArmGuideMode::Hidden:
	default:
		break;
	}

	// アセット未導入の縮退（brush 適用済みフラグで判定）
	if (bShowSolid && !bSolidArmBrushReady)
	{
		bShowSolid = false;
	}
	if (bShowDashed && !bDashedArmBrushReady)
	{
		bShowDashed = false;
	}

	// ---- 腕 2 枚：角度は毎フレームの実データへ直接追従（補間・自前アニメなし） ----
	if (SolidArmImage)
	{
		SolidArmImage->SetVisibility(bShowSolid ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		SolidArmImage->SetRenderTransformAngle(ComputeArmAngleDeg(InInput.bHandValid ? InInput.HandPercent : 1.0f));
	}
	if (DashedArmImage)
	{
		DashedArmImage->SetVisibility(bShowDashed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		DashedArmImage->SetRenderTransformAngle(ComputeArmAngleDeg(InInput.bTargetValid ? InInput.TargetPercent : 1.0f));
	}

	// ---- テキスト群（変化検出付き適用） ----
	ApplyTextIfChanged(ArrowText, ArrowGlyph, bShowArrow);
	ApplyTextIfChanged(PromptText, PromptString, true);
	ApplyTextIfChanged(TimerText, TimerString, bShowTimer);
}
// 2026.10.21 Lee endーーー

// 2026.10.22 Lee startーーー WBP テンプレート（デザイナー設定）の複写 ーーー
void UArmGuideWidget::ApplyDesignerTemplate(const UBorder* PreviewPanel, const UTextBlock* PreviewText)
{
	if (!PanelBorder || !PromptBox)
	{
		return;
	}

	// ---- パネル本体：プレビュー Border の slot レイアウト（アンカー・位置寸法・アライメント）と
	//      padding をそのまま複写する。これでデザイナーが WBP で調整した配置・寸法がそのまま生きる ----
	if (PreviewPanel)
	{
		if (const UCanvasPanelSlot* PreviewSlot = Cast<UCanvasPanelSlot>(PreviewPanel->Slot))
		{
			if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(PanelBorder->Slot))
			{
				PanelSlot->SetLayout(PreviewSlot->GetLayout());
			}
		}
		// 同一ローカル座標系を保つため padding も複写する
		PanelBorder->SetPadding(PreviewPanel->GetPadding());
	}

	// ---- 文字エリア：プレビュー Text の slot レイアウトを PromptBox へ複写する（3 行分の共通領域） ----
	if (PreviewText)
	{
		if (const UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(PreviewText->Slot))
		{
			if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(PromptBox->Slot))
			{
				BoxSlot->SetLayout(TextSlot->GetLayout());
			}
		}
	}

	/**
	 * @brief 提示文を領域幅で折り返すための水平幅だけの調整。
	 * @note VerticalBox 子項目の水平配置を Fill へ変える（HAlign_Center のままだと項目が内容幅へ
	 *       縮んで領域幅の折り返しにならないため）。垂直方向の既定の 3 行配分（Auto）は変更しない。
	 *       文字自体の中央寄せは SetJustification(ETextJustify::Center) で行う
	 *       （UE5.8 の UTextBlock には SetHorizontalAlignment が無いため）
	 */
	if (PromptText)
	{
		if (UVerticalBoxSlot* PromptSlot = Cast<UVerticalBoxSlot>(PromptText->Slot))
		{
			PromptSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		PromptText->SetJustification(ETextJustify::Center);
	}
}
// 2026.10.22 Lee endーーー

// 2026.10.21 Lee startーーー WidgetTree 構築の実体 ーーー
namespace
{
	/** @brief WidgetTree へ指定型のウィジェットを生成して取り出す小さなヘルパー（テンプレート非依存で安全に） */
	template<typename WidgetT>
	WidgetT* ConstructTreeWidget(UWidgetTree* Tree, UClass* WidgetType, const TCHAR* WidgetName)
	{
		return Tree->ConstructWidget<WidgetT>(WidgetType, WidgetName);
	}
}

void UArmGuideWidget::BuildWidgetTree()
{
	// ---- ルート：全面キャンバス（挿入先ルート Canvas と同寸法になる） ----
	RootCanvas = ConstructTreeWidget<UCanvasPanel>(WidgetTree, UCanvasPanel::StaticClass(), TEXT("ArmGuideRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// ---- 下部パネル（背景なし）：Border は内容レイアウト用の容器としてのみ使う ----
	PanelBorder = ConstructTreeWidget<UBorder>(WidgetTree, UBorder::StaticClass(), TEXT("ArmGuidePanelBorder"));
	/**
	 * @brief 背景を一切描画しない brush 設定（フィル・描画線とも無し）。
	 * @note ユーザー指示により灰色背景を廃止。Border は可視のまま内容（腕・文字）だけを表示し、
	 *       非表示にはしない（Collapsed すると内容ごと消えるため）。padding・slot・WBP レイアウト
	 *       複写（ApplyDesignerTemplate）は従来どおり
	 */
	FSlateBrush PanelBrush;
	PanelBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	PanelBorder->SetBrush(PanelBrush);
	PanelBorder->SetPadding(FMargin(18.0f, 14.0f));
	PanelBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RootCanvas->AddChildToCanvas(PanelBorder);
	if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(PanelBorder->Slot))
	{
		// 画面下端中央へ固定（魚の表示域を避ける）。寸法は既定値で、ArmGuidePreview テンプレートが
		// ある場合は ApplyDesignerTemplate が WBP の現在値で上書きする
		PanelSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		PanelSlot->SetPosition(FVector2D(0.0f, -PanelBottomMargin));
		PanelSlot->SetSize(PanelSize);
		PanelSlot->SetZOrder(0);
	}

	// ---- 内容キャンバス（腕エリア＋提示エリアの絶対配置用） ----
	GuideCanvas = ConstructTreeWidget<UCanvasPanel>(WidgetTree, UCanvasPanel::StaticClass(), TEXT("ArmGuideGuideCanvas"));
	GuideCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PanelBorder->SetContent(GuideCanvas);

	// ---- 破線（奥）／実線（手前）の同軸 2 枚 ----
	DashedArmImage = ConstructTreeWidget<UImage>(WidgetTree, UImage::StaticClass(), TEXT("ArmGuideDashedImage"));
	DashedArmImage->SetVisibility(ESlateVisibility::Collapsed);
	ApplyArmImageSlot(DashedArmImage, 0);

	SolidArmImage = ConstructTreeWidget<UImage>(WidgetTree, UImage::StaticClass(), TEXT("ArmGuideSolidImage"));
	SolidArmImage->SetVisibility(ESlateVisibility::Collapsed);
	ApplyArmImageSlot(SolidArmImage, 1);

	// ---- 提示エリア（右側：矢印／文案／残り時間の縦積み） ----
	PromptBox = ConstructTreeWidget<UVerticalBox>(WidgetTree, UVerticalBox::StaticClass(), TEXT("ArmGuidePromptBox"));
	PromptBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	GuideCanvas->AddChildToCanvas(PromptBox);
	if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(PromptBox->Slot))
	{
		// 腕エリアを除いた右側の中心へ点アンカー配置（比率はデザイナー変更可）
		const float HintCenterX = 0.5f + ArmZoneWidthRatio * 0.5f;
		const float HintWidth = PanelSize.X * (1.0f - ArmZoneWidthRatio) - 24.0f;
		BoxSlot->SetAnchors(FAnchors(HintCenterX, 0.0f, HintCenterX, 0.0f));
		BoxSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		BoxSlot->SetPosition(FVector2D(0.0f, 12.0f));
		BoxSlot->SetSize(FVector2D(HintWidth, PanelSize.Y - 24.0f));
	}

	ArrowText = ConstructTreeWidget<UTextBlock>(WidgetTree, UTextBlock::StaticClass(), TEXT("ArmGuideArrowText"));
	SetupTextStyle(ArrowText, ArrowFontSize);
	PromptBox->AddChildToVerticalBox(ArrowText);
	if (UVerticalBoxSlot* ArrowSlot = Cast<UVerticalBoxSlot>(ArrowText->Slot))
	{
		ArrowSlot->SetHorizontalAlignment(HAlign_Center);
		ArrowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	PromptText = ConstructTreeWidget<UTextBlock>(WidgetTree, UTextBlock::StaticClass(), TEXT("ArmGuidePromptText"));
	SetupTextStyle(PromptText, PromptFontSize);
	PromptText->SetAutoWrapText(true);
	PromptBox->AddChildToVerticalBox(PromptText);
	if (UVerticalBoxSlot* PromptSlot = Cast<UVerticalBoxSlot>(PromptText->Slot))
	{
		PromptSlot->SetHorizontalAlignment(HAlign_Center);
		PromptSlot->SetVerticalAlignment(VAlign_Center);
	}

	TimerText = ConstructTreeWidget<UTextBlock>(WidgetTree, UTextBlock::StaticClass(), TEXT("ArmGuideTimerText"));
	SetupTextStyle(TimerText, TimerFontSize);
	PromptBox->AddChildToVerticalBox(TimerText);
	if (UVerticalBoxSlot* TimerSlot = Cast<UVerticalBoxSlot>(TimerText->Slot))
	{
		TimerSlot->SetHorizontalAlignment(HAlign_Center);
		TimerSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}

	// パネル全体は入力を透過させる（判定系・レーザー操作に触れない純表示のため）
	SetVisibility(ESlateVisibility::Collapsed);
}

void UArmGuideWidget::ApplyArmImageSlot(UImage* ArmImage, int32 ZOrder)
{
	if (!ArmImage || !GuideCanvas)
	{
		return;
	}

	// 原図の縦横比を実テクスチャから取り、表示幅を自動算出する（引き伸ばし禁止のため）
	float SourceW = 958.0f;
	float SourceH = 1642.0f;
	if (ArmTexture)
	{
		const float TexW = static_cast<float>(ArmTexture->GetSurfaceWidth());
		const float TexH = static_cast<float>(ArmTexture->GetSurfaceHeight());
		if (TexW > 1.0f && TexH > 1.0f)
		{
			SourceW = TexW;
			SourceH = TexH;
		}
	}
	const float ArmWidth = ArmDisplayHeight * (SourceW / SourceH);

	UCanvasPanelSlot* ArmSlot = GuideCanvas->AddChildToCanvas(ArmImage);
	// 点アンカー(0,0)＋位置＝内容領域内の pivot 座標。アライメントを pivot UV にすると
	// フィット中心がその座標へ固定され、25〜125 度の全回転で alpha 包囲計算どおりの
	// 範囲に収まる（実線・破線とも同一 pivot で形が一致する）
	ArmSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
	ArmSlot->SetAlignment(ArmPivotUV);
	ArmSlot->SetPosition(ArmPivotPosition);
	ArmSlot->SetSize(FVector2D(ArmWidth, ArmDisplayHeight));
	ArmSlot->SetZOrder(ZOrder);

	// 描画変換の回転中心も同じ pivot へ揃える
	ArmImage->SetRenderTransformPivot(ArmPivotUV);
	ArmImage->SetRenderTransformAngle(0.0f);
}
// 2026.10.21 Lee endーーー

// 2026.10.21 Lee startーーー 補助処理 ーーー
void UArmGuideWidget::SetupTextStyle(UTextBlock* Text, float FontSize) const
{
	if (!Text)
	{
		return;
	}
	// エンジン内蔵サンセリフ（Roboto）の太字スタイル。動的フォントサイズはこれまでどおり反映する
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::RoundToInt32(FontSize));
	// 日本語は既定フォントのフォールバック描画のため、文字色と同色（白）の 1px アウトラインで
	// 縁を太くし、仮名まで確実に太く見せる（黒縁ではない）
	Font.OutlineSettings.OutlineSize = 1;
	Font.OutlineSettings.OutlineColor = FLinearColor(PanelTextColor);
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(PanelTextColor));
	Text->SetVisibility(ESlateVisibility::Collapsed);
}

float UArmGuideWidget::ComputeArmAngleDeg(float Percent) const
{
	// 原図直立＝0 度。UMG の RenderAngle（時計回り正）へ「125 - 100 * Clamp(p, 0, 1)」を
	// そのまま描画角として使う（符号変換なし）
	const float ClampedPercent = FMath::Clamp(Percent, 0.0f, 1.0f);
	return ArmAngleAtZeroPercent + (ArmAngleAtOnePercent - ArmAngleAtZeroPercent) * ClampedPercent;
}

void UArmGuideWidget::SetPanelVisible(bool bVisible)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (GetVisibility() != NewVisibility)
	{
		SetVisibility(NewVisibility);
	}
}

void UArmGuideWidget::ApplyTextIfChanged(UTextBlock* Text, const FText& NewText, bool bVisible)
{
	if (!Text)
	{
		return;
	}
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (Text->GetVisibility() != NewVisibility)
	{
		Text->SetVisibility(NewVisibility);
	}
	if (!Text->GetText().EqualTo(NewText))
	{
		Text->SetText(NewText);
	}
}
// 2026.10.21 Lee endーーー
