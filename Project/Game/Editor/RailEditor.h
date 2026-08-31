#pragma once
#include "MyMath.h"
#include <vector>
#include <memory>
#include <string>

// クラスの前方宣言
class Obj3D;
class Obj3dCommon;

class RailEditor{
public:
	// レールの制御点を表す構造体
	struct ControlPoint{
		Vector3 position; // 制御点の3D座標
		Vector3 rotation; // 通過時のカメラの角度
		float speed;      // 次の点への移動速度

		// レール間分岐移動用のデータ
		int branchTargetRailIndex = -1;  // 分岐先レールのインデックス(-1: 分岐なし)
		int branchTargetPointIndex = -1; // 分岐先レール側の対応する制御点インデックス
	};

	// レール間分岐移動用の分岐先情報
	struct BranchInfo{
		int targetRailIndex = -1;  // 分岐先レールのインデックス(-1: 分岐なし)
		int targetPointIndex = -1; // 分岐先レール側の対応する制御点インデックス
	};

	// ここから追加: 着地判定(オフレール時、全レール中から最も近いレールを探す)用の結果
	struct NearestRailResult{
		int railIndex = -1;  // 最も近いレールのインデックス(レールが1本も無い場合は-1)
		float t = 0.0f;      // そのレール上での進行度(0〜1)
		float distance = 0.0f; // そのレールまでの距離
	};
	// ここまで追加

	RailEditor();
	~RailEditor();

	// 初期化処理
	void Initialize(Obj3dCommon* objCommon);
	// 更新処理（ImGuiでの編集など）
	void Update();
	// デバッグ用の描画処理
	void Draw();

	// レール上の座標を取得 (t: 0〜1)。対象はアクティブなレール
	Vector3 GetPositionOnRail(float t) const;
	// レール上の回転を取得 (t: 0〜1)。対象はアクティブなレール
	Vector3 GetRotationOnRail(float t) const;
	// レール上の正規化された進行方向を取得 (t: 0〜1)。対象はアクティブなレール
	Vector3 GetForwardOnRail(float t) const;
	// レール上の速度(各制御点のSpeed値を補間したもの)を取得 (t: 0〜1)。対象はアクティブなレール
	float GetSpeedOnRail(float t) const;

	// ここから追加: レール座標によるオンレール判定用のAPI
	// 指定したワールド座標に最も近いレール上の進行度t(0〜1)を求める(対象はアクティブなレール)
	float FindNearestTOnRail(const Vector3& worldPos) const;
	// 指定したワールド座標からレール上の最近傍点までの距離を求める(対象はアクティブなレール)
	float GetDistanceToRail(const Vector3& worldPos) const;
	// 全レールの中から、指定したワールド座標に最も近いレール・進行度・距離を求める(着地先レールの探索用)
	NearestRailResult FindNearestRail(const Vector3& worldPos) const;
	// ここまで追加

	// 指定インデックスのレールをアクティブなレールとして切り替える(Playモードでのレール乗り換えにも使用)
	void SwitchActiveRail(int index);
	// 現在アクティブなレールのインデックスを取得(デバッグ表示用)
	int GetActiveRailIndex() const;
	// 読み込まれている全レールの本数を取得(デバッグ表示用)
	int GetRailCount() const;
	// 分岐先として強調表示したいレールのインデックスを指定する(-1で強調解除)
	void SetHighlightedRailIndex(int index);

	// アクティブなレールの制御点数を取得
	int GetControlPointCount() const;
	// 進行度tから、直近に通過した制御点のインデックスを取得(対象はアクティブなレール)
	int GetControlPointIndexFromT(float t) const;
	// 指定した制御点インデックスにちょうど乗る進行度tを取得(対象はアクティブなレール)
	float GetTFromControlPointIndex(int pointIndex) const;
	// アクティブなレールの指定インデックスの制御点に設定された分岐先情報を取得
	BranchInfo GetBranchAt(int pointIndex) const;
	// 指定したレール・制御点インデックスの座標を取得(乗り移り方向の判定用。アクティブレール以外も参照可能)
	Vector3 GetControlPointPosition(int railIndex,int pointIndex) const;

	// ImGui非表示時でもキー操作で表示切り替えを行うための関数(対象はアクティブなレール)
	void ToggleShowControlPointModels();
	void ToggleShowCurve();

	// アクティブなレールの全制御点の中心座標を取得
	Vector3 GetControlPointsCenter() const;
	// アクティブなレールの全制御点を囲む範囲の半径を取得
	float GetControlPointsRadius() const;

	// 全レールの制御点をそれぞれ対応するJSONファイルに保存する
	void SaveToJson();
	// 全レールの制御点をJSONファイルから読み込む(ディスクに存在する分だけ連番でレールを復元する)
	void LoadFromJson();

private:
	// 1本分のレールが持つデータ一式(制御点+描画用オブジェクト+表示設定)
	struct Rail{
		std::string name;                                   // レール名(UI表示・保存ファイル名の目印用)
		std::vector<ControlPoint> controlPoints;            // 制御点を保存する配列
		std::vector<std::unique_ptr<Obj3D>> pointObjects;   // 制御点描画用の3Dオブジェクト群
		std::vector<std::unique_ptr<Obj3D>> curveObjects;   // レール曲線サンプリング点描画用のオブジェクト群(固定数)
		bool showControlPointModels = true;                 // 制御点の球体モデルの描画フラグ
		bool showCurve = true;                              // 曲線表示フラグ
	};

	// 制御点描画用の3Dオブジェクト生成
	std::unique_ptr<Obj3D> CreatePointObject();
	// rail.pointObjectsの個数をrail.controlPointsに同期
	void SyncPointObjectsToControlPoints(Rail& rail);
	// 新しいレールを1本追加し、アクティブなレールとして切り替える
	void AddRail();
	// レールのインデックスから保存先ファイルパスを求める(0番目のみ既存の互換パスを使う)
	std::string GetRailFilePath(int index) const;
	// 進行度tと制御点配列から、レール上の座標を計算する(GetPositionOnRail・全レール描画で共用)
	Vector3 ComputePositionOnRail(const std::vector<ControlPoint>& controlPoints,float t) const;
	// ここから追加: 指定ワールド座標に最も近いレール上の進行度tを計算する(粗いサンプリング+三分探索で絞り込み)
	float ComputeNearestTOnRail(const std::vector<ControlPoint>& controlPoints,const Vector3& worldPos) const;
	// ここまで追加
	// 指定インデックスのレール1本だけをJSONファイルに保存する(SaveToJsonの内部実装)
	void SaveRailToFile(int index);
	// 指定インデックスのレール1本だけをJSONファイルから読み込む(LoadFromJsonの内部実装)
	void LoadRailFromFile(int index);

	std::vector<Rail> rails_;       // 全レールの配列
	int activeRailIndex_ = 0;       // 現在編集・使用中のレールのインデックス
	int highlightedRailIndex_ = -1; // 分岐先として強調表示するレールのインデックス(-1: 強調なし)

	// Hierarchy的な一覧(制御点リスト)で選択中の制御点インデックス(-1: 未選択)
	int selectedPointIndex_ = -1;

	Obj3dCommon* objCommon_ = nullptr; // 3Dオブジェクト共通設定へのポインタ

	// レール曲線可視化のサンプリング分割数
	static constexpr int kCurveSampleCount = 100;

	// ここから追加: 最近傍点探索用の定数
	// 粗いサンプリングの分割数(精度と負荷のバランスをとった値)
	static constexpr int kNearestSearchSampleCount = 200;
	// 粗探索後、三分探索で絞り込む反復回数
	static constexpr int kNearestRefineIterationCount = 20;
	// ここまで追加
};