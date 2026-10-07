#include "DebugTopCamera.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Obj3dCommon.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "Editor/RailEditor.h"

namespace{
	// プレイ用カメラの名前
	const char* kDefaultCameraName = "default";
	// 俯瞰デバッグカメラの名前
	const char* kDebugTopCameraName = "debug_top";
}

DebugTopCamera::DebugTopCamera() = default;
DebugTopCamera::~DebugTopCamera() = default;

// カメラの生成と初期位置の設定
void DebugTopCamera::Initialize(Obj3dCommon* objCommon){
	objCommon_ = objCommon;

	CameraManager::GetInstance()->CreateCamera(kDebugTopCameraName,objCommon_->GetDxCommon()->GetDevice());
	Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera(kDebugTopCameraName);
	// 真上から見下ろす位置に配置(X回転90度=pi/2で真下向き)
	debugTopCamera->SetTranslate({0.0f, kInitialHeight, 0.0f});
	debugTopCamera->SetRotate({kLookDownRotateX, 0.0f, 0.0f});
}

// ImGuiが無い環境でもレールの制御点を確認できるよう、F1キーで俯瞰デバッグカメラを切り替える
void DebugTopCamera::HandleToggleKey(Input* input,const RailEditor* railEditor){
	if(!input || !input->TriggerKey(DIK_F1)){
		return;
	}

	isEnabled_ = !isEnabled_;
	Camera* switchedCamera = CameraManager::GetInstance()->GetCamera(isEnabled_?kDebugTopCameraName:kDefaultCameraName);
	CameraManager::GetInstance()->SetActiveCamera(isEnabled_?kDebugTopCameraName:kDefaultCameraName);
	// SetCamera()を毎フレーム呼んでいないオブジェクトは
	// objCommon_のdefaultCamera_を参照し続けるため、こちらも切り替えないと追従しない
	if(switchedCamera && objCommon_){
		objCommon_->SetDefaultCamera(switchedCamera);
	}

	// ONにした瞬間だけ、制御点全体を囲むように自動フィットさせる
	if(isEnabled_){
		FitToRail(railEditor);
	}
}

// 俯瞰デバッグカメラが有効な間は、ImGui無しでもWASD(平面移動)+QE(高さ)で自由に動かせるようにする
void DebugTopCamera::UpdateMove(Input* input){
	if(!isEnabled_ || !input){
		return;
	}

	Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera(kDebugTopCameraName);
	if(!debugTopCamera){
		return;
	}

	const float moveDelta = kMoveSpeed * kDeltaTime;

	Vector3 debugCameraPos = debugTopCamera->GetTranslate();
	if(input->PushKey(DIK_W)) debugCameraPos.z += moveDelta;
	if(input->PushKey(DIK_S)) debugCameraPos.z -= moveDelta;
	if(input->PushKey(DIK_A)) debugCameraPos.x -= moveDelta;
	if(input->PushKey(DIK_D)) debugCameraPos.x += moveDelta;
	if(input->PushKey(DIK_E)) debugCameraPos.y += moveDelta;
	if(input->PushKey(DIK_Q)) debugCameraPos.y -= moveDelta;
	debugTopCamera->SetTranslate(debugCameraPos);
}

#ifdef USE_IMGUI
// 俯瞰デバッグカメラの切り替えボタン(ONにした瞬間だけ制御点全体にフィットさせる)
void DebugTopCamera::ShowToggleCheckbox(const RailEditor* railEditor){
	if(ImGui::Checkbox("Debug Top-Down View",&isEnabled_)){
		CameraManager::GetInstance()->SetActiveCamera(isEnabled_?kDebugTopCameraName:kDefaultCameraName);

		// ONにしたときだけ、制御点全体を囲むように自動フィット。以降は手動で自由に調整できる
		if(isEnabled_){
			FitToRail(railEditor);
		}
	}
}
#endif

// 制御点全体を囲むように、カメラの位置と向きを合わせる
void DebugTopCamera::FitToRail(const RailEditor* railEditor){
	if(!railEditor){
		return;
	}

	Vector3 center = railEditor->GetControlPointsCenter();
	float radius = railEditor->GetControlPointsRadius();
	float height = radius * kMarginFactor + kMinHeight;

	if(Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera(kDebugTopCameraName)){
		debugTopCamera->SetTranslate({center.x, height, center.z});
		debugTopCamera->SetRotate({kLookDownRotateX, 0.0f, 0.0f});
	}
}
