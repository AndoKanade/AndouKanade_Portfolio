#include "TitleLogo.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "GlobalVariables.h"
#include "Easing.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace{
	// タイトルロゴのモデル(ModelManager は resource/ からの相対パスで指定する)
	const std::string kLogoModelPath = "Title/title.obj";
	// GlobalVariablesのグループ名(タイトル演出の調整項目)
	const char* kTitleGroup = "Title";
	// ワールドの上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};
}

TitleLogo::TitleLogo() = default;
TitleLogo::~TitleLogo() = default;

// モデルの読み込みと調整項目の登録
void TitleLogo::Initialize(Obj3dCommon* object3dCommon){
	ModelManager::GetInstance()->LoadModel(kLogoModelPath);
	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(object3dCommon);
	obj_->SetModel(kLogoModelPath);

	// 実際の画面を見ながら位置・大きさを詰められるよう、調整項目として登録する
	// (Editモードの "Global Variables" パネル、または resource/GlobalVariables/Title.json の書き換えで変更できる)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->CreateGroup(kTitleGroup);
	gv->AddItem(kTitleGroup,"logoOffset",kDefaultOffset);
	gv->AddItem(kTitleGroup,"logoScale",kDefaultScale);
	// 消え方の番号(番号と消え方の対応は LogoHideType を参照)
	gv->AddItem(kTitleGroup,"logoHideMode",static_cast<int32_t>(kDefaultHideType));
}

// プレイヤーの位置と向きを基準にロゴを配置する
void TitleLogo::Update(const Vector3& pivot,const Vector3& gameplayRotate){
	if(!obj_){
		return;
	}

	GlobalVariables* gv = GlobalVariables::GetInstance();
	Vector3 offset = gv->GetVector3Value(kTitleGroup,"logoOffset");
	float baseScale = gv->GetFloatValue(kTitleGroup,"logoScale");

	// 消える演出中は、選んだ消え方に応じて大きさ・位置・回転・α値に変化を加える(演出前は変化なし)
	LogoHideEffect effect = CalculateLogoHideEffect(hideType_,GetHideProgress());
	Vector3 scale = {baseScale * effect.scaleRate.x, baseScale * effect.scaleRate.y, baseScale * effect.scaleRate.z};
	offset += effect.offset;
	// 登場演出中は、弾むイージングで上空から置く位置まで落とす(着地後はずれ無し)
	offset.y += kAppearDropHeight * (1.0f - Easing::EaseOutBounce(GetAppearProgress()));
	// 待機中はゆっくり浮き沈みさせる(sinの0から始まるため、着地した瞬間から途切れずにつながる)
	offset.y += kIdleBobHeight * std::sin(idleTimer_ * kIdleBobSpeed);
	if(Model::Material* material = obj_->GetMaterial()){
		material->color.w = effect.alpha;
	}

	// プレイ用カメラのヨーから、水平面での前方向と右方向を求める(上下の傾きは使わず、ロゴは常に直立させる)
	float yaw = gameplayRotate.y;
	Vector3 forward = {std::sin(yaw), 0.0f, std::cos(yaw)};
	Vector3 right = {std::cos(yaw), 0.0f, -std::sin(yaw)};

	// 文字列の中心を置きたい位置(プレイヤーから後ろ・上・右へずらした位置)
	Vector3 center = pivot - forward * offset.z + kWorldUp * offset.y + right * offset.x;

	// 回転演出と待機中の首振りの分だけ追加で回したときの、モデルのX軸の向き(ヨーだけ回したモデルのX軸と一致する)
	float logoYaw = yaw + effect.spinYaw + kIdleSwayAngle * std::sin(idleTimer_ * kIdleSwaySpeed);
	Vector3 logoRight = {std::cos(logoYaw), 0.0f, -std::sin(logoYaw)};

	// モデルの原点は文字の左下なので、文字列の中心がcenterに来るよう原点の位置をずらす
	// (大きさ・回転に合わせてずらすため、縮めたり回したりしても文字列の中心が軸になる)
	Vector3 origin = center - logoRight * (kModelCenterX * scale.x) - kWorldUp * (kModelCenterY * scale.y);

	// 文字の正面(モデルの+Z)をプレイ用カメラの前方向へ向けると、タイトル中のカメラ(プレイヤーの前方)から正面が見える
	obj_->SetTranslate(origin);
	obj_->SetRotate({0.0f, logoYaw, 0.0f});
	obj_->SetScale(scale);
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		obj_->SetCamera(activeCamera);
	}
	obj_->Update();
}

// 描画処理(消え終わった後は描画しない)
void TitleLogo::Draw(){
	if(obj_ && GetHideProgress() < 1.0f){
		obj_->Draw();
	}
}

// 表示状態に戻す
void TitleLogo::Reset(){
	hideTimer_ = 0.0f;
	isHiding_ = false;
	appearTimer_ = 0.0f;
	idleTimer_ = 0.0f;
}

// 登場演出を進める(待ち時間と落下時間の合計を超えないよう止める)
void TitleLogo::UpdateAppear(float deltaTime){
	appearTimer_ = (std::min)(appearTimer_ + deltaTime,kAppearDelay + kAppearDuration);
}

// 登場演出が終わったか
bool TitleLogo::IsAppearFinished() const{
	return GetAppearProgress() >= 1.0f;
}

// 待機中の揺れを進める(着地前に揺れると落下の動きと混ざるため、着地後だけ進める)
void TitleLogo::UpdateIdle(float deltaTime){
	if(IsAppearFinished()){
		idleTimer_ += deltaTime;
	}
}

// 登場演出の進行度(待ち時間中は0のまま)
float TitleLogo::GetAppearProgress() const{
	return std::clamp((appearTimer_ - kAppearDelay) / kAppearDuration,0.0f,1.0f);
}

// 消える演出を始める(消え方はこの時点の設定値を使い、消え終わるまでの時間は消え方ごとに決まった値を使う)
void TitleLogo::BeginHide(){
	hideTimer_ = 0.0f;
	isHiding_ = true;
	hideType_ = ToLogoHideType(GlobalVariables::GetInstance()->GetIntValue(kTitleGroup,"logoHideMode"),kDefaultHideType);
	hideDuration_ = GetLogoHideDuration(hideType_);
}

// 消える演出を進める(終了時間を超えないよう止める)
void TitleLogo::UpdateHide(float deltaTime){
	if(isHiding_){
		hideTimer_ = (std::min)(hideTimer_ + deltaTime,hideDuration_);
	}
}

// 消える演出の進行度(0〜1)
float TitleLogo::GetHideProgress() const{
	return isHiding_?(hideTimer_ / hideDuration_):0.0f;
}
