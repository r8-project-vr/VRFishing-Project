// Fill out your copyright notice in the Description page of Project Settings.


#include "Lee/component/HandHeightDetectorComponent.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "Math/UnrealMathUtility.h"
// @brief Debug出力に必要なヘッダーをインクルード
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

UHandHeightDetectorComponent::UHandHeightDetectorComponent()
{
	// ステート単体でのTickは無効化（Manager経由でUpdateStateが呼ばれる）
	PrimaryComponentTick.bCanEverTick = false;
}


// Called when the game starts
void UHandHeightDetectorComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// @brief ブループリントで手動指定されていない場合、自動でカメラコンポーネントを取得する
	if (!CameraRef.IsValid())
	{
		CameraRef = Cast<UCameraComponent>(Owner->GetComponentByClass(UCameraComponent::StaticClass()));
	}
	
}

void UHandHeightDetectorComponent::EnterState()
{
	Super::EnterState();

	// カウンターと状態フラグを初期化
	CurrentUpAndDownCount = 0;
	bIsHandAtTop = false;
	bIsCompleted = false;
	bHasPreviousLocation = false;
}

void UHandHeightDetectorComponent::UpdateState(float DeltaTime)
{
	Super::UpdateState(DeltaTime);

	// 処理完了済みなら判定を行わない（二重発火防止）
	if (bIsCompleted)
	{
		return;
	}

	// @brief 両方の必須コンポーネントが取得されていることを確認
	if (!CameraRef.IsValid() || !HandRef.IsValid())
	{
		return;
	}

	// @brief ワールド座標系での手の位置を取得
	const FVector CurrentHandLocation = HandRef->GetComponentLocation();
	const float HeadZ = CameraRef->GetComponentLocation().Z;
	const float HandZ = CurrentHandLocation.Z;

	// @brief 絶対高さの範囲を計算
	const float MinZ = HeadZ - BottomOffset;
	const float MaxZ = HeadZ + TopOffset;

	// @brief Unreal C++の範囲マッピング関数を使用して、[0.0, 1.0]の範囲にマッピングおよびクランプ
	HandHeightPercent = FMath::GetMappedRangeValueClamped(
		FVector2D(MinZ, MaxZ),
		FVector2D(0.0f, 1.0f),
		HandZ
	);

	// ==================== 移動速度の計算 ====================

	if (bHasPreviousLocation)
	{
		// @brief 前フレームからの移動距離を DeltaTime で割って速度 (cm/s) を算出
		const float Distance = FVector::Dist(CurrentHandLocation, PreviousHandLocation);
		CurrentHandSpeed = Distance / DeltaTime;

		// @brief 速度を閾値と比較して状態を判定
		if (CurrentHandSpeed < MinGoodSpeed)
		{
			HandSpeedState = EHandSpeedState::TooSlow;
		}
		else if (CurrentHandSpeed > MaxGoodSpeed)
		{
			HandSpeedState = EHandSpeedState::TooFast;
		}
		else
		{
			HandSpeedState = EHandSpeedState::Good;
		}
	}

	// ==================== 腕上下回数の判定 ====================

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
			bIsCompleted = true;
			OnFishingStateCompleted.Broadcast(true);
		}
	}

	// @brief 次のフレームのために現在位置を保存
	PreviousHandLocation = CurrentHandLocation;
	bHasPreviousLocation = true;

	// ==================== Debug 表示 ====================

	if (bShowDebug)
	{
		// @brief 画面左上のテキストデバッグ
		if (GEngine)
		{
			const TCHAR* StateText = TEXT("???");
			FColor StateColor = FColor::White;
			switch (HandSpeedState)
			{
			case EHandSpeedState::TooSlow:	StateText = TEXT("TOO SLOW");  StateColor = FColor::Yellow;  break;
			case EHandSpeedState::Good:		StateText = TEXT("GOOD");      StateColor = FColor::Green;   break;
			case EHandSpeedState::TooFast:	StateText = TEXT("TOO FAST");  StateColor = FColor::Red;     break;
			}

			FString DebugMsg = FString::Printf(
				TEXT("Hand: %.2f%% | Speed: %.1f cm/s [%s] | Hand Z: %.1f | Head Z: %.1f"),
				HandHeightPercent, CurrentHandSpeed, StateText, HandZ, HeadZ);

			GEngine->AddOnScreenDebugMessage(1, 0.0f, StateColor, DebugMsg);
		}
	}
}

void UHandHeightDetectorComponent::ExitState()
{
	Super::ExitState();

	// 変数リセット
	CurrentUpAndDownCount = 0;
	bIsHandAtTop = false;
	bIsCompleted = false;
}

