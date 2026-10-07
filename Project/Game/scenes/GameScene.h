#pragma once

#include "systems/BaseScene.h"
#include "MyMath.h"
#include <memory>
#include <string>
#include <vector>

// 前方宣言
class Input;
class Obj3D;
class Obj3dCommon;
class SpriteCommon;
class Skybox;
class SkyboxCommon;
class Application;
class Sprite;
class RailEditor;
class TargetEditor;
class Enemy;
class EnemyEditor;
class StartSequence;
class Model;

class GameScene : public BaseScene{
public:
	GameScene();
	~GameScene() override;

	// シーンの初期化
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	// シーンの終了処理
	void Finalize() override;
	// シーンの更新処理
	void Update() override;
	// シーンの描画処理
	void Draw() override;

private:
	// 的の配置エディターの内容をシーンの的リストに反映する
	void SyncTargetsFromEditor();

	// 敵の配置エディターの内容をシーンの敵リストに反映する
	void SyncEnemiesFromEditor();

	// ゲームを初期状態(レール先頭)から始め直す
	// Playに入った瞬間と、レールから落ちたときのリスタートで共通して使う
	void ResetPlayState();

	// 地面タイルを生成する(レール開始地点から奥へ向けて板モデルを敷き詰める)
	void CreateGroundTiles();

	// 外部から受け取るポインタ
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	// スカイボックス
	std::unique_ptr<SkyboxCommon> skyboxCommon_;
	std::unique_ptr<Skybox> skybox_;

	// レールエディター
	std::unique_ptr<RailEditor> railEditor_;

	// 簡易的な地面(板モデルを格子状に並べて表現する)
	// 位置・向き・大きさは生成時に決め打ちするため、毎フレームは行列の更新だけ行う
	std::vector<std::unique_ptr<Obj3D>> groundTiles_;

	// 板モデル1枚の1辺の長さ(plane.objは-1〜1の2x2なのでこの値になる)
	const float kGroundTilePlaneSize_ = 2.0f;
	// 板モデルに掛ける表示スケール(1タイルの1辺はkGroundTilePlaneSize_倍された長さになる)
	const float kGroundTileScale_ = 10.0f;
	// レール開始地点から奥(進行方向)へ並べるタイル数
	const int kGroundTileCountForward_ = 8;
	// レール開始地点から手前(進行方向の逆)へ並べるタイル数
	const int kGroundTileCountBack_ = 1;
	// 横方向へ並べるタイル数(左右対称にするため奇数にする)
	const int kGroundTileCountWidth_ = 5;
	// 地面を敷くY座標(レールの起伏に関係なく一定の高さにする)
	const float kGroundHeight_ = -3.0f;
	// plane.objは+Z向きの板なので、X軸を-90度回して法線を上向き(+Y)にする
	const float kGroundRotateX_ = -3.14159265f * 0.5f;

	// 的の配置エディター(的の座標はこちらが保持し、シーン側は毎フレーム同期する)
	std::unique_ptr<TargetEditor> targetEditor_;

	// プレイヤー(三人称視点用の人型モデル)
	std::unique_ptr<Obj3D> player_;
	// プレイヤーモデルの表示スケール
	const float kPlayerScale_ = 0.3f;
	// プレイヤーモデルをレール位置よりさらに下げるオフセット
	// 注意: カメラのFOV(1.0472rad≒60度)を超えて大きくしすぎると視野から外れて描画されなくなる
	const float kPlayerDownOffset_ = 0.1f;

	// 敵の配置エディター(敵の座標・往復方向・体力はこちらが保持し、シーン側は毎フレーム同期する)
	std::unique_ptr<EnemyEditor> enemyEditor_;

	// 雑魚敵(固定パターンで往復移動し、プレイヤーを検知すると向きを変える)
	std::vector<std::unique_ptr<Enemy>> enemies_;
	// 初回起動時に自動配置する雑魚敵のレール進行度(レールの中間地点)
	const float kEnemySpawnRailT_ = 0.5f;
	// 雑魚敵をレールより上に置くオフセット
	const float kEnemyUpOffset_ = 1.0f;
	// プレイヤーの弾1発が敵に与えるダメージ量
	const int kBulletDamageToEnemy_ = 1;

	// プレイヤーの体力と被弾処理
	// 現在の体力(0になるとそれ以上減らない)
	int playerHp_ = 3;
	// 被弾後の無敵時間の残り(秒)。0より大きい間は敵弾が当たっても体力が減らない
	float playerInvincibleTimer_ = 0.0f;

	// 体力の最大値(Play開始時にこの値へ戻す)
	const int kPlayerMaxHp_ = 3;
	// 被弾してから次に被弾できるようになるまでの無敵時間(秒)
	const float kPlayerInvincibleTime_ = 1.0f;

	// レール移動の進行度(0〜1)と速度
	float railT_ = 0.0f;
	float railSpeed_ = 0.05f; // 1秒あたりの進行量

	// レール終端(railT_=1.0)に到達したかどうか(到達後はループせず停止させる)
	bool isRailFinished_ = false;

	// レール座標によるオンレール判定
	// レールの乗り換えはジャンプしてプレイヤーを移動させ、着地判定で行う
	bool isOnRail_ = true;
	// 着地判定は「水平方向でレールの真上にいる」ことと「落下でレールの高さを跨いだ」ことの両方を条件にする
	// こちらはその水平方向(XZ平面)の許容距離(小さくすると真上を通らないと乗れなくなる)
	const float kOnRailHorizontalThreshold_ = 1.0f;

	// プレイヤーの自立(ジャンプ+WASD移動)用の状態
	// オフレール中の基準座標(オンレール中のrailPosに相当する、カメラ・プレイヤーの共通の基準点)
	Vector3 freePosition_ = {0.0f, 0.0f, 0.0f};
	// オフレール中のY方向の速度(ジャンプ初速・重力の適用に使用)
	float freeVelocityY_ = 0.0f;
	// オフレール中の基準向き(レールを離れた瞬間の向きを固定して保持する)
	Vector3 freeBaseRot_ = {0.0f, 0.0f, 0.0f};

	// プレイヤーのジャンプ初速(上方向、1秒あたりの速度)
	const float kJumpSpeed_ = 6.0f;
	// プレイヤーに適用する重力加速度(1秒あたりの下方向への速度変化量)
	const float kPlayerGravity_ = 9.8f;
	// オフレール中のWASD移動速度(1秒あたりの移動量)
	const float kPlayerMoveSpeed_ = 8.0f;

	// 前フレームがPlayモードだったか(Playに入った瞬間を検出してリセットするのに使う)
	bool wasPlayMode_ = false;

	// 三人称視点用に、カメラをレールそのものより少し上に置くオフセット
	const float kCameraHeightOffset_ = 1.25f;
	// 引きのカメラにするため、進行方向の後方にも下げるオフセット(プレイヤーの配置距離としても使用)
	const float kCameraBackOffset_ = 6.0f;

	// 俯瞰デバッグカメラON/OFF状態
	bool useDebugTopCamera_ = false;

	// マウスカーソルの表示状態(Playモード中にTABキーで切り替え。Editモードでは常に表示する)
	bool isCursorVisible_ = true;

	// プレイヤー入力によるカメラの照準オフセット(レールの向きに上乗せする)
	float aimYawOffset_ = 0.0f;   // 左右(Y軸回転)
	float aimPitchOffset_ = 0.0f; // 上下(X軸回転)

	// 照準の可動範囲・感度
	const float kMouseSensitivity_ = 0.0004f; // マウス1移動量あたりの回転量(ラジアン)
	const float kAimYawLimit_ = 0.6f;        // 左右の可動範囲(約34度)
	const float kAimPitchLimit_ = 0.5f;      // 上下の可動範囲(約29度)

	// テスト用の的
	struct Target{
		std::unique_ptr<Obj3D> obj;
		Vector3 position;
		bool isAlive = true;
	};
	std::vector<Target> targets_;

	// 的と弾に使う球モデル(当たり判定で境界球を求めるのに使う)
	Model* sphereModel_ = nullptr;

	// 弾(プロジェクタイル)。プレイヤーが発射し、的との境界球の重なりでヒットを取る
	struct Bullet{
		std::unique_ptr<Obj3D> obj;
		Vector3 position;
		Vector3 velocity;
		float lifeTime = 0.0f;
		bool isAlive = true;
	};
	std::vector<Bullet> bullets_;

	// 弾の移動速度(1秒あたりの移動量)
	const float kBulletSpeed_ = 40.0f;
	// 弾の表示スケール
	const float kBulletScale_ = 0.15f;
	// 弾が的に当たらなかった場合に消滅するまでの生存時間(秒)
	const float kBulletLifeTime_ = 2.0f;
	// 弾に適用する重力加速度(1秒あたりの下方向への速度変化量)
	const float kBulletGravity_ = 9.8f;

	// 照準判定の許容角度(ラジアン)。画面中央のレティクルがこの角度以内に的を捉えていればヒット
	const float kAimHitAngle_ = 0.09f; // 約5度

	// ステージ開始演出(タイトル → カメラの回り込み → カウントダウン → プレイ)
	std::unique_ptr<StartSequence> startSequence_;

	// 画面中央固定のレティクル(照準)を表示するスプライト
	// 外枠(常時表示)と中心ドット(狙えているときに強調表示)の2枚構成
	std::unique_ptr<Sprite> reticleOutlineSprite_;
	std::unique_ptr<Sprite> reticleCenterSprite_;
};