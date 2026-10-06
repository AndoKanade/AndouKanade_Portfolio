#pragma once

// イージング関数
// 引数tは0〜1の進行度で、補間に使う割合を返す(t=0で0、t=1で1)
namespace Easing{

	// ゆっくり動き出し、中盤で加速し、最後にゆっくり止まる(3次関数)
	inline float EaseInOutCubic(float t){
		// 前半と後半で式を切り替える境目
		constexpr float kHalf = 0.5f;
		// 前半(加速側)の係数。t=0.5でちょうど0.5になるよう 0.5 / 0.5^3 = 4 にしている
		constexpr float kInCoefficient = 4.0f;
		// 後半(減速側)で進行度を折り返すための係数(t=0.5〜1 を 1〜0 に変換する)
		constexpr float kOutScale = 2.0f;

		if(t < kHalf){
			return kInCoefficient * t * t * t;
		}

		// 後半は前半の式を点対称に折り返した形にする
		float reversed = -kOutScale * t + kOutScale;
		return 1.0f - (reversed * reversed * reversed) / kOutScale;
	}

	// ゆっくり動き出して、だんだん加速する(3次関数)
	inline float EaseInCubic(float t){
		return t * t * t;
	}

	// 勢いよく動き出して、だんだん減速して止まる(3次関数)
	inline float EaseOutCubic(float t){
		float reversed = 1.0f - t;
		return 1.0f - reversed * reversed * reversed;
	}

	// 一度逆方向へ少し戻ってから、加速して進む(溜めてから動くような動き)
	inline float EaseInBack(float t){
		// 逆方向へ戻る量の係数(一般的に使われる値で、最大で約10%戻る)
		constexpr float kBackAmount = 1.70158f;
		// 3次の項の係数(t=1でちょうど1になるよう kBackAmount + 1 にする)
		constexpr float kCubicCoefficient = kBackAmount + 1.0f;

		return kCubicCoefficient * t * t * t - kBackAmount * t * t;
	}

	// 落ちてきて地面で数回弾み、だんだん弾みが小さくなって止まる
	inline float EaseOutBounce(float t){
		// 放物線の鋭さの係数(一般的に使われる値)
		constexpr float kCoefficient = 7.5625f;
		// 1回の落下と3回の弾みに区切るための基準の幅
		constexpr float kDivisor = 2.75f;
		// 各区間の終わりの位置(落下、1回目、2回目の弾みの終わり)
		constexpr float kFirstEnd = 1.0f / kDivisor;
		constexpr float kSecondEnd = 2.0f / kDivisor;
		constexpr float kThirdEnd = 2.5f / kDivisor;
		// 各弾みの頂点の位置(区間の中央)
		constexpr float kSecondCenter = 1.5f / kDivisor;
		constexpr float kThirdCenter = 2.25f / kDivisor;
		constexpr float kFourthCenter = 2.625f / kDivisor;
		// 各弾みの頂点で地面から浮く量を決める値(弾むたびに小さくする)
		constexpr float kSecondBase = 0.75f;
		constexpr float kThirdBase = 0.9375f;
		constexpr float kFourthBase = 0.984375f;

		if(t < kFirstEnd){
			// 最初の落下
			return kCoefficient * t * t;
		}
		if(t < kSecondEnd){
			// 1回目の弾み
			float local = t - kSecondCenter;
			return kCoefficient * local * local + kSecondBase;
		}
		if(t < kThirdEnd){
			// 2回目の弾み
			float local = t - kThirdCenter;
			return kCoefficient * local * local + kThirdBase;
		}
		// 3回目の小さな弾み
		float local = t - kFourthCenter;
		return kCoefficient * local * local + kFourthBase;
	}
}
