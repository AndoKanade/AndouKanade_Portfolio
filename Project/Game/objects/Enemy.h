#pragma once
#include "MyMath.h"
#include "Model.h"
#include <memory>
#include <vector>

// クラスの前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// 雑魚敵
///
/// 指定された基準座標を中心に、決められた方向へ一定距離を往復する固定パターンで移動する。
/// プレイヤーが検知範囲内に入るとそちらを向き、弾が当たると撃破される。
/// 検知中は一定間隔でプレイヤーへ向けて弾を発射する。
/// </summary>
class Enemy{
public:
	Enemy();
	~Enemy();

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="basePosition">往復移動の中心となるワールド座標</param>
	/// <param name="patrolDirection">往復移動を行う方向(内部で正規化する)</param>
	/// <param name="maxHp">体力の最大値(この値からのHPで開始する)</param>
	void Initialize(Obj3dCommon* objCommon,const Vector3& basePosition,const Vector3& patrolDirection,int maxHp);

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="playerPosition">プレイヤーのワールド座標(検知・向きの計算に使用)</param>
	/// <param name="deltaTime">経過時間(秒)。0を渡すと移動せず表示更新のみ行う</param>
	void Update(const Vector3& playerPosition,float deltaTime);

	// 描画処理(撃破済みのときは本体を描画しない。発射済みの弾は残っていれば描画する)
	void Draw();

	// 初期状態(生存・移動位置の中心)へ戻す(Play開始時のリセット用)
	void Reset();

	/// <summary>
	/// 体力を減らす(撃破済みのときは何もしない)
	/// </summary>
	/// <param name="damage">減らす体力の量</param>
	/// <returns>このダメージで撃破されたときtrue</returns>
	bool TakeDamage(int damage);

	// 現在の体力を取得
	int GetHp() const{ return hp_; }
	// 体力の最大値を取得
	int GetMaxHp() const{ return maxHp_; }

	// 配置エディターからの編集内容を反映するための設定関数
	// 往復移動の中心座標を設定する(現在の往復位置を保ったまま移動させる)
	void SetBasePosition(const Vector3& basePosition);
	// 往復移動の方向を設定する(内部で正規化する)
	void SetPatrolDirection(const Vector3& patrolDirection);
	// 体力の最大値を設定する(現在の体力が最大値を超える場合は最大値に合わせる)
	void SetMaxHp(int maxHp);

	/// <summary>
	/// 発射済みの弾とプレイヤーの当たり判定を行い、命中した弾を消滅させる
	/// </summary>
	/// <param name="playerSphere">プレイヤーのモデルから求めたワールド座標系の境界球</param>
	/// <returns>このフレームでプレイヤーに命中した弾の数</returns>
	int CheckHitToPlayer(const Model::BoundingSphere& playerSphere);

	// 発射済みで生存している弾の数を取得(デバッグ表示用)
	int GetActiveBulletCount() const;

	// 現在のワールド座標を取得(当たり判定・撃破演出の発生位置に使用)
	const Vector3& GetPosition() const{ return position_; }
	// 生存しているかどうかを取得
	bool IsAlive() const{ return isAlive_; }
	// プレイヤーを検知しているかどうかを取得(デバッグ表示用)
	bool IsDetectingPlayer() const{ return isDetectingPlayer_; }
	// 当たり判定用に、モデルから求めたワールド座標系の境界球を取得
	Model::BoundingSphere GetHitSphere() const;

private:
	// 検知中はこの種類を順番に切り替えながら撃つことで、攻撃パターンに変化をつける
	enum class AttackType{
		Single,  // 単発: 発射した瞬間のプレイヤー方向へ直進する
		Spread3, // 3方向拡散: プレイヤー方向を中心に、左右へ角度をつけた弾を同時発射する
		Homing,  // 追尾弾: 発射後もプレイヤー方向へ少しずつ向きを変え続ける
	};

	// 敵弾
	// 発射のたびに生成・破棄すると無駄が多いため、初期化時に必要数だけ作っておき使い回す
	struct Bullet{
		std::unique_ptr<Obj3D> obj;            // 描画用の3Dオブジェクト(初期化時に一度だけ生成する)
		Vector3 position = {0.0f, 0.0f, 0.0f}; // 現在のワールド座標
		Vector3 velocity = {0.0f, 0.0f, 0.0f}; // 1秒あたりの移動量
		float lifeTime = 0.0f;                 // 発射してからの経過時間(秒)
		bool isAlive = false;                  // 使用中かどうか(falseなら未使用=再利用できる)
		float speed = 0.0f;                    // 移動速度(向きを変えても速さを保つために保持する)
		bool isHoming = false;                 // 追尾するかどうか(trueなら毎フレーム向きを補正する)
	};

	// 弾の更新処理(移動・追尾の向き補正・寿命切れの判定・描画用トランスフォームの更新)
	void UpdateBullets(const Vector3& playerPosition,float deltaTime);
	// 現在の攻撃の種類に応じた発射処理を行う
	void FireCurrentAttack(const Vector3& playerPosition);
	// 現在の攻撃の種類に応じた発射間隔(秒)を取得する
	float GetCurrentShotInterval() const;
	// 弾の発射位置(敵の胸の高さ)を取得する
	Vector3 GetBulletSpawnPosition() const;
	// 指定した方向へ弾を1発発射する(未使用の弾が無いときは何もしない)
	void FireBullet(const Vector3& spawnPosition,const Vector3& direction,float speed,bool isHoming);

	std::unique_ptr<Obj3D> obj_; // 描画用の3Dオブジェクト

	// 敵弾のモデル(当たり判定で弾の境界球を求めるのに使う)
	Model* bulletModel_ = nullptr;

	// 敵弾の管理用
	std::vector<Bullet> bullets_;  // 使い回す弾の実体(サイズはkMaxBulletCountで固定)
	float shotTimer_ = 0.0f;       // 次の発射までの残り時間(秒)

	// 現在使用している攻撃の種類(kAttackOrderの何番目かを指す)
	size_t attackIndex_ = 0;

	Vector3 basePosition_ = {0.0f, 0.0f, 0.0f};    // 往復移動の中心座標
	Vector3 patrolDirection_ = {1.0f, 0.0f, 0.0f}; // 往復移動の方向(正規化済み)
	Vector3 position_ = {0.0f, 0.0f, 0.0f};        // 現在のワールド座標

	float patrolOffset_ = 0.0f; // 中心座標からのずれ(往復のたびに符号が反転する)
	float patrolSign_ = 1.0f;   // 現在の進行向き(+1で正方向、-1で逆方向)
	float rotationY_ = 0.0f;    // 現在のY軸回転(向き)

	bool isAlive_ = true;             // 生存フラグ(falseで撃破済み)
	bool isDetectingPlayer_ = false;  // プレイヤーを検知しているかどうか

	// 敵の体力(0になると撃破される)
	int maxHp_ = kDefaultMaxHp; // 体力の最大値(配置エディターで敵ごとに設定できる)
	int hp_ = kDefaultMaxHp;    // 現在の体力

	// 往復移動の速度(1秒あたりの移動量)
	static constexpr float kMoveSpeed = 3.0f;
	// 中心座標から片側へ動ける距離(この距離に達すると折り返す)
	static constexpr float kPatrolRange = 5.0f;
	// プレイヤーを検知する距離(この距離以内ならプレイヤーの方を向く)
	static constexpr float kDetectionRange = 25.0f;
	// 表示スケール
	static constexpr float kScale = 0.4f;

	// 体力の最大値の初期値(配置エディターで指定が無いときに使う)
	static constexpr int kDefaultMaxHp = 3;
	// 体力の最大値として設定できる下限(0以下にすると生成直後に撃破されてしまうため)
	static constexpr int kMinMaxHp = 1;

	// 同時に存在できる弾の最大数(この数だけ初期化時に生成して使い回す)
	static constexpr size_t kMaxBulletCount = 16;
	// 単発の発射間隔(秒)
	static constexpr float kShotInterval = 1.2f;
	// 単発・3方向拡散ショットの弾の移動速度(1秒あたりの移動量)
	static constexpr float kBulletSpeed = 18.0f;
	// 弾が消滅するまでの生存時間(秒)
	static constexpr float kBulletLifeTime = 4.0f;
	// 弾の表示スケール
	static constexpr float kBulletScale = 0.2f;
	// 弾の発射位置を敵の足元より上へずらす量(胸の高さから撃っているように見せる)
	static constexpr float kBulletSpawnUpOffset = 0.5f;

	// 攻撃を切り替える順番の要素数
	static constexpr size_t kAttackOrderCount = 3;
	// 攻撃を切り替える順番(1回撃つごとに次の要素へ進み、末尾まで来たら先頭へ戻る)
	static constexpr AttackType kAttackOrder[kAttackOrderCount] = {
		AttackType::Single,
		AttackType::Spread3,
		AttackType::Homing
	};

	// 3方向拡散ショットで同時に発射する弾数(左右対称にするため奇数にする)
	static constexpr int kSpreadBulletCount = 3;
	// 3方向拡散ショットで隣の弾との間につける角度(ラジアン、約12度)
	static constexpr float kSpreadAngle = 0.21f;
	// 3方向拡散ショットの発射間隔(秒)。単発より弾数が多いため長めにする
	static constexpr float kSpreadShotInterval = 2.0f;

	// 追尾弾の移動速度(1秒あたりの移動量)。避けられる余地を残すため通常弾より遅くする
	static constexpr float kHomingBulletSpeed = 12.0f;
	// 追尾弾が1秒あたりにプレイヤー方向へ向きを補正する割合(大きいほど急に曲がる)
	static constexpr float kHomingTurnRate = 1.5f;
	// 追尾弾の発射間隔(秒)。避けにくい攻撃なので最も長めにする
	static constexpr float kHomingShotInterval = 2.5f;
};