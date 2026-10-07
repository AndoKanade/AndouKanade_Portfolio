#include "CloudWave.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace{
	constexpr float kPi = 3.14159265f;
	constexpr float kTwoPi = kPi * 2.0f;

	// 重ねる波(周波数・振幅・初期位相・速さ)
	// 大きくゆったりした波に細かい波を少しずつ足し、向きと速さをばらばらにして形がゆっくり崩れていくようにする
	struct WaveSetting{
		int frequencyU;   // タイル1枚の横方向に入る波の数(整数にして端をつなげる)
		int frequencyV;   // タイル1枚の縦方向に入る波の数(整数にして端をつなげる)
		float amplitude;  // 振幅(他の波との割合として使う)
		float phase;      // 初期位相(ラジアン)
		float speed;      // 位相が進む速さ(ラジアン/秒、符号で流れる向きが変わる)
	};
	constexpr std::array<WaveSetting,8> kWaves = {{
		{1, 0, 0.50f, 0.3f, 0.20f},
		{0, 1, 0.45f, 1.7f, -0.25f},
		{1, 1, 0.35f, 2.9f, 0.30f},
		{2, -1, 0.30f, 0.8f, -0.35f},
		{3, 2, 0.18f, 4.1f, 0.45f},
		{-2, 3, 0.15f, 5.3f, -0.40f},
		{4, 1, 0.10f, 2.2f, 0.55f},
		{1, -4, 0.08f, 3.6f, -0.60f},
	}};

	// 谷の折れ目をなめらかにするための値(絶対値を sqrt(x^2 + この値) で近似する)
	constexpr float kSoftness = 0.02f;

	// モデルのXY座標は-1〜1なので、0〜1の割合に直すための値
	constexpr float kPlaneHalfSize = 1.0f;
	constexpr float kHalf = 0.5f;

	// 振れ幅が0のときに割り算しないための下限
	constexpr float kMinPeak = 1e-4f;

	// 法線のZ成分(高さの傾きから法線を作るときの上向き成分)
	constexpr float kNormalUp = 1.0f;

	// 全ての波の振幅の合計(重ねた値を-1〜1に収めるために使う)
	constexpr float CalculateTotalAmplitude(){
		float total = 0.0f;
		for(const WaveSetting& wave : kWaves){
			total += wave.amplitude;
		}
		return total;
	}
	constexpr float kTotalAmplitude = CalculateTotalAmplitude();
	constexpr float kInvTotalAmplitude = 1.0f / kTotalAmplitude;
}

// 初期化
void CloudWave::Initialize(const std::vector<Model*>& models){
	models_.clear();
	for(Model* model : models){
		if(model){
			models_.push_back(model);
		}
	}
	if(models_.empty()){
		return;
	}

	// 全モデルが同じ格子なので、頂点の並びは先頭のモデルのものを使う
	vertices_ = models_.front()->GetModelData().vertices;
	const size_t vertexCount = vertices_.size();
	const size_t waveCount = kWaves.size();

	// 位置で決まる位相は時間で変わらないため、sinとcosを先に求めておく
	positionPhaseSin_.resize(vertexCount * waveCount);
	positionPhaseCos_.resize(vertexCount * waveCount);
	for(size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex){
		// -1〜1の座標を0〜1の割合に直す(タイル1枚で波がちょうど整数回入るようにするため)
		const float u = (vertices_[vertexIndex].position.x + kPlaneHalfSize) * kHalf;
		const float v = (vertices_[vertexIndex].position.y + kPlaneHalfSize) * kHalf;
		for(size_t waveIndex = 0; waveIndex < waveCount; ++waveIndex){
			const WaveSetting& wave = kWaves[waveIndex];
			const float positionPhase = kTwoPi * (static_cast<float>(wave.frequencyU) * u + static_cast<float>(wave.frequencyV) * v);
			positionPhaseSin_[vertexIndex * waveCount + waveIndex] = std::sin(positionPhase);
			positionPhaseCos_[vertexIndex * waveCount + waveIndex] = std::cos(positionPhase);
		}
	}

	// 初期の形から、高さを平均0・最大の振れ幅1にそろえる値を求める
	// 時間がたっても波の合計の範囲はほぼ変わらないため、初期化時に一度だけ求めれば足りる
	std::vector<float> rawHeights(vertexCount);
	float sum = 0.0f;
	for(size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex){
		float waveSum = 0.0f;
		for(size_t waveIndex = 0; waveIndex < waveCount; ++waveIndex){
			const WaveSetting& wave = kWaves[waveIndex];
			// sin(位置の位相 + 初期位相) を加法定理で求める
			waveSum += wave.amplitude * (positionPhaseSin_[vertexIndex * waveCount + waveIndex] * std::cos(wave.phase)
				+ positionPhaseCos_[vertexIndex * waveCount + waveIndex] * std::sin(wave.phase));
		}
		const float normalized = waveSum * kInvTotalAmplitude;
		rawHeights[vertexIndex] = std::sqrt(normalized * normalized + kSoftness);
		sum += rawHeights[vertexIndex];
	}
	heightMean_ = (vertexCount > 0)?sum / static_cast<float>(vertexCount):0.0f;
	float peak = kMinPeak;
	for(float rawHeight : rawHeights){
		// Windows.hのmaxマクロと衝突しないよう括弧で囲んで呼ぶ
		peak = (std::max)(peak,std::abs(rawHeight - heightMean_));
	}
	heightInvPeak_ = 1.0f / peak;

	time_ = 0.0f;
	ApplyWaves();
}

// 更新処理
void CloudWave::Update(float deltaTime){
	if(models_.empty()){
		return;
	}

	time_ += deltaTime;
	ApplyWaves();
}

// 現在の時間での頂点の高さと法線を計算して頂点バッファへ書き込む
void CloudWave::ApplyWaves(){
	const size_t waveCount = kWaves.size();

	// 時間で決まる位相のsinとcosは全頂点で共通なので、波ごとに1回だけ求める
	std::array<float,kWaves.size()> timePhaseSin{};
	std::array<float,kWaves.size()> timePhaseCos{};
	for(size_t waveIndex = 0; waveIndex < waveCount; ++waveIndex){
		const float timePhase = kWaves[waveIndex].phase + kWaves[waveIndex].speed * time_;
		timePhaseSin[waveIndex] = std::sin(timePhase);
		timePhaseCos[waveIndex] = std::cos(timePhase);
	}

	for(size_t vertexIndex = 0; vertexIndex < vertices_.size(); ++vertexIndex){
		// 波の合計と、X・Y方向の傾き(法線を求めるため)を同時に求める
		float waveSum = 0.0f;
		float slopeX = 0.0f;
		float slopeY = 0.0f;
		for(size_t waveIndex = 0; waveIndex < waveCount; ++waveIndex){
			const WaveSetting& wave = kWaves[waveIndex];
			const float positionSin = positionPhaseSin_[vertexIndex * waveCount + waveIndex];
			const float positionCos = positionPhaseCos_[vertexIndex * waveCount + waveIndex];
			// 加法定理で sin(位置の位相 + 時間の位相) と cos(位置の位相 + 時間の位相) を求める
			const float sinValue = positionSin * timePhaseCos[waveIndex] + positionCos * timePhaseSin[waveIndex];
			const float cosValue = positionCos * timePhaseCos[waveIndex] - positionSin * timePhaseSin[waveIndex];
			waveSum += wave.amplitude * sinValue;
			// 座標(-1〜1)で微分すると、0〜1の割合に直す係数(0.5)が掛かるため 2π×0.5 = π になる
			slopeX += wave.amplitude * cosValue * kPi * static_cast<float>(wave.frequencyU);
			slopeY += wave.amplitude * cosValue * kPi * static_cast<float>(wave.frequencyV);
		}

		// 絶対値をなめらかにした形で、丸い山と雲の切れ目のような谷を作る
		const float normalized = waveSum * kInvTotalAmplitude;
		const float rounded = std::sqrt(normalized * normalized + kSoftness);
		const float height = (rounded - heightMean_) * heightInvPeak_;

		// 高さの傾き = (丸めた形の傾き) × (波の合計の傾き) をそろえる係数で割ったもの
		const float slopeScale = (normalized / rounded) * kInvTotalAmplitude * heightInvPeak_;
		const Vector3 normal = Normalize(Vector3{-slopeX * slopeScale, -slopeY * slopeScale, kNormalUp});

		vertices_[vertexIndex].position.z = height;
		vertices_[vertexIndex].normal = normal;
	}

	// 計算は1回だけにして、同じ形でうねらせたい全モデルへ書き込む
	for(Model* model : models_){
		model->UpdateVertices(vertices_);
	}
}
