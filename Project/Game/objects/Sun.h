#pragma once

#include "MyMath.h"
#include <string>

// 前方宣言
struct DirectionalLight;

/// <summary>
/// 太陽(見た目は持たない光源)
/// 3Dオブジェクト共通の平行光源を、太陽の高さ・方角・色・明るさから設定する。
/// 方角はレール開始地点の進行方向を基準にするため、レールの向きを変えても光の当たり方が変わらない。
/// 調整項目はGlobalVariablesに登録し、ImGuiの編集や外部ファイルの書き換えを毎フレーム反映する。
/// </summary>
class Sun{
public:
	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	/// <param name="referenceForward">方角の基準にする向き(レール開始地点の進行方向)</param>
	void Initialize(const std::string& paramGroup,const Vector3& referenceForward);

	// 更新処理(調整項目の最新値から平行光源を設定する)
	void Update();

private:
	// 設定先の平行光源(3Dオブジェクト共通で使われているもの)
	DirectionalLight* light_ = nullptr;

	// 調整項目を登録しているグループ名
	std::string paramGroup_;

	// 方角の基準にする水平な前方向と右方向
	Vector3 referenceForward_ = {0.0f, 0.0f, 1.0f};
	Vector3 referenceRight_ = {1.0f, 0.0f, 0.0f};

	// 太陽の高さの初期値(地平線からの角度・度)。低めにして雲の起伏に陰影が出やすくする
	static constexpr float kDefaultElevationDegree = 25.0f;
	// 太陽の方角の初期値(進行方向から右回りの角度・度)。斜め前から当てて起伏を浮き立たせる
	static constexpr float kDefaultAzimuthDegree = 50.0f;
	// 光の明るさの初期値(太陽が低いと上向きの面が暗くなるため、1より強めにする)
	static constexpr float kDefaultIntensity = 1.5f;
	// 光の色の初期値(少し暖かい日差しの色)
	static constexpr Vector3 kDefaultColor = {1.0f, 0.92f, 0.8f};
};
