// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/widget/ExerciseTimeBarWidget.h"
#include "Lee/widget/ExerciseSecondsReader.h"
#include "Lee/component/FishingStateHandUpDown.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Tanimura/FishingGameModeBase.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "VRFishingLog.h"

UExerciseTimeBarWidget::UExerciseTimeBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/** @brief 生成時処理。コンポーネント購読 → 魚アイコン初期化 → 表示ゲート初回同期 */
void UExerciseTimeBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryInitializeComponents();

	// ---- 魚アイコンの初期設定（FishFightMeterWidget の PhaseFish 方式） ----
	// 貼り付けTexture とスロット寸法を実行時に反映する（デザイナー既定値に依存しない）。
	// Texture 未設定は魚アイコンを折りたたんでバーのみ表示する（nullptr の優雅な降級）
	if (Image_Fish)
	{
		float FishW = FishIconSize;
		float FishH = FishIconSize;
		if (FishIconTexture)
		{
			Image_Fish->SetBrushFromTexture(FishIconTexture);
			// テクスチャのアスペクト比を保つ（高さ = FishIconSize を基準に幅を自動算出）
			const float TexW = static_cast<float>(FishIconTexture->GetSurfaceWidth());
			const float TexH = static_cast<float>(FishIconTexture->GetSurfaceHeight());
			if (TexW > 1.0f && TexH > 1.0f)
			{
				FishW = FishIconSize * (TexW / TexH);
			}
		}
		else
		{
			Image_Fish->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (UCanvasPanelSlot* FishSlot = Cast<UCanvasPanelSlot>(Image_Fish->Slot))
		{
			FishSlot->SetSize(FVector2D(FishW, FishH));
		}
	}

	// 表示ゲートの初回同期（コンポーネント未就位でも「未アクティブ＝折りたたみ」へ揃える）
	ApplyVisibility();
}

/** @brief 破棄時処理。動的デリゲートの購読解除（解除漏れは GC 保護の逆効果になるため必須） */
void UExerciseTimeBarWidget::NativeDestruct()
{
	if (StateManager)
	{
		StateManager->OnFishingStateChanged.RemoveDynamic(this, &UExerciseTimeBarWidget::HandleFishingStateChanged);
	}
	Super::NativeDestruct();
}

/** @brief 毎フレーム処理。コンポーネント再試行 → 分母定期更新 → 進捗計算 → 魚アイコン移動 */
void UExerciseTimeBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bComponentsInitialized)
	{
		// 世界空間 UI では Pawn が遅れて就位することがあるため Tick で再試行する
		TryInitializeComponents();
		if (!bComponentsInitialized)
		{
			return;
		}
	}

	// ---- 分母の定期更新（0.25 秒間隔） ----
	// ExerciseLevel はセット成功ごとに上がるため NativeConstruct 時点の 1 回きり読み取りは禁止
	const double Now = FPlatformTime::Seconds();
	if (Now - LastTotalRefreshTime >= TotalRefreshInterval)
	{
		LastTotalRefreshTime = Now;
		RefreshTotalSeconds();
	}

	// ---- 非運動ステート中は何もしない ----
	if (!bExerciseActive)
	{
		return;
	}

	// ---- バー未束縛は BP 側の名前ミスが大半のため、最初の 1 回だけ警告して静かに諦める ----
	if (!ProgressBar_Exercise)
	{
		if (!bWarnedMissingBar)
		{
			bWarnedMissingBar = true;
			UE_LOG(LogFishing, Warning, TEXT("[ExerciseBar] ProgressBar_Exercise が見つかりません。WBP 側のウィジェット名を確認してください"));
		}
		return;
	}

	// ---- 分子（残り運動時間）の読み取り ----
	float Remaining = 0.0f;
	if (LeeFishingTime::ReadRemainingExerciseSeconds(ActiveState, Remaining))
	{
		DisplayedRemainingSeconds = Remaining;
	}

	// ---- 進捗の計算とバーへの反映（空→満＝消費済み割合） ----
	const float Total = FMath::Max(CurrentTotalSeconds, KINDA_SMALL_NUMBER);
	DisplayedProgress = 1.0f - FMath::Clamp(DisplayedRemainingSeconds / Total, 0.0f, 1.0f);
	ProgressBar_Exercise->SetPercent(DisplayedProgress);

	UpdateFishPosition(InDeltaTime);
}

// ==================== デリゲート ハンドラ ====================

void UExerciseTimeBarWidget::HandleFishingStateChanged(UFishingStateComponentBase* NewState)
{
	// 列挙型に依存させず、ステートコンポーネントと同一インスタンスかのポインタ比較だけで判定する
	// （RpmGaugeWidget の HandleFishingStateChanged と同一の方針）
	const bool bNewActive = (NewState != nullptr && (NewState == HandUpDownState || NewState == ReelState));
	bExerciseActive = bNewActive;

	if (bNewActive)
	{
		// アクティブ状態が切り替わったら進捗を最初からやり直す（魚も左端へスナップさせる）。
		// LastTotalRefreshTime の巻き戻しで次フレームの分母更新を強制する（レベルアップ直後の
		// 旧レベル分母での進捗計算を防ぐ）
		if (NewState != ActiveState)
		{
			ActiveState = NewState;
			DisplayedProgress = 0.0f;
			DisplayedRemainingSeconds = 0.0f;
			bFishPosInitialized = false;
			LastTotalRefreshTime = -1.0e9;
		}
	}
	else
	{
		ActiveState = nullptr;
	}

	ApplyVisibility();
}

// ==================== 内部処理 ====================

void UExerciseTimeBarWidget::TryInitializeComponents()
{
	APawn* OwnerPawn = GetOwningPlayerPawn();
	if (!OwnerPawn)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			OwnerPawn = PC->GetPawn();
		}
	}
	if (!OwnerPawn)
	{
		return; // Pawn が未就位 → Tick で再試行
	}

	HandUpDownState = OwnerPawn->FindComponentByClass<UFishingStateHandUpDown>();
	ReelState = OwnerPawn->FindComponentByClass<UFishingReelStateComponent>();
	StateManager = OwnerPawn->FindComponentByClass<UFishingStateManagerComponent>();

	if (StateManager)
	{
		// NativeConstruct が WidgetComponent の再構築で二度呼ばれても二重購読しない
		StateManager->OnFishingStateChanged.AddUniqueDynamic(this, &UExerciseTimeBarWidget::HandleFishingStateChanged);
		// 生成時点のフェーズを初回同期（運動中に生成されたケースに対応）
		HandleFishingStateChanged(StateManager->GetCurrentState());
	}

	bComponentsInitialized = true;
	LastTotalRefreshTime = -1.0e9; // 次の Tick で即座に 1 回目の分母取得を行う
}

void UExerciseTimeBarWidget::RefreshTotalSeconds()
{
	UWorld* World = GetWorld();
	AFishingGameModeBase* GameMode = World ? World->GetAuthGameMode<AFishingGameModeBase>() : nullptr;
	if (GameMode)
	{
		// 現在レベルの運動時間（各ステートの EnterState と同一の式）
		CurrentTotalSeconds = FMath::Max(GameMode->GetCurrentExerciseSeconds(), 0.0f);
	}
	else
	{
		// GameMode 無しレベルではステート側と同じフォールバック値を使う
		CurrentTotalSeconds = FallbackTotalSeconds;
	}
}

void UExerciseTimeBarWidget::ApplyVisibility()
{
	if (!bDisplayOnlyInExerciseStates)
	{
		SetVisibility(ESlateVisibility::Visible);
		return;
	}
	SetVisibility(bExerciseActive ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UExerciseTimeBarWidget::UpdateFishPosition(float DeltaTime)
{
	// 非表示中・Texture 未設定は位置更新も不要（FishFightMeterWidget の PhaseFish 方式）
	if (!Image_Fish || !FishIconTexture ||
		Image_Fish->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	// 進捗バー本体の実寸ジオメトリ（Fill は描画クリップのため本体ジオメトリ＝全条幅）。
	// NativeTick が ProgressBar_Exercise の null チェックを済ませているためここでは非 null 前提
	const FGeometry BarGeo = ProgressBar_Exercise->GetCachedGeometry();
	if (BarGeo.GetLocalSize().IsNearlyZero())
	{
		return; // レイアウト未確定（初フレーム等）は前回位置を維持
	}

	// バー左上を自 Widget ローカル座標へ変換（DPI スケールは AbsoluteToLocal が吸収する）
	const FVector2D BarLeftTopLocal = GetCachedGeometry().AbsoluteToLocal(BarGeo.GetAbsolutePosition());
	const float BarWidth = BarGeo.GetLocalSize().X;
	const float BarHeight = BarGeo.GetLocalSize().Y;

	// 魚アイコンの実描画寸法はスロットから取る（アスペクト比維持により正方形とは限らないため）
	FVector2D FishDrawSize(FishIconSize, FishIconSize);
	if (const UCanvasPanelSlot* FishSlot = Cast<UCanvasPanelSlot>(Image_Fish->Slot))
	{
		FishDrawSize = FishSlot->GetSize();
	}

	// 目標位置＝進捗先端（横）× バー縦中央。RenderTranslation は「レイアウト位置＋描画オフセット」
	// なので、Image_Fish のレイアウト位置は (0,0) に置いておくこと（WBP 手順の前提）
	FishTargetPos.X = BarLeftTopLocal.X + DisplayedProgress * BarWidth - FishDrawSize.X * 0.5f;
	FishTargetPos.Y = BarLeftTopLocal.Y + BarHeight * 0.5f - FishDrawSize.Y * 0.5f;

	// 初回はスナップ、以降は補間で滑らかに追従する（運動ステート突入時は移動アニメになる）
	if (!bFishPosInitialized)
	{
		FishCurrentPos = FishTargetPos;
		bFishPosInitialized = true;
	}
	else
	{
		FishCurrentPos = FMath::Vector2DInterpTo(FishCurrentPos, FishTargetPos, DeltaTime, FishInterpSpeed);
	}
	Image_Fish->SetRenderTranslation(FishCurrentPos);
}
