# 测试音效生成脚本（功能验证用，非正式素材）
# 全部 22050Hz / 16bit / 单声道，每个文件几 KB
# 用法: python generate_test_sfx.py
import wave
import struct
import math
import random
import os

SR = 22050
OUT_DIR = os.path.dirname(os.path.abspath(__file__))

# ---------- 基础工具 ----------

def write_wav(filename, samples):
    """单声道 int16 wav 写出，结尾 10ms 淡出防爆音"""
    n = len(samples)
    fade = int(SR * 0.01)
    frames = bytearray()
    for i, s in enumerate(samples):
        if i >= n - fade:
            s *= (n - i) / fade
        s = max(-1.0, min(1.0, s))
        frames += struct.pack('<h', int(s * 32767))
    path = os.path.join(OUT_DIR, filename)
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(bytes(frames))
    print(f"{filename}: {os.path.getsize(path)} bytes ({len(samples)/SR:.2f}s)")

def tone(freq, dur, vol=0.4, decay=0.0):
    """正弦音，decay>0 时指数衰减"""
    n = int(SR * dur)
    out = []
    for i in range(n):
        t = i / SR
        v = math.sin(2 * math.pi * freq * t) * vol
        if decay > 0:
            v *= math.exp(-decay * t)
        out.append(v)
    return out

def seq(*parts):
    """顺序拼接多段音频"""
    out = []
    for p in parts:
        out.extend(p)
    return out

# ---------- 鱼音（世界音 3D） ----------

def water_loop(dur=1.5):
    """挣扎循环水声替代：低通白噪声 + 2Hz 起伏，头尾淡入淡出让 Looping 播放自然"""
    random.seed(42)
    n = int(SR * dur)
    out = []
    lp = 0.0
    fade = int(SR * 0.02)
    for i in range(n):
        noise = random.uniform(-1.0, 1.0)
        lp += 0.15 * (noise - lp)
        t = i / SR
        env = 0.65 + 0.35 * math.sin(2 * math.pi * 2 * t)
        g = 1.0
        if i < fade:
            g = i / fade
        if i >= n - fade:
            g = (n - i) / fade
        out.append(lp * 0.9 * env * g)
    return out

def poke():
    """咬饵声替代：880Hz 高频短促"嘀"（v2——v1 的 150Hz 低频在头显/音箱上听不见）+ 前置噪声瞬态"""
    random.seed(7)
    n = int(SR * 0.12)
    out = []
    for i in range(n):
        t = i / SR
        v = math.sin(2 * math.pi * 880 * t) * 0.8 * math.exp(-20 * t)
        if i < SR * 0.02:
            v += random.uniform(-0.5, 0.5)
        out.append(v)
    return out

def escape():
    """逃走替代：0.4s 上滑音 300→900Hz"""
    n = int(SR * 0.4)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        f = 300 + 1500 * t
        phase += 2 * math.pi * f / SR
        out.append(math.sin(phase) * 0.35)
    return out

# ---------- UI 音（2D，不同音高便于区分触发源） ----------

sfx = {
    # 鱼音
    "SE_Fish_Struggle_Loop.wav": water_loop(),
    "SE_Fish_Poke.wav": poke(),
    "SE_Fish_Escape.wav": escape(),
    "SE_Fish_Caught.wav": seq(tone(523, 0.12), tone(659, 0.12), tone(784, 0.2, vol=0.5)),
    # UI 音
    "SE_UI_MenuMove.wav": tone(1200, 0.03, vol=0.3),
    "SE_UI_MenuConfirm.wav": seq(tone(800, 0.06), tone(1200, 0.08, vol=0.5)),
    "SE_UI_PhaseChange.wav": tone(1000, 0.05, vol=0.3),
    "SE_UI_PhaseSuccess.wav": seq(tone(660, 0.08), tone(880, 0.12, vol=0.5)),
    "SE_UI_PhaseFail.wav": seq(tone(400, 0.12), tone(300, 0.18, vol=0.5)),
    "SE_UI_Result.wav": seq(tone(523, 0.1), tone(659, 0.1), tone(784, 0.1), tone(1047, 0.25, vol=0.5)),
    "SE_UI_RpmJudge.wav": tone(800, 0.04, vol=0.3),
}

for name, samples in sfx.items():
    write_wav(name, samples)

print(f"\n完成: {len(sfx)} 个文件 -> {OUT_DIR}")
