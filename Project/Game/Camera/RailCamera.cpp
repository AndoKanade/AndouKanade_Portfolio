#include "RailCamera.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Input.h"
#include "GlobalVariables.h"

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
	// 照準(エイム)まわりの手触り調整用パラメータ(照準操作はマウスで行う)
	gv->AddItem(paramGroup_,"mouseSensitivity",kDefaultMouseSensitivity); // マウス1移動量あたりの回転量(ラジアン)
	gv->AddItem(paramGroup_,"aimYawLimit",kDefaultAimYawLimit);           // 左右の可動範囲
	gv->AddItem(paramGroup_,"aimPitchLimit",kDefaultAimPitchLimit);       // 上下の可動範囲
}

// 照準オフセットを初期状態に戻す
void RailCamera::Reset(){
	aimYawOffset_ = 0.0f;
	aimPitchOffset_ = 0.0f;
}

// 更新処理
void RailCamera::Update(const Vector3& basePosition,const Vector3& baseRotation,bool isInputEnabled,Input* input){
	// 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	float heightOffset = gv->GetFloatValue(paramGroup_,"cameraHeightOffset");
	float mouseSensitivity = gv->GetFloatValue(paramGroup_,"mouseSensitivity");
	float aimYawLimit = gv->GetFloatValue(paramGroup_,"aimYawLimit");
	float aimPitchLimit = gv->GetFloatValue(paramGroup_,"aimPitchLimit");

	// 三人称視点用に、カメラの実位置は基準位置そのものではなく少し上に置く
	position_ = basePosition + Vector3{0.0f, heightOffset, 0.0f};

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

	// 基準向き + 照準オフセットを最終的なカメラの向きとする
	rotation_ = {baseRotation.x + aimPitchOffset_, baseRotation.y + aimYawOffset_, baseRotation.z};

	if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
		mainCamera->SetTranslate(position_);
		mainCamera->SetRotate(rotation_);
	}

	// カメラの前方ベクトルを計算(rotation_ベースの回転行列を適用)
	Matrix4x4 rotateX = MakeRotateXMatrix(rotation_.x);
	Matrix4x4 rotateY = MakeRotateYMatrix(rotation_.y);
	Matrix4x4 rotateZ = MakeRotateZMatrix(rotation_.z);
	Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX,rotateY),rotateZ);

	forward_ = RotateVector(kBaseForward,rotateMatrix);

	// カメラの右方向ベクトルをオフレール中のWASD移動に使用するため、前方ベクトルと同じ回転行列から算出する
	right_ = RotateVector(kBaseRight,rotateMatrix);
}
