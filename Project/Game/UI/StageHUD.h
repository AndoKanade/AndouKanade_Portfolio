#pragma once

#include "MyMath.h"
#include <memory>
#include <string>
#include <vector>

// 前方宣言
class Sprite;
class SpriteCommon;
class ComboCounter;

/// <summary>
/// プレイ中の画面表示(HUD)
/// 左上に体力アイコン、右上に壊した的の数、その下に連鎖数の「+N」を出す。
/// ImGui を使わずスプライトで描くため、Release 構成でも表示される。
/// </summary>
class StageHUD{
public:
	StageHUD();
	~StageHUD();

	// テクスチャの読み込みとスプライトの生成
	void Initialize(SpriteCommon* spriteCommon);

	/// <summary>
	/// 表示内容の更新
	/// </summary>
	/// <param name="hp">プレイヤーの今の体力</param>
	/// <param name="maxHp">プレイヤーの体力の最大値</param>
	/// <param name="destroyedCount">壊した的の数</param>
	/// <param name="totalCount">配置されている的の総数</param>
	/// <param name="combo">連鎖数</param>
	void Update(int hp,int maxHp,int destroyedCount,int totalCount,const ComboCounter& combo);

	// 描画処理(スプライト共通の描画前処理も含む)
	void Draw();

private:
	/// <summary>
	/// 数字の文字列を、数字用スプライトを並べて配置する
	/// </summary>
	/// <param name="text">表示する文字列(0〜9, +, / のみ)</param>
	/// <param name="center">文字列全体の中心の画面座標</param>
	/// <param name="charHeight">1文字の高さ(ピクセル)</param>
	/// <param name="color">文字の色</param>
	void LayoutText(const std::string& text,const Vector2& center,float charHeight,const Vector4& color);

	SpriteCommon* spriteCommon_ = nullptr;

	// 体力アイコン(最大体力の数だけ並べる)
	std::vector<std::unique_ptr<Sprite>> lifeIcons_;
	// 壊した的の数の下地
	std::unique_ptr<Sprite> counterPanel_;
	// 壊した的の数の横に置く標的のアイコン
	std::unique_ptr<Sprite> targetIcon_;
	// 連鎖数の吹き出し
	std::unique_ptr<Sprite> comboBubble_;
	// 数字用のスプライト(使い回す。表示する文字数ぶんだけ使う)
	std::vector<std::unique_ptr<Sprite>> digitSprites_;
	// このフレームで使った数字用スプライトの数
	size_t usedDigitCount_ = 0;
	// 連鎖数の吹き出しを表示するかどうか
	bool isComboVisible_ = false;

	// 数字のテクスチャの1文字分の大きさ(ピクセル)
	static constexpr Vector2 kGlyphSize = {64.0f, 80.0f};
	// 数字のテクスチャで、0〜9の後に並んでいる記号の番号
	static constexpr int kGlyphPlusIndex = 10;
	static constexpr int kGlyphSlashIndex = 11;
	// 文字の間隔(1文字の幅に対する割合。縁取りの分だけ詰めて並べる)
	static constexpr float kCharSpacingRate = 0.78f;
	// 数字用スプライトの数(壊した数の「000/000」と連鎖数の「+000」が入る数)
	static constexpr size_t kMaxDigitCount = 12;

	// 画面の端からの余白(ピクセル)
	static constexpr float kScreenMargin = 32.0f;
	// 体力アイコンの大きさと間隔(ピクセル)
	static constexpr float kLifeIconSize = 52.0f;
	static constexpr float kLifeIconSpacing = 58.0f;
	// 減った体力のアイコンの色(暗く半透明にする)
	static constexpr Vector4 kLostLifeColor = {0.3f, 0.3f, 0.35f, 0.5f};
	// 下地の大きさ(ピクセル)
	static constexpr Vector2 kPanelSize = {256.0f, 80.0f};
	// 下地の中での標的アイコンの大きさと、左端からの位置(ピクセル)
	static constexpr float kTargetIconSize = 52.0f;
	static constexpr float kTargetIconOffsetX = 44.0f;
	// 下地の中での数字の中心の、左端からの位置と文字の高さ(ピクセル)
	static constexpr float kCounterTextOffsetX = 156.0f;
	static constexpr float kCounterCharHeight = 52.0f;
	// 吹き出しの大きさと、下地からの縦の間隔(ピクセル)
	static constexpr Vector2 kBubbleSize = {150.0f, 75.0f};
	static constexpr float kBubbleGapY = 12.0f;
	// 吹き出しの中の文字の高さ(ピクセル)
	static constexpr float kComboCharHeight = 48.0f;
	// 連鎖数が増えた瞬間に吹き出しを大きくする割合(1 + この値の倍率まで弾む)
	static constexpr float kComboPopScale = 0.35f;
	// 途切れる直前に薄くし始める、残り時間の割合
	static constexpr float kComboFadeStartRate = 0.3f;
	// 白(通常の文字・アイコンの色)
	static constexpr Vector4 kWhite = {1.0f, 1.0f, 1.0f, 1.0f};
	// 中心を基準点にするためのアンカー
	static constexpr Vector2 kCenterAnchor = {0.5f, 0.5f};
};
