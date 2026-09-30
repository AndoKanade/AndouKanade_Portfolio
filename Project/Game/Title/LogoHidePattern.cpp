#include "LogoHidePattern.h"
#include "Easing.h"
#include <algorithm>

namespace{
	// ---------- 共通 ----------

	// 放物線 4t(1-t) の係数(t=0.5 でちょうど1になる)
	constexpr float kParabolaCoefficient = 4.0f;

	// 大きさを変えないときの倍率
	constexpr Vector3 kNormalScale = {1.0f, 1.0f, 1.0f};
	// ジャンプ前にしゃがんで溜めたときの大きさの倍率(縦に潰れて横に広がる)
	constexpr Vector3 kCrouchScale = {1.15f, 0.7f, 1.0f};
	// 跳び上がった直後に縦へ伸びたときの大きさの倍率
	constexpr Vector3 kStretchScale = {0.9f, 1.25f, 1.0f};

	// ---------- Squash ----------

	// 縦に潰し終わる時点の進行度(ここまでは縦だけ潰し、ここから横に縮める)
	constexpr float kSquashSplitProgress = 0.5f;
	// 縦に潰し終わったときの縦の倍率(完全に0にせず、横線として見えるようにする)
	constexpr float kSquashMinHeightRate = 0.05f;
	// 縦に潰すのに合わせて横へ広げる倍率
	constexpr float kSquashMaxWidthRate = 1.2f;

	// ---------- JumpAway ----------

	// しゃがみ終わる時点の進行度
	constexpr float kJumpAwayCrouchEnd = 0.2f;
	// 奥へ跳んでいく距離
	constexpr float kJumpAwayDistance = 12.0f;
	// ジャンプの最高点の高さ
	constexpr float kJumpAwayHeight = 4.0f;
	// 薄くなり始める進行度
	constexpr float kJumpAwayFadeStart = 0.6f;

	// ---------- 消え終わるまでの時間(秒) ----------

	// 変形だけで消える、動きの少ない消え方
	constexpr float kShortHideDuration = 0.5f;
	// ジャンプで消える消え方
	constexpr float kJumpHideDuration = 1.1f;

	// 放物線(0→1→0)。ジャンプの高さに使う
	float Parabola(float t){
		return kParabolaCoefficient * t * (1.0f - t);
	}

	// 全体の進行度のうち、[start, end] の区間での進行度(0〜1、区間の外は端の値に止める)
	float SectionRate(float progress,float start,float end){
		return std::clamp((progress - start) / (end - start),0.0f,1.0f);
	}

	// 縦に潰れて横線になり、横にも縮んで消える
	LogoHideEffect CalculateSquash(float progress){
		LogoHideEffect effect;
		if(progress < kSquashSplitProgress){
			// 前半: 縦に潰しながら、少し横に広げる
			float squashRate = Easing::EaseOutCubic(progress / kSquashSplitProgress);
			effect.scaleRate.x = 1.0f + (kSquashMaxWidthRate - 1.0f) * squashRate;
			effect.scaleRate.y = 1.0f + (kSquashMinHeightRate - 1.0f) * squashRate;
		} else{
			// 後半: 横線になった状態から、横にも縮めて消す
			float shrinkRate = Easing::EaseInCubic(SectionRate(progress,kSquashSplitProgress,1.0f));
			effect.scaleRate.x = kSquashMaxWidthRate * (1.0f - shrinkRate);
			effect.scaleRate.y = kSquashMinHeightRate;
		}
		return effect;
	}

	// 一度少しふくらんでから縮む(逆方向へ戻る動きのイージングを使う)
	LogoHideEffect CalculatePopOut(float progress){
		LogoHideEffect effect;
		float popRate = 1.0f - Easing::EaseInBack(progress);
		effect.scaleRate = {popRate, popRate, popRate};
		return effect;
	}

	// しゃがんで溜めてから、放物線を描いて奥へジャンプしていく
	LogoHideEffect CalculateJumpAway(float progress){
		LogoHideEffect effect;
		if(progress < kJumpAwayCrouchEnd){
			// 通常の大きさからしゃがんだ大きさへ潰して溜める
			float crouchRate = SectionRate(progress,0.0f,kJumpAwayCrouchEnd);
			effect.scaleRate = Lerp(kNormalScale,kCrouchScale,Easing::EaseOutCubic(crouchRate));
		} else{
			// 奥へ一定の速さで進みながら、高さは放物線を描く
			float jumpRate = SectionRate(progress,kJumpAwayCrouchEnd,1.0f);
			effect.offset.z = kJumpAwayDistance * jumpRate;
			effect.offset.y = kJumpAwayHeight * Parabola(jumpRate);

			// 跳び上がった直後は縦に伸び、ジャンプの前半(0〜0.5)をかけて元の大きさへ戻す
			constexpr float kStretchReturnEnd = 0.5f;
			effect.scaleRate = Lerp(kStretchScale,kNormalScale,SectionRate(jumpRate,0.0f,kStretchReturnEnd));
		}

		// 最後にかけて薄くしていく
		effect.alpha = 1.0f - SectionRate(progress,kJumpAwayFadeStart,1.0f);
		return effect;
	}
}

// 設定値の番号を消え方の種類に変換する
LogoHideType ToLogoHideType(int number,LogoHideType defaultType){
	if(number < static_cast<int>(kFirstLogoHideType) || number > static_cast<int>(kLastLogoHideType)){
		return defaultType;
	}
	return static_cast<LogoHideType>(number);
}

// 消え方ごとの、消え終わるまでの時間
float GetLogoHideDuration(LogoHideType type){
	switch(type){
	case LogoHideType::JumpAway:
		return kJumpHideDuration;

	case LogoHideType::Squash:
	case LogoHideType::PopOut:
	default:
		return kShortHideDuration;
	}
}

// 消える演出の進行度から、ロゴの見た目に加える変化を求める
LogoHideEffect CalculateLogoHideEffect(LogoHideType type,float progress){
	switch(type){
	case LogoHideType::Squash:
		return CalculateSquash(progress);

	case LogoHideType::PopOut:
		return CalculatePopOut(progress);

	case LogoHideType::JumpAway:
	default:
		return CalculateJumpAway(progress);
	}
}
