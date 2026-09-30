#pragma once

/// <summary>
/// プレイ開始前のカウントダウン(3, 2, 1, GO)
/// 専用素材が無いため、今は ImGui で画面中央に文字を表示する。
/// </summary>
class StartCountdown{
public:
	// カウントダウンを最初(3)から始める
	void Start();

	// 非表示の状態に戻す
	void Stop();

	// 時間を進めて表示を更新する(GO の表示が終わるまで呼び続ける)
	void Update(float deltaTime);

	// GO になったか(プレイ開始の合図)
	bool IsCountFinished() const;

private:
	// カウントを始める数字
	static constexpr int kCountStart = 3;
	// 数字1つあたりの表示時間(秒)
	static constexpr float kCountInterval = 1.0f;
	// GO を表示し続ける時間(秒)
	static constexpr float kGoDisplayTime = 0.8f;
	// 文字の拡大率
	static constexpr float kFontScale = 6.0f;

	// カウント開始からの経過時間(秒)
	float timer_ = 0.0f;
	// カウントダウン中(GO の表示中も含む)かどうか
	bool isActive_ = false;
};
