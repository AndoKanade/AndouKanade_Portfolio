#include "TargetEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include "WinAPI.h"
#include "externals/json.hpp"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <string>

namespace{
	// 的の配置データの保存先ファイルパス(RailEditorと同じくresource配下の相対パス)
	const std::string kTargetSaveFilePath = "resource/target/target.json";

	// レイと平面が平行かどうかを判定するしきい値(内積の絶対値がこの値未満なら平行とみなす)
	constexpr float kRayPlaneEpsilon = 1e-6f;

	// 座標に行列を掛け、同次座標のwで割る(NDCからワールド座標への逆変換に使用)
	Vector3 TransformWithDivide(const Vector3& v,const Matrix4x4& m){
		float x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + m.m[3][0];
		float y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + m.m[3][1];
		float z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + m.m[3][2];
		float w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + m.m[3][3];

		// wが0に近い異常値のときは割らずにそのまま返す
		if(std::fabs(w) < kRayPlaneEpsilon){
			return {x, y, z};
		}
		return {x / w, y / w, z / w};
	}

	// レイと平面の交点を求める(平行、または平面がレイの後方にある場合はfalse)
	bool IntersectRayPlane(const Vector3& origin,const Vector3& direction,const Vector3& planePoint,const Vector3& planeNormal,Vector3& outPos){
		float denom = Dot(direction,planeNormal);
		if(std::fabs(denom) < kRayPlaneEpsilon){
			return false;
		}

		float t = Dot(planePoint - origin,planeNormal) / denom;
		if(t < 0.0f){
			return false;
		}

		outPos = origin + direction * t;
		return true;
	}
}

TargetEditor::TargetEditor() = default;
TargetEditor::~TargetEditor() = default;

// 初期化処理(保存済みJSONがあれば読み込む)
void TargetEditor::Initialize(){
	LoadFromJson();
}

// 更新処理(ImGuiでの編集UIとマウスドラッグによる移動)
void TargetEditor::Update(){
#ifdef USE_IMGUI
	// Playモード中は配置編集を行わない(ドラッグ状態も解除しておく)
	if(EditorContext::GetInstance()->IsPlayMode()){
		isDragging_ = false;
		return;
	}

	// 選択中のインデックスが範囲外になっていたら選択解除する(削除・読み込み後など)
	if(selectedIndex_ >= static_cast<int>(targets_.size())){
		selectedIndex_ = -1;
	}

	// 固定タイルレイアウトの下段・中央左に配置(Enemy EditorやPostProcess Settingsと重ならないようにする)
	EditorWidgets::BeginFixedPanel("Target Editor",EditorWidgets::ComputeLayout().bottomCenterLeft);

	// 配置モードのON/OFF切り替え(ONの間だけSceneビュー上のドラッグ移動を受け付ける)
	ImGui::Checkbox("Placement Mode",&isPlacementMode_);
	ImGui::TextDisabled("(Sceneビューで的を左ドラッグすると移動できます)");

	ImGui::Separator();

	// 的の追加ボタン(カメラの前方に1個生成する)
	if(ImGui::Button("Add Target")){
		AddTargetInFrontOfCamera();
	}
	ImGui::SameLine();

	// 選択中の的の削除ボタン(未選択のときは押せないようにする)
	bool hasSelection = (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(targets_.size()));
	ImGui::BeginDisabled(!hasSelection);
	if(ImGui::Button("Delete Target")){
		targets_.erase(targets_.begin() + selectedIndex_);
		selectedIndex_ = -1;
		isDragging_ = false;
	}
	ImGui::EndDisabled();

	ImGui::Separator();

	// JSONへの保存/再読込ボタン
	if(ImGui::Button("Save")){
		SaveToJson();
	}
	ImGui::SameLine();
	if(ImGui::Button("Load")){
		LoadFromJson();
	}
	ImGui::SameLine();
	ImGui::Text("(%s)",kTargetSaveFilePath.c_str());

	ImGui::Separator();

	// 現在の配置状況の表示(選択・ドラッグ中かどうかの確認用)
	ImGui::Text("Targets: %d",static_cast<int>(targets_.size()));
	ImGui::Text("Selected: %d",selectedIndex_);
	ImGui::Text("Dragging: %s",isDragging_?"true":"false");

	ImGui::End();

	// パネルの外(Sceneビュー上)でのマウスドラッグによる移動処理
	UpdateMouseDrag();
#endif
}

// 現在のカメラの前方に的を1個追加する(Add Targetボタン用)
void TargetEditor::AddTargetInFrontOfCamera(){
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if(!camera){
		// カメラが無い場合は原点に置く
		AddTargetAt({0.0f, 0.0f, 0.0f});
		return;
	}

	// ワールド行列の3行目がカメラの前方ベクトルにあたる
	const Matrix4x4& world = camera->GetWorldMatrix();
	Vector3 forward = Normalize(Vector3{world.m[2][0], world.m[2][1], world.m[2][2]});

	AddTargetAt(camera->GetTranslate() + forward * kSpawnDistance);
}

// 指定座標に的を1個追加する(初回起動時の初期配置生成用)
void TargetEditor::AddTargetAt(const Vector3& position){
	TargetPoint point{};
	point.position = position;
	targets_.push_back(point);

	// 追加した的をそのまま選択状態にして、すぐ動かせるようにする
	selectedIndex_ = static_cast<int>(targets_.size()) - 1;
}

// Editモード中にゲーム画面が実際に描かれている矩形(レターボックス適用後)を求める
// Application.cppのビューポート計算と同じ手順で求めることで、マウス座標と描画結果を一致させる
bool TargetEditor::ComputeGameViewportRect(float& outX,float& outY,float& outW,float& outH) const{
	const EditorRect& sceneRect = EditorContext::GetInstance()->GetSceneViewRect();
	if(sceneRect.w <= 0.0f || sceneRect.h <= 0.0f){
		return false;
	}

	const float targetAspect = static_cast<float>(WinAPI::kClientWidth) / static_cast<float>(WinAPI::kClientHeight);
	const float rectAspect = sceneRect.w / sceneRect.h;

	float viewWidth = 0.0f;
	float viewHeight = 0.0f;
	if(rectAspect > targetAspect){
		// 領域が横長 → 高さを合わせて左右をレターボックス
		viewHeight = sceneRect.h;
		viewWidth = viewHeight * targetAspect;
	} else{
		// 領域が縦長 → 幅を合わせて上下をレターボックス
		viewWidth = sceneRect.w;
		viewHeight = viewWidth / targetAspect;
	}

	outX = sceneRect.x + (sceneRect.w - viewWidth) * 0.5f;
	outY = sceneRect.y + (sceneRect.h - viewHeight) * 0.5f;
	outW = viewWidth;
	outH = viewHeight;
	return true;
}

// 現在のマウス位置から伸びるレイを求める(画面外・カメラ未取得のときはfalse)
bool TargetEditor::ComputeMouseRay(PickRay& outRay) const{
#ifdef USE_IMGUI
	float viewX = 0.0f, viewY = 0.0f, viewW = 0.0f, viewH = 0.0f;
	if(!ComputeGameViewportRect(viewX,viewY,viewW,viewH)){
		return false;
	}

	// ゲーム画面の外にマウスがあるときは対象外
	ImVec2 mousePos = ImGui::GetMousePos();
	if(mousePos.x < viewX || mousePos.x > viewX + viewW || mousePos.y < viewY || mousePos.y > viewY + viewH){
		return false;
	}

	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if(!camera){
		return false;
	}

	// ビューポート内のマウス位置を正規化デバイス座標(NDC)へ変換する
	float ndcX = (mousePos.x - viewX) / viewW * 2.0f - 1.0f;
	float ndcY = 1.0f - (mousePos.y - viewY) / viewH * 2.0f;

	// ビュープロジェクション行列の逆行列で、NDCの near/far 2点をワールド座標へ戻す
	// DirectXの深度範囲は 0(ニアクリップ面)〜1(ファークリップ面)
	Matrix4x4 invViewProjection = Inverse(camera->GetViewProjectionMatrix());
	Vector3 nearPos = TransformWithDivide({ndcX, ndcY, 0.0f},invViewProjection);
	Vector3 farPos = TransformWithDivide({ndcX, ndcY, 1.0f},invViewProjection);

	outRay.origin = nearPos;
	outRay.direction = Normalize(farPos - nearPos);
	return true;
#else
	(void)outRay;
	return false;
#endif
}

// レイに最も近い的のインデックスを求める(掴める距離に無ければ-1)
int TargetEditor::PickTargetIndex(const PickRay& ray) const{
	int bestIndex = -1;
	float bestDistanceAlongRay = 0.0f;

	for(size_t i = 0; i < targets_.size(); ++i){
		Vector3 toTarget = targets_[i].position - ray.origin;

		// レイ方向に対する射影距離。負ならカメラの後方にあるので対象外
		float distanceAlongRay = Dot(toTarget,ray.direction);
		if(distanceAlongRay < 0.0f){
			continue;
		}

		// レイ上の最近傍点と的との距離が掴める範囲に収まっているかを判定する
		Vector3 nearestOnRay = ray.origin + ray.direction * distanceAlongRay;
		if(Distance(nearestOnRay,targets_[i].position) > kPickRadius){
			continue;
		}

		// 複数の的が重なって見えている場合は、手前にあるものを優先する
		if(bestIndex < 0 || distanceAlongRay < bestDistanceAlongRay){
			bestIndex = static_cast<int>(i);
			bestDistanceAlongRay = distanceAlongRay;
		}
	}

	return bestIndex;
}

// マウスドラッグによる的の移動処理
void TargetEditor::UpdateMouseDrag(){
#ifdef USE_IMGUI
	// 配置モードOFF、またはImGuiのウィジェットを操作中のときはドラッグ判定を行わない
	if(!isPlacementMode_ || ImGui::IsAnyItemActive()){
		isDragging_ = false;
		return;
	}

	// 左ボタンが離されていればドラッグ終了
	if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)){
		isDragging_ = false;
	}

	PickRay ray{};
	if(!ComputeMouseRay(ray)){
		return;
	}

	// 左ボタンを押した瞬間、レイ上に的があれば掴む
	if(!isDragging_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
		int hitIndex = PickTargetIndex(ray);
		if(hitIndex >= 0){
			selectedIndex_ = hitIndex;
			isDragging_ = true;

			// 掴んだ瞬間のレイ方向を法線とする平面上で動かす(画面に平行な移動になる)
			dragPlaneNormal_ = ray.direction;
			dragPlanePoint_ = targets_[hitIndex].position;

			// 掴んだ位置と的の座標とのずれを保持し、掴んだ瞬間に的が跳ばないようにする
			Vector3 hitPos{};
			if(IntersectRayPlane(ray.origin,ray.direction,dragPlanePoint_,dragPlaneNormal_,hitPos)){
				dragOffset_ = targets_[hitIndex].position - hitPos;
			} else{
				dragOffset_ = {0.0f, 0.0f, 0.0f};
			}
		}
	}

	// ドラッグ中は、掴んだときの平面とレイの交点へ的を移動させる
	if(isDragging_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(targets_.size())){
		Vector3 hitPos{};
		if(IntersectRayPlane(ray.origin,ray.direction,dragPlanePoint_,dragPlaneNormal_,hitPos)){
			targets_[selectedIndex_].position = hitPos + dragOffset_;
		}
	}
#endif
}

// 配置されている的の個数を取得
int TargetEditor::GetTargetCount() const{
	return static_cast<int>(targets_.size());
}

// 指定インデックスの的の座標を取得
Vector3 TargetEditor::GetTargetPosition(int index) const{
	if(index < 0 || index >= static_cast<int>(targets_.size())){
		return {0.0f, 0.0f, 0.0f};
	}
	return targets_[index].position;
}

// 指定インデックスの的の座標への参照を取得(Inspectorからの編集用)
Vector3& TargetEditor::GetTargetPositionRef(int index){
	return targets_[index].position;
}

// 選択中の的のインデックスを取得(-1は未選択)
int TargetEditor::GetSelectedIndex() const{
	return selectedIndex_;
}

// 選択中の的のインデックスを設定する(-1で選択解除)
void TargetEditor::SetSelectedIndex(int index){
	if(index < -1 || index >= static_cast<int>(targets_.size())){
		return; // 範囲外は無視
	}
	selectedIndex_ = index;
}

// 配置内容をJSONファイルに保存する
void TargetEditor::SaveToJson(){
	// ルート要素。targetsは読み込み時の簡易フォーマットチェックにも使う
	nlohmann::json root;

	nlohmann::json targetsJson = nlohmann::json::array();
	for(const auto& target : targets_){
		nlohmann::json tj;
		tj["position"] = {target.position.x, target.position.y, target.position.z};
		targetsJson.push_back(tj);
	}
	root["targets"] = targetsJson;

	// 保存先フォルダが無ければ作成しておく
	std::filesystem::path savePath(kTargetSaveFilePath);
	if(savePath.has_parent_path()){
		std::filesystem::create_directories(savePath.parent_path());
	}

	// ファイルへ書き出し(setwで人が読める整形出力にする)
	std::ofstream file(kTargetSaveFilePath);
	if(!file.is_open()){
		return;
	}
	file << std::setw(4) << root << std::endl;
}

// 配置内容をJSONファイルから読み込む(ファイルが無ければ何もしない)
void TargetEditor::LoadFromJson(){
	std::ifstream file(kTargetSaveFilePath);
	if(!file.is_open()){
		// ファイルがまだ無い(初回起動など)ときは現状維持
		return;
	}

	nlohmann::json root;
	file >> root;

	// 最低限のフォーマットチェック。想定外なら読み込みを中止して現状維持
	if(!root.is_object() || !root.contains("targets") || !root["targets"].is_array()){
		return;
	}

	// 読み込んだ内容で配置を置き換える
	targets_.clear();
	for(const auto& tj : root["targets"]){
		TargetPoint point{};
		point.position.x = tj["position"][0].get<float>();
		point.position.y = tj["position"][1].get<float>();
		point.position.z = tj["position"][2].get<float>();
		targets_.push_back(point);
	}

	// 読み込みで的の個数が変わるため、選択とドラッグ状態は解除する
	selectedIndex_ = -1;
	isDragging_ = false;
}
