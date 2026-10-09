struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 ndc : TEXCOORD0; // 画面上の位置(NDC空間)。ピクセルごとに視線の向きを求めるのに使う
};

// 天球の描画パラメータ(VS・PS共通で参照する)
struct CelestialSphereParam
{
    float32_t4x4 inverseViewProjection; // 平行移動を除いたビュープロジェクション行列の逆行列
    float32_t3 toSun; // 地面から見た太陽の方向(正規化済み)
    float time; // 経過時間(秒)。星のまたたきに使う
    float32_t3 zenithColor; // 真上の空の色
    float starIntensity; // 星の明るさ(0で星を消す)
    float32_t3 middleColor; // 地平線と真上の間の空の色
    float padding0;
    float32_t3 horizonColor; // 地平線付近の空の色
    float padding1;
    float32_t3 sunColor; // 太陽の方向の光のにじみの色
    float padding2;
};
