#pragma once

#include "MyMath.h"
#include "Model.h"
#include <memory>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// プレイヤーの弾の管理
/// 発射・移動(重力あり)・寿命切れの処理と、的や敵との当たり判定に使う境界球の判定を行う。
/// </summary>
class PlayerBulletManager{
public:
	PlayerBulletManager();
	~PlayerBulletManager();

	// 初期化処理(弾のモデルを読み込む)
	void Initialize(Obj3dCommon* objCommon);

	/// <summary>
	/// 弾を1発発射する
	/// </summary>
	/// <param name="position">発射位置</param>
	/// <param name="direction">発射方向(正規化済み)</param>
	void Fire(const Vector3& position,const Vector3& direction);

	// 弾の移動と生存時間の更新(的に当たらなくても一定時間で消滅させる)
	void Move(float deltaTime);

	/// <summary>
	/// 指定した境界球に重なる生存中の弾を1発探し、見つかればその弾を消滅させる
	/// </summary>
	/// <param name="sphere">判定相手の境界球</param>
	/// <returns>弾が当たったときtrue</returns>
	bool CheckHit(const Model::BoundingSphere& sphere);

	// 命中または生存時間切れで消えた弾をリストから削除する
	void RemoveDeadBullets();

	// 弾のトランスフォームを更新する(描画用)
	void UpdateTransforms();

	// 発射中の弾を描画する
	void Draw();

	// すべての弾を消す(ゲームの開始時に残っている弾のリセット用)
	void Clear();

	// 弾1発が敵に与えるダメージ量を取得
	int GetDamage() const{ return kDamage; }

private:
	// 弾(プロジェクタイル)
	struct Bullet{
		std::unique_ptr<Obj3D> obj;
		Vector3 position;
		Vector3 velocity;
		float lifeTime = 0.0f;
		bool isAlive = true;
	};

	Obj3dCommon* objCommon_ = nullptr;

	// 弾に使う球モデル(当たり判定で境界球を求めるのに使う)
	Model* model_ = nullptr;

	// 発射済みの弾
	std::vector<Bullet> bullets_;

	// 弾の移動速度(1秒あたりの移動量)
	static constexpr float kSpeed = 40.0f;
	// 弾の表示スケール(当たり判定の大きさにも使う)
	static constexpr float kScale = 0.15f;
	// 見た目を筋にするための、表示スケールに掛ける太さと長さの割合(当たり判定には使わない)
	static constexpr float kStreakWidthRate = 0.6f;
	static constexpr float kStreakLengthRate = 5.0f;
	// 弾が的に当たらなかった場合に消滅するまでの生存時間(秒)
	static constexpr float kLifeTime = 2.0f;
	// 弾に適用する重力加速度(1秒あたりの下方向への速度変化量)
	static constexpr float kGravity = 9.8f;
	// 弾1発が敵に与えるダメージ量
	static constexpr int kDamage = 1;
};
