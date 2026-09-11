// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tanimura/Component/FishingStateComponentBase.h"
#include "FishingReelStateComponent.generated.h"

// RPMが算出されたことを通知するデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRPMCalculated, float, NewRPM);

/**
 * 掛かった魚を巻き上げるためのリール操作を管理するステートコンポーネント
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRFISHING_API UFishingReelStateComponent : public UFishingStateComponentBase
{
	GENERATED_BODY()

public:
	UFishingReelStateComponent();

	// 基底クラスオーバーライド
	virtual void EnterState() override;
	virtual void UpdateState(float DeltaTime) override;
	virtual void ExitState() override;
	// 2026.08.05 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	virtual FString GetStateDisplayName() const override; // ステートの表示名（ログ・UI表示用）
	// 2026.08.05 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	// リール操作中は制限時間を進める（計時対象）
	virtual bool IsTimeCountingState() const override;

	// スティック入力を基にRPMシミュレーションを実行
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	void SimulateReelByStick(FVector2D StickInput);

	// マウスホイールが1ノッチ回るたびに呼ばれ、RPMシミュレーションを実行
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	void SimulateReelByWheel();

	// 回転カウントを初期化する関数
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator")
	void ResetRevolutionCount();

	// 今回セットの累積回転数を取得する（リザルト集計用）
	UFUNCTION(BlueprintPure, Category = "Reel Simulator")
	int32 GetCurrentRevolutionCount() const;

	// 負荷レベル（0=Low, 1=Medium, 2=High）に応じてRPM閾値を設定する
	UFUNCTION(BlueprintCallable, Category = "Reel Simulator|Config")
	void ApplyRotationLoadLevel(int32 LoadLevel);

	// RPM算出時に実行されるイベント
	UPROPERTY(BlueprintAssignable, Category = "Reel Simulator")
	FOnRPMCalculated OnRPMCalculated;

	// 2026.09.10 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
	/**
	 * @brief 直近の回転入力で JudgeRPM に渡された速すぎ上限RPM（0.0＝まだ回転入力が無い）。
	 * @note 判定（JudgeRPM）が実際に使用した上限をそのまま公開する読み取り専用の出力。
	 *       スティック入力時は StickMaxAllowedRPM、ホイール入力時は WheelMaxAllowedRPM が入る。
	 *       表示側（FishFightMeterWidget / RpmGaugeWidget / FishingSeControllerSubsystem）が本値を
	 *       参照することで、入力デバイスの推定違いによる判定と表示の食い違いを防ぐ。
	 *       ※ 本値は Project Settings の負荷設定が適用された後の実値であり、閾値そのものではない。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Reel Simulator")
	float LastAppliedMaxAllowedRPM = 0.0f;
	// 2026.09.10 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

	/**
	 * @brief 直近の回転入力で JudgeRPM に渡された遅すぎ下限RPM（0.0＝まだ回転入力が無い）。
	 * @note 判定（JudgeRPM）が実際に使用した下限をそのまま公開する読み取り専用の出力。
	 *       スティック入力時は MinAllowedRPM、ホイール入力時は WheelMinAllowedRPM が入る。
	 *       表示側（FishFightMeterWidget / RpmGaugeWidget / FishingSeControllerSubsystem）が本値を
	 *       参照することで、入力デバイスの推定違いによる判定と表示の食い違いを防ぐ。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Reel Simulator")
	float LastAppliedMinAllowedRPM = 0.0f;

protected:
	// スティックのデッドゾーン
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config")
	float StickThreshold;

	// ホイール1ノッチ入力あたりの回転角度（ラジアン）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Debug", meta = (ClampMin = "0.01"))
	float WheelNotchAngleRad;

	// スティック／ASerial（自転車デバイス 0x03）操作時のRPM判定間隔（ラジアン）。π/2＝1/4回転ごとに出力・判定する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config", meta = (ClampMin = "0.01"))
	float StickJudgeIntervalRad;

	// 目標回転数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator", meta = (ClampMin = "1"))
	int32 TargetRevolutionCount;

	// ホイール操作時の上限RPM（これを超えると速すぎミス）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config", meta = (ClampMin = "0.0"))
	float WheelMaxAllowedRPM = 40.0f;

	// ホイール操作時の下限RPM（これを下回ると遅すぎミス）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config", meta = (ClampMin = "0.0"))
	float WheelMinAllowedRPM = 10.0f;

	// スティック操作時の上限RPM（これを超えると速すぎミス）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config", meta = (ClampMin = "0.0"))
	float StickMaxAllowedRPM = 60.0f;

	// スティック操作時の下限RPM（これを下回ると遅すぎミス。ASerial＝自転車デバイスも本値を使う）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config", meta = (ClampMin = "0.0"))
	float MinAllowedRPM = 10.0f;

	// 連続ミスの上限（速すぎ・遅すぎが続けてこの回転数分に達すると釣り失敗）
	// 判定間隔が1回転未満の入力では、判定回数を増やして同じ回転数分を維持する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config", meta = (ClampMin = "1"))
	int32 MaxMistakeCount = 3;

	// 停止とみなす無回転時間（秒）。最初の1回転を検知してから計測を開始する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reel Simulator|Config", meta = (ClampMin = "0.1"))
	float RevolveStopTimeoutSeconds = 5.0f;

	// 現在の累積回転数
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reel Simulator")
	int32 CurrentRevolutionCount;

	// 残り運動時間（秒）。0になると時間ベースで成功完了する（BPのUI表示用）
	UPROPERTY(BlueprintReadOnly, Category = "Reel Simulator")
	float RemainingExerciseSeconds = 20.0f;

private:
	// 判定間隔（JudgeIntervalRad）分の角度が溜まるたびにRPMを算出し、デリゲートを呼び出す
	void CalculateRPM(float DeltaAngle, float MaxAllowedRPM, float MinRPM, float JudgeIntervalRad);

	float	LastAngle;				// 前フレームの入力角度（ラジアン）
	float	AccumulatedAngleRad;	// 累積角度（ラジアン）＝回転数カウント用
	float	AccumulatedJudgeAngleRad;	// 累積角度（ラジアン）＝RPM判定間隔用
	double	RotationStartTime;		// 回転の計測を開始した時間（秒）
	bool	bIsMeasuringRotation;	// 計測が開始されているかのフラグ
	bool	bIsStickTracking;		// 入力を追跡中かどうか（スティック操作時用）
	bool	bIsCompleted;			// ステート完了（成功/失敗）フラグ

	// 速すぎミスの連続回数（許容範囲内または遅すぎミスで0に戻る）
	int32 OverRPMCount;

	// 遅すぎミスの連続回数（許容範囲内または速すぎミスで0に戻る）
	int32 UnderRPMCount;

	// RPMの許容範囲判定（速すぎ・遅すぎ）と失敗判定を行う。MistakeCountLimit は連続ミス許容回数（判定間隔換算済み）
	void JudgeRPM(float CalculatedRPM, float MaxAllowedRPM, float MinRPM, int32 MistakeCountLimit);

	// ミスログを画面と出力ログに表示する
	void ShowErrorLog(bool bIsTooFast, float CurrentRPM, int32 MistakeCountLimit);

	double	LastRevolutionTime;		// 最後に1回転を完了した時刻（秒、停止検知に使用）
};
