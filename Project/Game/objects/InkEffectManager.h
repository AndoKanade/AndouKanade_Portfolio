#pragma once

#include "MyMath.h"
#include <memory>
#include <random>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// インクのしぶき・的の破片・撃破時の閃光の演出
/// 毎回生成するとメモリ確保が重くなるため、起動時に決まった数だけ生成して使い回す。
/// 空きが無いときは一番古いものを上書きする。
/// </summary>
class InkEffectManager{
public:
	InkEffectManager();
	~InkEffectManager();

	// 初期化処理(モデルの読み込みと、使い回す3Dオブジェクトの生成)
	void Initialize(Obj3dCommon* objCommon);

	// 出ている演出をすべて消す
	void Reset();

	/// <summary>
	/// 的を壊したときの演出(破片・インクのしぶき・閃光)
	/// </summary>
	/// <param name="position">壊れた的の座標</param>
	void EmitTargetBreak(const Vector3& position);

	/// <summary>
	/// 指定した向きへ飛び散るインクのしぶき(弾を撃ったときの手元など)
	/// </summary>
	/// <param name="position">しぶきが出る座標</param>
	/// <param name="direction">飛び散る向き(正規化済み)</param>
	/// <param name="count">しぶきの数</param>
	void EmitInkSplash(const Vector3& position,const Vector3& direction,int count);

	/// <summary>
	/// レールを進んでいる間、足元から後ろへ飛ぶインクのしぶき(1回の呼び出しで1粒)
	/// </summary>
	/// <param name="position">足元の座標</param>
	/// <param name="backward">進行方向の逆向き(正規化済み)</param>
	void EmitRailTrail(const Vector3& position,const Vector3& backward);

	// 移動・回転・縮小・寿命の更新
	void Update(float deltaTime);

	// 出ている演出を描画する
	void Draw();

private:
	// 演出1つ分の種類(動き方と見た目が変わる)
	enum class Kind{
		Droplet, // インクのしぶき(重力で落ち、飛ぶ向きに伸びる)
		Shard,   // 的の破片(重力で落ち、回転する)
		Flash,   // 撃破時の閃光(その場で広がって消える)
	};

	// 演出1つ分
	struct Piece{
		std::unique_ptr<Obj3D> obj;
		Kind kind = Kind::Droplet;
		Vector3 position = {0.0f, 0.0f, 0.0f};
		Vector3 velocity = {0.0f, 0.0f, 0.0f};
		Vector3 rotate = {0.0f, 0.0f, 0.0f};
		Vector3 angularVelocity = {0.0f, 0.0f, 0.0f};
		Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f};
		float baseScale = 1.0f;
		float lifeTime = 0.0f;
		float maxLifeTime = 1.0f;
		bool isAlive = false;
	};

	// 使い回し用の配列から次に使う1つを取り出す(一番古いものから順に上書きする)
	Piece& Spawn(std::vector<Piece>& pool,size_t& nextIndex);

	// 指定した範囲の乱数を返す
	float RandomRange(float min,float max);

	// 各軸が -1〜1 の乱数のベクトルを返す
	Vector3 RandomVector();

	// 1つ分の行列と色を更新する
	void UpdatePieceTransform(Piece& piece);

	// インクのしぶき(球モデル)
	std::vector<Piece> droplets_;
	// 的の破片(三角柱モデル)
	std::vector<Piece> shards_;
	// 撃破時の閃光(球モデル)
	std::vector<Piece> flashes_;

	// 次に使う要素の番号
	size_t nextDroplet_ = 0;
	size_t nextShard_ = 0;
	size_t nextFlash_ = 0;

	// 飛び散り方のばらつきに使う乱数
	std::mt19937 randomEngine_;

	// 使い回す個数
	static constexpr size_t kDropletCount = 160;
	static constexpr size_t kShardCount = 48;
	static constexpr size_t kFlashCount = 8;

	// しぶき・破片に掛ける重力加速度(1秒あたりの下方向への速度変化量)
	static constexpr float kDropletGravity = 12.0f;
	static constexpr float kShardGravity = 14.0f;
	// しぶきを飛ぶ向きへ伸ばす割合(速さ1あたり)と、その上限
	static constexpr float kDropletStretchPerSpeed = 0.25f;
	static constexpr float kDropletMaxStretch = 3.0f;
	// 縮み始めるまでの寿命の割合(これを過ぎると小さくなりながら消える)
	static constexpr float kShrinkStartRate = 0.5f;
	// 閃光が最大まで広がったときの大きさ(開始時の何倍か)
	static constexpr float kFlashExpandScale = 4.0f;
	// 速さがこれ未満のときは向きが定まらないため、しぶきを伸ばさない
	static constexpr float kMinStretchSpeed = 0.001f;

	// 的を壊したときの破片の数・速さ・大きさ・寿命
	static constexpr int kBreakShardCount = 10;
	static constexpr float kBreakShardSpeedMin = 3.0f;
	static constexpr float kBreakShardSpeedMax = 7.0f;
	static constexpr float kBreakShardUpSpeed = 3.0f;
	static constexpr float kBreakShardScaleMin = 0.08f;
	static constexpr float kBreakShardScaleMax = 0.16f;
	static constexpr float kBreakShardLifeMin = 0.6f;
	static constexpr float kBreakShardLifeMax = 0.9f;
	static constexpr float kBreakShardSpinMax = 15.0f;
	// 的を壊したときのしぶきの数・速さ・大きさ・寿命
	static constexpr int kBreakDropletCount = 14;
	static constexpr float kBreakDropletSpeedMin = 2.0f;
	static constexpr float kBreakDropletSpeedMax = 6.0f;
	static constexpr float kBreakDropletUpSpeed = 2.0f;
	static constexpr float kBreakDropletScaleMin = 0.06f;
	static constexpr float kBreakDropletScaleMax = 0.12f;
	static constexpr float kBreakDropletLifeMin = 0.4f;
	static constexpr float kBreakDropletLifeMax = 0.7f;
	// 閃光の大きさ・寿命
	static constexpr float kFlashScale = 0.15f;
	static constexpr float kFlashLife = 0.12f;
	// 撃ったときなどのしぶきの速さ・横へのばらつき・大きさ・寿命
	static constexpr float kSplashSpeedMin = 3.0f;
	static constexpr float kSplashSpeedMax = 6.0f;
	static constexpr float kSplashSpread = 1.5f;
	static constexpr float kSplashScaleMin = 0.03f;
	static constexpr float kSplashScaleMax = 0.06f;
	static constexpr float kSplashLifeMin = 0.2f;
	static constexpr float kSplashLifeMax = 0.35f;
	// レール走行中のしぶきの後ろ・上への速さ・ばらつき・大きさ・寿命
	static constexpr float kTrailBackSpeed = 3.0f;
	static constexpr float kTrailUpSpeed = 1.5f;
	static constexpr float kTrailSpread = 1.5f;
	static constexpr float kTrailScaleMin = 0.04f;
	static constexpr float kTrailScaleMax = 0.08f;
	static constexpr float kTrailLifeMin = 0.25f;
	static constexpr float kTrailLifeMax = 0.4f;
};
