// Fill out your copyright notice in the Description page of Project Settings.


#include "Lee/component/HandHeightDetectorComponent.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "Math/UnrealMathUtility.h"
// @brief Debug出力に必要なヘッダーをインクルード
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

// Sets default values for this component's properties
UHandHeightDetectorComponent::UHandHeightDetectorComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
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


// Called every frame
void UHandHeightDetectorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// @brief 両方の必須コンポーネントが取得されていることを確認
	if (!CameraRef.IsValid() || !HandRef.IsValid())
	{
		return;
	}

	// @brief ワールド座標系での高さを取得
	const float HeadZ = CameraRef->GetComponentLocation().Z;
	const float HandZ = HandRef->GetComponentLocation().Z;

	// @brief 絶対高さの範囲を計算
	const float MinZ = HeadZ - BottomOffset;
	const float MaxZ = HeadZ + TopOffset;

	// @brief Unreal C++の範囲マッピング関数を使用して、[0.0, 1.0]の範囲にマッピングおよびクランプ
	HandHeightPercent = FMath::GetMappedRangeValueClamped(
		FVector2D(MinZ, MaxZ),
		FVector2D(0.0f, 1.0f),
		HandZ
	);

	if (bShowDebug)
	{
		// @brief 画面左上のテキストデバッグ
		if (GEngine)
		{
			FString DebugMsg = FString::Printf(TEXT("Hand Percent: %.2f | Hand Z: %.1f | Head Z: %.1f"),
				HandHeightPercent, HandZ, HeadZ);

			// @brief パラメータ説明：Key（1で前回のメッセージを上書き）, 持続時間（0で毎フレーム更新）, 色, テキスト内容
			GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Cyan, DebugMsg);
		}
	}
}

