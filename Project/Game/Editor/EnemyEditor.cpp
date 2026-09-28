#include "EnemyEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include "WinAPI.h"
#include "externals/json.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <string>

namespace{
	// 敵の配置データの保存先ファイルパス(TargetEditorと同じくresource配下の相対パス)
	const std::string kEnemySaveFilePath = "resource/enemy/enemy.json";

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

EnemyEditor::EnemyEditor() = default;
EnemyEditor::~EnemyEditor() = default;

// 初期化処理(保存済みJSONがあれば読み込む)
void EnemyEditor::Initialize(){
	LoadFromJson();
}

// 更新処理(ImGuiでの編集UIとマウスドラッグによる移動)
void EnemyEditor::Update(){
#ifdef USE_IMGUI
	// Playモード中は配置編集を行わない(ドラッグ状態も解除しておく)
	if(EditorContext::GetInstance()->IsPlayMode()){
		isDragging_ = false;
		return;
	}

	// 選択中のインデックスが範囲外になっていたら選択解除する(削除・読み込み後など)
	if(selectedIndex_ >= static_cast<int>(enemies_.size())){
		selectedIndex_ = -1;
	}

	// 固定タイルレイアウトの下段・中央右に配置(Target Editorの隣に並ぶ)
	EditorWidgets::BeginFixedPanel("Enemy Editor",EditorWidgets::ComputeLayout().bottomCenterRight);

	// 配置モードのON/OFF切り替え(ONの間だけSceneビュー上のドラッグ移動を受け付ける)
	ImGui::Checkbox("Placement Mode",&isPlacementMode_);
	ImGui::TextDisabled("(Sceneビューで敵を左ドラッグすると移動できます)");

	ImGui::Separator();

	// 敵の追加ボタン(カメラの前方に1体生成する)
	if(ImGui::Button("Add Enemy")){
		AddEnemyInFrontOfCamera();
	}
	ImGui::SameLine();

	// 選択中の敵の削除ボタン(未選択のときは押せないようにする)
	bool hasSelection = (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(enemies_.size()));
	ImGui::BeginDisabled(!hasSelection);
	if(ImGui::Button("Delete Enemy")){
		enemies_.erase(enemies_.begin() + selectedIndex_);
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
	ImGui::Text("(%s)",kEnemySaveFilePath.c_str());

	ImGui::Separator();

	// 現在の配置状況の表示(選択・ドラッグ中かどうかの確認用)
	ImGui::Text("Enemies: %d",static_cast<int>(enemies_.size()));
	ImGui::Text("Selected: %d",selectedIndex_);
	ImGui::Text("Dragging: %s",isDragging_?"true":"false");

	ImGui::Separator();

	// 選択中の敵ごとのパラメータ編集
	DrawSelectedEnemyUI();

	ImGui::Separator();

	// 配置されている敵の一覧(クリックで選択できるようにする)
	for(int i = 0; i < static_cast<int>(enemies_.size()); ++i){
		ImGui::PushID(i);
		char label[64];
		snprintf(label,sizeof(label),"Enemy %d (HP %d)",i,enemies_[i].maxHp);
		if(ImGui::Selectable(label,selectedIndex_ == i)){
			selectedIndex_ = i;
		}
		ImGui::PopID();
	}

	ImGui::End();

	// パネルの外(Sceneビュー上)でのマウスドラッグによる移動処理
	UpdateMouseDrag();
#endif
}

// 選択中の敵のパラメータ(座標・往復方向・体力)を編集するUIを表示する
void EnemyEditor::DrawSelectedEnemyUI(){
#ifdef USE_IMGUI
	// 未選択のときは編集対象が無いことを表示するだけにする
	if(selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(enemies_.size())){
		ImGui::TextDisabled("Select an enemy");
		return;
	}

	EnemyPoint& enemy = enemies_[selectedIndex_];

	ImGui::Text("Enemy %d",selectedIndex_);

	// 往復移動の中心となるワールド座標
	EditorWidgets::ButtonVector3("Position",enemy.position,0.1f,1.0f);

	// 往復移動の方向(0ベクトルになると向きが定まらないため既定値へ戻す)
	if(EditorWidgets::ButtonVector3("Patrol Dir",enemy.patrolDirection,0.1f,1.0f)){
		if(Length(enemy.patrolDirection) < kMinPatrolDirectionLength){
			enemy.patrolDirection = kDefaultPatrolDirection;
		}
	}

	// 体力の最大値(下限を下回らないように制限する)
	if(EditorWidgets::ButtonInt("Max HP",enemy.maxHp,1,5)){
		if(enemy.maxHp < kMinMaxHp){
			enemy.maxHp = kMinMaxHp;
		}
	}
#endif
}

// 現在のカメラの前方に敵を1体追加する(Add Enemyボタン用)
void EnemyEditor::AddEnemyInFrontOfCamera(){
	Camera* camera = CameraManager::GetInstance()->GetActiveCamera();
	if(!camera){
		// カメラが無い場合は原点に置く
		AddEnemyAt({0.0f, 0.0f, 0.0f},kDefaultPatrolDirection,kDefaultMaxHp);
		return;
	}

	// ワールド行列の3行目がカメラの前方ベクトルにあたる
	const Matrix4x4& world = camera->GetWorldMatrix();
	Vector3 forward = Normalize(Vector3{world.m[2][0], world.m[2][1], world.m[2][2]});

	AddEnemyAt(camera->GetTranslate() + forward * kSpawnDistance,kDefaultPatrolDirection,kDefaultMaxHp);
}

// 指定の内容で敵を1体追加する(初回起動時の初期配置生成用)
void EnemyEditor::AddEnemyAt(const Vector3& position,const Vector3& patrolDirection,int maxHp){
	EnemyPoint point{};
	point.position = position;

	// 往復方向が0ベクトルだと動きが定まらないため既定値に置き換える
	point.patrolDirection = (Length(patrolDirection) < kMinPatrolDirectionLength)?kDefaultPatrolDirection:patrolDirection;
	point.maxHp = (maxHp < kMinMaxHp)?kMinMaxHp:maxHp;

	enemies_.push_back(point);

	// 追加した敵をそのまま選択状態にして、すぐ動かせるようにする
	selectedIndex_ = static_cast<int>(enemies_.size()) - 1;
}

// Editモード中にゲーム画面が実際に描かれている矩形(レターボックス適用後)を求める
// Application.cppのビューポート計算と同じ手順で求めることで、マウス座標と描画結果を一致させる
bool EnemyEditor::ComputeGameViewportRect(float& outX,float& outY,float& outW,float& outH) const{
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
bool EnemyEditor::ComputeMouseRay(PickRay& outRay) const{
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

// レイに最も近い敵のインデックスを求める(掴める距離に無ければ-1)
int EnemyEditor::PickEnemyIndex(const PickRay& ray) const{
	int bestIndex = -1;
	float bestDistanceAlongRay = 0.0f;

	for(size_t i = 0; i < enemies_.size(); ++i){
		Vector3 toEnemy = enemies_[i].position - ray.origin;

		// レイ方向に対する射影距離。負ならカメラの後方にあるので対象外
		float distanceAlongRay = Dot(toEnemy,ray.direction);
		if(distanceAlongRay < 0.0f){
			continue;
		}

		// レイ上の最近傍点と敵との距離が掴める範囲に収まっているかを判定する
		Vector3 nearestOnRay = ray.origin + ray.direction * distanceAlongRay;
		if(Distance(nearestOnRay,enemies_[i].position) > kPickRadius){
			continue;
		}

		// 複数の敵が重なって見えている場合は、手前にあるものを優先する
		if(bestIndex < 0 || distanceAlongRay < bestDistanceAlongRay){
			bestIndex = static_cast<int>(i);
			bestDistanceAlongRay = distanceAlongRay;
		}
	}

	return bestIndex;
}

// マウスドラッグによる敵の移動処理
void EnemyEditor::UpdateMouseDrag(){
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

	// 左ボタンを押した瞬間、レイ上に敵があれば掴む
	if(!isDragging_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
		int hitIndex = PickEnemyIndex(ray);
		if(hitIndex >= 0){
			selectedIndex_ = hitIndex;
			isDragging_ = true;

			// 掴んだ瞬間のレイ方向を法線とする平面上で動かす(画面に平行な移動になる)
			dragPlaneNormal_ = ray.direction;
			dragPlanePoint_ = enemies_[hitIndex].position;

			// 掴んだ位置と敵の座標とのずれを保持し、掴んだ瞬間に敵が跳ばないようにする
			Vector3 hitPos{};
			if(IntersectRayPlane(ray.origin,ray.direction,dragPlanePoint_,dragPlaneNormal_,hitPos)){
				dragOffset_ = enemies_[hitIndex].position - hitPos;
			} else{
				dragOffset_ = {0.0f, 0.0f, 0.0f};
			}
		}
	}

	// ドラッグ中は、掴んだときの平面とレイの交点へ敵を移動させる
	if(isDragging_ && selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(enemies_.size())){
		Vector3 hitPos{};
		if(IntersectRayPlane(ray.origin,ray.direction,dragPlanePoint_,dragPlaneNormal_,hitPos)){
			enemies_[selectedIndex_].position = hitPos + dragOffset_;
		}
	}
#endif
}

// 配置されている敵の体数を取得
int EnemyEditor::GetEnemyCount() const{
	return static_cast<int>(enemies_.size());
}

// 指定インデックスの敵の配置データを取得(範囲外のときは既定値を返す)
EnemyEditor::EnemyPoint EnemyEditor::GetEnemyPoint(int index) const{
	if(index < 0 || index >= static_cast<int>(enemies_.size())){
		EnemyPoint defaultPoint{};
		defaultPoint.position = {0.0f, 0.0f, 0.0f};
		defaultPoint.patrolDirection = kDefaultPatrolDirection;
		defaultPoint.maxHp = kDefaultMaxHp;
		return defaultPoint;
	}
	return enemies_[index];
}

// 選択中の敵のインデックスを取得(-1は未選択)
int EnemyEditor::GetSelectedIndex() const{
	return selectedIndex_;
}

// 選択中の敵のインデックスを設定する(-1で選択解除)
void EnemyEditor::SetSelectedIndex(int index){
	if(index < -1 || index >= static_cast<int>(enemies_.size())){
		return; // 範囲外は無視
	}
	selectedIndex_ = index;
}

// 配置内容をJSONファイルに保存する
void EnemyEditor::SaveToJson(){
	// ルート要素。enemiesは読み込み時の簡易フォーマットチェックにも使う
	nlohmann::json root;

	nlohmann::json enemiesJson = nlohmann::json::array();
	for(const auto& enemy : enemies_){
		nlohmann::json ej;
		ej["position"] = {enemy.position.x, enemy.position.y, enemy.position.z};
		ej["patrolDirection"] = {enemy.patrolDirection.x, enemy.patrolDirection.y, enemy.patrolDirection.z};
		ej["maxHp"] = enemy.maxHp;
		enemiesJson.push_back(ej);
	}
	root["enemies"] = enemiesJson;

	// 保存先フォルダが無ければ作成しておく
	std::filesystem::path savePath(kEnemySaveFilePath);
	if(savePath.has_parent_path()){
		std::filesystem::create_directories(savePath.parent_path());
	}

	// ファイルへ書き出し(setwで人が読める整形出力にする)
	std::ofstream file(kEnemySaveFilePath);
	if(!file.is_open()){
		return;
	}
	file << std::setw(4) << root << std::endl;
}

// 配置内容をJSONファイルから読み込む(ファイルが無ければ何もしない)
void EnemyEditor::LoadFromJson(){
	std::ifstream file(kEnemySaveFilePath);
	if(!file.is_open()){
		// ファイルがまだ無い(初回起動など)ときは現状維持
		return;
	}

	nlohmann::json root;
	file >> root;

	// 最低限のフォーマットチェック。想定外なら読み込みを中止して現状維持
	if(!root.is_object() || !root.contains("enemies") || !root["enemies"].is_array()){
		return;
	}

	// 読み込んだ内容で配置を置き換える
	enemies_.clear();
	for(const auto& ej : root["enemies"]){
		EnemyPoint point{};
		point.position.x = ej["position"][0].get<float>();
		point.position.y = ej["position"][1].get<float>();
		point.position.z = ej["position"][2].get<float>();

		// 往復方向と体力は、古い保存データに項目が無くても読めるよう既定値で補う
		if(ej.contains("patrolDirection") && ej["patrolDirection"].is_array()){
			point.patrolDirection.x = ej["patrolDirection"][0].get<float>();
			point.patrolDirection.y = ej["patrolDirection"][1].get<float>();
			point.patrolDirection.z = ej["patrolDirection"][2].get<float>();
		} else{
			point.patrolDirection = kDefaultPatrolDirection;
		}
		if(Length(point.patrolDirection) < kMinPatrolDirectionLength){
			point.patrolDirection = kDefaultPatrolDirection;
		}

		point.maxHp = ej.contains("maxHp")?ej["maxHp"].get<int>():kDefaultMaxHp;
		if(point.maxHp < kMinMaxHp){
			point.maxHp = kMinMaxHp;
		}

		enemies_.push_back(point);
	}

	// 読み込みで敵の体数が変わるため、選択とドラッグ状態は解除する
	selectedIndex_ = -1;
	isDragging_ = false;
}
