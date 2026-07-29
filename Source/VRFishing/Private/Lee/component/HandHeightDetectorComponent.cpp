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
	// 常駐センサとして毎フレーム自律 Tick する
	PrimaryComponentTick.bCanEverTick = true;
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

void UHandHeightDetectorComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ==================== 外部データソース（BLE IMU等）パス ====================
	if (bUseExternalData)
	{
		// @brief HandRef の代わりに注入された値を使用
		HandHeightPercent = ExternalHeightPercent;

		// @brief GetHandHeightBelowHeadCm() 用に仮想 Z 座標を計算
		if (CameraRef.IsValid())
		{
			CachedHeadZ = CameraRef->GetComponentLocation().Z;
			// HandHeightPercent(0=下,1=上) を BottomOffset〜-TopOffset にマッピング
			const float VirtualZOffset = FMath::Lerp(BottomOffset, -TopOffset, ExternalHeightPercent);
			CachedHandZ = CachedHeadZ - VirtualZOffset;
		}

		// @brief 速度の処理（注入された速度が有効な場合のみ）
		if (ExternalSpeed >= 0.0f)
		{
			CurrentHandSpeed = ExternalSpeed;

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

		// @brief Debug 表示（外部データモード時）
		if (bShowDebug && GEngine)
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
				TEXT("[EXT] Hand: %.2f%% | Speed: %.1f [%s]"),
				HandHeightPercent, CurrentHandSpeed, StateText);

			GEngine->AddOnScreenDebugMessage(1, 0.0f, StateColor, DebugMsg);
		}
		return;
	}

	// ==================== OpenXR HandTracking パス（変更禁止） ====================

	// @brief 両方の必須コンポーネントが取得されていることを確認
	if (!CameraRef.IsValid() || !HandRef.IsValid())
	{
		return;
	}

	// @brief ワールド座標系での手の位置を取得
	const FVector CurrentHandLocation = HandRef->GetComponentLocation();
	const float HeadZ = CameraRef->GetComponentLocation().Z;
	const float HandZ = CurrentHandLocation.Z;

	// cm 意味インターフェース用にキャッシュ
	CachedHeadZ = HeadZ;
	CachedHandZ = HandZ;

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

	// @brief 次のフレームのために現在位置を保存
	PreviousHandLocation = CurrentHandLocation;
	bHasPreviousLocation = true;

	// ==================== Debug 表示 ====================

	if (bShowDebug && GEngine)
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

float UHandHeightDetectorComponent::GetHandHeightBelowHeadCm() const
{
	// 正 = 手が頭より下にある
	return CachedHeadZ - CachedHandZ;
}

void UHandHeightDetectorComponent::SetExternalHandData(float InHeightPercent, float InSpeed)
{
	ExternalHeightPercent = FMath::Clamp(InHeightPercent, 0.0f, 1.0f);
	ExternalSpeed = InSpeed;
	bUseExternalData = true;
}

void UHandHeightDetectorComponent::ClearExternalHandData()
{
	bUseExternalData = false;
	ExternalHeightPercent = 0.0f;
	ExternalSpeed = 0.0f;
}
