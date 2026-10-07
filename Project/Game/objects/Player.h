#pragma once

#include "MyMath.h"
#include "Model.h"
#include <memory>
#include <string>

// 前方宣言
class Input;
class Obj3D;
class Obj3dCommon;
class RailEditor;

/// <summary>
/// プレイヤー
/// レールに沿って自動で進み、ジャンプでレールを離れるとWASDで自由に動ける。
/// 落下中にレールの真上でレールの高さを跨ぐと、そのレールに乗り移る。
/// 体力を持ち、敵弾が当たると減る(被弾後は一定時間無敵)。
/// </summary>
class Player{
public:
	Player();
	~Player();

	/// <summary>
	/// 初期化処理(モデルの生成と、調整項目のGlobalVariablesへの登録)
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(Obj3dCommon* objCommon,const std::string& paramGroup);

	// レール先頭・オンレール・体力満タンの初期状態に戻す
	void Reset();

	/// <summary>
	/// レールの進行(プレイ可能かつオンレール中、かつ終端未到達のときだけ進める)
	/// </summary>
	/// <param name="railEditor">進行するレール</param>
	/// <param name="isGameplayActive">プレイ可能かどうか</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	void UpdateRailProgress(const RailEditor* railEditor,bool isGameplayActive,float deltaTime);

	// オンレール/オフレールの状態に応じて、カメラ・プレイヤーの基準位置と基準向きを求める
	void UpdateBasePose(const RailEditor* railEditor);

	/// <summary>
	/// 表示位置の計算とモデルの行列更新
	/// </summary>
	/// <param name="cameraForward">カメラの前方ベクトル(カメラの前方下に置くために使う)</param>
	/// <param name="hopHeight">タイトル中の跳ねの高さ(見た目だけに反映する)</param>
	void UpdateTransform(const Vector3& cameraForward,float hopHeight);

	// 被弾後の無敵時間を進める
	void UpdateInvincible(float deltaTime);

	// 敵弾が当たったときの処理(無敵中でなければ体力を1減らし、無敵時間を始める)
	void TakeDamage();

	/// <summary>
	/// ジャンプ・WASD移動・着地判定
	/// </summary>
	/// <param name="input">入力</param>
	/// <param name="railEditor">着地先を探すレール</param>
	/// <param name="cameraForward">カメラの前方ベクトル(WASD移動の基準)</param>
	/// <param name="cameraRight">カメラの右方向ベクトル(WASD移動の基準)</param>
	/// <param name="groundHeight">地面の高さ(ここまで落ちたらリスタートが必要)</param>
	/// <param name="deltaTime">経過時間(秒)</param>
	/// <returns>どのレールにも乗れずに地面まで落ち、リスタートが必要になったときtrue</returns>
	bool UpdateMovement(Input* input,RailEditor* railEditor,const Vector3& cameraForward,const Vector3& cameraRight,float groundHeight,float deltaTime);

	// 描画処理
	void Draw();

	// 当たり判定用に、モデルから求めたワールド座標系の境界球を取得
	Model::BoundingSphere GetHitSphere() const;

	// 表示位置を取得(敵の検知・開始演出の基準に使う。タイトル中の跳ねは含まない)
	const Vector3& GetPosition() const{ return position_; }
	// カメラ・プレイヤーの共通の基準位置を取得
	const Vector3& GetBasePosition() const{ return basePosition_; }
	// カメラ・プレイヤーの共通の基準向きを取得
	const Vector3& GetBaseRotation() const{ return baseRotation_; }

	// レールの終点に着いたかどうか(オンレール中に終端まで進んだらtrue)
	bool HasReachedGoal() const{ return isOnRail_ && isRailFinished_; }
	// 体力が尽きたかどうか
	bool IsDead() const{ return hp_ <= 0; }

	// デバッグ表示用の取得
	float GetRailT() const{ return railT_; }
	bool IsOnRail() const{ return isOnRail_; }
	float GetFreeVelocityY() const{ return freeVelocityY_; }
	int GetHp() const{ return hp_; }
	int GetMaxHp() const{ return kMaxHp; }
	float GetInvincibleTimer() const{ return invincibleTimer_; }

	// 着地判定で使う水平方向(XZ平面)の許容距離(小さくすると真上を通らないと乗れなくなる)
	// レールエディターで着地できる範囲を可視化するのにも使う
	static constexpr float kOnRailHorizontalThreshold = 1.0f;

private:
	// 描画用の3Dオブジェクト(三人称視点用の人型モデル)
	std::unique_ptr<Obj3D> obj_;

	// 調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;

	// レール移動の進行度(0〜1)
	float railT_ = 0.0f;
	// レール終端(railT_=1.0)に到達したかどうか(到達後はループせず停止させる)
	bool isRailFinished_ = false;

	// レール座標によるオンレール判定
	// レールの乗り換えはジャンプしてプレイヤーを移動させ、着地判定で行う
	bool isOnRail_ = true;

	// プレイヤーの自立(ジャンプ+WASD移動)用の状態
	// オフレール中の基準座標(オンレール中のレール上の座標に相当する、カメラ・プレイヤーの共通の基準点)
	Vector3 freePosition_ = {0.0f, 0.0f, 0.0f};
	// オフレール中のY方向の速度(ジャンプ初速・重力の適用に使用)
	float freeVelocityY_ = 0.0f;
	// オフレール中の基準向き(レールを離れた瞬間の向きを固定して保持する)
	Vector3 freeBaseRotation_ = {0.0f, 0.0f, 0.0f};

	// カメラ・プレイヤーの共通の基準位置と基準向き(UpdateBasePose()で求める)
	Vector3 basePosition_ = {0.0f, 0.0f, 0.0f};
	Vector3 baseRotation_ = {0.0f, 0.0f, 0.0f};
	// 表示位置(UpdateTransform()で求める)
	Vector3 position_ = {0.0f, 0.0f, 0.0f};

	// 現在の体力(0になるとそれ以上減らない)
	int hp_ = kMaxHp;
	// 被弾後の無敵時間の残り(秒)。0より大きい間は敵弾が当たっても体力が減らない
	float invincibleTimer_ = 0.0f;

	// レール全体の進行速度の初期値(1秒あたりの進行量。保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultRailSpeed = 0.05f;
	// モデルの表示スケール
	static constexpr float kScale = 0.3f;
	// カメラの前方へ置く距離(引きのカメラにするため、カメラから離して置く)
	static constexpr float kDistanceFromCamera = 6.0f;
	// モデルを基準位置よりさらに下げるオフセット
	// 注意: カメラのFOV(1.0472rad≒60度)を超えて大きくしすぎると視野から外れて描画されなくなる
	static constexpr float kDownOffset = 0.1f;
	// 体力の最大値(Reset()でこの値へ戻す)
	static constexpr int kMaxHp = 3;
	// 被弾してから次に被弾できるようになるまでの無敵時間(秒)
	static constexpr float kInvincibleTime = 1.0f;
	// ジャンプ初速(上方向、1秒あたりの速度)
	static constexpr float kJumpSpeed = 6.0f;
	// 重力加速度(1秒あたりの下方向への速度変化量)
	static constexpr float kGravity = 9.8f;
	// オフレール中のWASD移動速度(1秒あたりの移動量)
	static constexpr float kMoveSpeed = 8.0f;
	// レールの終端の進行度
	static constexpr float kRailEndT = 1.0f;
};
