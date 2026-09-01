// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#include "Lee/Actor/VRMenuPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Lee/component/FishingMenuNavigatorComponent.h"
#include "Lee/component/FishingLoadApplierComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "VRFishingLog.h"

AVRMenuPawn::AVRMenuPawn()
{
    PrimaryActorTick.bCanEverTick = false;

    // ルート：VR 追跡起点（XR テンプレートの BP_XRPawn と同じ構成）
    VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
    SetRootComponent(VROrigin);

    // HMD に追従するカメラ（テンプレート標準は LockToHmd）
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(VROrigin);
    Camera->bLockToHmd = true;

    // レーザー始点用コントローラ（Aim のみ作成。本編の Grip 相当は不要）
    MotionControllerLeftAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerLeftAim"));
    MotionControllerLeftAim->SetupAttachment(VROrigin);
    MotionControllerLeftAim->MotionSource = FName(TEXT("LeftAim"));

    MotionControllerRightAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionControllerRightAim"));
    MotionControllerRightAim->SetupAttachment(VROrigin);
    MotionControllerRightAim->MotionSource = FName(TEXT("RightAim"));

    // Widget 直押しレーザー（InteractionSource は既定の World ＝ コンポーネント位置から前方へレイを飛ばす）
    WidgetInteractionLeft = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractionLeft"));
    WidgetInteractionLeft->SetupAttachment(MotionControllerLeftAim);

    WidgetInteractionRight = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractionRight"));
    WidgetInteractionRight->SetupAttachment(MotionControllerRightAim);

    // タイトルメニュー操作（構造体既定の NavRows・IA で動作。MenuActorClass は子 BP 側で設定）
    MenuNavigator = CreateDefaultSubobject<UFishingMenuNavigatorComponent>(TEXT("FishingMenuNavigator"));

    // 運動時間・負荷プリセットの適用（タイトル残り時間表示の数値源）
    LoadApplier = CreateDefaultSubobject<UFishingLoadApplierComponent>(TEXT("FishingLoadApplier"));

    // XR テンプレート標準 IMC（IA_DebugReelStick / IA_DebugTitleConfirm を含む）。
    // BP_XRPawn のイベントグラフが担っている AddMappingContext を C++ 側で代行する。
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCFinder(
        TEXT("/Game/XRFramework/Input/IMC_Default.IMC_Default"));
    if (IMCFinder.Succeeded())
    {
        DefaultMappingContext = IMCFinder.Object;
    }

    // レーザー直押し用アクション（Grab 押下／解放）。A ボタン確定と入力を分離し二重実行を防ぐ
    static ConstructorHelpers::FObjectFinder<UInputAction> GrabLeftPressedFinder(
        TEXT("/Game/XRFramework/Input/Actions/IA_Grab_Left_Pressed.IA_Grab_Left_Pressed"));
    static ConstructorHelpers::FObjectFinder<UInputAction> GrabLeftReleasedFinder(
        TEXT("/Game/XRFramework/Input/Actions/IA_Grab_Left_Released.IA_Grab_Left_Released"));
    static ConstructorHelpers::FObjectFinder<UInputAction> GrabRightPressedFinder(
        TEXT("/Game/XRFramework/Input/Actions/IA_Grab_Right_Pressed.IA_Grab_Right_Pressed"));
    static ConstructorHelpers::FObjectFinder<UInputAction> GrabRightReleasedFinder(
        TEXT("/Game/XRFramework/Input/Actions/IA_Grab_Right_Released.IA_Grab_Right_Released"));
    if (GrabLeftPressedFinder.Succeeded())
    {
        GrabPressActionLeft = GrabLeftPressedFinder.Object;
    }
    if (GrabLeftReleasedFinder.Succeeded())
    {
        GrabReleaseActionLeft = GrabLeftReleasedFinder.Object;
    }
    if (GrabRightPressedFinder.Succeeded())
    {
        GrabPressActionRight = GrabRightPressedFinder.Object;
    }
    if (GrabRightReleasedFinder.Succeeded())
    {
        GrabReleaseActionRight = GrabRightReleasedFinder.Object;
    }
}

/** @brief 開始処理。追跡起点を座位基準（Local）へ揃え、既定 IMC を LocalPlayer へ追加する */
void AVRMenuPawn::BeginPlay()
{
    Super::BeginPlay();

    // 追跡起点を XR テンプレート（BP_XRPawn の BeginPlay）と同じ座位基準へ揃える
    //（UE5.8 では旧 Eye 相当は EHMDTrackingOrigin::Local に改名されている）
    UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Type::Local);

    // 既定 IMC を追加（メニューのスティック／A 確定の入力源）
    if (bAddDefaultMappingContext && DefaultMappingContext)
    {
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Subsystem->AddMappingContext(DefaultMappingContext, 0);
                }
            }
        }
        else
        {
            UE_LOG(LogFishing, Warning, TEXT("[MenuPawn] PlayerController が取得できないため既定 IMC を追加できません"));
        }
    }
}

/** @brief 終了処理。BeginPlay で追加した既定 IMC を明示的に解除する（次レベルへの残留を防ぐ） */
void AVRMenuPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 追加した IMC を明示的に外す（将来の再利用場面での残留を防ぐ。Level 破棄時は取得失敗しても問題ない）
    if (DefaultMappingContext)
    {
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    Subsystem->RemoveMappingContext(DefaultMappingContext);
                }
            }
        }
    }

    Super::EndPlay(EndPlayReason);
}

/**
 * @brief 入力バインド。Grab（側面ボタン）の押下/解放を左右両手のレーザーへバインドする。
 * @note BindAction のペイロードで対象の WidgetInteractionComponent を渡すため、
 *       左右でハンドラーを共用できる（A ボタン確定は MenuNavigator 側が担当し分離済み）。
 */
void AVRMenuPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Grab（側面ボタン）押下／解放 → レーザー先の Widget へ左クリックを送る
    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (GrabPressActionLeft)
        {
            EnhancedInput->BindAction(GrabPressActionLeft, ETriggerEvent::Started, this,
                &AVRMenuPawn::HandleGrabPressed, WidgetInteractionLeft.Get());
        }
        if (GrabReleaseActionLeft)
        {
            EnhancedInput->BindAction(GrabReleaseActionLeft, ETriggerEvent::Started, this,
                &AVRMenuPawn::HandleGrabReleased, WidgetInteractionLeft.Get());
        }
        if (GrabPressActionRight)
        {
            EnhancedInput->BindAction(GrabPressActionRight, ETriggerEvent::Started, this,
                &AVRMenuPawn::HandleGrabPressed, WidgetInteractionRight.Get());
        }
        if (GrabReleaseActionRight)
        {
            EnhancedInput->BindAction(GrabReleaseActionRight, ETriggerEvent::Started, this,
                &AVRMenuPawn::HandleGrabReleased, WidgetInteractionRight.Get());
        }
    }
}

void AVRMenuPawn::HandleGrabPressed(const FInputActionInstance& Instance, UWidgetInteractionComponent* WidgetInteraction)
{
    // レーザーが Widget を指していれば左クリック押下として通知する
    if (WidgetInteraction)
    {
        WidgetInteraction->PressPointerKey(FKey(TEXT("LeftMouseButton")));
    }
}

void AVRMenuPawn::HandleGrabReleased(const FInputActionInstance& Instance, UWidgetInteractionComponent* WidgetInteraction)
{
    // レーザーが Widget を指していれば左クリック解放として通知する
    if (WidgetInteraction)
    {
        WidgetInteraction->ReleasePointerKey(FKey(TEXT("LeftMouseButton")));
    }
}
