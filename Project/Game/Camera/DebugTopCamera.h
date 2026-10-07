#pragma once

#include "MyMath.h"

// 前方宣言
class Input;
class Obj3dCommon;
class RailEditor;

/// <summary>
/// レール全体を真上から見下ろす俯瞰デバッグカメラ
/// F1キー(ImGuiが無い環境用)またはImGuiのチェックボックスでプレイ用カメラと切り替える。
/// 有効な間はWASD(平面移動)+QE(高さ)で自由に動かせる。
/// </summary>
class DebugTopCamera{
public:
	DebugTopCamera();
	~DebugTopCamera();

	// カメラの生成と初期位置の設定
	void Initialize(Obj3dCommon* objCommon);

	/// <summary>
	/// F1キーでの切り替え処理
	/// ONにした瞬間だけ、制御点全体を囲むように自動フィットさせる
	/// </summary>
	/// <param name="input">入力</param>
	/// <param name="railEditor">フィット先のレール</param>
	void HandleToggleKey(Input* input,const RailEditor* railEditor);

	// 有効な間の移動処理(WASDで平面移動、QEで高さ)
	void UpdateMove(Input* input);

#ifdef USE_IMGUI
	// 切り替え用のチェックボックスを表示する(ONにした瞬間だけ制御点全体にフィットさせる)
	void ShowToggleCheckbox(const RailEditor* railEditor);
#endif

	// 俯瞰デバッグカメラが有効かどうかを取得
	bool IsEnabled() const{ return isEnabled_; }

private:
	// 制御点全体を囲むように、カメラの位置と向きを合わせる
	void FitToRail(const RailEditor* railEditor);

	Obj3dCommon* objCommon_ = nullptr;

	// 俯瞰デバッグカメラON/OFF状態
	bool isEnabled_ = false;

	// 初期位置の高さ
	static constexpr float kInitialHeight = 30.0f;
	// 真下を向かせるためのX軸回転(90度=pi/2)
	static constexpr float kLookDownRotateX = 3.14159265f * 0.5f;
	// フィット時に確保する最低限の高さ
	static constexpr float kMinHeight = 10.0f;
	// 画角に対する余白の目安(仮値)
	static constexpr float kMarginFactor = 2.2f;
	// 1秒あたりの移動量
	static constexpr float kMoveSpeed = 10.0f;
	// 1フレームの経過時間(エンジンが固定60fps前提)
	static constexpr float kDeltaTime = 1.0f / 60.0f;
};
