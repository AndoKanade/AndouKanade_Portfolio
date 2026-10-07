#pragma once

#include "MyMath.h"
#include "Title/TitleCameraDirector.h"
#include "Title/StartCountdown.h"
#include <memory>

// 前方宣言
class Input;
class Obj3dCommon;
class SpriteCommon;
class TitleLogo;
class TitleUI;

/// <summary>
/// ステージ開始までの流れ(タイトル → カメラの回り込み → カウントダウン → プレイ)を管理する
/// GameScene からは Update() を呼び、IsPlayable() でプレイ可能かを受け取るだけにする。
/// </summary>
class StartSequence{
public:
	// 開始演出の段階
	enum class Phase{
		None,       // 演出を行っていない(Editモード中)
		Title,      // タイトル表示中(スタート入力待ち)
		CameraTurn, // カメラがプレイヤーの背後へ回り込み中
		Countdown,  // 3, 2, 1 のカウントダウン中
		Playing,    // プレイ中
	};

	StartSequence();
	~StartSequence();

	// 初期化(タイトルロゴのモデルとタイトル表示用のスプライト生成)
	void Initialize(Obj3dCommon* object3dCommon,SpriteCommon* spriteCommon,Input* input);

	/// <summary>
	/// タイトルから始める(Playに入った瞬間に呼ぶ)
	/// </summary>
	/// <param name="skipTitle">trueならタイトル・カメラの回り込み・カウントダウンを飛ばして、すぐプレイ可能にする(デバッグ用)</param>
	void Start(bool skipTitle = false);

	// 演出を止めて何も行わない状態に戻す(Editモードに戻ったときに呼ぶ)
	void Stop();

	// 更新処理(入力の受付・段階の切り替え・表示の更新)
	void Update(float deltaTime);

	/// <summary>
	/// タイトルロゴをプレイヤーの位置・向きに合わせて配置する(IsControllingCamera() が true の間に使う)
	/// </summary>
	/// <param name="pivot">基準にするプレイヤーの座標</param>
	/// <param name="gameplayRotate">プレイ用カメラの向き</param>
	void UpdateLogo(const Vector3& pivot,const Vector3& gameplayRotate);

	// 3D描画処理(タイトルロゴのモデル)
	void Draw3D();

	// 2D描画処理(PRESS SPACE のスプライト)
	void Draw2D();

	// レール進行・操作・射撃などのゲーム処理を動かしてよいか
	bool IsPlayable() const{ return phase_ == Phase::Playing; }

	// カメラを演出側で動かしている最中か
	bool IsControllingCamera() const{ return phase_ == Phase::Title || phase_ == Phase::CameraTurn; }

	// タイトル中にプレイヤーをその場で跳ねさせる高さ(描画位置にだけ足す。跳ねていないときは0)
	float GetPlayerHopHeight() const;

	/// <summary>
	/// 演出中のカメラ座標・向きを求める(IsControllingCamera() が true の間に使う)
	/// </summary>
	/// <param name="pivot">回転の中心(プレイヤーの座標)</param>
	/// <param name="gameplayPosition">プレイ用カメラの座標</param>
	/// <param name="gameplayRotate">プレイ用カメラの向き</param>
	/// <param name="outPosition">演出中のカメラ座標</param>
	/// <param name="outRotate">演出中のカメラの向き</param>
	void CalculateCamera(const Vector3& pivot,const Vector3& gameplayPosition,const Vector3& gameplayRotate,Vector3& outPosition,Vector3& outRotate) const;

private:
	Input* input_ = nullptr;

	// 現在の段階
	Phase phase_ = Phase::None;

	// タイトル中・回り込み中のカメラワーク
	TitleCameraDirector cameraDirector_;
	// タイトルロゴ(3Dモデル)
	std::unique_ptr<TitleLogo> titleLogo_;
	// タイトル表示(PRESS SPACE)
	std::unique_ptr<TitleUI> titleUI_;
	// プレイ開始前のカウントダウン
	StartCountdown countdown_;

	// プレイヤーが跳ねる周期を数えるための経過時間(秒)
	float playerHopTimer_ = 0.0f;

	// タイトル表示の濃さ(表示中)
	static constexpr float kTitleVisible = 1.0f;
	// プレイヤーが跳ねる間隔(秒)。この周期の終わりに1回跳ねる
	static constexpr float kPlayerHopInterval = 3.0f;
	// 1回の跳ねで浮いている時間(秒)
	static constexpr float kPlayerHopDuration = 0.4f;
	// 跳ねたときの最高の高さ
	static constexpr float kPlayerHopHeight = 0.3f;
	// 円周率(跳ねの高さをsinの半周期で表すために使う)
	static constexpr float kPi = 3.14159265f;
	// タイトルBGMの音量の初期値(0〜1)
	static constexpr float kDefaultBgmVolume = 0.5f;
	// 決定音の音量の初期値(0〜1)
	static constexpr float kDefaultSeVolume = 1.0f;
};
