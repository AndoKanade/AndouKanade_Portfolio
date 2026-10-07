#include "TargetManager.h"
#include "PlayerBulletManager.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "Model.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "ParticleManager.h"
#include "GlobalVariables.h"
#include "ImGuiManager.h"
#include "EditorWidgets.h"
#include "Editor/RailEditor.h"
#include "Editor/TargetEditor.h"
#include <cstdio>

namespace{
	// 的の表示に使用するモデル
	const std::string kTargetModelPath = "Sphere/sphere.obj";
}

TargetManager::TargetManager() = default;
TargetManager::~TargetManager() = default;

// 初期化処理
void TargetManager::Initialize(Obj3dCommon* objCommon,const RailEditor* railEditor,const std::string& paramGroup){
	objCommon_ = objCommon;
	paramGroup_ = paramGroup;

	// 調整項目の登録(第3引数はデフォルト値。保存済みJSONがあればそちらが優先される)
	GlobalVariables::GetInstance()->AddItem(paramGroup_,"aimHitAngle",kDefaultAimHitAngle); // ヒット判定の許容角度

	ModelManager::GetInstance()->LoadModel(kTargetModelPath);
	model_ = ModelManager::GetInstance()->FindModel(kTargetModelPath);

	// 的の配置エディターの生成と初期化(保存済みJSONがあればここで読み込まれる)
	editor_ = std::make_unique<TargetEditor>();
	editor_->Initialize();

	// 保存済みの配置が無い初回起動時のみ、従来どおりレール沿いの自動配置で初期データを作る
	if(railEditor && editor_->GetTargetCount() == 0){
		CreateDefaultPlacement(railEditor);
	}

	// 配置エディターの内容をシーンの的リストへ反映する
	SyncFromEditor();
}

// 保存済みの配置が無いときの自動配置
// 的をレール沿いの複数の進行度(t)に、左右・上下・奥行き(進行方向)へオフセットして配置(ゲームらしく散らばらせる)
void TargetManager::CreateDefaultPlacement(const RailEditor* railEditor){
	constexpr size_t kTargetCount = 16;                  // 的の個数
	constexpr float kTargetRailTMin = 0.08f;             // 配置開始位置(レール進行度)
	constexpr float kTargetRailTMax = 0.92f;             // 配置終了位置(レール進行度)
	constexpr float kTargetSideOffsetAmount = 3.5f;      // 左右オフセットの振れ幅(交互に左右へ配置)
	constexpr float kTargetUpOffsetMin = 1.0f;           // 上下オフセットの最小値
	constexpr float kTargetUpOffsetMax = 3.5f;           // 上下オフセットの最大値
	constexpr float kTargetForwardOffsetMin = 2.0f;      // 奥行き(進行方向)オフセットの最小値
	constexpr float kTargetForwardOffsetMax = 8.0f;      // 奥行き(進行方向)オフセットの最大値
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};

	for(size_t i = 0; i < kTargetCount; ++i){
		// 0〜1の範囲で的の配置順を正規化し、各オフセットの補間に使う
		float ratio = static_cast<float>(i) / static_cast<float>(kTargetCount - 1);
		float t = kTargetRailTMin + (kTargetRailTMax - kTargetRailTMin) * ratio;

		// レール上の基準位置と進行方向から、進行方向に対して直角な「右」方向を求める
		Vector3 basePos = railEditor->GetPositionOnRail(t);
		Vector3 forward = railEditor->GetForwardOnRail(t);
		Vector3 right = Normalize(Cross(kWorldUp,forward));

		// 左右は交互に振り分け、上下・奥行きは配置順に応じて緩やかに変化させる
		float side = (i % 2 == 0)?-kTargetSideOffsetAmount:kTargetSideOffsetAmount;
		float up = kTargetUpOffsetMin + (kTargetUpOffsetMax - kTargetUpOffsetMin) * ratio;
		float forwardOffset = kTargetForwardOffsetMin + (kTargetForwardOffsetMax - kTargetForwardOffsetMin) * ratio;

		Vector3 pos = basePos + right * side + kWorldUp * up + forward * forwardOffset;

		// 座標は配置エディター側が保持する(描画用オブジェクトはSyncFromEditorで生成される)
		editor_->AddTargetAt(pos);
	}

	// 自動配置直後は何も選択していない状態にしておく
	editor_->SetSelectedIndex(-1);
}

// 配置エディターの更新(配置モード中のドラッグ移動もここで処理される)
void TargetManager::UpdateEditor(){
	if(!editor_){
		return;
	}

	editor_->Update();
	// 編集結果(追加・削除・ドラッグ移動)をシーンの的リストへ反映する
	SyncFromEditor();
}

// 配置エディターの内容を的のリストに反映する
// 個数が変わったときだけ描画用オブジェクトを生成・削除し、毎フレームの生成を避ける
void TargetManager::SyncFromEditor(){
	if(!editor_){
		return;
	}

	size_t editorCount = static_cast<size_t>(editor_->GetTargetCount());

	// 足りない分の的を生成する(生成時のみモデルの初期化を行う)
	while(targets_.size() < editorCount){
		Target target;
		target.obj = std::make_unique<Obj3D>();
		target.obj->Initialize(objCommon_);
		target.obj->SetModel(kTargetModelPath);
		target.isAlive = true;
		targets_.push_back(std::move(target));
	}

	// 多すぎる分の的を末尾から削除する
	while(targets_.size() > editorCount){
		targets_.pop_back();
	}

	// 座標をエディターの配置内容で上書きする
	for(size_t i = 0; i < targets_.size(); ++i){
		targets_[i].position = editor_->GetTargetPosition(static_cast<int>(i));
	}
}

// 的の当たり判定(画面中央固定のレティクル方式)
// レティクルは常に画面中央=カメラの前方ベクトル方向なので、
// 「カメラ→的」の方向とカメラ前方ベクトルのなす角が閾値以内なら狙えている(表示上のフィードバック用)
// 実際の命中判定は、発射した弾と的のモデルから求めた境界球が重なったかどうかで行う
bool TargetManager::Update(const Vector3& cameraPosition,const Vector3& cameraForward,PlayerBulletManager& bullets){
	// 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
	float aimHitAngle = GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,"aimHitAngle");

	bool isAimingAtAnyTarget = false; // レティクル中心の色変えに使う
	for(auto& target : targets_){
		if(!target.isAlive) continue;

		Vector3 toTarget = target.position - cameraPosition;
		float distance = Length(toTarget);
		if(distance < kMinAimDistance) continue; // カメラと的が重なっている異常値は無視

		float angle = AngleBetween(cameraForward,toTarget);
		bool isAimed = angle <= aimHitAngle;
		if(isAimed){
			isAimingAtAnyTarget = true;
		}

		// 狙えているときは少し大きくして視覚的にフィードバック
		// 当たり判定も見た目の大きさに合わせるため、判定より先に求めておく
		float scale = isAimed?kAimedScale:kNormalScale;

		// 的と生存している弾の境界球が重なっていればヒット
		// 的は描画用オブジェクトの行列がこの後で更新されるため、現在座標と表示スケールから境界球を求める
		if(model_ && bullets.CheckHit(model_->GetBoundingSphere(target.position,scale))){
			target.isAlive = false;

			// 的の撃破位置に火花パーティクルを発生させる
			ParticleManager::GetInstance()->EmitSpark(target.position);
		}

		if(target.obj){
			target.obj->SetTranslate(target.position);
			target.obj->SetScale({scale, scale, scale});
			if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
				target.obj->SetCamera(activeCamera);
			}
			target.obj->Update();
		}
	}

	return isAimingAtAnyTarget;
}

// すべての的を生存状態に戻す
void TargetManager::Reset(){
	for(auto& target : targets_){
		target.isAlive = true;
	}
}

// 生存している的だけ描画する
void TargetManager::Draw(){
	for(auto& target : targets_){
		if(target.isAlive && target.obj){
			target.obj->Draw();
		}
	}
}

#ifdef USE_IMGUI
// シーン階層(Hierarchy)の最小版
// Unity/Unreal風の「一覧から選択 → Inspectorで編集」フロー
void TargetManager::ShowHierarchy(){
	ImGui::Text("Targets: %d",static_cast<int>(targets_.size()));
	ImGui::Separator();
	for(int i = 0; i < static_cast<int>(targets_.size()); ++i){
		ImGui::PushID(i);
		// 行ラベル(倒された的は (dead) を付ける)
		char label[32];
		snprintf(label,sizeof(label),"Target %d%s",i,targets_[i].isAlive?"":" (dead)");
		// クリックで選択。選択中の行はハイライトされる(選択状態は配置エディターが保持する)
		if(ImGui::Selectable(label,editor_ && editor_->GetSelectedIndex() == i)){
			if(editor_){
				editor_->SetSelectedIndex(i);
			}
		}
		ImGui::PopID();
	}
}

// Inspectorの最小版
void TargetManager::ShowInspector(){
	int selectedTargetIndex = editor_?editor_->GetSelectedIndex():-1;
	if(selectedTargetIndex >= 0 && selectedTargetIndex < static_cast<int>(targets_.size())){
		ImGui::Text("Target %d",selectedTargetIndex);
		ImGui::Separator();
		// 位置は配置エディターのデータを直接編集する(次フレームのSyncFromEditorで反映される)
		EditorWidgets::ButtonVector3("Position",editor_->GetTargetPositionRef(selectedTargetIndex),0.1f,1.0f);
		// 生存フラグ(OFFで非表示、ONで復活)
		ImGui::Checkbox("Alive",&targets_[selectedTargetIndex].isAlive);
	} else{
		ImGui::TextDisabled("Select a target in Hierarchy");
	}
}
#endif
