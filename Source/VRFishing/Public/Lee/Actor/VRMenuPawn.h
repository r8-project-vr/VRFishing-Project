// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "VRMenuPawn.generated.h"

class USceneComponent;
class UCameraComponent;
class UMotionControllerComponent;
class UWidgetInteractionComponent;
class UInputMappingContext;
class UInputAction;
class UFishingMenuNavigatorComponent;
class UFishingLoadApplierComponent;

/**
 * @brief タイトル（LV_Aquarium）／リザルト（LV_GameResult）用の軽量 VR ポーン。
 * @note 釣りステートマシン・手部モデル・テレポート等の本編機能を持たず、
 *       VR カメラ＋コントローラ＋レーザー（Widget 直押し）＋タイトルメニュー操作
 *       （スティック＋A ボタン確定）のみを提供する。
 * @note 子 BP「BP_MenuPawn」を経由して運用する。本編マップ（LV_MainGame）は
 *       従来どおり BP_XRPawn を使用するため、本クラスは AVRPawn / BP_XRPawn を変更しない。
 * @note レーザー直押しは Grab（側面ボタン）で行う。A ボタン（IA_DebugTitleConfirm）は
 *       MenuNavigator のフォーカス確定専用とし、二重実行を防ぐ。
 */
UCLASS()
class VRFISHING_API AVRMenuPawn : public APawn
{
    GENERATED_BODY()

public:
    AVRMenuPawn();

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** BeginPlay で LocalPlayer へ追加する既定 IMC（IA_DebugReelStick / IA_DebugTitleConfirm を含む XR テンプレート標準） */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    /** 既定 IMC を追加するか（IMC_GameResult のみを使用するレベルでは false にする） */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    bool bAddDefaultMappingContext = true;

    /** レーザー直押し（押下）用アクション（左）。既定で IA_Grab_Left_Pressed を読み込む */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> GrabPressActionLeft;

    /** レーザー直押し（解放）用アクション（左）。既定で IA_Grab_Left_Released を読み込む */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> GrabReleaseActionLeft;

    /** レーザー直押し（押下）用アクション（右）。既定で IA_Grab_Right_Pressed を読み込む */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> GrabPressActionRight;

    /** レーザー直押し（解放）用アクション（右）。既定で IA_Grab_Right_Released を読み込む */
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> GrabReleaseActionRight;

    /** VR 追跡起点（ルート） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|MenuPawn")
    TObjectPtr<USceneComponent> VROrigin;

    /** HMD に追従するカメラ */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|MenuPawn")
    TObjectPtr<UCameraComponent> Camera;

    /** 左コントローラ（レーザー始点） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|MenuPawn")
    TObjectPtr<UMotionControllerComponent> MotionControllerLeftAim;

    /** 右コントローラ（レーザー始点） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|MenuPawn")
    TObjectPtr<UMotionControllerComponent> MotionControllerRightAim;

    /** 左手レーザー（Widget 直押し用） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|MenuPawn")
    TObjectPtr<UWidgetInteractionComponent> WidgetInteractionLeft;

    /** 右手レーザー（Widget 直押し用） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|MenuPawn")
    TObjectPtr<UWidgetInteractionComponent> WidgetInteractionRight;

    /** タイトルメニュー操作（スティック＋A 確定）。MenuActorClass は子 BP 側で BP_TitleMenu を指定する */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|MenuPawn")
    TObjectPtr<UFishingMenuNavigatorComponent> MenuNavigator;

    /** 運動時間・負荷プリセットの GameMode／Subsystem への適用（タイトル残り時間表示の数値源） */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|MenuPawn")
    TObjectPtr<UFishingLoadApplierComponent> LoadApplier;

private:
    /** @brief Grab 押下でレーザー先の Widget に左クリック押下を送る */
    void HandleGrabPressed(const FInputActionInstance& Instance, UWidgetInteractionComponent* WidgetInteraction);

    /** @brief Grab 解放でレーザー先の Widget に左クリック解放を送る */
    void HandleGrabReleased(const FInputActionInstance& Instance, UWidgetInteractionComponent* WidgetInteraction);
};
