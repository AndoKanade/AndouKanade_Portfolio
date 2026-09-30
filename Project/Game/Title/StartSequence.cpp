#include "StartSequence.h"
#include "Title/TitleLogo.h"
#include "Title/TitleUI.h"
#include "Input.h"

StartSequence::StartSequence() = default;
StartSequence::~StartSequence() = default;

// 初期化
void StartSequence::Initialize(Obj3dCommon* object3dCommon,SpriteCommon* spriteCommon,Input* input){
	input_ = input;

	titleLogo_ = std::make_unique<TitleLogo>();
	titleLogo_->Initialize(object3dCommon);

	titleUI_ = std::make_unique<TitleUI>();
	titleUI_->Initialize(spriteCommon);
}

// タイトルから始める
void StartSequence::Start(){
	phase_ = Phase::Title;
	cameraDirector_.Reset();
	countdown_.Stop();
	if(titleLogo_){
		titleLogo_->Reset();
	}
	if(titleUI_){
		titleUI_->Reset();
	}
}

// 演出を止める
void StartSequence::Stop(){
	phase_ = Phase::None;
	countdown_.Stop();
}

// 更新処理
void StartSequence::Update(float deltaTime){
	switch(phase_){
	case Phase::Title:
		// カメラをゆっくり揺らしながら、SPACE の入力を待つ
		cameraDirector_.UpdateIdle(deltaTime);
		if(titleUI_){
			titleUI_->Update(deltaTime,kTitleVisible);
		}
		// スタートと同時にタイトルロゴを縮めて消し始め、回り込むカメラにかぶらないようにする
		if(input_ && input_->TriggerKey(DIK_SPACE)){
			cameraDirector_.BeginTurn();
			if(titleLogo_){
				titleLogo_->BeginHide();
			}
			phase_ = Phase::CameraTurn;
		}
		break;

	case Phase::CameraTurn:
		// 回り込みの進行に合わせてタイトル表示を薄くしていく
		cameraDirector_.UpdateTurn(deltaTime);
		if(titleLogo_){
			titleLogo_->UpdateHide(deltaTime);
		}
		if(titleUI_){
			titleUI_->Update(deltaTime,kTitleVisible - cameraDirector_.GetTurnProgress());
		}
		if(cameraDirector_.IsTurnFinished()){
			countdown_.Start();
			phase_ = Phase::Countdown;
		}
		break;

	case Phase::Countdown:
		// GO になった瞬間からプレイ可能にする
		countdown_.Update(deltaTime);
		if(countdown_.IsCountFinished()){
			phase_ = Phase::Playing;
		}
		break;

	case Phase::Playing:
		// プレイ開始後もしばらく GO を表示するため、カウントダウンの更新を続ける
		countdown_.Update(deltaTime);
		break;

	case Phase::None:
	default:
		break;
	}
}

// タイトルロゴを配置する
void StartSequence::UpdateLogo(const Vector3& pivot,const Vector3& gameplayRotate){
	if(titleLogo_){
		titleLogo_->Update(pivot,gameplayRotate);
	}
}

// 3D描画処理(タイトルロゴはタイトル中と回り込み中だけ描画する)
void StartSequence::Draw3D(){
	if(titleLogo_ && IsControllingCamera()){
		titleLogo_->Draw();
	}
}

// 2D描画処理(タイトル表示はタイトル中と回り込み中だけ描画する)
void StartSequence::Draw2D(){
	if(titleUI_ && IsControllingCamera()){
		titleUI_->Draw();
	}
}

// 演出中のカメラ座標・向きを求める
void StartSequence::CalculateCamera(const Vector3& pivot,const Vector3& gameplayPosition,const Vector3& gameplayRotate,Vector3& outPosition,Vector3& outRotate) const{
	cameraDirector_.Calculate(pivot,gameplayPosition,gameplayRotate,outPosition,outRotate);
}
