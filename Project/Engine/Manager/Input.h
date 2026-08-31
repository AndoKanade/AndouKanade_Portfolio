#pragma once

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定 (includeより前に書く必要がある)
#include <Windows.h>
#include <wrl.h>
#include <dinput.h>
#include <cassert>
#include "WinAPI.h"

// 必要なライブラリのリンク
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

/// <summary>
/// 入力管理クラス
/// DirectInputを使用したキーボード・マウス入力を管理します。
/// </summary>
class Input{
public:
	// 名前空間の省略 (ComPtr用)
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="winApi">WinAPIラッパーへのポインタ (ウィンドウハンドル取得用)</param>
	void Initialize(WinAPI* winApi);

	/// <summary>
	/// 更新処理
	/// 毎フレーム呼び出し、キーの入力状態を取得します。
	/// </summary>
	void Update();

	/// <summary>
	/// キーが押されているか (長押し判定)
	/// </summary>
	/// <param name="keyNumber">DIK_SPACE などのキーコード</param>
	/// <returns>押されていれば true</returns>
	bool PushKey(BYTE keyNumber);

	/// <summary>
	/// キーがこのフレームで押されたか (トリガー判定)
	/// </summary>
	/// <param name="keyNumber">DIK_SPACE などのキーコード</param>
	/// <returns>押された瞬間なら true</returns>
	bool TriggerKey(BYTE keyNumber);

	// マウスのX軸移動量を取得 (1フレーム分の相対移動量)
	float GetMouseDeltaX() const;
	// マウスのY軸移動量を取得 (1フレーム分の相対移動量)
	float GetMouseDeltaY() const;

private:
	// 借りてくるインスタンス
	WinAPI* winApi_ = nullptr;

	// DirectInputのインターフェース
	ComPtr<IDirectInput8> directInput_ = nullptr;
	ComPtr<IDirectInputDevice8> keyboard_ = nullptr;
	ComPtr<IDirectInputDevice8> mouse_ = nullptr; // マウスデバイス

	// 全キーの入力状態 (256キー分)
	BYTE key_[256] = {};     // 現在のフレームのキー状態
	BYTE preKey_[256] = {};  // 1フレーム前のキー状態

	// マウスの現在フレームの状態 (相対移動量など)
	DIMOUSESTATE mouseState_ = {};
};