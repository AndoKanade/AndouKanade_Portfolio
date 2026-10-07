#pragma once

#include "MyMath.h"
#include <memory>
#include <random>
#include <vector>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// 画面の中心から外側へ流れる白いスピード線
/// レールを進んでいる間だけ表示し、乗っていないときはゆっくり消す。
/// </summary>
class SpeedLines{
public:
	SpeedLines();
	~SpeedLines();

	// テクスチャの読み込みとスプライトの生成
	void Initialize(SpriteCommon* spriteCommon);

	// すべての線を消し、表示の濃さを0に戻す
	void Reset();

	/// <summary>
	/// 線の移動と表示の濃さの更新
	/// </summary>
	/// <param name="isActive">線を出すかどうか(レールを進んでいる間だけtrue)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(bool isActive,float deltaTime);

	// 描画処理(スプライト共通の描画前処理も含む)
	void Draw();

private:
	// 線1本分
	struct Line{
		std::unique_ptr<Sprite> sprite;
		float angle = 0.0f;    // 画面の中心から見た向き(ラジアン)
		float distance = 0.0f; // 画面の中心からの距離(ピクセル)
		float speed = 0.0f;    // 外側へ流れる速さ(1秒あたりのピクセル)
		float length = 0.0f;   // 線の長さ(ピクセル)
	};

	// 線を画面の中心寄りの位置へ置き直す
	// isInitialPlacementがtrueのときは、最初から画面全体に散らばるよう距離もばらつかせる
	void Respawn(Line& line,bool isInitialPlacement);

	// 指定した範囲の乱数を返す
	float RandomRange(float min,float max);

	SpriteCommon* spriteCommon_ = nullptr;

	std::vector<Line> lines_;

	// 表示の濃さ(0〜1)。出し始め・消すときに急に切り替わらないよう少しずつ変える
	float intensity_ = 0.0f;

	std::mt19937 randomEngine_;

	// 線の本数
	static constexpr size_t kLineCount = 28;
	// 線が出てくる、画面の中心からの距離の範囲(ピクセル)。中心付近は自機と照準が見えるよう空けておく
	static constexpr float kSpawnDistanceMin = 280.0f;
	static constexpr float kSpawnDistanceMax = 420.0f;
	// 線が画面の外へ出たとみなす、画面の中心からの距離(ピクセル)
	static constexpr float kDespawnDistance = 1000.0f;
	// 外側へ流れる速さの範囲(1秒あたりのピクセル)
	static constexpr float kSpeedMin = 1600.0f;
	static constexpr float kSpeedMax = 2600.0f;
	// 線の長さの範囲と太さ(ピクセル)
	static constexpr float kLengthMin = 120.0f;
	static constexpr float kLengthMax = 260.0f;
	static constexpr float kThickness = 6.0f;
	// 一番濃いときの線の透明度
	static constexpr float kMaxAlpha = 0.45f;
	// 表示の濃さが1秒あたりに変わる量
	static constexpr float kFadeInSpeed = 3.0f;
	static constexpr float kFadeOutSpeed = 2.0f;
	// 円周率(線の向きを一周の範囲で決めるのに使う)
	static constexpr float kPi = 3.14159265f;
	// 線の根元(中心側の端)を基準点にするアンカー
	static constexpr Vector2 kLineAnchor = {0.0f, 0.5f};
};
