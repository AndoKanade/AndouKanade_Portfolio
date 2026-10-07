#include "DebugTopCamera.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Obj3dCommon.h"
#include "Input.h"
#include "ImGuiManager.h"
#include "EditorContext.h"
#include "Editor/RailEditor.h"
#include <cmath>

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
	ApplyActiveCamera();

	// ONにした瞬間だけ、制御点全体を囲むように自動フィットさせる
	if(isEnabled_){
		FitToRail(railEditor);
	}
}

// 俯瞰デバッグカメラが有効な間は、ImGui無しでもWASD(平面移動)+QE(高さ)+右ドラッグ(向き)で自由に動かせるようにする
void DebugTopCamera::UpdateMove(Input* input){
	if(!isEnabled_ || !input){
		return;
	}

	Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera(kDebugTopCameraName);
	if(!debugTopCamera){
		return;
	}

	// 右ボタンをゲーム画面の上で押したときだけ回転を始め、離すまで回転を続ける
	// 編集パネルの上で押したときは、パネル側の操作を優先してカメラを回さない
	const bool isRotateButtonDown = input->PushMouseButton(kRotateMouseButton);
	if(isRotateButtonDown && !wasRotateButtonDown_){
		isRotating_ = IsMouseOnGameView();
	}
	if(!isRotateButtonDown){
		isRotating_ = false;
	}
	wasRotateButtonDown_ = isRotateButtonDown;

	if(isRotating_){
		yaw_ += input->GetMouseDeltaX() * kRotateSensitivity;
		pitch_ += input->GetMouseDeltaY() * kRotateSensitivity;

		// 真上・真下を越えて裏返らないよう、上下の向きを制限する
		if(pitch_ > kLookDownRotateX) pitch_ = kLookDownRotateX;
		if(pitch_ < -kLookDownRotateX) pitch_ = -kLookDownRotateX;

		debugTopCamera->SetRotate({pitch_, yaw_, 0.0f});
	}

	// 移動は向いている左右方向(Y軸回転)だけを基準にした水平移動にする
	// 真下を向いていても、Wで画面の上方向へ進めるようにするため
	const Vector3 forwardXZ = {std::sin(yaw_), 0.0f, std::cos(yaw_)};
	const Vector3 rightXZ = {std::cos(yaw_), 0.0f, -std::sin(yaw_)};

	const float moveDelta = kMoveSpeed * kDeltaTime;

	Vector3 debugCameraPos = debugTopCamera->GetTranslate();
	if(input->PushKey(DIK_W)) debugCameraPos += forwardXZ * moveDelta;
	if(input->PushKey(DIK_S)) debugCameraPos += -forwardXZ * moveDelta;
	if(input->PushKey(DIK_D)) debugCameraPos += rightXZ * moveDelta;
	if(input->PushKey(DIK_A)) debugCameraPos += -rightXZ * moveDelta;
	if(input->PushKey(DIK_E)) debugCameraPos.y += moveDelta;
	if(input->PushKey(DIK_Q)) debugCameraPos.y -= moveDelta;
	debugTopCamera->SetTranslate(debugCameraPos);
}

// 俯瞰デバッグカメラをOFFにして、プレイ用カメラに戻す
void DebugTopCamera::Disable(){
	if(!isEnabled_){
		return;
	}
	isEnabled_ = false;
	isRotating_ = false;
	ApplyActiveCamera();
}

// マウスカーソルがゲーム画面の上にあるか
bool DebugTopCamera::IsMouseOnGameView() const{
#ifdef USE_IMGUI
	// Editモードではゲーム画面の上に背景透過の「Scene」パネルが重なっているため、
	// ImGuiのウィンドウ上かどうかだけでは判定できない。Sceneパネルの領域内かどうかも見る
	const ImGuiIO& io = ImGui::GetIO();
	const EditorRect& sceneRect = EditorContext::GetInstance()->GetSceneViewRect();
	const bool isInsideSceneView =
		io.MousePos.x >= sceneRect.x && io.MousePos.x <= sceneRect.x + sceneRect.w &&
		io.MousePos.y >= sceneRect.y && io.MousePos.y <= sceneRect.y + sceneRect.h;

	// Sceneパネルの領域内か、どのImGuiウィンドウの上でもなければゲーム画面の上とみなす
	return isInsideSceneView || !io.WantCaptureMouse;
#else
	// ImGuiが無い環境では画面全体がゲーム画面
	return true;
#endif
}

#ifdef USE_IMGUI
// 俯瞰デバッグカメラの切り替えボタン(ONにした瞬間だけ制御点全体にフィットさせる)
void DebugTopCamera::ShowToggleCheckbox(const RailEditor* railEditor){
	if(ImGui::Checkbox("Debug Top-Down View",&isEnabled_)){
		ApplyActiveCamera();

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

	// 向きも真下に戻す(右ドラッグで変えた向きはリセットする)
	pitch_ = kLookDownRotateX;
	yaw_ = 0.0f;

	if(Camera* debugTopCamera = CameraManager::GetInstance()->GetCamera(kDebugTopCameraName)){
		debugTopCamera->SetTranslate({center.x, height, center.z});
		debugTopCamera->SetRotate({pitch_, yaw_, 0.0f});
	}
}

// ON/OFFの状態に合わせて、アクティブなカメラを切り替える
// F1キーとImGuiのチェックボックスのどちらで切り替えても同じ結果になるよう、ここにまとめる
void DebugTopCamera::ApplyActiveCamera(){
	const char* cameraName = isEnabled_?kDebugTopCameraName:kDefaultCameraName;
	CameraManager::GetInstance()->SetActiveCamera(cameraName);

	// SetCamera()を毎フレーム呼んでいないオブジェクトは
	// objCommon_のdefaultCamera_を参照し続けるため、こちらも切り替えないと追従しない
	Camera* switchedCamera = CameraManager::GetInstance()->GetCamera(cameraName);
	if(switchedCamera && objCommon_){
		objCommon_->SetDefaultCamera(switchedCamera);
	}
}
