#pragma once

/// <summary>
/// 的を続けて壊したときの連鎖数
/// 壊すたびに連鎖数が1増え、一定時間次の的を壊さないと0に戻る。
/// 先に進む条件にはせず、狙って撃つ理由づけと気持ちよさのために使う。
/// </summary>
class ComboCounter{
public:
	// 連鎖数・最大連鎖数を0に戻す
	void Reset();

	// 的を1つ壊したときに呼ぶ(連鎖数を増やし、途切れるまでの時間を最初からにする)
	void AddHit();

	// 途切れるまでの時間と、増えたときの弾む表示の時間を進める
	void Update(float deltaTime);

	// 今の連鎖数を取得(0なら途切れている)
	int GetCombo() const{ return combo_; }
	// このプレイで一番長く続いた連鎖数を取得
	int GetMaxCombo() const{ return maxCombo_; }
	// 途切れるまでの残り時間の割合を取得(1: 壊した直後、0: 途切れた)
	float GetRemainingRate() const{ return remainingTime_ / kComboWindow; }
	// 増えた直後の弾む表示の割合を取得(1: 増えた直後、0: 弾み終わり)
	float GetPopRate() const{ return popTime_ / kPopDuration; }

private:
	// 今の連鎖数
	int combo_ = 0;
	// このプレイで一番長く続いた連鎖数
	int maxCombo_ = 0;
	// 連鎖が途切れるまでの残り時間(秒)
	float remainingTime_ = 0.0f;
	// 増えたときの弾む表示の残り時間(秒)
	float popTime_ = 0.0f;

	// 次の的を壊すまでに連鎖が途切れない時間(秒)
	static constexpr float kComboWindow = 2.0f;
	// 増えたときに表示を弾ませる時間(秒)
	static constexpr float kPopDuration = 0.15f;
};
