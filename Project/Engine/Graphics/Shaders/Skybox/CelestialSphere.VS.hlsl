#include "CelestialSphere.hlsli"

// 頂点数
static const uint32_t kNumVertex = 3;

// 全画面を覆う三角形の頂点座標(NDC空間)
// 奥行きは最も奥(1)にして、他のオブジェクトより必ず後ろに見えるようにする
static const float32_t4 kPositions[kNumVertex] =
{
    { -1.0f, 1.0f, 1.0f, 1.0f }, // 左上
    { 3.0f, 1.0f, 1.0f, 1.0f }, // 右上(画面外まで飛ばす)
    { -1.0f, -3.0f, 1.0f, 1.0f } // 左下(画面外まで飛ばす)
};

// 頂点番号(SV_VertexID)を直接受け取る
VertexShaderOutput main(uint32_t vertexId : SV_VertexID)
{
    VertexShaderOutput output;
    output.position = kPositions[vertexId];
    output.ndc = kPositions[vertexId].xy;
    return output;
}
