#pragma once

#include "MyMath.h"
#include "Model.h"
#include <memory>
#include <vector>

// 前方宣言
class Obj3dCommon;
class RailEditor;
class Enemy;
class EnemyEditor;
class PlayerBulletManager;

/// <summary>
/// 雑魚敵の管理
/// 配置エディター(EnemyEditor)の内容を敵のリストに反映し、更新・当たり判定・描画をまとめて行う。
/// </summary>
class EnemyManager{
public:
	EnemyManager();
	~EnemyManager();

	/// <summary>
	/// 初期化処理
	/// 配置エディターを生成し、保存済みの配置が無い初回起動時のみレールの中間地点に自動配置する
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="railEditor">自動配置の基準にするレール</param>
	void Initialize(Obj3dCommon* objCommon,const RailEditor* railEditor);

	// 配置エディターの更新と、編集結果(追加・削除・ドラッグ移動・パラメータ変更)の敵リストへの反映
	void UpdateEditor();

	/// <summary>
	/// 全ての敵の更新(往復移動・プレイヤー検知・発射)
	/// </summary>
	/// <param name="playerPosition">プレイヤーのワールド座標</param>
	/// <param name="deltaTime">経過時間(秒)。0を渡すと移動せず表示更新のみ行う</param>
	void Update(const Vector3& playerPosition,float deltaTime);

	/// <summary>
	/// 全ての敵の弾とプレイヤーの当たり判定を行い、命中した弾を消滅させる
	/// </summary>
	/// <param name="playerSphere">プレイヤーのモデルから求めたワールド座標系の境界球</param>
	/// <returns>このフレームでプレイヤーに命中した弾の合計数</returns>
	int CheckHitToPlayer(const Model::BoundingSphere& playerSphere);

	// プレイヤーの弾と敵本体の当たり判定(当たった敵の体力を減らす)
	void CheckHitByBullets(PlayerBulletManager& bullets);

	// 全ての敵を初期状態(体力満タン)に戻す
	void Reset();

	// 全ての敵を描画する(撃破済みのときはEnemy側で描画をスキップする)
	void Draw();

#ifdef USE_IMGUI
	// 敵の状態(生存数・弾の数・体ごとの体力)をデバッグ表示する
	void ShowDebugInfo() const;
#endif

private:
	// 保存済みの配置が無いときに、レールの中間地点へ自動配置した初期データを作る
	void CreateDefaultPlacement(const RailEditor* railEditor);

	// 配置エディターの内容を敵のリストに反映する
	void SyncFromEditor();

	Obj3dCommon* objCommon_ = nullptr;

	// 敵の配置エディター(敵の座標・往復方向・体力はこちらが保持し、こちらは毎フレーム同期する)
	std::unique_ptr<EnemyEditor> editor_;

	// 雑魚敵(固定パターンで往復移動し、プレイヤーを検知すると向きを変える)
	std::vector<std::unique_ptr<Enemy>> enemies_;

	// 初回起動時に自動配置する雑魚敵のレール進行度(レールの中間地点)
	static constexpr float kSpawnRailT = 0.5f;
	// 雑魚敵をレールより上に置くオフセット
	static constexpr float kUpOffset = 1.0f;
};
