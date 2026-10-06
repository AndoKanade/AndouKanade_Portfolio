#pragma once

#include "MyMath.h"
#include "Title/LogoHidePattern.h"
#include <memory>

// 前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// タイトルロゴ(立体文字のモデル resource/Title/title.obj)
/// タイトル中のカメラから見て、プレイヤーの奥の上空に置く。
/// プレイ用カメラよりさらに後ろに置くため、カメラが背後へ回り込み終わると自然に画面外へ外れる。
/// スタート後はカメラの回り込みの邪魔にならないよう消す。
/// 消え方(LogoHideType)・置く位置と大きさは GlobalVariables(グループ "Title")で調整できる。
/// </summary>
class TitleLogo{
public:
	TitleLogo();
	~TitleLogo();

	// モデルの読み込みと調整項目の登録
	void Initialize(Obj3dCommon* object3dCommon);

	/// <summary>
	/// プレイヤーの位置と向きを基準にロゴを配置する
	/// </summary>
	/// <param name="pivot">基準にするプレイヤーの座標</param>
	/// <param name="gameplayRotate">プレイ用カメラの向き(ヨーだけを使う)</param>
	void Update(const Vector3& pivot,const Vector3& gameplayRotate);

	// 描画処理(消え終わった後は描画しない)
	void Draw();

	// 表示状態に戻す(タイトル開始時に呼ぶ)
	void Reset();

	// 登場演出(上から落ちてきて弾む)を進める
	void UpdateAppear(float deltaTime);

	// 登場演出が終わったか
	bool IsAppearFinished() const;

	// 待機中の揺れ(上下の浮き沈みと左右の首振り)を進める。着地前は進めない
	void UpdateIdle(float deltaTime);

	// 消える演出を始める(スタート入力時に呼ぶ)
	void BeginHide();

	// 消える演出を進める
	void UpdateHide(float deltaTime);

private:
	// 消える演出の進行度(0で通常表示、1で消え終わり)
	float GetHideProgress() const;

	// 登場演出の進行度(0で落ち始め、1で着地して止まった状態)
	float GetAppearProgress() const;

	std::unique_ptr<Obj3D> obj_;

	// 登場演出の経過時間(秒、落ち始めるまでの待ち時間も含む)
	float appearTimer_ = 0.0f;
	// 着地してからの待機中の揺れの経過時間(秒)
	float idleTimer_ = 0.0f;

	// 消える演出の経過時間(秒)
	float hideTimer_ = 0.0f;
	// 消える演出中かどうか
	bool isHiding_ = false;
	// 消える演出中の消え方。演出の途中で設定が変わっても崩れないよう、開始時に設定値から取り込む
	LogoHideType hideType_ = kDefaultHideType;
	// 消え終わるまでの時間(秒)。消え方ごとに決まっている値を開始時に取り込む
	float hideDuration_ = GetLogoHideDuration(kDefaultHideType);

	// モデルの文字列の中心(読み込み後のモデル座標)
	// title.obj は文字の左下が原点で、読み込み時の左手系変換でX方向が反転するため、文字は原点から -X 側へ伸びる
	// X: 文字の横幅 約7.04 の半分、Y: 文字の高さ 約1.14 の中心
	static constexpr float kModelCenterX = -3.545f;
	static constexpr float kModelCenterY = 0.467f;

	// 置く位置の初期値(プレイヤーから見て x: 右、y: 上、z: 後ろ への距離)
	// プレイ用カメラ(プレイヤーの約6後ろ)より後ろに置き、回り込み後に映らないようにする
	static constexpr Vector3 kDefaultOffset = {0.0f, 4.0f, 10.0f};
	// 表示スケールの初期値
	static constexpr float kDefaultScale = 1.5f;
	// 消え方の初期値
	static constexpr LogoHideType kDefaultHideType = LogoHideType::JumpAway;

	// 登場演出で落ち始めるまでの待ち時間(秒)。画面のフェードインで景色が見え始めてから落とす
	static constexpr float kAppearDelay = 0.7f;
	// 落ち始めてから弾み終わるまでの時間(秒)
	static constexpr float kAppearDuration = 1.2f;
	// 落ち始める高さ(置く位置からの上方向の距離)
	static constexpr float kAppearDropHeight = 6.0f;

	// 待機中に上下へ浮き沈みする幅(置く位置からの最大のずれ)
	static constexpr float kIdleBobHeight = 0.15f;
	// 上下の浮き沈みの速さ(1秒あたりの位相の進み、ラジアン)
	static constexpr float kIdleBobSpeed = 2.0f;
	// 待機中に左右へ首を振る最大の角度(ラジアン)
	static constexpr float kIdleSwayAngle = 0.06f;
	// 首振りの速さ(浮き沈みと周期をずらし、動きが単調にならないようにする)
	static constexpr float kIdleSwaySpeed = 1.3f;
};
