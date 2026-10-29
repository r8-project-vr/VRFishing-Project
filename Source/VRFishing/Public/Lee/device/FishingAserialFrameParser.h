// Copyright 2026 JEC ProjectVR TeamRehab. All Rights Reserved.

#pragma once

#include <cstdint>

/**
 * @brief ASerial 受信バイトストリームをフレームへ分解するパーサ（純 C++・UE 非依存）。
 * @note  docs の診断ツール RpmMonitor.cs（AserialFrameParser）と同一規則の移植版:
 *        - D0 = フレーム先頭（フレーム読み取り中の裸 D0 は再同期: 途中フレームを破棄して数える）
 *        - AD = 加算エスケープ（次バイト +1。AD の連続は明示エラー）
 *        - データ数上限 32、超過は明示エラー（次の D0 まで破棄）
 *        - チェックサム = payload の符号無し 16bit 総和（Hi, Lo の順）
 *        スレッド非安全（単スレッド前提）。コールバックは Feed 呼び出し内で同期的に呼ぶ。
 *        拆包（任意バイト境界の分割）・粘着（複数フレームの連結）のどちらの投入でも動作する。
 */
class FAserialFrameParser
{
public:
	/** フレーム先頭フラグ */
	static constexpr uint8_t StartFlag = 0xD0;

	/** 加算エスケープ接頭辞 */
	static constexpr uint8_t AddFlagByte = 0xAD;

	/** ASerial 規格のデータ数上限 */
	static constexpr int MaxDataNum = 32;

	/** @brief 完成フレーム通知ハンドラ（長さと payload のコピーを受け取る） */
	typedef void (*FFrameHandler)(void* UserData, int Length, const uint8_t* Payload);

	/** @brief コンストラクタ。完成フレームのコールバックを登録する */
	FAserialFrameParser(FFrameHandler InHandler, void* InUserData = nullptr);

	/** @brief 解析状態を初期化する（診断カウンタは保持する） */
	void Reset();

	/** @brief 受信バイト 1 個を解析へ渡す */
	void FeedByte(uint8_t b);

	/** @brief 受信バイト列をまとめて解析へ渡す（分割・粘着どちらでも可） */
	void FeedBytes(const uint8_t* Buf, int Count);

	// --- 診断カウンタ ---
	/** チェックサム不一致で破棄したフレーム数 */
	long long BadChecksumCount = 0;

	/** 長さ超過や AD 連続など明示エラーで破棄した回数 */
	long long BadFrameCount = 0;

	/** フレーム読み取り中の裸 D0 で再同期した回数 */
	long long ResyncCount = 0;

private:
	/** 完成フレーム通知先 */
	FFrameHandler Handler = nullptr;

	/** コールバックへ渡すユーザーデータ */
	void* UserData = nullptr;

	/** payload 蓄積バッファ */
	uint8_t PayloadBuf[MaxDataNum] = { 0 };

	// 解析状態（ASerialPacket.cpp のメンバ変数方式に準拠）
	/** フレーム読み取り中か */
	bool InFrame = false;
	/** 直前バイトが AD か */
	bool AddFlag = false;
	/** エラー検出後、次の D0 まで破棄するか */
	bool ErrorPending = false;
	/** 0=長さ, 1=payload, 2=チェックHi, 3=チェックLo */
	int Step = 0;
	/** 受信中フレームの payload 長 */
	int DataLen = 0;
	/** payload 読み取り済み数 */
	int ReadCount = 0;
	/** payload 総和（16bit） */
	int CheckSum = 0;
	/** チェックサム上位バイト */
	int CkHi = 0;
};
