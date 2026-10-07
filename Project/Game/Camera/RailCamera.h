#pragma once

#include "MyMath.h"
#include <string>

// 前方宣言
class Input;

/// <summary>
/// レール追従カメラ(三人称視点のプレイ用カメラ)
/// プレイヤーの基準位置より少し上に置き、基準向きにマウスでの照準オフセットを上乗せした向きにする。
/// 計算した位置・向きはプレイ用カメラ("default")に反映する。
/// </summary>
class RailCamera{
public:
	RailCamera();
	~RailCamera();

	/// <summary>
	/// 初期化処理(調整項目をGlobalVariablesに登録する)
	/// </summary>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(const std::string& paramGroup);

	// 照準オフセットを初期状態(レールの向きそのまま)に戻す
	void Reset();

	/// <summary>
	/// 更新処理(位置・向き・前方/右方向ベクトルを計算し、プレイ用カメラに反映する)
	/// </summary>
	/// <param name="basePosition">カメラ・プレイヤーの共通の基準位置</param>
	/// <param name="baseRotation">カメラ・プレイヤーの共通の基準向き</param>
	/// <param name="isInputEnabled">照準の入力を受け付けるかどうか(プレイ可能な間のみtrue)</param>
	/// <param name="input">入力</param>
	void Update(const Vector3& basePosition,const Vector3& baseRotation,bool isInputEnabled,Input* input);

	// カメラの位置を取得
	const Vector3& GetPosition() const{ return position_; }
	// カメラの向き(基準向き+照準オフセット)を取得
	const Vector3& GetRotation() const{ return rotation_; }
	// カメラの前方ベクトルを取得
	const Vector3& GetForward() const{ return forward_; }
	// カメラの右方向ベクトルを取得
	const Vector3& GetRight() const{ return right_; }

private:
	// 調整項目を登録したGlobalVariablesのグループ名
	std::string paramGroup_;

	// プレイヤー入力によるカメラの照準オフセット(レールの向きに上乗せする)
	float aimYawOffset_ = 0.0f;   // 左右(Y軸回転)
	float aimPitchOffset_ = 0.0f; // 上下(X軸回転)

	// 計算結果
	Vector3 position_ = {0.0f, 0.0f, 0.0f}; // カメラの位置
	Vector3 rotation_ = {0.0f, 0.0f, 0.0f}; // カメラの向き
	Vector3 forward_ = {0.0f, 0.0f, 1.0f};  // 前方ベクトル
	Vector3 right_ = {1.0f, 0.0f, 0.0f};    // 右方向ベクトル

	// 調整項目の初期値(保存済みJSONがあればそちらが優先される)
	// 三人称視点用に、カメラをレールそのものより少し上に置くオフセット
	static constexpr float kDefaultHeightOffset = 1.25f;
	// マウス1移動量あたりの回転量(ラジアン)
	static constexpr float kDefaultMouseSensitivity = 0.0004f;
	// 左右の可動範囲(約34度)
	static constexpr float kDefaultAimYawLimit = 0.6f;
	// 上下の可動範囲(約29度)
	static constexpr float kDefaultAimPitchLimit = 0.5f;

	// 回転前の前方ベクトル
	static constexpr Vector3 kBaseForward = {0.0f, 0.0f, 1.0f};
	// 回転前の右方向ベクトル
	static constexpr Vector3 kBaseRight = {1.0f, 0.0f, 0.0f};
};
