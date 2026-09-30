#include "StartCountdown.h"
#include "WinAPI.h"
#include <cstdio>

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

namespace{
	// 文字を画面中央に表示するための割合(画面サイズに対する位置、ウィンドウの基準点)
	constexpr float kScreenCenterRate = 0.5f;
	// 表示する文字列("3"〜"GO!")を入れるバッファの長さ
	constexpr size_t kTextBufferSize = 8;
}

// カウントダウンを最初から始める
void StartCountdown::Start(){
	timer_ = 0.0f;
	isActive_ = true;
}

// 非表示の状態に戻す
void StartCountdown::Stop(){
	timer_ = 0.0f;
	isActive_ = false;
}

// GO になったか
bool StartCountdown::IsCountFinished() const{
	return timer_ >= static_cast<float>(kCountStart) * kCountInterval;
}

// 時間を進めて表示を更新する
void StartCountdown::Update(float deltaTime){
	if(!isActive_){
		return;
	}

	timer_ += deltaTime;

	// GO の表示時間を過ぎたら非表示にする
	const float countEndTime = static_cast<float>(kCountStart) * kCountInterval;
	if(timer_ >= countEndTime + kGoDisplayTime){
		isActive_ = false;
		return;
	}

#ifdef USE_IMGUI
	// 経過時間から今表示する数字を求める(0〜1秒は3、1〜2秒は2 …)。カウントが終わったら GO を表示する
	char text[kTextBufferSize];
	if(IsCountFinished()){
		snprintf(text,sizeof(text),"GO!");
	} else{
		int count = kCountStart - static_cast<int>(timer_ / kCountInterval);
		snprintf(text,sizeof(text),"%d",count);
	}

	// 画面中央に、タイトルバーや背景の無いウィンドウで大きく表示する
	ImGui::SetNextWindowPos(
		{static_cast<float>(WinAPI::kClientWidth) * kScreenCenterRate, static_cast<float>(WinAPI::kClientHeight) * kScreenCenterRate},
		ImGuiCond_Always,
		{kScreenCenterRate, kScreenCenterRate});
	ImGui::Begin("Countdown",nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoInputs);
	ImGui::SetWindowFontScale(kFontScale);
	ImGui::Text("%s",text);
	ImGui::End();
#endif
}
