#include "TitleCameraDirector.h"
#include "Easing.h"
#include <algorithm>
#include <cmath>

// タイトル開始時の状態に戻す
void TitleCameraDirector::Reset(){
	idleTime_ = 0.0f;
	turnTimer_ = 0.0f;
	isTurning_ = false;
	turnStartAngle_ = 0.0f;
}

// タイトル中の揺れを進める
void TitleCameraDirector::UpdateIdle(float deltaTime){
	idleTime_ += deltaTime;
}

// 回り込みを開始する
void TitleCameraDirector::BeginTurn(){
	turnStartAngle_ = GetIdleAngle();
	turnTimer_ = 0.0f;
	isTurning_ = true;
}

// 回り込みを進める(終了時間を超えないよう止める)
void TitleCameraDirector::UpdateTurn(float deltaTime){
	turnTimer_ = (std::min)(turnTimer_ + deltaTime,kTurnDuration);
}

// 回り込みが終わったか
bool TitleCameraDirector::IsTurnFinished() const{
	return isTurning_ && turnTimer_ >= kTurnDuration;
}

// 回り込みの進行度(0〜1、イージング前)
float TitleCameraDirector::GetTurnProgress() const{
	return isTurning_?(turnTimer_ / kTurnDuration):0.0f;
}

// タイトル中の回転角
// プレイヤーの正面(プレイ用カメラから180度回った位置)を中心に、ゆっくり左右へ揺らす
float TitleCameraDirector::GetIdleAngle() const{
	return kPi + kSwayAmplitude * std::sin(idleTime_ * kSwaySpeed);
}

// 演出中のカメラ座標・向きを求める
void TitleCameraDirector::Calculate(const Vector3& pivot,const Vector3& gameplayPosition,const Vector3& gameplayRotate,Vector3& outPosition,Vector3& outRotate) const{
	// プレイ用カメラからの回転角と、プレイヤーとの距離の倍率を決める
	float angle = GetIdleAngle();
	float distanceScale = kTitleDistanceScale;
	if(isTurning_){
		// 回り込み中はイージングを掛けた進行度で、開始時の角度からプレイ用カメラ(角度0)へ近づける
		float easedProgress = Easing::EaseInOutCubic(GetTurnProgress());
		angle = Lerp(turnStartAngle_,0.0f,easedProgress);
		distanceScale = Lerp(kTitleDistanceScale,kGameplayDistanceScale,easedProgress);
	}

	// プレイヤーから見たプレイ用カメラの位置を、距離の倍率を掛けてからY軸まわりに回転させる
	// 回転の向きはカメラのY回転(ヨー)と揃えているため、向きにも同じ角度を足せば常にプレイヤーを同じ構図で映せる
	Vector3 offset = (gameplayPosition - pivot) * distanceScale;
	float sinAngle = std::sin(angle);
	float cosAngle = std::cos(angle);
	Vector3 rotatedOffset = {
		offset.x * cosAngle + offset.z * sinAngle,
		offset.y,
		-offset.x * sinAngle + offset.z * cosAngle
	};

	outPosition = pivot + rotatedOffset;
	outRotate = {gameplayRotate.x, gameplayRotate.y + angle, gameplayRotate.z};
}
