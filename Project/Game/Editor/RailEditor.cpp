#include "RailEditor.h"
#include "ImGuiManager.h"
#include "imgui.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include "externals/json.hpp"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>

namespace{
	// 制御点データの保存先ディレクトリ・拡張子(LevelManagerと同じくresource配下の相対パス)
	const std::string kRailSaveDir = "resource/rail/";
	// 0番目のレールのみ、複数化対応前から存在するファイルパスと互換性を保つ
	const std::string kFirstRailSaveFilePath = "resource/rail/rail.json";
	// 制御点リセット用のデフォルト値(初期制御点/Add Control Pointボタンと同じ初期値)
	constexpr RailEditor::ControlPoint kDefaultControlPoint = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f};

	// レールごとの色分け用パレット(レール数がパレット数を超えたら循環して使う)
	constexpr Vector4 kRailColorPalette[] = {
		{1.0f, 1.0f, 1.0f, 1.0f}, // Rail 0: 白
		{0.3f, 0.6f, 1.0f, 1.0f}, // Rail 1: 水色
		{1.0f, 0.4f, 0.8f, 1.0f}, // Rail 2: ピンク
		{0.6f, 1.0f, 0.4f, 1.0f}, // Rail 3: 黄緑
	};
	constexpr int kRailColorPaletteCount = 4;

	// 分岐先として乗り移り可能になったレールを強調する色(目立つ黄色)
	constexpr Vector4 kHighlightColor = {1.0f, 0.9f, 0.1f, 1.0f};
}

RailEditor::RailEditor() = default;
RailEditor::~RailEditor() = default;

// 制御点描画用のObj3Dを1つ生成する(Initialize/追加ボタン/ロードで共通利用)
std::unique_ptr<Obj3D> RailEditor::CreatePointObject(){
	auto obj = std::make_unique<Obj3D>();
	obj->Initialize(objCommon_);
	obj->SetModel("Sphere/sphere.obj");
	return obj;
}

// rail.pointObjectsの個数をrail.controlPointsの個数にそろえる
void RailEditor::SyncPointObjectsToControlPoints(Rail& rail){
	// 足りなければ生成、多ければ末尾を削る
	while(rail.pointObjects.size() < rail.controlPoints.size()){
		rail.pointObjects.push_back(CreatePointObject());
	}
	while(rail.pointObjects.size() > rail.controlPoints.size()){
		rail.pointObjects.pop_back();
	}
}

// 新しいレールを1本追加し、アクティブなレールとして切り替える
void RailEditor::AddRail(){
	Rail rail;
	rail.name = "Rail " + std::to_string(rails_.size());
	rail.controlPoints.push_back(kDefaultControlPoint);
	rail.pointObjects.push_back(CreatePointObject());

	// レール曲線可視化用のサンプリング点オブジェクトを固定数だけ生成しておく
	rail.curveObjects.reserve(kCurveSampleCount);
	for(int i = 0; i < kCurveSampleCount; ++i){
		rail.curveObjects.push_back(CreatePointObject());
	}

	rails_.push_back(std::move(rail));
	activeRailIndex_ = static_cast<int>(rails_.size()) - 1;
}

// レールのインデックスから保存先ファイルパスを求める(0番目のみ既存の互換パスを使う)
std::string RailEditor::GetRailFilePath(int index) const{
	if(index <= 0){
		return kFirstRailSaveFilePath;
	}
	return kRailSaveDir + "rail" + std::to_string(index) + ".json";
}

// エディターの初期化処理
void RailEditor::Initialize(Obj3dCommon* objCommon){
	objCommon_ = objCommon;

	// 制御点描画用のモデルをロード
	ModelManager::GetInstance()->LoadModel("Sphere/sphere.obj");

	// 最初のレールを1本用意する
	AddRail();

	// 保存済みのJSONがあれば読み込んで初期状態を上書きする(ホットロードの起動時オート読込)
	LoadFromJson();
}

// エディターの更新処理
void RailEditor::Update(){
#ifdef USE_IMGUI
	// Playモード中はレール編集UIを出さない
	if(EditorContext::GetInstance()->IsPlayMode()){ return; }

	// 固定タイルレイアウトの左・下段に配置
	EditorWidgets::BeginFixedPanel("Rail Editor",EditorWidgets::ComputeLayout().railEditor);

	// レールの切り替え・新規作成UI
	ImGui::Text("Active Rail: %s",rails_[activeRailIndex_].name.c_str());
	for(size_t i = 0; i < rails_.size(); ++i){
		ImGui::PushID(static_cast<int>(i) + 10000); // 制御点側のPushIDと衝突しないようオフセット
		if(ImGui::Selectable(rails_[i].name.c_str(),static_cast<int>(i) == activeRailIndex_)){
			SwitchActiveRail(static_cast<int>(i));
		}
		ImGui::PopID();
	}
	if(ImGui::Button("New Rail")){
		AddRail();
	}

	ImGui::Separator();

	// 以降はアクティブなレールに対する編集操作
	Rail& activeRail = rails_[activeRailIndex_];

	// 制御点の追加ボタン
	if(ImGui::Button("Add Control Point")){
		// リストの最後に新しい制御点を追加
		activeRail.controlPoints.push_back(kDefaultControlPoint);

		// 追加した制御点用の3Dオブジェクトを生成
		activeRail.pointObjects.push_back(CreatePointObject());
	}

	ImGui::Separator();

	// JSONへの保存/読み込みボタン(ImGuiで編集した値をそのまま反映させる)
	if(ImGui::Button("Save")){
		SaveToJson();
	}
	ImGui::SameLine();
	if(ImGui::Button("Load")){
		LoadFromJson();
	}
	ImGui::SameLine();
	ImGui::Text("(%s)",GetRailFilePath(activeRailIndex_).c_str());

	ImGui::Separator();

	// 制御点モデルの表示ON/OFF切り替え(他のオブジェクトが見づらくなるとき用)
	ImGui::Checkbox("Show Control Point Models",&activeRail.showControlPointModels);
	// レール曲線(サンプリング点列)の表示ON/OFF切り替え
	ImGui::Checkbox("Show Rail Curve",&activeRail.showCurve);

	ImGui::Separator();

	// 選択中の制御点インデックスが範囲外になっていたら選択解除する(削除・レール切り替え後など)
	if(selectedPointIndex_ >= static_cast<int>(activeRail.controlPoints.size())){
		selectedPointIndex_ = -1;
	}

	// 削除ボタンが押された制御点のインデックス(未押下なら-1のまま)
	int deleteIndex = -1;

	// 制御点一覧(Hierarchy的な一覧表示)。クリックした行を選択状態にする
	ImGui::Text("Control Points: %zu",activeRail.controlPoints.size());
	for(size_t i = 0; i < activeRail.controlPoints.size(); ++i){
		// IDの重複を防ぐためPushIDを使用
		ImGui::PushID(static_cast<int>(i));

		// 分岐先が設定されている制御点には目印を付ける
		bool hasBranch = (activeRail.controlPoints[i].branchTargetRailIndex >= 0);
		char label[32];
		snprintf(label,sizeof(label),"Point %zu%s",i,hasBranch?" (Branch)":"");

		// クリックで選択。選択中の行はハイライトされる
		if(ImGui::Selectable(label,selectedPointIndex_ == static_cast<int>(i))){
			selectedPointIndex_ = static_cast<int>(i);
		}
		ImGui::PopID();
	}

	ImGui::Separator();

	// 選択中の制御点のみ編集UIを表示する(未選択時は何も編集できない)
	if(selectedPointIndex_ >= 0 && selectedPointIndex_ < static_cast<int>(activeRail.controlPoints.size())){
		ImGui::PushID(selectedPointIndex_);
		ControlPoint& selectedPoint = activeRail.controlPoints[selectedPointIndex_];
		ImGui::Text("Point %d",selectedPointIndex_);

		// 座標、回転、速度の編集UI
		ImGui::DragFloat3("Position",&selectedPoint.position.x,0.1f);
		ImGui::DragFloat3("Rotation",&selectedPoint.rotation.x,0.1f);
		ImGui::DragFloat("Speed",&selectedPoint.speed,0.1f);

		// レール間分岐移動用の設定欄(-1で分岐なし)
		ImGui::InputInt("Branch Target Rail",&selectedPoint.branchTargetRailIndex);
		ImGui::InputInt("Branch Target Point",&selectedPoint.branchTargetPointIndex);
		ImGui::TextDisabled("(-1で分岐なし。分岐先レールIndexと対応する制御点Indexを指定)");

		// 制御点のパラメータをデフォルト値に戻すボタン
		if(ImGui::Button("Reset")){
			selectedPoint = kDefaultControlPoint;
		}
		ImGui::SameLine();

		// 制御点の削除ボタン(最低1点は残す必要があるため、1点しかないときは押せないようにする)
		bool isOnlyPoint = (activeRail.controlPoints.size() <= 1);
		ImGui::BeginDisabled(isOnlyPoint);
		if(ImGui::Button("Delete")){
			deleteIndex = selectedPointIndex_;
		}
		ImGui::EndDisabled();

		ImGui::PopID();
	} else{
		ImGui::TextDisabled("Select a point above");
	}

	// 削除処理(一覧選択の状態も合わせて解除する)
	if(deleteIndex >= 0){
		activeRail.controlPoints.erase(activeRail.controlPoints.begin() + deleteIndex);
		// pointObjectsの個数もcontrolPointsに合わせる(以降Draw()でインデックスがずれないようにする)
		SyncPointObjectsToControlPoints(activeRail);
		selectedPointIndex_ = -1;
	}

	ImGui::End();
#endif
}

// デバッグ用の描画処理
void RailEditor::Draw(){
	// Playモード中もレール(制御点の球・曲線)を見えるようにする
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();

	// レール間分岐移動を目視確認できるよう、アクティブなレールだけでなく全レールを描画する
	// レールごとに色を分け、分岐先として強調指定されているレールは目立つ色で上書きする
	for(size_t railIndex = 0; railIndex < rails_.size(); ++railIndex){
		Rail& rail = rails_[railIndex];

		Vector4 railColor = kRailColorPalette[railIndex % kRailColorPaletteCount];
		if(static_cast<int>(railIndex) == highlightedRailIndex_){
			railColor = kHighlightColor;
		}

		// 表示OFFのときは制御点のモデルを描画しない
		if(rail.showControlPointModels){
			// 制御点ごとに専用の3Dオブジェクトで描画
			for(size_t i = 0; i < rail.controlPoints.size(); ++i){
				rail.pointObjects[i]->SetTranslate(rail.controlPoints[i].position);
				// 視認性向上のためスケールを縮小
				rail.pointObjects[i]->SetScale({0.5f, 0.5f, 0.5f});
				if(Model::Material* mat = rail.pointObjects[i]->GetMaterial()){
					mat->color = railColor;
				}

				// アクティブカメラに毎フレーム同期
				if(activeCamera){
					rail.pointObjects[i]->SetCamera(activeCamera);
				}

				rail.pointObjects[i]->Update();
				rail.pointObjects[i]->Draw();
			}
		}

		// レール曲線をサンプリング点列(小さい球)で可視化する簡易版
		// Catmull-Romは制御点4つ以上必要なので、それ未満のときは何も描画しない
		if(rail.showCurve && rail.controlPoints.size() >= 4){
			for(int i = 0; i < kCurveSampleCount; ++i){
				float t = static_cast<float>(i) / static_cast<float>(kCurveSampleCount - 1);
				Vector3 pos = ComputePositionOnRail(rail.controlPoints,t);

				rail.curveObjects[i]->SetTranslate(pos);
				// 制御点よりさらに小さくして、線のように見せる
				rail.curveObjects[i]->SetScale({0.04f, 0.04f, 0.04f});
				if(Model::Material* mat = rail.curveObjects[i]->GetMaterial()){
					mat->color = railColor;
				}

				if(activeCamera){
					rail.curveObjects[i]->SetCamera(activeCamera);
				}

				rail.curveObjects[i]->Update();
				rail.curveObjects[i]->Draw();
			}
		}
	}
}

// 進行度tと制御点配列から、レール上の座標を計算する(GetPositionOnRail・全レール描画で共用)
Vector3 RailEditor::ComputePositionOnRail(const std::vector<ControlPoint>& controlPoints,float t) const{
	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return {0.0f, 0.0f, 0.0f};
	}

	// tを区間数でスケールし、現在の区間(segment)を算出
	size_t numSegments = controlPoints.size() - 3;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	// 区間内でのローカルな進行度(0〜1)
	float localT = scaledT - static_cast<float>(segment);

	const Vector3& p0 = controlPoints[segment].position;
	const Vector3& p1 = controlPoints[segment + 1].position;
	const Vector3& p2 = controlPoints[segment + 2].position;
	const Vector3& p3 = controlPoints[segment + 3].position;

	return CatmullRom(p0,p1,p2,p3,localT);
}

// 進行度t(0〜1)からレール上の座標を取得(対象はアクティブなレール)
Vector3 RailEditor::GetPositionOnRail(float t) const{
	return ComputePositionOnRail(rails_[activeRailIndex_].controlPoints,t);
}

// ここから追加: レール座標によるオンレール判定用のAPI

// 指定ワールド座標に最も近いレール上の進行度tを計算する
// 手順: まずkNearestSearchSampleCount分割で粗くサンプリングして最も近い点を求め、
// その前後の区間だけをkNearestRefineIterationCount回の三分探索で絞り込み、精度を上げる
float RailEditor::ComputeNearestTOnRail(const std::vector<ControlPoint>& controlPoints,const Vector3& worldPos) const{
	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return 0.0f;
	}

	// 粗いサンプリングで最も近い点を探す
	float bestT = 0.0f;
	float bestDistSq = -1.0f;
	for(int i = 0; i <= kNearestSearchSampleCount; ++i){
		float t = static_cast<float>(i) / static_cast<float>(kNearestSearchSampleCount);
		Vector3 pos = ComputePositionOnRail(controlPoints,t);
		Vector3 diff = pos - worldPos;
		float distSq = Dot(diff,diff);
		if(bestDistSq < 0.0f || distSq < bestDistSq){
			bestDistSq = distSq;
			bestT = t;
		}
	}

	// 粗探索で見つけた点の前後の区間を三分探索でさらに絞り込む
	float rangeWidth = 1.0f / static_cast<float>(kNearestSearchSampleCount);
	float lo = bestT - rangeWidth;
	float hi = bestT + rangeWidth;
	if(lo < 0.0f) lo = 0.0f;
	if(hi > 1.0f) hi = 1.0f;

	for(int i = 0; i < kNearestRefineIterationCount; ++i){
		float m1 = lo + (hi - lo) / 3.0f;
		float m2 = hi - (hi - lo) / 3.0f;

		Vector3 diff1 = ComputePositionOnRail(controlPoints,m1) - worldPos;
		Vector3 diff2 = ComputePositionOnRail(controlPoints,m2) - worldPos;

		if(Dot(diff1,diff1) < Dot(diff2,diff2)){
			hi = m2;
		} else{
			lo = m1;
		}
	}

	return (lo + hi) * 0.5f;
}

// 指定ワールド座標に最も近いレール上の進行度t(0〜1)を求める(対象はアクティブなレール)
float RailEditor::FindNearestTOnRail(const Vector3& worldPos) const{
	return ComputeNearestTOnRail(rails_[activeRailIndex_].controlPoints,worldPos);
}

// 指定ワールド座標からレール上の最近傍点までの距離を求める(対象はアクティブなレール)
float RailEditor::GetDistanceToRail(const Vector3& worldPos) const{
	float nearestT = FindNearestTOnRail(worldPos);
	Vector3 nearestPos = GetPositionOnRail(nearestT);
	return Distance(nearestPos,worldPos);
}

// 全レールの中から、指定したワールド座標に最も近いレール・進行度・距離を求める(着地先レールの探索用)
RailEditor::NearestRailResult RailEditor::FindNearestRail(const Vector3& worldPos) const{
	NearestRailResult result;
	float bestDistance = -1.0f;

	for(size_t i = 0; i < rails_.size(); ++i){
		float t = ComputeNearestTOnRail(rails_[i].controlPoints,worldPos);
		Vector3 nearestPos = ComputePositionOnRail(rails_[i].controlPoints,t);
		float distance = Distance(nearestPos,worldPos);

		if(bestDistance < 0.0f || distance < bestDistance){
			bestDistance = distance;
			result.railIndex = static_cast<int>(i);
			result.t = t;
			result.distance = distance;
		}
	}

	return result;
}
// ここまで追加

// 進行度t(0〜1)からレール上の回転(オイラー角)を取得(対象はアクティブなレール)
Vector3 RailEditor::GetRotationOnRail(float t) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	Vector3 forward = GetForwardOnRail(t);

	// Y成分をasinの定義域[-1, 1]にクランプ
	float clampedY = forward.y;
	if(clampedY > 1.0f) clampedY = 1.0f;
	if(clampedY < -1.0f) clampedY = -1.0f;

	// 進行方向からピッチ(X軸回転)とヨー(Y軸回転)を逆算
	float pitch = -std::asin(clampedY);
	float yaw = std::atan2(forward.x,forward.z);

	// ロール(Z軸回転)は制御点間の値を補間して使用(バンク演出用)
	float roll = 0.0f;
	if(controlPoints.size() >= 2){
		size_t numSegments = controlPoints.size() - 1;
		float scaledT = t * static_cast<float>(numSegments);
		size_t segment = static_cast<size_t>(scaledT);

		if(segment >= numSegments){
			segment = numSegments - 1;
		}

		float localT = scaledT - static_cast<float>(segment);
		roll = Lerp(controlPoints[segment].rotation.z,controlPoints[segment + 1].rotation.z,localT);
	}

	return {pitch, yaw, roll};
}

// 進行度t(0〜1)における進行方向(接線ベクトル)を取得(対象はアクティブなレール)
Vector3 RailEditor::GetForwardOnRail(float t) const{
	const float kEpsilon = 0.001f;
	float t0 = t - kEpsilon;
	float t1 = t + kEpsilon;

	if(t0 < 0.0f) t0 = 0.0f;
	if(t1 > 1.0f) t1 = 1.0f;

	Vector3 p0 = GetPositionOnRail(t0);
	Vector3 p1 = GetPositionOnRail(t1);
	Vector3 diff = p1 - p0;

	float length = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

	// 差分がほぼ0の場合は正面向きを返す
	if(length < 1e-5f){
		return {0.0f, 0.0f, 1.0f};
	}

	return {diff.x / length, diff.y / length, diff.z / length};
}

// 進行度t(0〜1)からレール上の速度(各制御点のSpeed値を補間したもの)を取得(対象はアクティブなレール)
float RailEditor::GetSpeedOnRail(float t) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// 制御点が無い場合は既定速度を返す
	if(controlPoints.empty()){
		return 1.0f;
	}
	// 制御点が1つだけの場合は補間できないためその点のSpeed値をそのまま返す
	if(controlPoints.size() == 1){
		return controlPoints[0].speed;
	}

	// GetRotationOnRailのロール補間と同じ区間分割(全制御点をそのまま区間とする)でSpeed値を線形補間する
	size_t numSegments = controlPoints.size() - 1;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	float localT = scaledT - static_cast<float>(segment);
	return Lerp(controlPoints[segment].speed,controlPoints[segment + 1].speed,localT);
}

// 指定インデックスのレールをアクティブなレールとして切り替える(Playモードでのレール乗り換えにも使用)
void RailEditor::SwitchActiveRail(int index){
	if(index < 0 || index >= static_cast<int>(rails_.size())){
		return; // 範囲外は無視
	}
	activeRailIndex_ = index;

	// レールが変わったら、別レールの制御点を指したままにならないよう選択を解除する
	selectedPointIndex_ = -1;
}

// 現在アクティブなレールのインデックスを取得(デバッグ表示用)
int RailEditor::GetActiveRailIndex() const{
	return activeRailIndex_;
}

// 読み込まれている全レールの本数を取得(デバッグ表示用)
int RailEditor::GetRailCount() const{
	return static_cast<int>(rails_.size());
}

// 分岐先として強調表示したいレールのインデックスを指定する(-1で強調解除)
void RailEditor::SetHighlightedRailIndex(int index){
	highlightedRailIndex_ = index;
}

// アクティブなレールの制御点数を取得
int RailEditor::GetControlPointCount() const{
	return static_cast<int>(rails_[activeRailIndex_].controlPoints.size());
}

// 進行度tから、直近に通過した制御点のインデックスを取得(対象はアクティブなレール)
// GetPositionOnRailと同じ区間分割を用い、区間の始点(制御点)のインデックスを返す
int RailEditor::GetControlPointIndexFromT(float t) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return 0;
	}

	size_t numSegments = controlPoints.size() - 3;
	float scaledT = t * static_cast<float>(numSegments);
	size_t segment = static_cast<size_t>(scaledT);

	if(segment >= numSegments){
		segment = numSegments - 1;
	}

	// 区間の始点(GetPositionOnRailで言うp1)のインデックスが「直近に通過した制御点」
	return static_cast<int>(segment) + 1;
}

// 指定した制御点インデックスにちょうど乗る進行度tを取得(対象はアクティブなレール)
// GetControlPointIndexFromTの逆算(pointIndex = segment + 1 の関係を利用)
float RailEditor::GetTFromControlPointIndex(int pointIndex) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	// Catmull-Rom補間には最低4点必要
	if(controlPoints.size() < 4){
		return 0.0f;
	}

	int numSegments = static_cast<int>(controlPoints.size()) - 3;
	int segment = pointIndex - 1;

	if(segment < 0) segment = 0;
	if(segment >= numSegments) segment = numSegments - 1;

	return static_cast<float>(segment) / static_cast<float>(numSegments);
}

// アクティブなレールの指定インデックスの制御点に設定された分岐先情報を取得
RailEditor::BranchInfo RailEditor::GetBranchAt(int pointIndex) const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	if(pointIndex < 0 || pointIndex >= static_cast<int>(controlPoints.size())){
		return BranchInfo{};
	}

	BranchInfo info;
	info.targetRailIndex = controlPoints[pointIndex].branchTargetRailIndex;
	info.targetPointIndex = controlPoints[pointIndex].branchTargetPointIndex;
	return info;
}

// 指定したレール・制御点インデックスの座標を取得(乗り移り方向の判定用。アクティブレール以外も参照可能)
Vector3 RailEditor::GetControlPointPosition(int railIndex,int pointIndex) const{
	if(railIndex < 0 || railIndex >= static_cast<int>(rails_.size())){
		return {0.0f, 0.0f, 0.0f};
	}

	const std::vector<ControlPoint>& controlPoints = rails_[railIndex].controlPoints;
	if(pointIndex < 0 || pointIndex >= static_cast<int>(controlPoints.size())){
		return {0.0f, 0.0f, 0.0f};
	}

	return controlPoints[pointIndex].position;
}

// アクティブなレールの全制御点を囲む範囲の中心座標を取得
Vector3 RailEditor::GetControlPointsCenter() const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	if(controlPoints.empty()){
		return {0.0f, 0.0f, 0.0f};
	}

	Vector3 minPos = controlPoints[0].position;
	Vector3 maxPos = controlPoints[0].position;

	for(const auto& point : controlPoints){
		minPos.x = (point.position.x < minPos.x)?point.position.x:minPos.x;
		minPos.y = (point.position.y < minPos.y)?point.position.y:minPos.y;
		minPos.z = (point.position.z < minPos.z)?point.position.z:minPos.z;

		maxPos.x = (point.position.x > maxPos.x)?point.position.x:maxPos.x;
		maxPos.y = (point.position.y > maxPos.y)?point.position.y:maxPos.y;
		maxPos.z = (point.position.z > maxPos.z)?point.position.z:maxPos.z;
	}

	return {
		(minPos.x + maxPos.x) * 0.5f,
		(minPos.y + maxPos.y) * 0.5f,
		(minPos.z + maxPos.z) * 0.5f
	};
}

// アクティブなレールの全制御点を囲む範囲のおおよその半径を取得
float RailEditor::GetControlPointsRadius() const{
	const std::vector<ControlPoint>& controlPoints = rails_[activeRailIndex_].controlPoints;

	if(controlPoints.empty()){
		return 0.0f;
	}

	Vector3 center = GetControlPointsCenter();
	float maxDistSq = 0.0f;

	for(const auto& point : controlPoints){
		float dx = point.position.x - center.x;
		float dz = point.position.z - center.z;
		float distSq = dx * dx + dz * dz; // XZ平面での距離の2乗

		if(distSq > maxDistSq){
			maxDistSq = distSq;
		}
	}

	return std::sqrt(maxDistSq);
}

// ImGui非表示時でもキー操作で表示切り替えを行うための関数(対象はアクティブなレール)
void RailEditor::ToggleShowControlPointModels(){
	rails_[activeRailIndex_].showControlPointModels = !rails_[activeRailIndex_].showControlPointModels;
}
void RailEditor::ToggleShowCurve(){
	rails_[activeRailIndex_].showCurve = !rails_[activeRailIndex_].showCurve;
}

// 指定インデックスのレール1本だけをJSONファイルに保存する
void RailEditor::SaveRailToFile(int index){
	Rail& rail = rails_[index];

	// ルート要素。nameは読み込み時の簡易フォーマットチェック・レール名の保存用
	nlohmann::json root;
	root["name"] = rail.name;

	// 各制御点を配列に詰める
	nlohmann::json pointsJson = nlohmann::json::array();
	for(const auto& cp : rail.controlPoints){
		nlohmann::json pj;
		pj["position"] = {cp.position.x, cp.position.y, cp.position.z};
		pj["rotation"] = {cp.rotation.x, cp.rotation.y, cp.rotation.z};
		pj["speed"] = cp.speed;
		// レール間分岐移動用のデータ
		pj["branchTargetRailIndex"] = cp.branchTargetRailIndex;
		pj["branchTargetPointIndex"] = cp.branchTargetPointIndex;
		pointsJson.push_back(pj);
	}
	root["control_points"] = pointsJson;

	std::string savePathStr = GetRailFilePath(index);

	// 保存先フォルダが無ければ作成しておく
	std::filesystem::path savePath(savePathStr);
	if(savePath.has_parent_path()){
		std::filesystem::create_directories(savePath.parent_path());
	}

	// ファイルへ書き出し(setwで人が読める整形出力にする)
	std::ofstream file(savePathStr);
	if(!file.is_open()){
		return;
	}
	file << std::setw(4) << root << std::endl;
}

// 指定インデックスのレール1本だけをJSONファイルから読み込む(ファイルが無ければ何もしない)
void RailEditor::LoadRailFromFile(int index){
	Rail& rail = rails_[index];

	std::ifstream file(GetRailFilePath(index));
	if(!file.is_open()){
		// ファイルがまだ無い(初回起動・新規追加直後など)ときは現状維持
		return;
	}

	nlohmann::json root;
	file >> root;

	// 最低限のフォーマットチェック。想定外なら読み込みを中止して現状維持
	if(!root.is_object() || !root.contains("control_points") || !root["control_points"].is_array()){
		return;
	}

	// レール名が保存されていれば復元する
	if(root.contains("name") && root["name"].is_string()){
		rail.name = root["name"].get<std::string>();
	}

	// 読み込んだ内容で制御点を置き換える
	rail.controlPoints.clear();
	for(const auto& pj : root["control_points"]){
		ControlPoint cp{};
		cp.position.x = pj["position"][0].get<float>();
		cp.position.y = pj["position"][1].get<float>();
		cp.position.z = pj["position"][2].get<float>();
		cp.rotation.x = pj["rotation"][0].get<float>();
		cp.rotation.y = pj["rotation"][1].get<float>();
		cp.rotation.z = pj["rotation"][2].get<float>();
		cp.speed = pj["speed"].get<float>();
		// レール間分岐移動用のデータ(旧フォーマットのJSONには無いため、value()で既定値-1を補う)
		cp.branchTargetRailIndex = pj.value("branchTargetRailIndex",-1);
		cp.branchTargetPointIndex = pj.value("branchTargetPointIndex",-1);
		rail.controlPoints.push_back(cp);
	}

	// 制御点が0個だと他の処理(補間など)が破綻するので最低1点は保証する
	if(rail.controlPoints.empty()){
		rail.controlPoints.push_back(kDefaultControlPoint);
	}

	// 描画用オブジェクトの数を制御点数に合わせる
	SyncPointObjectsToControlPoints(rail);
}

// 全レールをそれぞれ対応するJSONファイルへ保存する
void RailEditor::SaveToJson(){
	for(size_t i = 0; i < rails_.size(); ++i){
		SaveRailToFile(static_cast<int>(i));
	}
}

// 全レールをJSONファイルから読み込む
// メモリ上に既にあるレールは対応ファイルで上書きし、ディスクにのみ存在するレール(rail1.json, rail2.json...)は
// New Rail相当の追加をしながら連番で読み込んでいく(ファイルが途切れたところで打ち切り)
void RailEditor::LoadFromJson(){
	// 既存のレールを、それぞれ対応するファイルがあれば読み込み直す
	for(size_t i = 0; i < rails_.size(); ++i){
		LoadRailFromFile(static_cast<int>(i));
	}

	// メモリ上にまだ無いレールも、ファイルが存在する限り新規追加しながら連番で読み込む
	int index = static_cast<int>(rails_.size());
	while(std::filesystem::exists(GetRailFilePath(index))){
		AddRail();
		LoadRailFromFile(index);
		++index;
	}

	// 読み込み後は先頭のレールをアクティブな編集対象にする
	activeRailIndex_ = 0;
}