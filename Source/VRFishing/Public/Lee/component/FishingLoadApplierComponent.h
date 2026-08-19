// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FishingLoadApplierComponent.generated.h"

/**
 * @brief 負荷設定を各ゲームコンポーネントへ適用するコンポーネント。
 * @note BP_XRPawn に手動で追加して使用する。BeginPlay 時に UFishingLoadSettingsSubsystem の値を
 *       各ステートコンポーネント・GameMode へ書き込む（レベル遷移で破棄されないサブシステム値の適用窓口）。
 * @note 上下運動・リールの目標回数はプリセット（Medium 含む）に関わらず無条件で上書きする
 *       （サブシステムを唯一の正とする）。運動時間のみ未設定時は GameMode アセット値を尊重する。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingLoadApplierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFishingLoadApplierComponent();

	/// @brief ゲーム開始時にサブシステムの負荷設定を各コンポーネントへ適用する
	virtual void BeginPlay() override;

	/**
	 * @brief 他開発者の protected UPROPERTY への反射書き込み（int32 版）。
	 * @note チーム規約により他人のコードを変更できないため、setter を追加せず
	 *       プロパティ名で直接書き込む（対象: UFishingReelStateComponent::TargetRevolutionCount）。
	 * @param Object 書き込み先オブジェクト
	 * @param PropertyName プロパティの FName
	 * @param Value 書き込む値
	 * @return 書き込み成功で true
	 */
	static bool WriteProtectedIntProperty(UObject* Object, const TCHAR* PropertyName, int32 Value);

	/**
	 * @brief 他開発者の protected UPROPERTY への反射書き込み（float 版）。
	 * @note 対象: AFishingGameModeBase::TotalGameTime。
	 * @param Object 書き込み先オブジェクト
	 * @param PropertyName プロパティの FName
	 * @param Value 書き込む値
	 * @return 書き込み成功で true
	 */
	static bool WriteProtectedFloatProperty(UObject* Object, const TCHAR* PropertyName, float Value);

private:
	/** @brief サブシステムの値を収集して各コンポーネントへ反映する */
	void ApplyLoadSettings();
};
