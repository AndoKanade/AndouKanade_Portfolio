#pragma once

#include "Model.h"
#include <vector>

/// <summary>
/// 雲海の起伏を時間とともにうねらせる
/// 雲の板モデルの頂点の高さと法線を、複数の波を重ねた式で毎フレーム計算し直して頂点バッファへ書き込む。
/// 波の周波数はタイル1枚あたり整数回にしているため、タイルを並べても端どうしで形がつながる。
/// 同じモデルを使う全タイルが一緒に動く。
/// 雲の上に重ねる霧の層も同じ形でうねらせるため、同じ格子のモデルを複数まとめて動かせるようにしている。
/// </summary>
class CloudWave{
public:
	/// <summary>
	/// 初期化(モデルの頂点を元に、毎フレームの計算に使う値を前もって求める)
	/// </summary>
	/// <param name="models">うねらせる板モデル(XY平面で-1〜1の範囲、高さはZ)。全て同じ格子の頂点であること</param>
	void Initialize(const std::vector<Model*>& models);

	/// <summary>
	/// 波の時間を進めて頂点の高さと法線を更新する
	/// </summary>
	/// <param name="deltaTime">1フレームの経過時間(秒)</param>
	void Update(float deltaTime);

private:
	// 現在の時間での頂点の高さと法線を計算して頂点バッファへ書き込む
	void ApplyWaves();

	// うねらせる対象のモデル(1回計算した頂点データをすべてに書き込む)
	std::vector<Model*> models_;

	// 頂点バッファへ書き込む頂点データ(位置のXYとUVは読み込み時のまま、高さと法線だけ書き換える)
	std::vector<Model::VertexData> vertices_;

	// 頂点ごと・波ごとの「位置で決まる位相」のsinとcos
	// 毎フレームは時間で決まる位相との加法定理で計算し、頂点ごとの三角関数の計算を省く
	std::vector<float> positionPhaseSin_;
	std::vector<float> positionPhaseCos_;

	// 波を重ねた値を、平均0・最大の振れ幅1にそろえるための値(初期化時の形から求める)
	float heightMean_ = 0.0f;
	float heightInvPeak_ = 1.0f;

	// 経過時間(秒)
	float time_ = 0.0f;
};
