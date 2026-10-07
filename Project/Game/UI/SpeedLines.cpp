#include "SpeedLines.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"
#include <cmath>
#include <string>

namespace{
	// 中央が濃く両端が消える、横長の白い線のテクスチャ
	const std::string kSpeedLineTexture = "resource/UI/speedLine.png";
	// 画面の中心を求めるための係数(画面サイズの半分)
	constexpr float kScreenCenterRate = 0.5f;
}

SpeedLines::SpeedLines() = default;
SpeedLines::~SpeedLines() = default;

// テクスチャの読み込みとスプライトの生成
void SpeedLines::Initialize(SpriteCommon* spriteCommon){
	spriteCommon_ = spriteCommon;

	TextureManager::GetInstance()->LoadTexture(kSpeedLineTexture);

	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());

	lines_.resize(kLineCount);
	for(Line& line : lines_){
		line.sprite = std::make_unique<Sprite>();
		line.sprite->Initialize(spriteCommon_,kSpeedLineTexture);
		// 根元を基準に回転させ、画面の中心から外向きに伸びる線にする
		line.sprite->SetAnchorPoint(kLineAnchor);
		Respawn(line,true);
	}
}

// すべての線を置き直し、表示の濃さを0に戻す
void SpeedLines::Reset(){
	intensity_ = 0.0f;
	for(Line& line : lines_){
		Respawn(line,true);
	}
}

// 指定した範囲の乱数を返す
float SpeedLines::RandomRange(float min,float max){
	std::uniform_real_distribution<float> distribution(min,max);
	return distribution(randomEngine_);
}

// 線を画面の中心寄りの位置へ置き直す
void SpeedLines::Respawn(Line& line,bool isInitialPlacement){
	line.angle = RandomRange(-kPi,kPi);
	line.distance = isInitialPlacement?RandomRange(kSpawnDistanceMin,kDespawnDistance):RandomRange(kSpawnDistanceMin,kSpawnDistanceMax);
	line.speed = RandomRange(kSpeedMin,kSpeedMax);
	line.length = RandomRange(kLengthMin,kLengthMax);
}

// 線の移動と表示の濃さの更新
void SpeedLines::Update(bool isActive,float deltaTime){
	// レールを進んでいる間は濃く、それ以外は薄くしていく
	if(isActive){
		intensity_ += kFadeInSpeed * deltaTime;
		if(intensity_ > 1.0f){
			intensity_ = 1.0f;
		}
	} else{
		intensity_ -= kFadeOutSpeed * deltaTime;
		if(intensity_ < 0.0f){
			intensity_ = 0.0f;
		}
	}

	// 見えないときは位置の計算も行わない
	if(intensity_ <= 0.0f){
		return;
	}

	const float centerX = static_cast<float>(WinAPI::kClientWidth) * kScreenCenterRate;
	const float centerY = static_cast<float>(WinAPI::kClientHeight) * kScreenCenterRate;

	for(Line& line : lines_){
		line.distance += line.speed * deltaTime;
		if(line.distance > kDespawnDistance){
			Respawn(line,false);
		}

		// 画面の中心から線の向きへ距離だけ離した位置に根元を置き、外向きに回転させる
		line.sprite->SetPosition({centerX + std::cos(line.angle) * line.distance, centerY + std::sin(line.angle) * line.distance});
		line.sprite->SetRotation(line.angle);
		line.sprite->SetSize({line.length, kThickness});
		line.sprite->SetColor({1.0f, 1.0f, 1.0f, kMaxAlpha * intensity_});
		line.sprite->Update();
	}
}

// 描画処理
void SpeedLines::Draw(){
	if(!spriteCommon_ || intensity_ <= 0.0f){
		return;
	}

	spriteCommon_->Draw(); // 描画前処理
	for(Line& line : lines_){
		line.sprite->Draw();
	}
}
