// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Lee/component/FishingStateHandUpDown.h"
#include "Lee/component/HandHeightDetectorComponent.h"
#include "Tanimura/Component/FishingStateManagerComponent.h"
#include "Tanimura/Component/FishingReadyStateComponent.h"
#include "VRFishingLog.h"
#include "GameFramework/Actor.h"

UFishingStateHandUpDown::UFishingStateHandUpDown()
{
	// ステート単体でのTickは無効化（Manager経由でUpdateStateが呼ばれる）
	PrimaryComponentTick.bCanEverTick = false;
}

void UFishingStateHandUpDown::EnterState()
{
	Super::EnterState();

	// カウンターと状態フラグを初期化
	CurrentUpAndDownCount = 0;
	bIsHandAtTop = false;
	bIsCompleted = false;

	// 矢印状態を初期化
	ArrowPosition = 0.0f;
	ArrowState = EFishArrowState::MovingUp;

	// スコア状態を初期化
	CurrentScore = 0.0f;
	FinalScore = 0.0f;
	TotalQualitySum = 0.0f;
	TotalFrameCount = 0;

	// センサを所有者から取得（未取得の場合のみ）
	if (!Detector.IsValid() && GetOwner())
	{
		Detector = GetOwner()->FindComponentByClass<UHandHeightDetectorComponent>();
	}

	// ステートマネージャと Wait ステートを所有者から取得（未取得の場合のみ。失敗時の復帰用）
	if (!StateManager.IsValid() && GetOwner())
	{
		StateManager = GetOwner()->FindComponentByClass<UFishingStateManagerComponent>();
	}
	if (!WaitState.IsValid() && GetOwner())
	{
		WaitState = GetOwner()->FindComponentByClass<UFishingReadyStateComponent>();
	}

	// 失敗検知状態を初期化
	FailTimeAccumulated = 0.0f;
	bIsFailed = false;
}

void UFishingStateHandUpDown::UpdateState(float DeltaTime)
{
	Super::UpdateState(DeltaTime);

	// 処理完了済みなら判定を行わない（二重発火防止）
	if (bIsCompleted)
	{
		return;
	}

	// センサが取得できていない場合は判定不可
	if (!Detector.IsValid())
	{
		return;
	}

	const float HandHeightPercent = Detector->HandHeightPercent;
	const float HandPercentSpeed = Detector->HandPercentSpeed;

	// ==================== 1. 矢印状態の更新 ====================
	TickArrow(DeltaTime, HandHeightPercent);

	// ==================== 2. スコア計算（毎フレーム） ====================

	// 矢印の位相に応じて誤差の定義を切り替える
	//  - 移動位相: 手の垂直速度と推奨速度の差（符号付きのため、方向が逆だと大きく減点）
	//  - 待機位相: 手の位置と矢印位置の差（手が端点に到達しているかを評価。中間で停止する不正を防止）
	float Error;
	switch (ArrowState)
	{
	case EFishArrowState::MovingUp:
		Error = FMath::Abs(HandPercentSpeed - RecommendedSpeed);
		break;

	case EFishArrowState::MovingDown:
		Error = FMath::Abs(HandPercentSpeed + RecommendedSpeed);
		break;

	case EFishArrowState::WaitingAtTop:
	case EFishArrowState::WaitingAtBottom:
		Error = FMath::Abs(HandHeightPercent - ArrowPosition);
		break;

	default:
		Error = 1.0f;
		break;
	}

	const float MatchQuality = CalcMatchQuality(Error);

	TotalQualitySum += MatchQuality;
	TotalFrameCount++;

	const float Alpha = FMath::Clamp(ScoreSmoothingFactor * DeltaTime, 0.0f, 1.0f);
	CurrentScore = FMath::Lerp(CurrentScore, MatchQuality * 100.0f, Alpha);

	// ==================== 3. 腕上下回数の判定 ====================

	if (!bIsHandAtTop && HandHeightPercent >= UpperThresholdPercent)
	{
		// 手が上端領域に到達
		bIsHandAtTop = true;
	}
	else if (bIsHandAtTop && HandHeightPercent <= LowerThresholdPercent)
	{
		// 上端に到達した状態から下端領域まで下がったため 1 回とカウント
		bIsHandAtTop = false;
		CurrentUpAndDownCount++;

		// 目標回数に達したらステート完了を通知
		if (CurrentUpAndDownCount >= TargetUpAndDownCount)
		{
			// 最終スコアを全フレームの真の平均から算出
			FinalScore = (TotalFrameCount > 0) ? (TotalQualitySum / TotalFrameCount) * 100.0f : 0.0f;

			bIsCompleted = true;
			OnFishingStateCompleted.Broadcast(true);
		}
	}

	// ==================== 4. 失敗検知（過速・過遅） ====================

	// 低品質フレームの連続時間を累積（回復でリセット）
	if (MatchQuality < FailQualityThreshold)
	{
		FailTimeAccumulated += DeltaTime;
	}
	else
	{
		FailTimeAccumulated = 0.0f;
	}

	// 失敗発動（bIsCompleted ガード: 乱れ動作中にカウントが偶然達した場合の二重発火防止。成功優先）
	if (!bIsCompleted && FailTimeAccumulated >= FailTimeSeconds)
	{
		HandleFailure();
	}
}

void UFishingStateHandUpDown::ExitState()
{
	Super::ExitState();

	// 変数リセット
	CurrentUpAndDownCount = 0;
	bIsHandAtTop = false;
	bIsCompleted = false;

	// 矢印状態をリセット
	ArrowPosition = 0.0f;
	ArrowState = EFishArrowState::MovingUp;

	// スコア状態をリセット
	CurrentScore = 0.0f;
	FinalScore = 0.0f;
	TotalQualitySum = 0.0f;
	TotalFrameCount = 0;

	// 失敗検知状態をリセット
	FailTimeAccumulated = 0.0f;
	bIsFailed = false;
}

FString UFishingStateHandUpDown::GetStateDisplayName() const
{
	return TEXT("上下運動");
}

float UFishingStateHandUpDown::CalcMatchQuality(float Error) const
{
	// 誤差が perfect 以下なら満点、fail 以上なら 0 点、間は線形補間
	if (Error <= ScoringPerfectThreshold)
	{
		return 1.0f;
	}
	if (Error >= ScoringFailThreshold)
	{
		return 0.0f;
	}
	return 1.0f - (Error - ScoringPerfectThreshold) / (ScoringFailThreshold - ScoringPerfectThreshold);
}

void UFishingStateHandUpDown::HandleFailure()
{
	// 二重発火防止
	bIsCompleted = true;
	bIsFailed = true;

	// 失敗時も最終スコアを残す（UI表示用）
	FinalScore = (TotalFrameCount > 0) ? (TotalQualitySum / TotalFrameCount) * 100.0f : 0.0f;

	UE_LOG(LogFishing, Warning, TEXT("[FishingState] HandUpDown Failed: 過速または過遅が %.1f 秒継続 (FinalScore=%.0f)"),
		FailTimeSeconds, FinalScore);

	// 失敗を外部へ通知（魚の逃走など、将来のリスナー用）。
	// ※Broadcast は ChangeState より前に実行する: ChangeState 内の ExitState() が全出力変数を
	//   リセットするため、先に通知しないとリスナーがゼロ値しか読めない。
	OnFishingStateCompleted.Broadcast(false);

	// ゲームを止めないよう状態機を Wait へ戻す（ExitState で本状態の全変数がリセットされる）
	if (StateManager.IsValid() && WaitState.IsValid())
	{
		StateManager->ChangeState(WaitState.Get());
	}
	else
	{
		UE_LOG(LogFishing, Error, TEXT("[FishingState] HandUpDown Failed: StateManager または WaitState が無効のため復帰不可"));
	}
}

void UFishingStateHandUpDown::TickArrow(float InDeltaTime, float HandPercent)
{
	switch (ArrowState)
	{
	case EFishArrowState::MovingUp:
	{
		ArrowPosition += RecommendedSpeed * InDeltaTime;
		if (ArrowPosition >= 1.0f)
		{
			ArrowPosition = 1.0f;
			ArrowState = EFishArrowState::WaitingAtTop;
		}
		break;
	}

	case EFishArrowState::WaitingAtTop:
	{
		if (HandPercent >= (1.0f - ArrowWaitThreshold))
		{
			ArrowState = EFishArrowState::MovingDown;
		}
		break;
	}

	case EFishArrowState::MovingDown:
	{
		ArrowPosition -= RecommendedSpeed * InDeltaTime;
		if (ArrowPosition <= 0.0f)
		{
			ArrowPosition = 0.0f;
			ArrowState = EFishArrowState::WaitingAtBottom;

		}
		break;
	}

	case EFishArrowState::WaitingAtBottom:
	{
		if (HandPercent <= ArrowWaitThreshold)
		{
			ArrowState = EFishArrowState::MovingUp;
		}
		break;
	}
	}
}
