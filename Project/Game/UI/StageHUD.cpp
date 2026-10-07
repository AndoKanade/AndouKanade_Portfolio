#include "StageHUD.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include "objects/ComboCounter.h"

namespace{
	// 数字(0〜9, +, /)を横一列に並べたテクスチャ
	const std::string kDigitsTexture = "resource/UI/digits.png";
	// 体力アイコン
	const std::string kLifeIconTexture = "resource/UI/lifeIcon.png";
	// 壊した的の数の下地
	const std::string kPanelTexture = "resource/UI/panel.png";
	// 壊した的の数の横に置く標的のアイコン
	const std::string kTargetIconTexture = "resource/UI/targetIcon.png";
	// 連鎖数の吹き出し
	const std::string kComboBubbleTexture = "resource/UI/comboBubble.png";

	// 左上を基準点にするためのアンカー
	constexpr Vector2 kTopLeftAnchor = {0.0f, 0.0f};
}

StageHUD::StageHUD() = default;
StageHUD::~StageHUD() = default;

// テクスチャの読み込みとスプライトの生成
void StageHUD::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	TextureManager* textureManager = TextureManager::GetInstance();
	textureManager->LoadTexture(kDigitsTexture);
	textureManager->LoadTexture(kLifeIconTexture);
	textureManager->LoadTexture(kPanelTexture);
	textureManager->LoadTexture(kTargetIconTexture);
	textureManager->LoadTexture(kComboBubbleTexture);

	// 位置は毎フレームのUpdate()で決めるため、ここでは生成と大きさの設定だけ行う
	auto createSprite = [spriteCommon](const std::string& texture,const Vector2& size,const Vector2& anchor){
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon,texture);
		sprite->SetSize(size);
		sprite->SetAnchorPoint(anchor);
		return sprite;
	};

	counterPanel_ = createSprite(kPanelTexture,kPanelSize,kTopLeftAnchor);
	targetIcon_ = createSprite(kTargetIconTexture,{kTargetIconSize, kTargetIconSize},kCenterAnchor);
	comboBubble_ = createSprite(kComboBubbleTexture,kBubbleSize,kCenterAnchor);

	digitSprites_.reserve(kMaxDigitCount);
	for(size_t i = 0; i < kMaxDigitCount; ++i){
		auto digit = createSprite(kDigitsTexture,kGlyphSize,kCenterAnchor);
		// テクスチャ全体ではなく1文字分だけを切り出して使う
		digit->SetTextureSize(kGlyphSize);
		digitSprites_.push_back(std::move(digit));
	}
}

// 表示内容の更新
void StageHUD::Update(int hp,int maxHp,int destroyedCount,int totalCount,const ComboCounter& combo){
	usedDigitCount_ = 0;

	// 体力アイコン(最大体力が変わったときだけ作り直す)
	while(static_cast<int>(lifeIcons_.size()) < maxHp){
		auto icon = std::make_unique<Sprite>();
		icon->Initialize(spriteCommon_,kLifeIconTexture);
		icon->SetSize({kLifeIconSize, kLifeIconSize});
		icon->SetAnchorPoint(kTopLeftAnchor);
		lifeIcons_.push_back(std::move(icon));
	}
	while(static_cast<int>(lifeIcons_.size()) > maxHp){
		lifeIcons_.pop_back();
	}
	for(int i = 0; i < static_cast<int>(lifeIcons_.size()); ++i){
		lifeIcons_[i]->SetPosition({kScreenMargin + kLifeIconSpacing * static_cast<float>(i), kScreenMargin});
		// 残っている体力は明るく、減った分は暗く表示する
		lifeIcons_[i]->SetColor(i < hp?kWhite:kLostLifeColor);
		lifeIcons_[i]->Update();
	}

	// 壊した的の数(右上)
	const float panelLeft = static_cast<float>(WinAPI::kClientWidth) - kScreenMargin - kPanelSize.x;
	const float panelTop = kScreenMargin;
	const float panelCenterY = panelTop + kPanelSize.y * kCenterAnchor.y;
	counterPanel_->SetPosition({panelLeft, panelTop});
	counterPanel_->Update();
	targetIcon_->SetPosition({panelLeft + kTargetIconOffsetX, panelCenterY});
	targetIcon_->Update();
	LayoutText(std::to_string(destroyedCount) + "/" + std::to_string(totalCount),{panelLeft + kCounterTextOffsetX, panelCenterY},kCounterCharHeight,kWhite);

	// 連鎖数の「+N」(壊した数の下)
	isComboVisible_ = combo.GetCombo() > 0;
	if(isComboVisible_){
		// 途切れる直前はだんだん薄くして、もうすぐ途切れることを伝える
		float alpha = combo.GetRemainingRate() / kComboFadeStartRate;
		if(alpha > 1.0f){
			alpha = 1.0f;
		}
		// 増えた瞬間に大きくして、壊した手応えを出す
		float scale = 1.0f + kComboPopScale * combo.GetPopRate();

		const Vector2 bubbleCenter = {
			static_cast<float>(WinAPI::kClientWidth) - kScreenMargin - kBubbleSize.x * kCenterAnchor.x,
			panelTop + kPanelSize.y + kBubbleGapY + kBubbleSize.y * kCenterAnchor.y
		};
		comboBubble_->SetPosition(bubbleCenter);
		comboBubble_->SetSize({kBubbleSize.x * scale, kBubbleSize.y * scale});
		comboBubble_->SetColor({kWhite.x, kWhite.y, kWhite.z, alpha});
		comboBubble_->Update();
		LayoutText("+" + std::to_string(combo.GetCombo()),bubbleCenter,kComboCharHeight * scale,{kWhite.x, kWhite.y, kWhite.z, alpha});
	}
}

// 数字の文字列を、数字用スプライトを並べて配置する
void StageHUD::LayoutText(const std::string& text,const Vector2& center,float charHeight,const Vector4& color){
	const float charWidth = charHeight * (kGlyphSize.x / kGlyphSize.y);
	const float advance = charWidth * kCharSpacingRate;
	// 文字列全体の幅から、1文字目の中心の位置を求める
	const float totalWidth = advance * static_cast<float>(text.size() - 1);
	float x = center.x - totalWidth * kCenterAnchor.x;

	for(char c : text){
		if(usedDigitCount_ >= digitSprites_.size()){
			return;
		}

		// 文字からテクスチャ上の番号を求める(0〜9 の後に + と / が並んでいる)
		int glyphIndex = 0;
		if(c == '+'){
			glyphIndex = kGlyphPlusIndex;
		} else if(c == '/'){
			glyphIndex = kGlyphSlashIndex;
		} else{
			glyphIndex = c - '0';
		}

		Sprite* sprite = digitSprites_[usedDigitCount_].get();
		sprite->SetTextureLeftTop({kGlyphSize.x * static_cast<float>(glyphIndex), 0.0f});
		sprite->SetPosition({x, center.y});
		sprite->SetSize({charWidth, charHeight});
		sprite->SetColor(color);
		sprite->Update();

		++usedDigitCount_;
		x += advance;
	}
}

// 描画処理
void StageHUD::Draw(){
	if(!spriteCommon_){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理

	for(auto& icon : lifeIcons_){
		icon->Draw();
	}

	counterPanel_->Draw();
	targetIcon_->Draw();
	if(isComboVisible_){
		comboBubble_->Draw();
	}

	// 数字は下地・吹き出しの上に重ねる
	for(size_t i = 0; i < usedDigitCount_; ++i){
		digitSprites_[i]->Draw();
	}
}
