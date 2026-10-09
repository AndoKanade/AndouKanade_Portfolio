# 天球(ゲームの背景の空)のキューブマップ画像を生成するスクリプト
# 霧で白んだ水色の空に、霧にかすむ遠景の建造物のシルエットを描き、Skyboxで使うDDS(キューブマップ)として書き出す。
# 使い方: python tools/generate_celestial_sphere.py  (Projectフォルダで実行する。numpyが必要)
# 出力先: resource/Skybox/celestialSphere.dds

import math
import struct
import sys
from pathlib import Path

import numpy as np

# 出力するファイルのパス(Projectフォルダからの相対パス)
OUTPUT_PATH = Path("resource/Skybox/celestialSphere.dds")
# キューブマップの1面の大きさ(ピクセル)。ミップマップは読み込み時にエンジン側で生成される
FACE_SIZE = 1024

# 空の色(リニア値)
ZENITH_COLOR = np.array([0.17, 0.35, 0.40])   # 真上の空の色(少し濃い青緑)
MIDDLE_COLOR = np.array([0.28, 0.51, 0.56])   # 地平線と真上の間の空の色(くすんだ水色)
HORIZON_COLOR = np.array([0.51, 0.75, 0.77])  # 地平線付近の空の色(霧で白んだ明るい水色)
# 空の色が地平線の色から中ほどの色に変わりきる高さ(視線の向きのY成分)
MIDDLE_COLOR_HEIGHT = 0.3
# 地平線の帯状の明るさの鋭さと強さ
HORIZON_GLOW_SHARPNESS = 12.0
HORIZON_GLOW_STRENGTH = 0.2
# 地平線より下の色の暗さ(雲海に隠れる部分なので地平線の色を少し暗くするだけにする)
BELOW_HORIZON_DARKEN = 0.7

# 太陽の位置(Sunの初期値に合わせる)。方角はレール開始地点の進行方向(+X)から右回りの角度
SUN_ELEVATION_DEGREE = 25.0
SUN_AZIMUTH_DEGREE = 50.0
RAIL_START_FORWARD = np.array([1.0, 0.0, 0.0])
SUN_COLOR = np.array([1.0, 0.92, 0.8])
# 太陽の方向の霞んだ光のにじみ(広い方と狭い方)の鋭さと強さ
SUN_WIDE_GLOW_POWER = 4.0
SUN_WIDE_GLOW_STRENGTH = 0.15
SUN_NARROW_GLOW_POWER = 40.0
SUN_NARROW_GLOW_STRENGTH = 0.25

# 遠景の建造物のシルエット(奥・中・手前の3層)
# 天球を方角ごとに区切ったマスに、柱・張り出し・頂上のふた・浮いたかけらを組み合わせた建造物を置く
STRUCTURE_SLOT_COUNT = [40.0, 26.0, 16.0]      # 一周あたりのマス数(奥の層ほど多くして小さく見せる)
STRUCTURE_PROBABILITY = [0.75, 0.65, 0.55]     # 建造物を置くマスの割合
STRUCTURE_FOG = [0.7, 0.5, 0.3]                # 霧の濃さ(1で空の色と同じになる)
STRUCTURE_TOP_MIN = [0.08, 0.15, 0.25]         # 頂上の高さの範囲(地平線からの角度・ラジアン)
STRUCTURE_TOP_MAX = [0.3, 0.55, 0.9]
STRUCTURE_LAYER_SHIFT = 0.37                   # 層どうしの建造物が縦に並ばないよう方角をずらす量(マス単位)
STRUCTURE_COLOR = np.array([0.72, 0.78, 0.78]) # 建造物の太陽側の面の色
STRUCTURE_SHADOW_BRIGHTNESS = 0.75             # 太陽と反対側の面の明るさ
STRUCTURE_BASE_FADE_HEIGHT = 0.12              # 根元を霧に溶かす高さ(ラジアン)
STRUCTURE_ARM_COUNT = 3                        # 張り出しの数
# 以下の大きさはすべてマスの幅を1とした単位
PILLAR_CENTER_RANGE = 0.2
PILLAR_HALF_WIDTH_MIN = 0.06
PILLAR_HALF_WIDTH_RANGE = 0.08
CAP_WIDTH_SCALE = 2.0
CAP_THICKNESS = 0.08
ARM_HEIGHT_MIN = 0.25
ARM_HEIGHT_RANGE = 0.6
ARM_LENGTH_MIN = 0.15
ARM_LENGTH_RANGE = 0.2
ARM_HALF_THICKNESS = 0.05
CHUNK_GAP_MIN = 0.12
CHUNK_GAP_RANGE = 0.25
CHUNK_OFFSET_RANGE = 0.4
CHUNK_HALF_SIZE_MIN = 0.03
CHUNK_HALF_SIZE_RANGE = 0.04
PILLAR_BOTTOM = -10.0
# 輪郭のぼかし幅(キューブマップの1ピクセルの角度に対する倍率)
EDGE_SOFTNESS = 1.5
# 乱数の種(同じマスでも用途ごとに別の乱数を使うための番号)
SHAPE_SEED = 1.0
PILLAR_SEED = 2.0
CHUNK_SEED = 3.0
ARM_SEED_BASE = 4.0

# DDSの定数
DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29
D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3
DDS_RESOURCE_MISC_TEXTURECUBE = 0x4


def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def hash33(a, b, c):
    """3つの値から0〜1の乱数を3つ作る"""
    p = np.stack(np.broadcast_arrays(a, b, c), -1).astype(np.float64)
    p = np.mod(p * np.array([0.1031, 0.1030, 0.0973]), 1.0)
    p = p + np.sum(p * (p[..., [1, 0, 2]] + 33.33), axis=-1, keepdims=True)
    return np.mod((p[..., [0, 0, 1]] + p[..., [1, 0, 0]]) * p[..., [2, 1, 0]], 1.0)


def box(position, low, high, aa):
    """範囲[low, high]の内側で1、外側で0になる値を、輪郭をaaの幅でぼかして返す"""
    return np.clip((position - low) / aa + 0.5, 0.0, 1.0) * np.clip((high - position) / aa + 0.5, 0.0, 1.0)


def sun_direction():
    """地面から見た太陽の方向(Sun::Updateと同じ求め方)"""
    up = np.array([0.0, 1.0, 0.0])
    forward = RAIL_START_FORWARD / np.linalg.norm(RAIL_START_FORWARD)
    right = np.cross(up, forward)
    right = right / np.linalg.norm(right)
    elevation = math.radians(SUN_ELEVATION_DEGREE)
    azimuth = math.radians(SUN_AZIMUTH_DEGREE)
    horizontal = forward * math.cos(azimuth) + right * math.sin(azimuth)
    return horizontal * math.cos(elevation) + up * math.sin(elevation)


def draw_structures(direction, sky_color, to_sun, pixel_angle):
    """遠景の建造物のシルエットを空の色に重ねる"""
    azimuth = np.arctan2(direction[..., 0], direction[..., 2])
    elevation = np.arcsin(np.clip(direction[..., 1], -1.0, 1.0))
    view_right = np.stack([np.cos(azimuth), np.zeros_like(azimuth), -np.sin(azimuth)], -1)
    sun_side = np.where(view_right @ to_sun >= 0.0, 1.0, -1.0)
    base_fog = 1.0 - smoothstep(0.0, STRUCTURE_BASE_FADE_HEIGHT, elevation)

    color = sky_color
    for layer in range(len(STRUCTURE_SLOT_COUNT)):
        slot_angle = 2.0 * math.pi / STRUCTURE_SLOT_COUNT[layer]
        slot_coordinate = azimuth / slot_angle + layer * STRUCTURE_LAYER_SHIFT
        slot = np.floor(slot_coordinate)
        x = slot_coordinate - slot - 0.5
        y = elevation / slot_angle
        aa = pixel_angle / slot_angle

        shape = hash33(slot, layer, SHAPE_SEED)
        pillar = hash33(slot, layer, PILLAR_SEED)
        chunk = hash33(slot, layer, CHUNK_SEED)
        exists = shape[..., 0] <= STRUCTURE_PROBABILITY[layer]
        top = (STRUCTURE_TOP_MIN[layer] + (STRUCTURE_TOP_MAX[layer] - STRUCTURE_TOP_MIN[layer]) * shape[..., 1]) / slot_angle
        center_x = (shape[..., 2] - 0.5) * PILLAR_CENTER_RANGE
        half_width = PILLAR_HALF_WIDTH_MIN + PILLAR_HALF_WIDTH_RANGE * pillar[..., 0]

        # 柱と頂上のふた
        mask = box(x, center_x - half_width, center_x + half_width, aa) * box(y, PILLAR_BOTTOM, top, aa)
        cap_half_width = half_width * CAP_WIDTH_SCALE
        mask = np.maximum(mask, box(x, center_x - cap_half_width, center_x + cap_half_width, aa) * box(y, top - CAP_THICKNESS, top, aa))

        # 柱の左右へ張り出す板
        for arm in range(STRUCTURE_ARM_COUNT):
            arm_random = hash33(slot, layer, ARM_SEED_BASE + arm)
            arm_y = top * (ARM_HEIGHT_MIN + ARM_HEIGHT_RANGE * arm_random[..., 0])
            arm_length = ARM_LENGTH_MIN + ARM_LENGTH_RANGE * arm_random[..., 2]
            is_left = arm_random[..., 1] < 0.5
            arm_left = np.where(is_left, center_x - arm_length, center_x)
            arm_right = np.where(is_left, center_x, center_x + arm_length)
            mask = np.maximum(mask, box(x, arm_left, arm_right, aa) * box(y, arm_y - ARM_HALF_THICKNESS, arm_y + ARM_HALF_THICKNESS, aa))

        # 頂上の上に浮いているかけら
        chunk_y = top + CHUNK_GAP_MIN + CHUNK_GAP_RANGE * chunk[..., 0]
        chunk_x = center_x + (chunk[..., 1] - 0.5) * CHUNK_OFFSET_RANGE
        chunk_half_size = CHUNK_HALF_SIZE_MIN + CHUNK_HALF_SIZE_RANGE * chunk[..., 2]
        mask = np.maximum(mask, box(x, chunk_x - chunk_half_size, chunk_x + chunk_half_size, aa) * box(y, chunk_y - chunk_half_size, chunk_y + chunk_half_size, aa))
        mask = mask * exists

        # 太陽側の面を明るく、反対側を暗くし、層の遠さと根元の霧の分だけ空の色に近づける
        shade = np.where((x - center_x) * sun_side > 0.0, 1.0, STRUCTURE_SHADOW_BRIGHTNESS)
        fog = STRUCTURE_FOG[layer] + (1.0 - STRUCTURE_FOG[layer]) * base_fog
        structure_color = STRUCTURE_COLOR * shade[..., None]
        structure_color = structure_color + (color - structure_color) * fog[..., None]
        color = color + (structure_color - color) * mask[..., None]
    return color


def sky_color(direction, pixel_angle):
    """視線の向きから空の色(リニア値)を求める"""
    to_sun = sun_direction()
    height = direction[..., 1][..., None]

    # 地平線から真上へのグラデーション(間に中ほどの色をはさんで2段階で変化させる)
    lower = HORIZON_COLOR + (MIDDLE_COLOR - HORIZON_COLOR) * smoothstep(0.0, MIDDLE_COLOR_HEIGHT, height)
    upper = MIDDLE_COLOR + (ZENITH_COLOR - MIDDLE_COLOR) * smoothstep(MIDDLE_COLOR_HEIGHT, 1.0, height)
    color = np.where(height < MIDDLE_COLOR_HEIGHT, lower, upper)

    # 地平線付近をうっすら明るくし、地平線より下は地平線の色を少し暗くしただけにする
    color = color + HORIZON_COLOR * np.exp(-np.abs(height) * HORIZON_GLOW_SHARPNESS) * HORIZON_GLOW_STRENGTH
    below = HORIZON_COLOR * BELOW_HORIZON_DARKEN
    color = np.where(height < 0.0, color + (below - color) * np.clip(-height, 0.0, 1.0), color)

    # 遠景の建造物(太陽の光のにじみは霧の手前にかかるので、その前に描く)
    color = draw_structures(direction, color, to_sun, pixel_angle)

    # 太陽の方向の霞んだ光のにじみ
    sun_dot = np.clip(direction @ to_sun, 0.0, 1.0)[..., None]
    color = color + SUN_COLOR * (sun_dot ** SUN_WIDE_GLOW_POWER * SUN_WIDE_GLOW_STRENGTH + sun_dot ** SUN_NARROW_GLOW_POWER * SUN_NARROW_GLOW_STRENGTH)
    return np.clip(color, 0.0, 1.0)


def face_directions(face, size):
    """キューブマップの面(+X,-X,+Y,-Y,+Z,-Z の順)の各ピクセルが表す向き(DirectXの規約)"""
    t, s = np.mgrid[0:size, 0:size]
    s = (s + 0.5) / size * 2.0 - 1.0
    t = (t + 0.5) / size * 2.0 - 1.0
    one = np.ones_like(s)
    directions = [
        (one, -t, -s),
        (-one, -t, s),
        (s, one, t),
        (s, -one, -t),
        (s, -t, one),
        (-s, -t, -one),
    ][face]
    direction = np.stack(directions, -1)
    return direction / np.linalg.norm(direction, axis=-1, keepdims=True)


def linear_to_srgb(color):
    return np.where(color <= 0.0031308, color * 12.92, 1.055 * np.power(color, 1.0 / 2.4) - 0.055)


def write_dds_cubemap(path, faces):
    """RGBA8(sRGB)のキューブマップをDDS(DX10ヘッダー)で書き出す"""
    size = faces[0].shape[0]
    flags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000  # CAPS | HEIGHT | WIDTH | PITCH | PIXELFORMAT
    pitch = size * 4
    pixel_format = struct.pack("<II4sIIIII", 32, 0x4, b"DX10", 0, 0, 0, 0, 0)
    caps = struct.pack("<IIII", 0x1000 | 0x8, 0xFE00, 0, 0)  # TEXTURE | COMPLEX, CUBEMAP_ALLFACES
    header = struct.pack("<IIIIIII", 124, flags, size, size, pitch, 0, 1) + b"\0" * 44 + pixel_format + caps + struct.pack("<I", 0)
    dx10 = struct.pack("<IIIII", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, D3D10_RESOURCE_DIMENSION_TEXTURE2D, DDS_RESOURCE_MISC_TEXTURECUBE, 1, 0)
    with open(path, "wb") as file:
        file.write(b"DDS " + header + dx10)
        for face in faces:
            file.write(face.tobytes())


def main():
    size = int(sys.argv[1]) if len(sys.argv) > 1 else FACE_SIZE
    pixel_angle = (math.pi / 2.0) / size * EDGE_SOFTNESS
    faces = []
    for face in range(6):
        color = sky_color(face_directions(face, size), pixel_angle)
        rgb = (linear_to_srgb(color) * 255.0 + 0.5).astype(np.uint8)
        alpha = np.full(rgb.shape[:2] + (1,), 255, dtype=np.uint8)
        faces.append(np.concatenate([rgb, alpha], -1))
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    write_dds_cubemap(OUTPUT_PATH, faces)
    print(f"wrote {OUTPUT_PATH} ({size}x{size} x 6)")


if __name__ == "__main__":
    main()
