#pragma once

#include "MyMath.h"

/// <summary>
/// タイトルロゴの消え方の種類
/// GlobalVariables(グループ "Title")の "logoHideMode" に番号で指定する。
/// </summary>
enum class LogoHideType{
	Squash = 1,   // 縦に潰れて横線になり、横にも縮んで消える(テレビを消したような動き)
	PopOut = 2,   // 一瞬ふくらんでから、勢いよく縮む
	JumpAway = 3, // しゃがんで溜めてから、放物線を描いて奥へジャンプしていく
};

// 消え方の番号として有効な範囲
constexpr LogoHideType kFirstLogoHideType = LogoHideType::Squash;
constexpr LogoHideType kLastLogoHideType = LogoHideType::JumpAway;

/// <summary>
/// 消える演出の途中で、ロゴの見た目に加える変化
/// </summary>
struct LogoHideEffect{
	Vector3 scaleRate = {1.0f, 1.0f, 1.0f}; // 大きさの倍率(x: 横、y: 縦、z: 厚み)
	Vector3 offset = {0.0f, 0.0f, 0.0f};    // 位置のずれ(プレイヤーから見て x: 右、y: 上、z: 後ろ)
	float spinYaw = 0.0f;                   // 文字列の中心を軸にした追加のY回転(ラジアン)
	float alpha = 1.0f;                     // α値(0で透明)
};

/// <summary>
/// 消える演出の進行度から、ロゴの見た目に加える変化を求める
/// </summary>
/// <param name="type">消え方の種類</param>
/// <param name="progress">消える演出の進行度(0で開始、1で消え終わり)</param>
/// <returns>ロゴの見た目に加える変化</returns>
LogoHideEffect CalculateLogoHideEffect(LogoHideType type,float progress);

/// <summary>
/// 消え方ごとの、消え終わるまでの時間(秒)を取得する
/// 動きの多い消え方ほど長くしている(カメラの回り込み2秒より短くする)
/// </summary>
float GetLogoHideDuration(LogoHideType type);

/// <summary>
/// 設定値の番号を消え方の種類に変換する
/// </summary>
/// <param name="number">設定値の番号</param>
/// <param name="defaultType">範囲外の番号のときに使う消え方</param>
LogoHideType ToLogoHideType(int number,LogoHideType defaultType);
