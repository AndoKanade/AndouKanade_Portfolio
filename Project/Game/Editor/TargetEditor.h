#pragma once
#include "MyMath.h"
#include <vector>

/// <summary>
/// 的の配置エディター
///
/// 配置モード(Placement Mode)をONにすると、Sceneビュー上でマウスドラッグにより的を移動できる。
/// 配置内容はJSONファイルへの保存・再読込に対応しており、シーン側は本クラスの座標を参照して的を生成する。
/// </summary>
class TargetEditor{
public:
	// 配置された的1個分のデータ
	struct TargetPoint{
		Vector3 position; // 的のワールド座標
	};

	TargetEditor();
	~TargetEditor();

	// 初期化処理(保存済みJSONがあれば読み込む)
	void Initialize();
	// 更新処理(ImGuiでの編集UIとマウスドラッグによる移動)
	void Update();

	// 配置されている的の個数を取得
	int GetTargetCount() const;
	// 指定インデックスの的の座標を取得
	Vector3 GetTargetPosition(int index) const;
	// 指定インデックスの的の座標への参照を取得(Inspectorからの編集用)
	Vector3& GetTargetPositionRef(int index);

	// 指定座標に的を1個追加する(初回起動時の初期配置生成用)
	void AddTargetAt(const Vector3& position);

	// 選択中の的のインデックスを取得(-1は未選択)
	int GetSelectedIndex() const;
	// 選択中の的のインデックスを設定する(-1で選択解除)
	void SetSelectedIndex(int index);

	// 配置内容をJSONファイルに保存する
	void SaveToJson();
	// 配置内容をJSONファイルから読み込む
	void LoadFromJson();

private:
	// マウス位置から画面奥へ伸びるレイ
	struct PickRay{
		Vector3 origin;    // レイの始点(ニアクリップ面上の点)
		Vector3 direction; // レイの正規化された方向
	};

	// 現在のカメラの前方に的を1個追加する(Add Targetボタン用)
	void AddTargetInFrontOfCamera();
	// Editモード中にゲーム画面が実際に描かれている矩形(レターボックス適用後)を求める
	bool ComputeGameViewportRect(float& outX,float& outY,float& outW,float& outH) const;
	// 現在のマウス位置から伸びるレイを求める(画面外・カメラ未取得のときはfalse)
	bool ComputeMouseRay(PickRay& outRay) const;
	// レイに最も近い的のインデックスを求める(掴める距離に無ければ-1)
	int PickTargetIndex(const PickRay& ray) const;
	// マウスドラッグによる的の移動処理
	void UpdateMouseDrag();

	std::vector<TargetPoint> targets_; // 配置されている的の配列

	int selectedIndex_ = -1;      // 選択中の的のインデックス(-1: 未選択)
	bool isPlacementMode_ = true; // 配置モード(ONの間だけマウスドラッグでの移動を受け付ける)

	// マウスドラッグ中の状態
	bool isDragging_ = false;                        // ドラッグ中かどうか
	Vector3 dragPlaneNormal_ = {0.0f, 0.0f, 1.0f};   // ドラッグ面の法線(掴んだ瞬間のレイ方向)
	Vector3 dragPlanePoint_ = {0.0f, 0.0f, 0.0f};    // ドラッグ面が通る点(掴んだ瞬間の的の座標)
	Vector3 dragOffset_ = {0.0f, 0.0f, 0.0f};        // 掴んだ位置と的の座標とのずれ(掴み直しの跳びを防ぐ)

	// 新規追加時にカメラ前方へ置く距離
	static constexpr float kSpawnDistance = 15.0f;
	// マウスで的を掴めるとみなす、レイから的までの距離
	static constexpr float kPickRadius = 1.0f;
};
