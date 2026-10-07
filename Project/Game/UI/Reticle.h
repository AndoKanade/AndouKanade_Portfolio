#pragma once

#include "MyMath.h"
#include <memory>

// 前方宣言
class Sprite;
class SpriteCommon;

/// <summary>
/// 画面中央固定のレティクル(照準)
/// 外枠(常時表示)と中心ドット(狙えているときに赤く強調表示)の2枚構成。
/// </summary>
class Reticle{
public:
	Reticle();
	~Reticle();

	// スプライトの生成
	void Initialize(SpriteCommon* spriteCommon);

	// 更新処理(画面中央固定なので位置は変わらないが、内部行列更新のため毎フレーム呼ぶ)
	void Update();

	// 的を狙えているかどうかを設定する(狙えているときは中心ドットを赤くする)
	void SetAiming(bool isAiming);

	// 描画処理(スプライト共通の描画前処理も含む)
	void Draw();

private:
	SpriteCommon* spriteCommon_ = nullptr;

	// 外枠(常時表示)
	std::unique_ptr<Sprite> outlineSprite_;
	// 中心ドット(狙えているときに色を変える)
	std::unique_ptr<Sprite> centerSprite_;

	// スプライトの大きさ(ピクセル)
	static constexpr Vector2 kSize = {48.0f, 48.0f};
	// 中心を基準点にするためのアンカー
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
	// 画面中央の位置を求めるための係数(画面サイズの半分)
	static constexpr float kScreenCenterRate = 0.5f;
	// 狙えているときの中心ドットの色(赤)
	static constexpr Vector4 kAimingColor = {1.0f, 0.2f, 0.2f, 1.0f};
	// 狙えていないときの中心ドットの色(白)
	static constexpr Vector4 kNormalColor = {1.0f, 1.0f, 1.0f, 1.0f};
};
