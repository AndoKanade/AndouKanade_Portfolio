#include "ComboCounter.h"

// 連鎖数・最大連鎖数を0に戻す
void ComboCounter::Reset(){
	combo_ = 0;
	maxCombo_ = 0;
	remainingTime_ = 0.0f;
	popTime_ = 0.0f;
}

// 的を1つ壊したときの処理
void ComboCounter::AddHit(){
	++combo_;
	if(combo_ > maxCombo_){
		maxCombo_ = combo_;
	}
	remainingTime_ = kComboWindow;
	popTime_ = kPopDuration;
}

// 途切れるまでの時間と、弾む表示の時間を進める
void ComboCounter::Update(float deltaTime){
	if(popTime_ > 0.0f){
		popTime_ -= deltaTime;
		if(popTime_ < 0.0f){
			popTime_ = 0.0f;
		}
	}

	if(remainingTime_ > 0.0f){
		remainingTime_ -= deltaTime;
		// 時間内に次の的を壊せなかったら連鎖を途切れさせる
		if(remainingTime_ <= 0.0f){
			remainingTime_ = 0.0f;
			combo_ = 0;
		}
	}
}
