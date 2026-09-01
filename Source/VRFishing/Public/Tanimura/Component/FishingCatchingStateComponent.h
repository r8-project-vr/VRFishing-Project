// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "FishingCatchingStateComponent.generated.h"

// 2026.08.31 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
class UHandHeightDetectorComponent;
// 2026.08.31 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

/**
 * 魚を釣り上げる動作を管理するステートコンポーネント
 * 2026.08.31 Lee: 手を頭上まで上げる（収竿）動作で完了を通知する正式実装に変更
 * （旧: アクティブ化から規定時間経過後に完了を通知する仮処理）
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingCatchingStateComponent : public UFishingStateComponentBase
{
	GENERATED_BODY()

public:
	UFishingCatchingStateComponent();

	// 基底クラスオーバーライド
	virtual void EnterState() override;
	virtual void UpdateState(float DeltaTime) override;
	virtual void ExitState() override;
	// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	virtual FString GetStateDisplayName() const override; // ステートの表示名（ログ・UI表示用）
	// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

protected:
	// 2026.08.31 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	//// 完了とみなすまでの経過時間（秒）
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fishing|Catching")
	//float RequiredHoldTime = 1.0f;

	// 手を頭上どれだけ上げたら収竿（釣り上げ完了）とみなすか（cm、正値で指定。
	// Ready の RequiredDownDistance（下げ判定）と対称の上げ判定）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fishing|Catching")
	float RequiredUpDistance = 20.0f;

	// 手部運動センサへの参照（未取得時は所有者から FindComponentByClass で取得）
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Fishing|Catching")
	TWeakObjectPtr<UHandHeightDetectorComponent> HandHeightDetector;
	// 2026.08.31 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

private:
	// ステート開始からの経過時間（旧・仮処理の計時用。2026.08.31 Lee の置換により未使用）
	float ElapsedTime;

	// 完了フラグ
	bool bIsCompleted;
};
