#include "RailCamera.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Input.h"
#include "GlobalVariables.h"
#include <cmath>

namespace{
	// プレイ用カメラの名前
	const char* kDefaultCameraName = "default";

	// 回転行列でベクトルを回転させる(行ベクトル × 行列の順で掛ける)
	Vector3 RotateVector(const Vector3& v,const Matrix4x4& m){
		return {
			v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
			v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
			v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]
		};
	}

	// 角度の差を -π〜π に収める(1周回った角度を逆回りで追いかけないようにする)
	float WrapAngle(float angle,float pi){
		const float twoPi = pi + pi;
		angle = std::fmod(angle + pi,twoPi);
		if(angle < 0.0f){
			angle += twoPi;
		}
		return angle - pi;
	}
}

RailCamera::RailCamera() = default;
RailCamera::~RailCamera() = default;

// 初期化処理
// ImGuiの "Global Variables" ウィンドウから実行中に編集・保存でき、外部ファイルの書き換えも自動反映される(ホットリロード)
void RailCamera::Initialize(const std::string& paramGroup){
	paramGroup_ = paramGroup;

	// 第3引数はデフォルト値。保存済みJSONがあればそちらが優先される(AddItemは未登録キーのみ追加)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->AddItem(paramGroup_,"cameraHeightOffset",kDefaultHeightOffset);
	gv->AddItem(paramGroup_,"cameraDistance",kDefaultDistance);                  // プレイヤーの後ろへ離す距離
	gv->AddItem(paramGroup_,"cameraFollowSharpness",kDefaultFollowSharpness);    // レールの向きへの追従の速さ
	// 照準(エイム)まわりの手触り調整用パラメータ(照準操作はマウスで行う)
	gv->AddItem(paramGroup_,"mouseSensitivity",kDefaultMouseSensitivity); // マウス1移動量あたりの回転量(ラジアン)
	gv->AddItem(paramGroup_,"aimYawLimit",kDefaultAimYawLimit);           // 左右の可動範囲
	gv->AddItem(paramGroup_,"aimPitchLimit",kDefaultAimPitchLimit);       // 上下の可動範囲
}

// 照準オフセットを初期状態に戻す
void RailCamera::Reset(){
	aimYawOffset_ = 0.0f;
	aimPitchOffset_ = 0.0f;
	// リスタート直後に前回の向きから回り込まないよう、次の更新でレールの向きへそのまま合わせる
	isFollowSnapRequested_ = true;
}

// 更新処理
// プレイヤーの基準位置を回転の中心にし、カメラの向きの後ろ側へ離して置くことで、照準を動かすとプレイヤーの周りを回り込む
void RailCamera::Update(const Vector3& basePosition,const Vector3& baseRotation,bool isInputEnabled,Input* input,float deltaTime){
	// 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	float heightOffset = gv->GetFloatValue(paramGroup_,"cameraHeightOffset");
	float distance = gv->GetFloatValue(paramGroup_,"cameraDistance");
	float followSharpness = gv->GetFloatValue(paramGroup_,"cameraFollowSharpness");
	float mouseSensitivity = gv->GetFloatValue(paramGroup_,"mouseSensitivity");
	float aimYawLimit = gv->GetFloatValue(paramGroup_,"aimYawLimit");
	float aimPitchLimit = gv->GetFloatValue(paramGroup_,"aimPitchLimit");

	// プレイヤー入力で照準(カメラの向き)をレールの向きに上乗せする
	// Edit中・開始演出中は入力を受け付けない(ゲームは静止)
	if(isInputEnabled && input){
		aimYawOffset_ += input->GetMouseDeltaX() * mouseSensitivity;
		aimPitchOffset_ += input->GetMouseDeltaY() * mouseSensitivity;

		// 可動範囲でクランプ(振り向きすぎないように)
		if(aimYawOffset_ > aimYawLimit) aimYawOffset_ = aimYawLimit;
		if(aimYawOffset_ < -aimYawLimit) aimYawOffset_ = -aimYawLimit;
		if(aimPitchOffset_ > aimPitchLimit) aimPitchOffset_ = aimPitchLimit;
		if(aimPitchOffset_ < -aimPitchLimit) aimPitchOffset_ = -aimPitchLimit;
	}

	// レールの向き(基準向き)へ少し遅れて追従させる
	// 経過時間に依存しない指数的な補間にして、フレームレートが変わっても追従の速さを同じにする
	if(isFollowSnapRequested_){
		followRotation_ = baseRotation;
		isFollowSnapRequested_ = false;
	} else{
		float followRate = 1.0f - std::exp(-followSharpness * deltaTime);
		followRotation_.x += WrapAngle(baseRotation.x - followRotation_.x,kPi) * followRate;
		followRotation_.y += WrapAngle(baseRotation.y - followRotation_.y,kPi) * followRate;
		followRotation_.z += WrapAngle(baseRotation.z - followRotation_.z,kPi) * followRate;
	}

	// 追従させた向き + 照準オフセットを最終的なカメラの向きとする
	rotation_ = {followRotation_.x + aimPitchOffset_, followRotation_.y + aimYawOffset_, followRotation_.z};

	// カメラの前方ベクトルを計算(rotation_ベースの回転行列を適用)
	Matrix4x4 rotateX = MakeRotateXMatrix(rotation_.x);
	Matrix4x4 rotateY = MakeRotateYMatrix(rotation_.y);
	Matrix4x4 rotateZ = MakeRotateZMatrix(rotation_.z);
	Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX,rotateY),rotateZ);

	forward_ = RotateVector(kBaseForward,rotateMatrix);

	// カメラの右方向ベクトルをオフレール中のWASD移動に使用するため、前方ベクトルと同じ回転行列から算出する
	right_ = RotateVector(kBaseRight,rotateMatrix);

	// 三人称視点用に、基準位置より少し上を中心にして、カメラの向きの後ろ側へ離して置く
	position_ = basePosition + Vector3{0.0f, heightOffset, 0.0f} - forward_ * distance;

	if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
		mainCamera->SetTranslate(position_);
		mainCamera->SetRotate(rotation_);
	}
}
