#include "Input.h"

/// <summary>
/// 初期化処理
/// DirectInputオブジェクトの生成と、キーボード・マウスデバイスの設定を行います。
/// </summary>
void Input::Initialize(WinAPI* winApi){
	this->winApi_ = winApi;

	HRESULT result;

	// 1. DirectInput8 インターフェースの作成
	result = DirectInput8Create(
		winApi->GetHinstance(),
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)directInput_.GetAddressOf(),
		nullptr);
	assert(SUCCEEDED(result));

	// 2. キーボードデバイスの作成
	result = directInput_->CreateDevice(GUID_SysKeyboard,&keyboard_,NULL);
	assert(SUCCEEDED(result));

	// 3. 入力データ形式のセット (標準的なキーボードフォーマット)
	result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result));

	// 4. 協調レベルのセット
	//    DISCL_FOREGROUND   : 画面が手前にある場合のみ入力を受け付ける
	//    DISCL_NONEXCLUSIVE : 他のアプリとデバイスを共有する
	//    DISCL_NOWINKEY     : Windowsキーを無効にする (ゲーム中の誤操作防止)
	result = keyboard_->SetCooperativeLevel(
		winApi->GetHwnd(),
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));

	// 5. マウスデバイスの作成
	result = directInput_->CreateDevice(GUID_SysMouse,&mouse_,NULL);
	assert(SUCCEEDED(result));

	// 6. マウスの入力データ形式のセット (標準的なマウスフォーマット。相対移動量を取得する)
	result = mouse_->SetDataFormat(&c_dfDIMouse);
	assert(SUCCEEDED(result));

	// 7. マウスの協調レベルのセット
	//    DISCL_FOREGROUND   : 画面が手前にある場合のみ入力を受け付ける
	//    DISCL_NONEXCLUSIVE : 他のアプリとデバイスを共有する (マウスカーソルを掴んだままにしない)
	result = mouse_->SetCooperativeLevel(
		winApi->GetHwnd(),
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(result));
}

/// <summary>
/// 更新処理
/// 毎フレーム呼び出し、キーボードとマウスの状態を取得します。
/// </summary>
void Input::Update(){
	// 前回のキー入力を保存 (memcpyで高速コピー)
	memcpy(preKey_,key_,sizeof(key_));

	// キーボード情報の取得を開始
	// ※ Acquireはデバイスの権限を取得するもの。
	//    ウィンドウがアクティブでない間は失敗する可能性があります。
	keyboard_->Acquire();

	// 全キーの状態を取得
	HRESULT result = keyboard_->GetDeviceState(sizeof(key_),key_);

	// 取得に失敗した場合 (ウィンドウ外をクリックした、最小化した等)
	if(FAILED(result)){
		// 入力データをクリアする
		// キーを押したままウィンドウを切り替えた際などの誤入力を防ぐ
		memset(key_,0,sizeof(key_));
	}

	// マウス情報の取得を開始
	mouse_->Acquire();

	// マウスの相対移動量を取得
	HRESULT mouseResult = mouse_->GetDeviceState(sizeof(DIMOUSESTATE),&mouseState_);

	// 取得に失敗した場合は移動量を0にする (誤動作防止)
	if(FAILED(mouseResult)){
		mouseState_ = {};
	}
}

/// <summary>
/// キーが押されているか (長押し判定)
/// </summary>
bool Input::PushKey(BYTE keyNumber){
	// 値が0でなければ押されている
	if(key_[keyNumber]){
		return true;
	}
	return false;
}

/// <summary>
/// キーがこのフレームで押されたか (トリガー判定)
/// </summary>
bool Input::TriggerKey(BYTE keyNumber){
	// 前フレームで押されておらず、現在押されている場合
	if(!preKey_[keyNumber] && key_[keyNumber]){
		return true;
	}
	return false;
}

// マウスのX軸移動量を取得 (1フレーム分の相対移動量)
float Input::GetMouseDeltaX() const{
	return static_cast<float>(mouseState_.lX);
}

// マウスのY軸移動量を取得 (1フレーム分の相対移動量)
float Input::GetMouseDeltaY() const{
	return static_cast<float>(mouseState_.lY);
}