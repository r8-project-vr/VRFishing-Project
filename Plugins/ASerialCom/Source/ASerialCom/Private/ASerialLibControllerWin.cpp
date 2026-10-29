// Fill out your copyright notice in the Description page of Project Settings.


#include "ASerialLibControllerWin.h"

UASerialLibControllerWin::UASerialLibControllerWin()
{
}

// 2025.08.06 ウー start
//UASerialLibControllerWin::UASerialLibControllerWin(int target_device_id, int device_ver)
//    : UASerialPacket(target_device_id)
//{
//    m_device_ver_max = device_ver;
//    m_device_ver_min = device_ver;
//}
//
//UASerialLibControllerWin::UASerialLibControllerWin(int target_device_id, int device_ver_min, int device_ver_max)
//    : UASerialPacket(target_device_id)
//{
//    m_device_ver_max = device_ver_max;
//    m_device_ver_min = device_ver_min;
//}
// 2025.08.06 end

void UASerialLibControllerWin::Initialize(int target_device_id, int device_ver)
{
    Super::Initialize(target_device_id);
    m_device_ver_max = device_ver;
    m_device_ver_min = device_ver;
}

void UASerialLibControllerWin::Initialize(int target_device_id, int device_ver_min, int device_ver_max)
{
    Super::Initialize(target_device_id);
    m_device_ver_max = device_ver_max;
    m_device_ver_min = device_ver_min;
}

ConnectResult UASerialLibControllerWin::ConnectDevice(int COM_num)
{
    if (GetConnectionState() == true) {
        return ConnectResult::Fail;
    }

    int st = m_inteface->OpenPort(COM_num);
    if (st != 0) {
        return ConnectResult::Fail;
    }

    m_inteface->clear(); //接続したときにバッファをクリア

    // 2026.10.29 Lee(2): 情報リクエストの送信失敗を明示検査する。
    // 旧コードは戻り値を st に保持したまま読取ループへ入り、最初の ReadDataProcess で
    // 上書きされるため、書込失敗が検出されないまま待ちに入っていた。
    // 失敗時はここでポートを閉じて Fail へ（書込エラーとポート後片付けの正規化）
    if (WriteData(RESERVED_COMMAND_GET_INFO) != 0) {
        m_inteface->ClosePort();
        return ConnectResult::Fail;
    }

    // 2026.10.29 Lee(2): 応答バッファを明示ゼロ初期化する。
    // （NSDMI 付き構造体だが、既定初期化の暗黙依存を排し、未受信時の残留/不定値判定を構造的に排除）
    ASerialDataStruct::ASerialData data_buf{};

    clock_t read_time = clock();

    bool bAborted = false;
    while (1) {
        // 2026.10.29 Lee: 停止要求があれば即座に待ちを打ち切る（スレッド停止のブロック回避）。
        // 中断時は応答未検証として扱うため、フラグを破棄せず保持する
        if (IsAbortRequested()) {
            bAborted = true;
            break;
        }
        st = ReadDataProcess(&data_buf);

        if (st != 0 || clock() - read_time >= 50) {
            break;
        }
    }

// 2026.10.29 Lee(2) startーーーーーーーーーーーーーーーーーーーーーーーーーーー
// 接続成立の判定を「応答フレームの実在」ベースへ厳格化（独立審査指摘対応）。
// 2026.08.26 Lee 改修後の旧条件（消さずにコメントで残す）:
//     if (clock() - read_time >= 50 || st == -1 || data_buf.data[0] != GetID() ||
//         (data_buf.data[1] < m_device_ver_min || data_buf.data[1] > m_device_ver_max)) {
//         m_inteface->ClosePort();
//         return ConnectResult::Fail;
//     }
// 問題点:
//   - 失敗判定が st == -1 のみのため、-2（ポート読取失敗）や中断時の残留 st
//     （情報リクエスト書込成功の 0 など）が失敗条件に掛からない
//   - 応答の data_num（payload 長）を検証していなかったため、実フレーム未受信の
//     まま data_buf の値が偶然 ID/Ver と一致しても誤って接続成功になり得た
// 改修: ①未中断 ②タイムアウト無し ③st == 1（校験合格の完成フレーム受信）
//   ④payload 長 2 以上 ⑤ID 一致 ⑥Ver が対応範囲内、の全条件を満たす場合にのみ
//   Succ とする。いずれかでも欠けたらポートを閉じて Fail へ
    const bool bTimedOut = (clock() - read_time >= 50);
    const bool bInfoFrameValid =
        !bAborted &&
        !bTimedOut &&
        st == 1 &&
        data_buf.data_num >= 2 &&
        data_buf.data[0] == GetID() &&
        data_buf.data[1] >= m_device_ver_min &&
        data_buf.data[1] <= m_device_ver_max;

    if (!bInfoFrameValid) {
        m_inteface->ClosePort();
        return ConnectResult::Fail;
    }
// 2026.10.29 Lee(2) endーーーーーーーーーーーーーーーーーーーーーーーーーーーー

    SetConnectionState(true);
    m_com = COM_num;

    return ConnectResult::Succ;
}

ConnectResult UASerialLibControllerWin::AutoConnectDevice() {
    if (GetConnectionState() == true) {
        return ConnectResult::Fail;
    }

    // 2025.08.06 ウー start
    //int ret = 0;
    ConnectResult ret = ConnectResult::Fail;
    // 2025.08.06 end

    // 2026.09.02 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーー
    // ASerial 規格 6-5「シリアルポートの最大数 256 個試す」に準拠させ、探査上限を COM255 から COM256 へ拡大。
    // 元コード（消さずにコメントで残す）:
    //     for (int i = 1; i <= 255; ++i) {
    for (int i = 1; i <= 256; ++i) {
    // 2026.09.02 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
        // 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
        // 全 COM 走査中も停止要求を検査する（以前は COM256 まで走り切るため、
        // スレッド停止時に走査完了まで待たされることがあった）
        if (IsAbortRequested()) {
            break;
        }
        // 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー
        int st = ConnectDevice(i);

        if (st == 0) {
            // 2025.08.06 ウー start
            //ret = i;
            m_com = i;
            ret = ConnectResult::Succ;
            // 2025.08.06 end
            break;
        }
    }
    // 2025.08.06 ウー start
    //if (ret == 0) {
    //    return ConnectResult::Fail;
    //}
    // 2025.08.06 end
    return ret;
}

ConnectResult UASerialLibControllerWin::DisConnectDevice() {
    if (m_inteface->GetState() == false) {
        return ConnectResult::Fail;
    }

    int st = m_inteface->ClosePort();

    SetConnectionState(false);

    // 2025.08.06 ウー start
    //return st;
    if (st == 0)
        return ConnectResult::Succ;
    else
        return ConnectResult::Fail;
    // 2025.08.06 end
}

void UASerialLibControllerWin::SetInterfacePt(WindowsSerial* interface_pt) { m_inteface = interface_pt; }

// 2026.10.29 Lee startーーーーーーーーーーーーーーーーーーーーーーーーーーーー
// システムコール失敗を -2 で区別して上位へ伝播するよう改修。
// 元コード（消さずにコメントで残す）:
//     int UASerialLibControllerWin::ReadDataProcess(ASerialDataStruct::ASerialData* read_data_buf)
//     {
//         if (m_inteface->GetState() == false) {
//             return -1;
//         }
//
//         int st = 0;
//         if (m_inteface->available() > 0) {
//             uint8_t read_c = (uint8_t)m_inteface->read();
//             st = this->ReadPacketData(read_c, read_data_buf);
//         }
//
//         if (st == -1) {
//             return -1;
//         }
//
//         return st;
//     }
// 改修点（デバイス抜去時の根因の一つ）:
//   1. available() が負値（ポート状態取得失敗）なら解析へ入らず -2 を返す
//   2. read() の失敗値 -1 を uint8_t へキャストして 0xFF データ byte として
//      解析へ混入させない（失敗は -2 で伝播。0xFF 混入はフレーム解析状態の破壊原因）
int UASerialLibControllerWin::ReadDataProcess(ASerialDataStruct::ASerialData* read_data_buf)
{
    if (m_inteface->GetState() == false) {
        return -1;
    }

    const int avail = m_inteface->available();
    if (avail < 0) {
        return -2;  // ポート状態取得失敗（抜去・I/Oエラー等）
    }

    int st = 0;
    if (avail > 0) {
        const int read_c = m_inteface->read();
        if (read_c < 0) {
            return -2;  // 読取失敗（失敗値をデータ byte として解析へ渡さない）
        }
        st = this->ReadPacketData((uint8_t)read_c, read_data_buf);
    }

    if (st == -1) {
        return -1;
    }

    return st;
}
// 2026.10.29 Lee endーーーーーーーーーーーーーーーーーーーーーーーーーーーーー

int UASerialLibControllerWin::ReadData(ASerialDataStruct::ASerialData* read_data_buf) {
    if (m_inteface->GetState() == false) {
        return -1;
    }

    constexpr clock_t time_out = 25;

    bool time_out_flag = false;
    bool error_flag = false;

    clock_t time_buf = clock();
    while (1) {
        // 2026.10.29 Lee: 停止要求があればタイムアウト待ちを打ち切る
        if (IsAbortRequested()) {
            time_out_flag = true;
            break;
        }

        int st = ReadDataProcess(read_data_buf);

        if (st == 1) {
            break;
        }
        // 2026.10.29 Lee: -2（ポート読取失敗）もエラー扱いへ（従来は -1 のみ。放置で
        // 抜去後もタイムアウトまで回り続けていた。負値はすべて読取失敗として扱う）
        else if (st < 0) {
            error_flag = true;
            break;
        }

        if (clock() - time_buf >= time_out) {
            time_out_flag = true;
            break;
        }
    }

    if (error_flag == true) {    //読み取りエラー
        return -1;
    }
    else if (time_out_flag == true) { //タイムアウトエラー
        return -2;
    }

    return 0;
}

int UASerialLibControllerWin::WriteData(uint8_t command, uint8_t* data, uint8_t data_num)
{
    int BUF_SIZE = this->GetNeedPacketBufSize(command, data, data_num);

    uint8_t* packet_buf = new uint8_t[BUF_SIZE];

    int st = MakePacketData(data, data_num, command, packet_buf);

    if (st == -1) {
        delete[] packet_buf;
        return -1;
    }

    int write_size = m_inteface->write(packet_buf, BUF_SIZE);

    delete[] packet_buf;

    if (write_size != BUF_SIZE) {
        return -1;
    }

    return 0;
}

int UASerialLibControllerWin::WriteData(uint8_t command)
{
    const int BUF_SIZE = this->GetNeedPacketBufSize(command);

    uint8_t* packet_buf = new uint8_t[BUF_SIZE];

    int st = MakePacketData(command, packet_buf);

    if (st == -1) {
        return -1;
    }

    int write_size = m_inteface->write(packet_buf, BUF_SIZE);

    delete[] packet_buf;

    if (write_size != BUF_SIZE) {
        return -1;
    }

    return 0;
}