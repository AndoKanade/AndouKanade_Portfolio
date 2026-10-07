#include "Reticle.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include <string>

namespace{
	// 外枠のテクスチャ
	const std::string kOutlineTexture = "resource/Reticle/reticleOutline.png";
	// 中心ドットのテクスチャ
	const std::string kCenterTexture = "resource/Reticle/reticle.png";
}

Reticle::Reticle() = default;
Reticle::~Reticle() = default;

// スプライトの生成
void Reticle::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	TextureManager::GetInstance()->LoadTexture(kOutlineTexture);
	TextureManager::GetInstance()->LoadTexture(kCenterTexture);

	// 外枠と中心ドットはどちらも画面中央に置く
	const Vector2 screenCenter = {
		static_cast<float>(WinAPI::kClientWidth) * kScreenCenterRate,
		static_cast<float>(WinAPI::kClientHeight) * kScreenCenterRate
	};

	outlineSprite_ = std::make_unique<Sprite>();
	outlineSprite_->Initialize(spriteCommon_,kOutlineTexture);
	outlineSprite_->SetSize(kSize);
	outlineSprite_->SetAnchorPoint(kCenterAnchor); // 中心を基準点にする
	outlineSprite_->SetPosition(screenCenter);

	centerSprite_ = std::make_unique<Sprite>();
	centerSprite_->Initialize(spriteCommon_,kCenterTexture);
	centerSprite_->SetSize(kSize);
	centerSprite_->SetAnchorPoint(kCenterAnchor);
	centerSprite_->SetPosition(screenCenter);
}

// 更新処理
void Reticle::Update(){
	if(outlineSprite_) outlineSprite_->Update();
	if(centerSprite_) centerSprite_->Update();
}

// 狙えているときはレティクル中心を赤く、それ以外は白のままにする
void Reticle::SetAiming(bool isAiming){
	if(centerSprite_){
		centerSprite_->SetColor(isAiming?kAimingColor:kNormalColor);
	}
}

// 描画処理(2D描画のため、SpriteCommonの描画前処理を先に呼ぶ)
void Reticle::Draw(){
	if((!outlineSprite_ && !centerSprite_) || !spriteCommon_){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理
	if(outlineSprite_) outlineSprite_->Draw(); // 外枠を先に描画
	if(centerSprite_) centerSprite_->Draw();   // 中心ドットを上から重ねる
}
