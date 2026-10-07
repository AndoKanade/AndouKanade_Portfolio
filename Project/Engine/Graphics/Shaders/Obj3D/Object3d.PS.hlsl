#include "Object3d.hlsli"

// --- 構造体定義 ---
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t alphaCutoff;
    float32_t4x4 uvTransform;
    float32_t shininess;
    float32_t environmentCoefficient;
    int32_t useBlinnPhong;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float32_t intensity;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t radius;
    float32_t decay;
    float32_t padding[2];
};

struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t distance;
    float32_t cosAngle;
    float32_t decay;
    float32_t cosFalloffStart;
    float32_t padding[2];
};

struct Camera
{
    float32_t3 worldPosition;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// --- 定数バッファ・テクスチャ ---
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b2);
ConstantBuffer<Camera> gCamera : register(b3);
ConstantBuffer<PointLight> gPointLight : register(b4);
ConstantBuffer<SpotLight> gSpotLight : register(b5);
Texture2D<float32_t4> gTexture : register(t0);
TextureCube<float32_t4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);

// --- メイン関数 ---
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV Transform適用 & テクスチャサンプリング
    float32_t4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // 基本色の決定
    float32_t3 finalColor = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 baseColor = gMaterial.color.rgb * textureColor.rgb;

    if (gMaterial.enableLighting != 0)
    {
        float32_t3 normal = normalize(input.normal);
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

        // --- 1. Directional Light ---
        float32_t3 dirFactor = normalize(-gDirectionalLight.direction);
        float32_t dirCos = pow(dot(normal, dirFactor) * 0.5f + 0.5f, 2.0f);
        float32_t3 dirDiffuse = baseColor * gDirectionalLight.color.rgb * dirCos * gDirectionalLight.intensity;
        
        // [修正] 鏡面反射計算の切り替え
        float32_t dirSpecularPow;
        if (gMaterial.useBlinnPhong != 0)
        {
            float32_t3 halfVector = normalize(dirFactor + toEye);
            dirSpecularPow = pow(saturate(dot(normal, halfVector)), gMaterial.shininess);
        }
        else
        {
            dirSpecularPow = pow(saturate(dot(reflect(-dirFactor, normal), toEye)), gMaterial.shininess);
        }
        float32_t3 dirSpecular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * dirSpecularPow;

        // --- 2. Point Light ---
        float32_t3 pointVec = input.worldPosition - gPointLight.position;
        float32_t pDistance = length(pointVec);
        float32_t3 pFactor = normalize(-pointVec);
        float32_t pAttenuation = pow(saturate(1.0f - (pDistance / gPointLight.radius)), gPointLight.decay);
        
        float32_t pCos = pow(dot(normal, pFactor) * 0.5f + 0.5f, 2.0f);
        float32_t3 pDiffuse = baseColor * gPointLight.color.rgb * pCos * gPointLight.intensity * pAttenuation;
        
        // [修正] 鏡面反射計算の切り替え
        float32_t pSpecularPow;
        if (gMaterial.useBlinnPhong != 0)
        {
            float32_t3 halfVector = normalize(pFactor + toEye);
            pSpecularPow = pow(saturate(dot(normal, halfVector)), gMaterial.shininess);
        }
        else
        {
            pSpecularPow = pow(saturate(dot(reflect(-pFactor, normal), toEye)), gMaterial.shininess);
        }
        float32_t3 pSpecular = gPointLight.color.rgb * gPointLight.intensity * pSpecularPow * pAttenuation;

        // --- 3. Spot Light ---
        float32_t3 spotVec = input.worldPosition - gSpotLight.position;
        float32_t sDistance = length(spotVec);
        float32_t3 spotDirP = normalize(spotVec);
        
        float32_t sCosAngle = dot(spotDirP, normalize(gSpotLight.direction));
        float32_t sFalloff = saturate((sCosAngle - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
        float32_t sAttenuation = pow(saturate(1.0f - (sDistance / gSpotLight.distance)), gSpotLight.decay);
        
        float32_t3 sFactor = -spotDirP;
        float32_t sCos = pow(dot(normal, sFactor) * 0.5f + 0.5f, 2.0f);
        float32_t3 sDiffuse = baseColor * gSpotLight.color.rgb * sCos * gSpotLight.intensity * sAttenuation * sFalloff;
        
        // [修正] 鏡面反射計算の切り替え
        float32_t sSpecularPow;
        if (gMaterial.useBlinnPhong != 0)
        {
            float32_t3 halfVector = normalize(sFactor + toEye);
            sSpecularPow = pow(saturate(dot(normal, halfVector)), gMaterial.shininess);
        }
        else
        {
            sSpecularPow = pow(saturate(dot(reflect(-sFactor, normal), toEye)), gMaterial.shininess);
        }
        float32_t3 sSpecular = gSpotLight.color.rgb * gSpotLight.intensity * sSpecularPow * sAttenuation * sFalloff;

        // --- 4. Environment Mapping ---
        float32_t3 reflectedVector = reflect(normalize(input.worldPosition - gCamera.worldPosition), normal);
        float32_t3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector).rgb;

        // 全てのライティングを合計
        finalColor = dirDiffuse + dirSpecular + pDiffuse + pSpecular + sDiffuse + sSpecular;
        finalColor += environmentColor * gMaterial.environmentCoefficient;
    }
    else
    {
        finalColor = baseColor;
    }

    // 最終出力の計算
    output.color.rgb = finalColor;
    output.color.a = gMaterial.color.a * textureColor.a;

    // アルファテスト(しきい値はマテリアルごとに指定し、霧などの半透明は0にして透明部分だけ捨てる)
    if (textureColor.a <= gMaterial.alphaCutoff || output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}