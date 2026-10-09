#include "CelestialSphere.hlsli"

ConstantBuffer<CelestialSphereParam> gParam : register(b0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// 空の色が地平線の色から中ほどの色に変わりきる高さ(視線の向きのY成分)
static const float kMiddleColorHeight = 0.3f;
// 地平線の帯状の明るさの鋭さ(大きいほど帯が細くなる)
static const float kHorizonGlowSharpness = 12.0f;
// 地平線の帯状の明るさの強さ
static const float kHorizonGlowStrength = 0.35f;
// 地平線より下の色の暗さ(雲海に隠れる部分なので地平線の色を少し暗くするだけにする)
static const float kBelowHorizonDarken = 0.7f;

// 太陽の円盤の見かけの半径(ラジアン)
static const float kSunDiscRadius = 0.03f;
// 太陽の円盤の縁のぼかし幅(ラジアン)
static const float kSunDiscEdge = 0.006f;
// 太陽の円盤の明るさ
static const float kSunDiscStrength = 4.0f;
// 太陽のまわりの光のにじみ(広い方と狭い方)の鋭さと強さ
static const float kSunWideGlowPower = 6.0f;
static const float kSunWideGlowStrength = 0.25f;
static const float kSunNarrowGlowPower = 120.0f;
static const float kSunNarrowGlowStrength = 0.8f;

// 星を置く格子の細かさ(天球を囲む立方体格子の1辺あたりのマス数)
static const float kStarGridDensity = 60.0f;
// 星を置くマスの割合(0〜1)
static const float kStarProbability = 0.18f;
// 星の半径(マスの大きさに対する割合)
static const float kStarRadius = 0.09f;
// 星の中心をマスの端から離す割合(隣のマスとの境目で星が欠けないようにする)
static const float kStarMargin = 0.2f;
// 星が見え始める高さと、くっきり見える高さ(視線の向きのY成分)
static const float kStarFadeStartHeight = 0.05f;
static const float kStarFadeEndHeight = 0.5f;
// 星のまたたきの速さと揺れ幅
static const float kStarTwinkleSpeed = 3.0f;
static const float kStarTwinkleAmount = 0.35f;
// 太陽に近い星を消す範囲(太陽方向との内積がこの値より大きいと消える)
static const float kStarSunHideDot = 0.9f;
// 円周率の2倍(またたきの位相をばらけさせるのに使う)
static const float kTwoPi = 6.28318530f;

// 3次元の格子座標から0〜1の乱数を3つ作る
float32_t3 Hash33(float32_t3 p)
{
    p = frac(p * float32_t3(0.1031f, 0.1030f, 0.0973f));
    p += dot(p, p.yxz + 33.33f);
    return frac((p.xxy + p.yxx) * p.zyx);
}

// 視線の向きに見える星の明るさを求める
float StarBrightness(float32_t3 direction)
{
    // 天球上の点を格子に区切り、マスごとに星を置くかどうかを乱数で決める
    float32_t3 gridPosition = direction * kStarGridDensity;
    float32_t3 cell = floor(gridPosition);
    float32_t3 random = Hash33(cell);
    if (random.x > kStarProbability)
    {
        return 0.0f;
    }

    // マスの中の星の位置(端に寄りすぎないようにする)を天球の表面に乗せ、視線との距離を求める
    float32_t3 starPosition = cell + lerp(kStarMargin, 1.0f - kStarMargin, Hash33(cell + random));
    starPosition = normalize(starPosition) * kStarGridDensity;
    float distanceToStar = length(gridPosition - starPosition);
    float shape = 1.0f - smoothstep(0.0f, kStarRadius, distanceToStar);

    // 星ごとに明るさとまたたきのタイミングを変える
    float twinkle = 1.0f - kStarTwinkleAmount * (0.5f + 0.5f * sin(gParam.time * kStarTwinkleSpeed + random.z * kTwoPi));
    return shape * random.y * twinkle;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 画面上の位置から、カメラから見た視線の向き(ワールド空間)を求める
    float32_t4 nearPoint = mul(float32_t4(input.ndc, 0.0f, 1.0f), gParam.inverseViewProjection);
    float32_t3 direction = normalize(nearPoint.xyz / nearPoint.w);
    float height = direction.y;

    // 地平線から真上へのグラデーション
    // 暖色と藍色を直接混ぜると灰色にくすむため、間に青空の色をはさんで2段階で変化させる
    float32_t3 color;
    if (height < kMiddleColorHeight)
    {
        color = lerp(gParam.horizonColor, gParam.middleColor, smoothstep(0.0f, kMiddleColorHeight, height));
    }
    else
    {
        color = lerp(gParam.middleColor, gParam.zenithColor, smoothstep(kMiddleColorHeight, 1.0f, height));
    }

    // 地平線付近をうっすら明るくする
    color += gParam.horizonColor * exp(-abs(height) * kHorizonGlowSharpness) * kHorizonGlowStrength;

    // 地平線より下は雲海に隠れるので、地平線の色を少し暗くしただけにする
    if (height < 0.0f)
    {
        color = lerp(color, gParam.horizonColor * kBelowHorizonDarken, saturate(-height));
    }

    // 太陽の円盤とまわりの光のにじみ
    float sunDot = saturate(dot(direction, gParam.toSun));
    float sunAngle = acos(sunDot);
    float sunDisc = 1.0f - smoothstep(kSunDiscRadius - kSunDiscEdge, kSunDiscRadius, sunAngle);
    float sunGlow = pow(sunDot, kSunWideGlowPower) * kSunWideGlowStrength + pow(sunDot, kSunNarrowGlowPower) * kSunNarrowGlowStrength;
    color += gParam.sunColor * (sunDisc * kSunDiscStrength + sunGlow);

    // 星は高いところほどはっきり見せ、太陽のそばでは消す
    float starFade = smoothstep(kStarFadeStartHeight, kStarFadeEndHeight, height);
    float sunHide = 1.0f - smoothstep(kStarSunHideDot - (1.0f - kStarSunHideDot), kStarSunHideDot, sunDot);
    color += StarBrightness(direction) * starFade * sunHide * gParam.starIntensity;

    output.color = float32_t4(color, 1.0f);
    return output;
}
