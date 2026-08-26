// Fill out your copyright notice in the Description page of Project Settings.

#include "Lee/widget/FishFightMeterWidget.h"
#include "Tanimura/Component/FishingReelStateComponent.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "Tanimura/Component/FishingReadyStateComponent.h"
#include "Tanimura/Component/FishingCatchingStateComponent.h"
#include "Tanimura/Component/FishingResultStateComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "VRFishingLog.h"

namespace
{
	/** ReelState の閾値が読み取れなかった旨の警告を出力済みか（多重度の警告を防ぐ） */
	bool bReelRPMThresholdWarned = false;

	/**
	 * @brief リールステートの RPM 判定閾値を反射で読み取る。
	 * @note MinAllowedRPM / WheelMaxAllowedRPM / StickMaxAllowedRPM は protected UPROPERTY のため、
	 *       チーム規約により本人のコードへ getter を追加できずリフレクションで読み取る。
	 * @param ReelState         読み取り元のリールステート
	 * @param OutMinRPM         遅すぎ閾値（入力デバイス共通）
	 * @param OutWheelMaxRPM    速すぎ閾値（マウスホイール入力用）
	 * @param OutStickMaxRPM    速すぎ閾値（スティック入力用）
	 * @return 3 つの閾値をすべて取得できた場合 true
	 */
	bool ReadReelRPMThresholds(const UFishingReelStateComponent* ReelState, float& OutMinRPM, float& OutWheelMaxRPM, float& OutStickMaxRPM)
	{
		const UClass* ReelClass = ReelState->GetClass();
		const FFloatProperty* MinProp = FindFProperty<FFloatProperty>(ReelClass, TEXT("MinAllowedRPM"));
		const FFloatProperty* WheelProp = FindFProperty<FFloatProperty>(ReelClass, TEXT("WheelMaxAllowedRPM"));
		const FFloatProperty* StickProp = FindFProperty<FFloatProperty>(ReelClass, TEXT("StickMaxAllowedRPM"));
		if (!MinProp || !WheelProp || !StickProp)
		{
			if (!bReelRPMThresholdWarned)
			{
				bReelRPMThresholdWarned = true;
				UE_LOG(LogTemp, Warning, TEXT("[FightMeter] ReelState の RPM 閾値が読み取れないため、デザイナー設定値で判定表示します"));
			}
			return false;
		}

		OutMinRPM = MinProp->GetPropertyValue_InContainer(ReelState);
		OutWheelMaxRPM = WheelProp->GetPropertyValue_InContainer(ReelState);
		OutStickMaxRPM = StickProp->GetPropertyValue_InContainer(ReelState);
		return true;
	}

	/**
	 * @brief 現在の入力デバイスに応じた速すぎ閾値を返す。
	 * @note HMD のステレオ描画が有効＝VR 起動中はスティック、非 VR 実行（デスクトップ PIE など）は
	 *       マウスホイールの閾値を使用する。HMD が接続していても VR が無効なデスクトップ実行では
	 *       StereoRenderingDevice は無効になるため、接続ではなく実行状態の判定として機能する。
	 */
	float ResolveMaxAllowedRPM(float WheelMaxRPM, float StickMaxRPM)
	{
		return (GEngine && GEngine->StereoRenderingDevice.IsValid()) ? StickMaxRPM : WheelMaxRPM;
	}
}

UFishFightMeterWidget::UFishFightMeterWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFishFightMeterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	APawn* OwnerPawn = GetOwningPlayerPawn();
	if (!OwnerPawn)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			OwnerPawn = PC->GetPawn();
		}
	}

	if (OwnerPawn)
	{
		HandHeightDetector = OwnerPawn->FindComponentByClass<UHandHeightDetectorComponent>();
		HandUpDownState = OwnerPawn->FindComponentByClass<UFishingStateHandUpDown>();
		ReelSimulator = OwnerPawn->FindComponentByClass<UFishingReelStateComponent>();

		if (HandUpDownState)
		{
			HandUpDownState->OnFishingStateCompleted.AddDynamic(this, &UFishFightMeterWidget::OnHandUpDownCompleted);
		}

		if (ReelSimulator)
		{
			ReelSimulator->OnRPMCalculated.AddDynamic(this, &UFishFightMeterWidget::OnRPMUpdated);
		}

		StateManager  = OwnerPawn->FindComponentByClass<UFishingStateManagerComponent>();
		ReadyState    = OwnerPawn->FindComponentByClass<UFishingReadyStateComponent>();
		CatchingState = OwnerPawn->FindComponentByClass<UFishingCatchingStateComponent>();
		ResultState   = OwnerPawn->FindComponentByClass<UFishingResultStateComponent>();

		if (StateManager)
		{
			// 二重購読対策で AddUniqueDynamic を使用（VRPawn と同じ購読方法）
			StateManager->OnFishingStateChanged.AddUniqueDynamic(this, &UFishFightMeterWidget::HandleFishingStateChanged);
		}

		bComponentsInitialized = true;
	}

	// RPM 表示を初期化
	OnRPMChanged(0.0f, EHandSpeedState::TooSlow);

	// フェーズ表示の初回同期：Widget がゲーム途中で生成されても現在フェーズを即時反映する
	if (StateManager && StateManager->GetCurrentState())
	{
		const EFishingPhase InitialPhase = ResolvePhase(StateManager->GetCurrentState());
		// RPM ロック状態も生成時点のフェーズから初期化（Reel 中の生成に対応）
		bReelUnlocked = (InitialPhase == EFishingPhase::Reel);
		ApplyPhase(InitialPhase, StateManager->GetCurrentStateName(), false);
	}
	else
	{
		ApplyPhase(EFishingPhase::Ready, TEXT("よーい！"), false);
	}
}

void UFishFightMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bComponentsInitialized)
	{
		return;
	}

	const float HandPercent = HandHeightDetector ? HandHeightDetector->HandHeightPercent : 0.0f;

	// ---- コンポーネントから表示データを読み取り ----
	if (HandUpDownState)
	{
		ArrowPosition = HandUpDownState->ArrowPosition;
		ArrowState = HandUpDownState->ArrowState;
		CycleCount = HandUpDownState->CurrentUpAndDownCount;
		CurrentScore = HandUpDownState->CurrentScore;
		FinalScore = HandUpDownState->FinalScore;
	}

	// ---- BP イベント発火 ----
	OnArrowUpdated(ArrowPosition, ArrowState);
	OnScoreChanged(CurrentScore);

	// ==================== Debug ====================
	if (GEngine)
	{
		const TCHAR* ArrowStateText = TEXT("???");
		FColor ArrowColor = FColor::White;
		switch (ArrowState)
		{
		case EFishArrowState::MovingUp:			ArrowStateText = TEXT("↑");		ArrowColor = FColor::Cyan;		break;
		case EFishArrowState::WaitingAtTop:		ArrowStateText = TEXT("WAIT_TOP");	ArrowColor = FColor::Yellow;	break;
		case EFishArrowState::MovingDown:		ArrowStateText = TEXT("↓");		ArrowColor = FColor::Orange;	break;
		case EFishArrowState::WaitingAtBottom:	ArrowStateText = TEXT("WAIT_BOT");	ArrowColor = FColor::Yellow;	break;
		}

		const int32 TargetCount = HandUpDownState ? HandUpDownState->TargetUpAndDownCount : 0;
		const FString ArrowMsg = FString::Printf(
			TEXT("[Arrow] Pos:%.0f%% %s | Hand:%.0f%% | %d/%d %s"),
			ArrowPosition * 100.0f, ArrowStateText, HandPercent * 100.0f,
			CycleCount, TargetCount,
			bReelUnlocked ? TEXT("UNLOCKED") : TEXT("LOCKED"));

		GEngine->AddOnScreenDebugMessage(2, 0.0f, ArrowColor, ArrowMsg);

		const TCHAR* RPMText = TEXT("???");
		FColor RPMColor = FColor::White;
		switch (RPMState)
		{
		case EHandSpeedState::Good:		RPMText = TEXT("OK");	RPMColor = FColor::Green;	break;
		case EHandSpeedState::TooSlow:	RPMText = TEXT("SLOW");	RPMColor = FColor::Yellow;	break;
		case EHandSpeedState::TooFast:	RPMText = TEXT("FAST");	RPMColor = FColor::Red;		break;
		}

		const FString RPMMsg = FString::Printf(
			TEXT("[RPM] %.1f [%s] (%.0f+/-%.0f) %s"),
			CurrentRPM, RPMText, TargetRPM, RPMTolerance,
			bReelUnlocked ? TEXT("") : TEXT("| LOCKED"));

		GEngine->AddOnScreenDebugMessage(3, 0.0f, RPMColor, RPMMsg);

		const FColor ScoreColor = FinalScore >= 80.0f ? FColor::Green : (FinalScore >= 50.0f ? FColor::Yellow : FColor::Red);
		const FString ScoreMsg = FString::Printf(
			TEXT("[Score] Final: %.0f / 100 | Live: %.0f"),
			FinalScore, CurrentScore);
		GEngine->AddOnScreenDebugMessage(4, 0.0f, ScoreColor, ScoreMsg);
	}
}

void UFishFightMeterWidget::OnRPMUpdated(float NewRPM)
{
	// 規定回数完了まで RPM 入力を受け付けない
	if (!bReelUnlocked)
	{
		// 2026.08.20 Lee：ロック中に RPM が届いた=リール入力自体は生きているが表示だけロック中。
		// 切り分け用に 1 秒に 1 回だけ警告を出す（毎回出すとログが流れるため）
		static double LastLockedWarnTime = 0.0;
		const double Now = FPlatformTime::Seconds();
		if (Now - LastLockedWarnTime >= 1.0)
		{
			LastLockedWarnTime = Now;
			UE_LOG(LogFishing, Warning, TEXT("[FightMeter] RPM 受信をロック中に破棄: RPM=%.1f (Phase=%d)"), NewRPM, static_cast<int32>(CurrentPhase));
		}
		return;
	}

	CurrentRPM = NewRPM;

	// ゲーム側判定（JudgeRPM）と同じ閾値を ReelState から読み取り、同一区間 [Min, Max] で分類する。
	// 閾値は負荷プリセット（ApplyRotationLoadLevel）で変わるため、回転ごとに最新値を読み取る。
	float MinRPM = 0.0f;
	float WheelMaxRPM = 0.0f;
	float StickMaxRPM = 0.0f;
	if (ReelSimulator && ReadReelRPMThresholds(ReelSimulator, MinRPM, WheelMaxRPM, StickMaxRPM))
	{
		// 入力デバイスに応じた速すぎ閾値を使用（VR 起動中＝スティック／非 VR＝マウスホイール）
		const float MaxRPM = ResolveMaxAllowedRPM(WheelMaxRPM, StickMaxRPM);

		if (NewRPM < MinRPM)
		{
			RPMState = EHandSpeedState::TooSlow;
		}
		else if (NewRPM > MaxRPM)
		{
			RPMState = EHandSpeedState::TooFast;
		}
		else
		{
			RPMState = EHandSpeedState::Good;
		}

		// デバッグ表示と BP 向けに、判定区間を中心＋半幅の形式で表示値へ反映する
		TargetRPM = (MinRPM + MaxRPM) * 0.5f;
		RPMTolerance = (MaxRPM - MinRPM) * 0.5f;
	}
	else
	{
		// フォールバック：ReelState の閾値が読み取れない場合はデザイナー設定値で分類（旧挙動）
		if (NewRPM < TargetRPM - RPMTolerance)
		{
			RPMState = EHandSpeedState::TooSlow;
		}
		else if (NewRPM > TargetRPM + RPMTolerance)
		{
			RPMState = EHandSpeedState::TooFast;
		}
		else
		{
			RPMState = EHandSpeedState::Good;
		}
	}

	OnRPMChanged(CurrentRPM, RPMState);
}

void UFishFightMeterWidget::OnHandUpDownCompleted(bool bIsSuccess)
{
	// 2026.08.20 Lee：bReelUnlocked の切り替えは HandleFishingStateChanged の状態駆動に一本化した。
	// 旧実装（ここで解除 + NativeTick で再ロック検出）はイベント順序と Widget 生成タイミングに
	// 依存し、2セット目以降にロックが外れない現象の温床になっていた。
	UE_LOG(LogFishing, Log, TEXT("[FightMeter] OnHandUpDownCompleted: bIsSuccess=%d"), bIsSuccess ? 1 : 0);

	if (!bIsSuccess)
	{
		// 失敗（過速・過遅）時は表示をリセットする
		CycleCount = 0;
		FinalScore = 0;
		return;
	}

	// 表示を 5/5 等に更新（コンポーネントの CurrentUpAndDownCount は既に目標値に達している）
	if (HandUpDownState)
	{
		CycleCount = HandUpDownState->CurrentUpAndDownCount;
		FinalScore = HandUpDownState->FinalScore;
	}

	OnScoreChanged(FinalScore);
}

void UFishFightMeterWidget::HandleFishingStateChanged(UFishingStateComponentBase* NewState)
{
	if (!NewState)
	{
		return;
	}

	const EFishingPhase NewPhase = ResolvePhase(NewState);

	// RPM 表示のロック/解放はステート遷移から直接導出する（2026.08.20 Lee）。
	// Reel フェーズに入った瞬間に解放、それ以外のフェーズでは必ずロック。
	// 旧実装（HandUpDown 完了イベントで解除＋Tick で再ロック検出）はイベント順序と
	// Widget の生成タイミングに依存し、2セット目以降のロック残留の原因になり得たため廃止。
	const bool bNewReelUnlocked = (NewPhase == EFishingPhase::Reel);
	if (bNewReelUnlocked != bReelUnlocked)
	{
		UE_LOG(LogFishing, Log, TEXT("[FightMeter] RPM 表示ロック切替: %d → %d (Phase=%d)"),
			bReelUnlocked ? 1 : 0, bNewReelUnlocked ? 1 : 0, static_cast<int32>(NewPhase));
	}
	bReelUnlocked = bNewReelUnlocked;

	// 中間フェーズを飛ぶ遷移（例：リール失敗→結果）を汎用に検出
	const bool bSkipped = static_cast<int32>(NewPhase) > static_cast<int32>(PreviousPhase) + 1;
	ApplyPhase(NewPhase, NewState->GetStateDisplayName(), bSkipped);
}

EFishingPhase UFishFightMeterWidget::ResolvePhase(const UFishingStateComponentBase* State) const
{
	// ポインタ比較で各ステートコンポーネントを識別（HandUpDown／Reel は既存メンバを流用）
	if (State == HandUpDownState)
	{
		return EFishingPhase::HandUpDown;
	}
	if (State == ReelSimulator)
	{
		return EFishingPhase::Reel;
	}
	if (State == CatchingState)
	{
		return EFishingPhase::Catching;
	}
	if (State == ResultState)
	{
		return EFishingPhase::Result;
	}
	// 待機ステートまたは未知のステート
	return EFishingPhase::Ready;
}

void UFishFightMeterWidget::ApplyPhase(EFishingPhase NewPhase, const FString& PhaseName, bool bSkipped)
{
	PreviousPhase     = NewPhase;
	CurrentPhase      = NewPhase;
	CurrentPhaseIndex = static_cast<int32>(NewPhase);
	CurrentPhaseName  = PhaseName;
	bPhaseSkipped     = bSkipped;

	// BP イベント発火（OnArrowUpdated と同じプッシュ方式）
	OnPhaseChanged(CurrentPhase, CurrentPhaseName, bPhaseSkipped);

	// VR テスト用の画面デバッグ表示（VRPawn の [Fishing Mode] と同じ形式）
	if (GEngine)
	{
		const FString PhaseMsg = FString::Printf(
			TEXT("[Phase] %d/5 %s%s"),
			CurrentPhaseIndex + 1, *CurrentPhaseName,
			bPhaseSkipped ? TEXT(" (skipped)") : TEXT(""));
		GEngine->AddOnScreenDebugMessage(5, 3600.0f, FColor::Cyan, PhaseMsg);
	}
}
