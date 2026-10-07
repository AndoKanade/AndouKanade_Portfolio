#pragma once

#include "MyMath.h"
#include <memory>
#include <string>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;
class Model;
class RailEditor;
class TargetEditor;
class PlayerBulletManager;

/// <summary>
/// 的の管理
/// 配置エディター(TargetEditor)の内容を的のリストに反映し、照準判定・弾との当たり判定・描画を行う。
/// </summary>
class TargetManager{
public:
	TargetManager();
	~TargetManager();

	/// <summary>
	/// 初期化処理
	/// 配置エディターを生成し、保存済みの配置が無い初回起動時のみレール沿いに自動配置する
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="railEditor">自動配置の基準にするレール</param>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(Obj3dCommon* objCommon,const RailEditor* railEditor,const std::string& paramGroup);

	// 配置エディターの更新と、編集結果(追加・削除・ドラッグ移動)の的リストへの反映
	void UpdateEditor();

	/// <summary>
	/// 照準判定・弾との当たり判定・描画用の行列更新
	/// </summary>
	/// <param name="cameraPosition">カメラの位置</param>
	/// <param name="cameraForward">カメラの前方ベクトル(画面中央のレティクルの方向)</param>
	/// <param name="bullets">当たり判定を行うプレイヤーの弾</param>
	/// <returns>いずれかの的をレティクルで狙えているときtrue</returns>
	bool Update(const Vector3& cameraPosition,const Vector3& cameraForward,PlayerBulletManager& bullets);

	// すべての的を生存状態に戻す
	void Reset();

	// 生存している的を描画する
	void Draw();

	// 直前のUpdate()で壊れた的の座標の一覧を取得(撃破演出・連鎖数の加算に使う)
	const std::vector<Vector3>& GetDestroyedPositions() const{ return destroyedPositions_; }
	// 配置されている的の総数を取得
	int GetTotalCount() const{ return static_cast<int>(targets_.size()); }
	// 壊された的の数を取得
	int GetDestroyedCount() const;

#ifdef USE_IMGUI
	// Hierarchyパネルの中身(的の一覧。クリックで選択できる)を表示する
	void ShowHierarchy();
	// Inspectorパネルの中身(選択中の的の座標・生存フラグの編集)を表示する
	void ShowInspector();
#endif

private:
	// 的
	struct Target{
		std::unique_ptr<Obj3D> obj;
		Vector3 position;
		bool isAlive = true;
	};

	// 保存済みの配置が無いときに、レール沿いへ自動配置した初期データを作る
	void CreateDefaultPlacement(const RailEditor* railEditor);

	// 配置エディターの内容を的のリストに反映する
	void SyncFromEditor();

	Obj3dCommon* objCommon_ = nullptr;

	// 調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;

	// 的の配置エディター(的の座標はこちらが保持し、こちらは毎フレーム同期する)
	std::unique_ptr<TargetEditor> editor_;

	// 的に使う円盤モデル(当たり判定で境界球を求めるのに使う)
	Model* model_ = nullptr;

	// 的のリスト
	std::vector<Target> targets_;

	// 直前のUpdate()で壊れた的の座標(Update()の最初に空にする)
	std::vector<Vector3> destroyedPositions_;

	// 照準判定の許容角度の初期値(ラジアン、約5度。保存済みJSONがあればそちらが優先される)
	static constexpr float kDefaultAimHitAngle = 0.09f;
	// 狙えているときの表示スケール(少し大きくして視覚的にフィードバックする)
	static constexpr float kAimedScale = 0.6f;
	// 狙えていないときの表示スケール
	static constexpr float kNormalScale = 0.4f;
	// カメラと的が重なっているとみなす距離(向きが定まらないため照準判定をしない)
	static constexpr float kMinAimDistance = 0.001f;
};
