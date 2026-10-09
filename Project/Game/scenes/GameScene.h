#pragma once

#include "systems/BaseScene.h"
#include "MyMath.h"
#include <memory>

// 前方宣言
class Input;
class Obj3dCommon;
class SpriteCommon;
class Skybox;
class SkyboxCommon;
class RailEditor;
class StartSequence;
class Ground;
class Sun;
class Player;
class RailCamera;
class DebugTopCamera;
class PlayerBulletManager;
class TargetManager;
class EnemyManager;
class Reticle;
class InkEffectManager;
class ComboCounter;
class StageHUD;
class SpeedLines;

/// <summary>
/// ゲームシーン
/// ステージ開始演出(タイトル)からプレイ、クリア・ゲームオーバーへの遷移までを管理する。
/// プレイヤー・カメラ・弾・的・敵などの処理はそれぞれのクラスに任せ、このクラスは呼び出す順番と、
/// Edit / Play モードの切り替え、勝敗判定、デバッグ表示を担当する。
/// </summary>
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
	// ゲームを初期状態(レール先頭)から始め直す
	// Playに入った瞬間と、レールから落ちたときのリスタートで共通して使う
	void ResetPlayState();

	// マウスカーソルの表示/非表示を切り替える
	void UpdateCursorVisibility(bool isPlayMode);

	// プレイ中のゲーム処理(レール進行・カメラ・プレイヤー・敵・弾・的・勝敗判定)
	void UpdateGameplay();

#ifdef USE_IMGUI
	// レール・プレイヤー・敵の状態確認用のデバッグウィンドウを表示する(Playモード中も含めて常に表示する)
	void ShowStatusWindow();
	// Editモード中の編集用パネル(カメラ・ライティング・的のHierarchy/Inspector)を表示する
	void ShowEditorPanels();
#endif

	// 外部から受け取るポインタ
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	// スカイボックス
	std::unique_ptr<SkyboxCommon> skyboxCommon_;
	std::unique_ptr<Skybox> skybox_;

	// レールエディター
	std::unique_ptr<RailEditor> railEditor_;

	// 簡易的な地面
	std::unique_ptr<Ground> ground_;

	// 太陽(見た目は持たない光源)
	std::unique_ptr<Sun> sun_;

	// プレイヤー
	std::unique_ptr<Player> player_;

	// レール追従カメラ(プレイ用カメラ)
	std::unique_ptr<RailCamera> railCamera_;

	// 俯瞰デバッグカメラ
	std::unique_ptr<DebugTopCamera> debugTopCamera_;

	// プレイヤーの弾
	std::unique_ptr<PlayerBulletManager> bulletManager_;

	// 的
	std::unique_ptr<TargetManager> targetManager_;

	// 雑魚敵
	std::unique_ptr<EnemyManager> enemyManager_;

	// ステージ開始演出(タイトル → カメラの回り込み → カウントダウン → プレイ)
	std::unique_ptr<StartSequence> startSequence_;

	// 画面中央固定のレティクル(照準)
	std::unique_ptr<Reticle> reticle_;

	// インクのしぶき・的の破片・撃破時の閃光の演出
	std::unique_ptr<InkEffectManager> inkEffect_;

	// 的を続けて壊したときの連鎖数
	std::unique_ptr<ComboCounter> comboCounter_;

	// プレイ中の画面表示(体力・壊した的の数・連鎖数)
	std::unique_ptr<StageHUD> stageHUD_;

	// レールを進んでいる間に出すスピード線
	std::unique_ptr<SpeedLines> speedLines_;

	// 前フレームがPlayモードだったか(Playに入った瞬間を検出してリセットするのに使う)
	bool wasPlayMode_ = false;

	// マウスカーソルの表示状態(Playモード中にTABキーで切り替え。Editモードでは常に表示する)
	bool isCursorVisible_ = true;

	// Playに入ったときにタイトル演出を飛ばしてすぐプレイを始めるか(デバッグ用。ImGuiのチェックボックスで切り替える)
	bool skipTitle_ = false;

	// 1フレームの経過時間(エンジンが固定60fps前提(TimeManagerで60fps固定)なので、そのまま合わせる)
	static constexpr float kDeltaTime = 1.0f / 60.0f;
	// 射撃に使うマウスボタン(左ボタン)
	static constexpr int kShootMouseButton = 0;
	// 弾を撃ったときに手元から飛び散らせるインクのしぶきの数
	static constexpr int kMuzzleSplashCount = 3;
};
