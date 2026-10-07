#pragma once

#include "MyMath.h"

// 前方宣言
class Input;
class Obj3dCommon;
class RailEditor;

/// <summary>
/// レール全体を確認するためのデバッグカメラ
/// F1キー(ImGuiが無い環境用)またはImGuiのチェックボックスでプレイ用カメラと切り替える。
/// ONにした瞬間はレール全体を真上から見下ろし、以降はWASD(平面移動)+QE(高さ)+右ドラッグ(向き)で自由に動かせる。
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

	// 有効な間の移動処理(WASDで向いている方向基準の平面移動、QEで高さ、右ドラッグで向きの変更)
	void UpdateMove(Input* input);

#ifdef USE_IMGUI
	// 切り替え用のチェックボックスを表示する(ONにした瞬間だけ制御点全体にフィットさせる)
	void ShowToggleCheckbox(const RailEditor* railEditor);
#endif

	// 俯瞰デバッグカメラが有効かどうかを取得
	bool IsEnabled() const{ return isEnabled_; }

	// 俯瞰デバッグカメラをOFFにして、プレイ用カメラに戻す(Playを押したときに呼ぶ)
	void Disable();

private:
	// 制御点全体を囲むように、カメラの位置と向きを合わせる
	void FitToRail(const RailEditor* railEditor);

	// ON/OFFの状態に合わせて、アクティブなカメラを俯瞰デバッグカメラとプレイ用カメラで切り替える
	void ApplyActiveCamera();

	// マウスカーソルがゲーム画面の上にあるか(編集パネルの上で右ボタンを押したときに回転させないための判定)
	bool IsMouseOnGameView() const;

	Obj3dCommon* objCommon_ = nullptr;

	// 俯瞰デバッグカメラON/OFF状態
	bool isEnabled_ = false;

	// 右ドラッグで変更するカメラの向き
	float pitch_ = kLookDownRotateX; // 上下(X軸回転)
	float yaw_ = 0.0f;               // 左右(Y軸回転)

	// 右ドラッグで向きを変えている最中か
	bool isRotating_ = false;
	// 前フレームで右ボタンが押されていたか(押した瞬間を検出するのに使う)
	bool wasRotateButtonDown_ = false;

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
	// 右ドラッグ時のマウス1移動量あたりの回転量(ラジアン)
	static constexpr float kRotateSensitivity = 0.003f;
	// 向きを変えるときに押すマウスボタン(右ボタン)
	static constexpr int kRotateMouseButton = 1;
	// 1フレームの経過時間(エンジンが固定60fps前提)
	static constexpr float kDeltaTime = 1.0f / 60.0f;
};
