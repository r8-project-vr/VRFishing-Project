// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BicycleBridgeComponent.generated.h"

class AVRPawn;
class UFishingWiredDeviceSubsystem;

/**
 * @brief 自転車デバイス（ASerial 0x03）を Reel 状態へ橋渡しするブリッジ。
 * @note データ経路はモードで分岐する（2026.10.29 Lee、イベント駆動方式）:
 *       - 模擬モード（従来経路・変更なし）:
 *           GetLatestBicycleSample()（正弦 RPS）→ 仮想角度 θ を毎フレーム進める
 *           → AVRPawn::InjectReelStickInput((cosθ, sinθ)) → SimulateReelByStick
 *       - 実機（完走周期イベント経路）:
 *           ワーカーがデコーダの完走周期 1 件ごとにイベント（実時刻・物理方向・周期の実効 RPM・
 *           接続代次）をスレッド安全キューへ積む → 本ブリッジが DrainBicycleCycleEvents() で
 *           取り出し、イベントの実時刻をゲーム時刻へ写像して
 *           UFishingReelStateComponent::ProcessDeviceCycle へ渡す。
 *           逆方向のイベントは推進も停止基準更新も行わない。測定中（初回周期）は推進のみ。
 *           RPM は周期ごとの実効値をそのまま判定・通知へ使う（角度積分で再計算しない）。
 *       履歴の破棄（補算しない）は次のときに行う: 再接続（代次変化） /
 *       Project Settings の方向反転設定の変更 / リールフェーズの入れ替わり（IsActive 立ち上がり）。
 *       いずれもイベントキューを drain して捨てる。低フレームレートでイベントが滞留しても
 *       キューが保持するため圈は欠損しない（満杯時はワーカー側が警告付きで最古を破棄）。
 *       Reel 状態が非アクティブのときは駆動しない。BP_XRPawn へ手動追加して使用する。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UBicycleBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBicycleBridgeComponent();

	// --- UActorComponent overrides ---
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ==================== 設定パラメータ ====================

	// 2026.09.03 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 旧パラメータ bRespectDeviceDirection / bInvertVirtualRotation は削除。
	// ゲーム側 SimulateReelByStick は「正方向の角差」のみ受理する（DeltaAngle>0 フィルタ）ため、
	// 方向 0x23=0 (逆転) を負の RPS として注入すると負角差が全てフィルタされ「完全無反応」になる。
	// → 有効 RPS は絶対値へ正規化して常に正方向へ進める方式へ変更（回転方向はログ/表示専用）。
	//   なお旧 bInvertVirtualRotation は毎フレーム θ の符号を反転する実装であり、
	//   反転ではなく ±振動（巨大な角差ジャンプ）を生むバグだったため、一緒に撤去した。
	// 2026.09.03 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief RPS に掛ける係数（装置のギヤ比などで実効速度を調整する場合に使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice", meta = (ClampMin = "0.0"))
	float RpsScale = 1.0f;

	/** @brief この秒数データが更新されなかったら注入を停止する [秒]（模擬経路で使用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice", meta = (ClampMin = "0.1"))
	float DataStaleTimeoutSeconds = 1.0f;

	/** @brief Debug 表示を有効にするか */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fishing|WiredDevice|Debug")
	bool bShowDebug = true;

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief イベントの物理方向をゲーム内前進とみなすか（方向反転設定を反映した純関数・テスト対象）。
	 *  @param bDirectionInverted Project Settings「方向反転」の現在値
	 *  @param EventDir イベントの物理方向（+1=正 / -1=逆） */
	static bool IsGameForwardEvent(bool bDirectionInverted, int32 EventDir)
	{
		return bDirectionInverted ? (EventDir < 0) : (EventDir > 0);
	}

	/** @brief 履歴（イベントキューと消費基準）を再構築すべきか（純関数・テスト対象）。
	 *  @note リール非アクティブ / 接続代次変化 / 方向設定変更 / IsActive 立ち上がりのいずれかで true */
	static bool ShouldRebuildBaseline(bool bReelActive, bool bWasReelActive,
		uint32 Generation, uint32 BaselineGeneration,
		bool bInverted, bool bBaselineInverted)
	{
		if (!bReelActive) { return true; }
		if (Generation != BaselineGeneration) { return true; }
		if (bInverted != bBaselineInverted) { return true; }
		if (bReelActive && !bWasReelActive) { return true; }
		return false;
	}

	// 2026.10.29 Lee(7) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 再構築時に drain したイベント批次を消費してよいか（純関数・テスト対象）。
	 *  @param bReelActive リールがアクティブか（再構築後の状態）
	 *  @param bWasReelActive リールが再構築直前までアクティブだったか
	 *  @param bGenerationChanged 再構築の原因に接続代次の変化（＝再接続）を含むか
	 *  @param bDirectionChanged 再構築の原因に方向設定の変更を含むか
	 *  @return true = 批次を消費へ進める（旧代次の混入は消費側の代次フィルタで除外）
	 *  @note 安全側の設計（2026.10.29 Lee(8) 改訂）:
	 *        - 消費を許すのは「純粋な再接続」（活性が継続し、方向設定が不変で、
	 *          代次のみが変化した場合）のみ。この場合の批次はワーカーが握手直後に
	 *          旧キューをクリアしているため「代次変化境界の後」であることが証明できる。
	 *        - フェーズの立ち上がりが絡む批次は、非アクティブ期間の積压（履歴）と
	 *          握手後の新データを区別できないため全破棄する。
	 *        - 方向設定の変更が絡む批次は、旧方向语义で完走した圈を新方向へ補算する
	 *          危険があるため全破棄する。
	 *        すなわち同帧混合（階段/方向の変更と再接続が重なった場合）は、境界後と
	 *        証明できるイベントがないため安全側の破棄を優先する（既知の制限）。 */
	static bool ShouldConsumeEventsOnRebuild(bool bReelActive, bool bWasReelActive,
		bool bGenerationChanged, bool bDirectionChanged)
	{
		return bReelActive && bWasReelActive && bGenerationChanged && !bDirectionChanged;
	}
	// 2026.10.29 Lee(7) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

private:
	/** @brief オーナーからサブシステムを取得する（毎フレーム軽量取得） */
	UFishingWiredDeviceSubsystem* FindSubsystem() const;

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/** @brief 模擬モードの従来経路（RPS→仮想角度 θ→InjectReelStickInput。挙動は変更なし） */
	void TickSimulator(UFishingWiredDeviceSubsystem* Subsystem, AVRPawn* Pawn, float DeltaTime);

	/** @brief 実機の完走周期イベント経路（実時刻写像＋周期ごとの標定 RPM で Reel へ駆動） */
	void TickDevice(UFishingWiredDeviceSubsystem* Subsystem, AVRPawn* Pawn, float DeltaTime);
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/** @brief 仮想スティックの現在角度[rad]（RPS に応じて毎フレーム進める。模擬経路で使用） */
	float VirtualAngleRad = 0.0f;

	// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	// --- イベント消費の基準（再構築時は履歴を補算しない） ---
	/** 基準構築時の接続代次（変化したらイベント履歴を破棄して再構築） */
	uint32 BaselineGeneration = 0;
	/** 基準構築時の方向反転設定値（変化したらイベント履歴を破棄して再構築） */
	bool bBaselineDirectionInverted = false;
	/** 前フレームのリール状態アクティブ値（false→true の立ち上がり＝フェーズ入替で再構築） */
	bool bWasReelActive = false;
	// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
};
