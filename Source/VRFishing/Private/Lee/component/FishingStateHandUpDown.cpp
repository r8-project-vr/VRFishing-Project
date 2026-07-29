// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.


#include "Lee/component/FishingStateHandUpDown.h"
#include "Lee/component/HandHeightDetectorComponent.h"
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

	// ==================== 1. 矢印状態の更新 ====================
	TickArrow(DeltaTime, HandHeightPercent);

	// ==================== 2. スコア計算（毎フレーム） ====================
	{
		const float PositionError = FMath::Abs(HandHeightPercent - ArrowPosition);
		float MatchQuality;
		if (PositionError <= ScoringPerfectThreshold)
		{
			MatchQuality = 1.0f;
		}
		else if (PositionError >= ScoringFailThreshold)
		{
			MatchQuality = 0.0f;
		}
		else
		{
			MatchQuality = 1.0f - (PositionError - ScoringPerfectThreshold) / (ScoringFailThreshold - ScoringPerfectThreshold);
		}

		TotalQualitySum += MatchQuality;
		TotalFrameCount++;

		const float Alpha = FMath::Clamp(ScoreSmoothingFactor * DeltaTime, 0.0f, 1.0f);
		CurrentScore = FMath::Lerp(CurrentScore, MatchQuality * 100.0f, Alpha);
	}

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
