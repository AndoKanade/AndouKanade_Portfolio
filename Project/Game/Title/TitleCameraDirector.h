#pragma once

#include "MyMath.h"

/// <summary>
/// タイトル演出のカメラワーク
/// タイトル中はプレイヤーを正面から映し、スタート後はプレイヤーを中心に180度回り込んで
/// プレイ用カメラ(レール追従カメラ)の位置・向きへ移動させる。
/// プレイ用カメラをプレイヤー中心に水平回転させた形で求めるため、回り込み終了時にプレイ用カメラと完全に一致する。
/// </summary>
class TitleCameraDirector{
public:
	// タイトル開始時の状態に戻す
	void Reset();

	// タイトル中の揺れを進める
	void UpdateIdle(float deltaTime);

	// 回り込みを開始する(現在の揺れの角度から回り始める)
	void BeginTurn();

	// 回り込みを進める
	void UpdateTurn(float deltaTime);

	// 回り込みが終わったか
	bool IsTurnFinished() const;

	// 回り込みの進行度(0〜1、イージング前)
	float GetTurnProgress() const;

	/// <summary>
	/// 演出中のカメラ座標・向きを求める
	/// </summary>
	/// <param name="pivot">回転の中心(プレイヤーの座標)</param>
	/// <param name="gameplayPosition">プレイ用カメラの座標</param>
	/// <param name="gameplayRotate">プレイ用カメラの向き</param>
	/// <param name="outPosition">演出中のカメラ座標</param>
	/// <param name="outRotate">演出中のカメラの向き</param>
	void Calculate(const Vector3& pivot,const Vector3& gameplayPosition,const Vector3& gameplayRotate,Vector3& outPosition,Vector3& outRotate) const;

private:
	// タイトル中の揺れを含めた、プレイ用カメラからの回転角(Y軸)
	float GetIdleAngle() const;

	// 円周率
	static constexpr float kPi = 3.14159265f;
	// 回り込みにかける時間(秒)
	static constexpr float kTurnDuration = 2.0f;
	// タイトル中のカメラ距離の倍率(プレイ用カメラとプレイヤーの距離に掛けて、少し寄った画にする)
	static constexpr float kTitleDistanceScale = 0.6f;
	// プレイ用カメラと同じ距離の倍率(回り込み終了時の値)
	static constexpr float kGameplayDistanceScale = 1.0f;
	// タイトル中の左右の揺れ幅(ラジアン、約5度)
	static constexpr float kSwayAmplitude = 0.08f;
	// タイトル中の揺れの速さ(1秒あたりの位相の進み、ラジアン)
	static constexpr float kSwaySpeed = 0.8f;

	// タイトル中の経過時間(揺れの位相に使う)
	float idleTime_ = 0.0f;
	// 回り込みの経過時間(秒)
	float turnTimer_ = 0.0f;
	// 回り込み中かどうか
	bool isTurning_ = false;
	// 回り込み開始時の回転角(揺れの途中から滑らかに回り始めるため記録する)
	float turnStartAngle_ = 0.0f;
};
