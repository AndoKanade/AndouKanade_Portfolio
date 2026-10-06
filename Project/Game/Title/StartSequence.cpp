#include "StartSequence.h"
#include "Title/TitleLogo.h"
#include "Title/TitleUI.h"
#include "Input.h"
#include "SoundManager.h"
#include "GlobalVariables.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace{
	// タイトル中に流すBGM
	const std::string kTitleBgmPath = "resource/music/bgm/nemui.mp3";
	// SPACE を押してスタートしたときの決定音
	const std::string kDecideSePath = "resource/music/se/decide.mp3";
	// GlobalVariablesのグループ名(タイトル演出の調整項目)
	const char* kTitleGroup = "Title";
	// BGM・SEの音量の調整項目名
	const char* kBgmVolumeKey = "bgmVolume";
	const char* kSeVolumeKey = "seVolume";
	// 音量の範囲(0=無音, 1=元の音量)
	constexpr float kMinVolume = 0.0f;
	constexpr float kMaxVolume = 1.0f;

	// 調整項目から音量を取得する(ImGuiで範囲外の値を入れられても音が割れないよう範囲内に収める)
	float GetVolume(const char* key){
		return std::clamp(GlobalVariables::GetInstance()->GetFloatValue(kTitleGroup,key),kMinVolume,kMaxVolume);
	}
}

StartSequence::StartSequence() = default;
StartSequence::~StartSequence() = default;

// 初期化
void StartSequence::Initialize(Obj3dCommon* object3dCommon,SpriteCommon* spriteCommon,Input* input){
	input_ = input;

	titleLogo_ = std::make_unique<TitleLogo>();
	titleLogo_->Initialize(object3dCommon);

	titleUI_ = std::make_unique<TitleUI>();
	titleUI_->Initialize(spriteCommon);

	// BGM・SEの読み込み
	SoundManager::GetInstance()->SoundLoadFile(kTitleBgmPath);
	SoundManager::GetInstance()->SoundLoadFile(kDecideSePath);

	// 実際に聞きながら音量を詰められるよう、調整項目として登録する
	// (ImGuiの "Global Variables" パネル、または resource/GlobalVariables/Title.json の書き換えで変更できる)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->CreateGroup(kTitleGroup);
	gv->AddItem(kTitleGroup,kBgmVolumeKey,kDefaultBgmVolume);
	gv->AddItem(kTitleGroup,kSeVolumeKey,kDefaultSeVolume);
}

// タイトルから始める
void StartSequence::Start(){
	phase_ = Phase::Title;
	playerHopTimer_ = 0.0f;
	cameraDirector_.Reset();
	countdown_.Stop();
	if(titleLogo_){
		titleLogo_->Reset();
	}
	if(titleUI_){
		titleUI_->Reset();
	}
	// タイトルBGMを最初からループ再生する
	SoundManager::GetInstance()->PlayAudio(kTitleBgmPath,GetVolume(kBgmVolumeKey),true);
}

// 演出を止める
void StartSequence::Stop(){
	phase_ = Phase::None;
	countdown_.Stop();
	SoundManager::GetInstance()->StopAudio(kTitleBgmPath);
}

// 更新処理
void StartSequence::Update(float deltaTime){
	switch(phase_){
	case Phase::Title:
		// カメラをゆっくり揺らしながら、SPACE の入力を待つ
		cameraDirector_.UpdateIdle(deltaTime);
		// ImGuiで変更した音量をすぐ反映させる
		SoundManager::GetInstance()->SetVolume(kTitleBgmPath,GetVolume(kBgmVolumeKey));
		if(titleUI_){
			titleUI_->Update(deltaTime,kTitleVisible);
		}
		// タイトルロゴの登場演出と、着地後の揺れを進める
		if(titleLogo_){
			titleLogo_->UpdateAppear(deltaTime);
			titleLogo_->UpdateIdle(deltaTime);
		}
		// プレイヤーが跳ねる周期を進める
		playerHopTimer_ += deltaTime;
		// 登場演出の途中で消える演出と重ならないよう、ロゴが着地してから入力を受け付ける
		// スタートと同時にタイトルロゴを縮めて消し始め、回り込むカメラにかぶらないようにする
		if((!titleLogo_ || titleLogo_->IsAppearFinished()) && input_ && input_->TriggerKey(DIK_SPACE)){
			cameraDirector_.BeginTurn();
			if(titleLogo_){
				titleLogo_->BeginHide();
			}
			// 決定音を鳴らす
			SoundManager::GetInstance()->PlayAudio(kDecideSePath,GetVolume(kSeVolumeKey));
			phase_ = Phase::CameraTurn;
		}
		break;

	case Phase::CameraTurn:
		// 回り込みの進行に合わせてタイトル表示を薄くしていく
		cameraDirector_.UpdateTurn(deltaTime);
		// スタート時に跳ねている途中なら、着地するまでは跳ねを進める(着地後は高さ0のまま止まる)
		if(GetPlayerHopHeight() > 0.0f){
			playerHopTimer_ += deltaTime;
		}
		if(titleLogo_){
			titleLogo_->UpdateHide(deltaTime);
		}
		if(titleUI_){
			titleUI_->Update(deltaTime,kTitleVisible - cameraDirector_.GetTurnProgress());
		}
		// タイトル表示と同じく、回り込みの進行に合わせてBGMを小さくしていく
		SoundManager::GetInstance()->SetVolume(kTitleBgmPath,GetVolume(kBgmVolumeKey) * (kTitleVisible - cameraDirector_.GetTurnProgress()));
		if(cameraDirector_.IsTurnFinished()){
			// 回り込みが終わったらBGMを止める
			SoundManager::GetInstance()->StopAudio(kTitleBgmPath);
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

// タイトル中にプレイヤーをその場で跳ねさせる高さ
float StartSequence::GetPlayerHopHeight() const{
	if(!IsControllingCamera()){
		return 0.0f;
	}

	// 周期の中での経過時間を求め、周期の最後の kPlayerHopDuration 秒間だけ跳ねさせる
	float cycleTime = std::fmod(playerHopTimer_,kPlayerHopInterval);
	float hopStartTime = kPlayerHopInterval - kPlayerHopDuration;
	if(cycleTime < hopStartTime){
		return 0.0f;
	}

	// 跳ねている間の進行度(0〜1)をsinの半周期に当てはめ、山なりに上がって下りる高さにする
	float hopProgress = (cycleTime - hopStartTime) / kPlayerHopDuration;
	return kPlayerHopHeight * std::sin(hopProgress * kPi);
}

// 演出中のカメラ座標・向きを求める
void StartSequence::CalculateCamera(const Vector3& pivot,const Vector3& gameplayPosition,const Vector3& gameplayRotate,Vector3& outPosition,Vector3& outRotate) const{
	cameraDirector_.Calculate(pivot,gameplayPosition,gameplayRotate,outPosition,outRotate);
}
