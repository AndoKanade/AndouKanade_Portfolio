#pragma once

#include "MyMath.h"
#include <memory>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// タイトル中の2D表示(PRESS SPACE)
/// 専用素材が無いため、今は既存テクスチャを仮のスプライトとして表示する。
/// タイトルロゴは3Dモデルとして TitleLogo が表示する。
/// </summary>
class TitleUI{
public:
	TitleUI();
	~TitleUI();

	// スプライトの生成
	void Initialize(SpriteCommon* spriteCommon);

	// タイトル開始時の状態(点滅の位相)に戻す
	void Reset();

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="deltaTime">経過時間(秒)</param>
	/// <param name="visibility">表示の濃さ(1で表示、0で非表示。スタート後のフェードアウトに使う)</param>
	void Update(float deltaTime,float visibility);

	// 描画処理(スプライト共通の描画前処理も含む)
	void Draw();

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// PRESS SPACE の表示(仮)
	std::unique_ptr<Sprite> pressSprite_;
	// タイトル開始時に黒から明るくするための、画面全体を覆うスプライト
	std::unique_ptr<Sprite> fadeSprite_;

	// 点滅の経過時間
	float blinkTime_ = 0.0f;
	// フェードインの経過時間(秒)
	float fadeTimer_ = 0.0f;
	// 表示の濃さ(0のときは描画しない)
	float visibility_ = 1.0f;

	// PRESS SPACE の大きさ(ピクセル)
	static constexpr Vector2 kPressSize = {360.0f, 60.0f};
	// PRESS SPACE の中心の高さ(画面上端からの割合)
	static constexpr float kPressHeightRate = 0.8f;
	// 点滅の速さ(1秒あたりの位相の進み、ラジアン)
	static constexpr float kBlinkSpeed = 4.0f;
	// 点滅で最も薄くなったときの濃さ
	static constexpr float kBlinkMinAlpha = 0.2f;
	// 明るくし始める前に、真っ黒のまま止めておく時間(秒)
	static constexpr float kFadeHoldTime = 0.2f;
	// 明るくし始めてから完全に明るくなるまでの時間(秒)
	static constexpr float kFadeInDuration = 1.0f;
};
