#include "TitleUI.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include <cmath>
#include <string>

namespace{
	// PRESS SPACE の仮テクスチャ(専用素材ができたら差し替える)
	const std::string kPressTexture = "resource/gradationLine.png";
	// スプライトを中心基準で配置するためのアンカー
	constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
	// 画面の横方向の中心(画面幅に対する割合)
	constexpr float kScreenCenterRate = 0.5f;
	// sin波(-1〜1)に1を足してから掛け、0〜1に変換する倍率
	constexpr float kWaveToRateScale = 0.5f;
}

TitleUI::TitleUI() = default;
TitleUI::~TitleUI() = default;

// スプライトの生成
void TitleUI::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	TextureManager::GetInstance()->LoadTexture(kPressTexture);

	const float screenWidth = static_cast<float>(WinAPI::kClientWidth);
	const float screenHeight = static_cast<float>(WinAPI::kClientHeight);

	pressSprite_ = std::make_unique<Sprite>();
	pressSprite_->Initialize(spriteCommon_,kPressTexture);
	pressSprite_->SetSize(kPressSize);
	pressSprite_->SetAnchorPoint(kCenterAnchor);
	pressSprite_->SetPosition({screenWidth * kScreenCenterRate, screenHeight * kPressHeightRate});
}

// タイトル開始時の状態に戻す
void TitleUI::Reset(){
	blinkTime_ = 0.0f;
	visibility_ = 1.0f;
}

// 更新処理
void TitleUI::Update(float deltaTime,float visibility){
	blinkTime_ += deltaTime;
	visibility_ = visibility;

	// PRESS SPACE はsin波で kBlinkMinAlpha〜1 の間を往復させて点滅させ、さらにフェードの濃さを掛ける
	if(pressSprite_){
		float wave = (std::sin(blinkTime_ * kBlinkSpeed) + 1.0f) * kWaveToRateScale;
		float blinkAlpha = kBlinkMinAlpha + (1.0f - kBlinkMinAlpha) * wave;
		pressSprite_->SetColor({1.0f, 1.0f, 1.0f, blinkAlpha * visibility_});
		pressSprite_->Update();
	}
}

// 描画処理
void TitleUI::Draw(){
	// 完全にフェードアウトした後は描画しない
	if(visibility_ <= 0.0f || !spriteCommon_){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理
	if(pressSprite_) pressSprite_->Draw();
}
