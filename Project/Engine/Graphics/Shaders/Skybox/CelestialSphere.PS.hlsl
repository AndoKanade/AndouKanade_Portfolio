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
static const float kHorizonGlowStrength = 0.2f;
// 地平線より下の色の暗さ(雲海に隠れる部分なので地平線の色を少し暗くするだけにする)
static const float kBelowHorizonDarken = 0.7f;

// 太陽の方向の霞んだ光のにじみ(広い方と狭い方)の鋭さと強さ
// 霧の中にいるような空にするため、太陽の円盤は描かず光のにじみだけで方向を感じさせる
static const float kSunWideGlowPower = 4.0f;
static const float kSunWideGlowStrength = 0.15f;
static const float kSunNarrowGlowPower = 40.0f;
static const float kSunNarrowGlowStrength = 0.25f;

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
// 円周率の2倍(またたきの位相をばらけさせる・方角を区切るのに使う)
static const float kTwoPi = 6.28318530f;

// 遠景の建造物のシルエット
// 天球を方角ごとに区切ったマスに、柱・張り出し・頂上のふた・浮いたかけらを組み合わせた建造物を置く。
// 奥・中・手前の3層を重ね、奥の層ほど小さく霧に溶けた色にして奥行きを出す
static const uint32_t kStructureLayerCount = 3;
// 層ごとの一周あたりのマス数(奥の層ほど多くして建造物を小さく見せる)
static const float kStructureSlotCount[kStructureLayerCount] = { 40.0f, 26.0f, 16.0f };
// 層ごとの建造物を置くマスの割合(0〜1)
static const float kStructureProbability[kStructureLayerCount] = { 0.75f, 0.65f, 0.55f };
// 層ごとの霧の濃さ(1で空の色と同じになり見えなくなる)
static const float kStructureFog[kStructureLayerCount] = { 0.7f, 0.5f, 0.3f };
// 層ごとの建造物の頂上の高さの範囲(地平線からの角度・ラジアン)
static const float kStructureTopMin[kStructureLayerCount] = { 0.08f, 0.15f, 0.25f };
static const float kStructureTopMax[kStructureLayerCount] = { 0.3f, 0.55f, 0.9f };
// 層ごとに方角をずらして、層どうしの建造物が縦に並ばないようにする量(マス単位)
static const float kStructureLayerShift = 0.37f;
// 張り出しの数
static const uint32_t kStructureArmCount = 3;
// 乱数の種(同じマスでも用途ごとに別の乱数を使うための番号)
static const float kShapeRandomSeed = 1.0f;
static const float kPillarRandomSeed = 2.0f;
static const float kChunkRandomSeed = 3.0f;
static const float kArmRandomSeedBase = 4.0f;

// 以下の大きさはすべてマスの幅を1とした単位
// 柱の中心がマスの中央からずれる幅
static const float kPillarCenterRange = 0.2f;
// 柱の半分の太さの最小値と、乱数で足す幅
static const float kPillarHalfWidthMin = 0.06f;
static const float kPillarHalfWidthRange = 0.08f;
// 頂上のふたの太さ(柱の太さに対する倍率)と厚み
static const float kCapWidthScale = 2.0f;
static const float kCapThickness = 0.08f;
// 張り出しをつける高さの範囲(頂上の高さに対する割合)
static const float kArmHeightMin = 0.25f;
static const float kArmHeightRange = 0.6f;
// 張り出しの長さの最小値と、乱数で足す幅
static const float kArmLengthMin = 0.15f;
static const float kArmLengthRange = 0.2f;
// 張り出しの半分の厚み
static const float kArmHalfThickness = 0.05f;
// 浮いたかけらの頂上からの離れ具合の最小値と、乱数で足す幅
static const float kChunkGapMin = 0.12f;
static const float kChunkGapRange = 0.25f;
// 浮いたかけらが柱から横にずれる幅
static const float kChunkOffsetRange = 0.4f;
// 浮いたかけらの半分の大きさの最小値と、乱数で足す幅
static const float kChunkHalfSizeMin = 0.03f;
static const float kChunkHalfSizeRange = 0.04f;
// 柱の下端(雲海の下まで続いているように十分低くする)
static const float kPillarBottom = -10.0f;

// 太陽と反対側の面の明るさ(太陽側を1とした割合)
static const float kStructureShadowBrightness = 0.75f;
// 根元を霧に溶かす高さ(地平線からの角度・ラジアン。これより低いほど空の色に近づく)
static const float kStructureBaseFadeHeight = 0.12f;
// 輪郭のぼかし幅(ピクセル数)
static const float kStructureEdgeSoftness = 1.5f;

// 3次元の格子座標から0〜1の乱数を3つ作る
float32_t3 Hash33(float32_t3 p)
{
    p = frac(p * float32_t3(0.1031f, 0.1030f, 0.0973f));
    p += dot(p, p.yxz + 33.33f);
    return frac((p.xxy + p.yxx) * p.zyx);
}

// 範囲[low, high]の内側で1、外側で0になる値を、輪郭をaaの幅でぼかして返す
float Box(float position, float low, float high, float aa)
{
    return saturate((position - low) / aa + 0.5f) * saturate((high - position) / aa + 0.5f);
}

// 遠景の建造物のシルエットを空の色に重ねる
// coverageには建造物で覆われている割合(0〜1)を返し、星を隠すのに使う
float32_t3 DrawStructures(float32_t3 direction, float32_t3 skyColor, out float coverage)
{
    // 視線の方角と、地平線からの角度
    float azimuth = atan2(direction.x, direction.z);
    float elevation = asin(clamp(direction.y, -1.0f, 1.0f));

    // 1ピクセルぶんの角度(輪郭のぼかし幅に使う)
    float pixelAngle = length(fwidth(direction)) * kStructureEdgeSoftness;

    // 太陽が視線の右にあるか左にあるか(建造物の太陽側の面を明るくするのに使う)
    float32_t3 viewRight = float32_t3(cos(azimuth), 0.0f, -sin(azimuth));
    float sunSide = dot(viewRight, gParam.toSun) >= 0.0f ? 1.0f : -1.0f;

    // 根元ほど霧が濃くなり、雲海の下へ溶けていくようにする
    float baseFog = 1.0f - smoothstep(0.0f, kStructureBaseFadeHeight, elevation);

    float32_t3 color = skyColor;
    coverage = 0.0f;

    // 奥の層から順に重ねる
    for (uint32_t layer = 0; layer < kStructureLayerCount; ++layer)
    {
        // 方角をマスに区切り、マスの中の位置(中央が0)と高さをマスの幅を1とした単位で表す
        float slotAngle = kTwoPi / kStructureSlotCount[layer];
        float slotCoordinate = azimuth / slotAngle + layer * kStructureLayerShift;
        float slot = floor(slotCoordinate);
        float x = slotCoordinate - slot - 0.5f;
        float y = elevation / slotAngle;
        float aa = pixelAngle / slotAngle;

        // マスごとの乱数で、建造物の有無と形を決める
        float32_t3 shapeRandom = Hash33(float32_t3(slot, layer, kShapeRandomSeed));
        if (shapeRandom.x > kStructureProbability[layer])
        {
            continue;
        }
        float32_t3 pillarRandom = Hash33(float32_t3(slot, layer, kPillarRandomSeed));
        float32_t3 chunkRandom = Hash33(float32_t3(slot, layer, kChunkRandomSeed));
        float top = lerp(kStructureTopMin[layer], kStructureTopMax[layer], shapeRandom.y) / slotAngle;
        float centerX = (shapeRandom.z - 0.5f) * kPillarCenterRange;
        float halfWidth = kPillarHalfWidthMin + kPillarHalfWidthRange * pillarRandom.x;

        // 柱と頂上のふた
        float mask = Box(x, centerX - halfWidth, centerX + halfWidth, aa) * Box(y, kPillarBottom, top, aa);
        float capHalfWidth = halfWidth * kCapWidthScale;
        mask = max(mask, Box(x, centerX - capHalfWidth, centerX + capHalfWidth, aa) * Box(y, top - kCapThickness, top, aa));

        // 柱の左右へ張り出す板(高さ・向き・長さを張り出しごとに乱数で変える)
        for (uint32_t arm = 0; arm < kStructureArmCount; ++arm)
        {
            float32_t3 armRandom = Hash33(float32_t3(slot, layer, kArmRandomSeedBase + arm));
            float armY = top * (kArmHeightMin + kArmHeightRange * armRandom.x);
            float armLength = kArmLengthMin + kArmLengthRange * armRandom.z;
            bool isLeft = armRandom.y < 0.5f;
            float armLeft = isLeft ? centerX - armLength : centerX;
            float armRight = isLeft ? centerX : centerX + armLength;
            mask = max(mask, Box(x, armLeft, armRight, aa) * Box(y, armY - kArmHalfThickness, armY + kArmHalfThickness, aa));
        }

        // 頂上の上に浮いているかけら
        float chunkY = top + kChunkGapMin + kChunkGapRange * chunkRandom.x;
        float chunkX = centerX + (chunkRandom.y - 0.5f) * kChunkOffsetRange;
        float chunkHalfSize = kChunkHalfSizeMin + kChunkHalfSizeRange * chunkRandom.z;
        mask = max(mask, Box(x, chunkX - chunkHalfSize, chunkX + chunkHalfSize, aa) * Box(y, chunkY - chunkHalfSize, chunkY + chunkHalfSize, aa));

        // 太陽側の面を明るく、反対側を暗くし、層の遠さと根元の霧の分だけ空の色に近づける
        float shade = (x - centerX) * sunSide > 0.0f ? 1.0f : kStructureShadowBrightness;
        float fog = lerp(kStructureFog[layer], 1.0f, baseFog);
        float32_t3 structureColor = lerp(gParam.structureColor * shade, color, fog);

        float visibleMask = mask * gParam.structureVisibility;
        color = lerp(color, structureColor, visibleMask);
        coverage = max(coverage, visibleMask);
    }
    return color;
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
    // 2色を直接混ぜるだけだと中間の色を選べないため、間に中ほどの色をはさんで2段階で変化させる
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

    // 遠景の建造物のシルエット(太陽の光のにじみは霧の手前にかかるので、その前に描く)
    float structureCoverage;
    color = DrawStructures(direction, color, structureCoverage);

    // 太陽の方向の霞んだ光のにじみ
    float sunDot = saturate(dot(direction, gParam.toSun));
    float sunGlow = pow(sunDot, kSunWideGlowPower) * kSunWideGlowStrength + pow(sunDot, kSunNarrowGlowPower) * kSunNarrowGlowStrength;
    color += gParam.sunColor * sunGlow;

    // 星は高いところほどはっきり見せ、太陽のそばでは消す
    float starFade = smoothstep(kStarFadeStartHeight, kStarFadeEndHeight, height);
    float sunHide = 1.0f - smoothstep(kStarSunHideDot - (1.0f - kStarSunHideDot), kStarSunHideDot, sunDot);
    color += StarBrightness(direction) * starFade * sunHide * (1.0f - structureCoverage) * gParam.starIntensity;

    output.color = float32_t4(color, 1.0f);
    return output;
}
