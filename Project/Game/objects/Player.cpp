#include "Player.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "ParticleManager.h"
#include "GlobalVariables.h"
#include "Input.h"
#include "Editor/RailEditor.h"

namespace{
	// プレイヤーの表示に使用するモデル
	const std::string kPlayerModelPath = "human/walk.gltf";

	// 体の色(参考にしたスプラトゥーンの映像に合わせ、自分のインクと同じ黄色にする)
	constexpr Vector4 kPlayerColor = {1.0f, 0.85f, 0.15f, 1.0f};
}

Player::Player() = default;
Player::~Player() = default;

// 初期化処理
void Player::Initialize(Obj3dCommon* objCommon,const std::string& paramGroup){
	paramGroup_ = paramGroup;

	// 人型モデルの読み込みと生成
	ModelManager::GetInstance()->LoadModel(kPlayerModelPath);
	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kPlayerModelPath);

	// 体の色は生成時に一度設定するだけでよいため、ここで黄色にしておく
	if(Model::Material* material = obj_->GetMaterial()){
		material->color = kPlayerColor;
	}

	// 調整項目の登録(第3引数はデフォルト値。保存済みJSONがあればそちらが優先される)
	GlobalVariables::GetInstance()->AddItem(paramGroup_,"railSpeed",kDefaultRailSpeed);
}

// レール先頭・オンレール・体力満タンの初期状態に戻す
void Player::Reset(){
	railT_ = 0.0f;
	isRailFinished_ = false; // レール終端フラグもリセットする

	// オンレール/オフレールの状態もリセットし、必ずオンレールから開始する
	isOnRail_ = true;
	freeVelocityY_ = 0.0f;

	// 体力・無敵時間も初期状態に戻す
	hp_ = kMaxHp;
	invincibleTimer_ = 0.0f;
}

// レールの進行
// Edit中・開始演出中・オフレール中はその位置で静止させる
// 終端に到達したらループさせず、その場で停止させる
void Player::UpdateRailProgress(const RailEditor* railEditor,bool isGameplayActive,float deltaTime){
	if(!railEditor || !isGameplayActive || !isOnRail_ || isRailFinished_){
		return;
	}

	// 調整項目から最新の値を取得(ImGui編集/ホットリロードが即反映される)
	float railSpeed = GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,"railSpeed");

	// 現在位置に対応する制御点のSpeed値を取得し、全体速度(railSpeed)に掛けて今フレームに進む距離を求める
	// 進行度に直接足すと制御点間の距離で速さが変わるため、距離として進めて一定の速さにする
	float pointSpeed = railEditor->GetSpeedOnRail(railT_);
	railT_ = railEditor->AdvanceTByDistance(railT_,pointSpeed * railSpeed * deltaTime);
	if(railT_ >= kRailEndT){
		railT_ = kRailEndT; // 終端で固定する
		isRailFinished_ = true;
	}
}

// 基準位置・基準向きの計算
// オフレール中は、直前フレームで更新した自由移動座標(freePosition_)とジャンプ時に固定した向き(freeBaseRotation_)を基準にする
void Player::UpdateBasePose(const RailEditor* railEditor){
	if(!railEditor){
		return;
	}

	Vector3 railPos = railEditor->GetPositionOnRail(railT_);
	Vector3 railRot = railEditor->GetRotationOnRail(railT_);

	basePosition_ = isOnRail_?railPos:freePosition_;
	baseRotation_ = isOnRail_?railRot:freeBaseRotation_;
}

// 表示位置の計算とモデルの行列更新
// プレイヤーはレールの上(基準位置)に立たせ、体は照準の方向(カメラの左右の向き)へ向ける
// レールの傾きに合わせて体を倒すと照準とずれて見えるため、体は常に直立させる
void Player::UpdateTransform(const Vector3& cameraRotation,float hopHeight){
	position_ = basePosition_;

	if(!obj_){
		return;
	}

	// タイトル中の跳ねは見た目だけに反映し、カメラ・ロゴ・敵が基準にする position_ は動かさない
	obj_->SetTranslate({position_.x, position_.y + hopHeight, position_.z});
	obj_->SetRotate({0.0f, cameraRotation.y, 0.0f});
	obj_->SetScale({kScale, kScale, kScale}); // 小さめのスケールで表示
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		obj_->SetCamera(activeCamera);
	}
	obj_->Update();
}

// 被弾後の無敵時間を進める
void Player::UpdateInvincible(float deltaTime){
	if(invincibleTimer_ > 0.0f){
		invincibleTimer_ -= deltaTime;
		if(invincibleTimer_ < 0.0f){
			invincibleTimer_ = 0.0f;
		}
	}
}

// 敵弾が当たったときの処理
// 連続被弾で一瞬に体力が尽きないよう、被弾後は一定時間無敵にする
void Player::TakeDamage(){
	if(invincibleTimer_ > 0.0f || hp_ <= 0){
		return;
	}

	--hp_;
	invincibleTimer_ = kInvincibleTime;

	// 被弾位置に火花パーティクルを発生させ、当たったことを見た目で分かるようにする
	ParticleManager::GetInstance()->EmitSpark(position_);
}

// プレイヤーの自立(ジャンプ+WASD移動)
// オンレール中はジャンプ入力でレールを離れて自由移動状態に切り替え、
// オフレール中はWASDでの水平移動と重力・ジャンプ初速による垂直移動を行い、
// レール座標によるオンレール判定を使って着地先レールへ再度乗り移る
bool Player::UpdateMovement(Input* input,RailEditor* railEditor,const Vector3& cameraForward,const Vector3& cameraRight,float groundHeight,float deltaTime){
	if(!input || !railEditor){
		return false;
	}

	if(isOnRail_){
		// ジャンプキー(SPACE)でレールを離れ、自由移動状態に切り替える
		// タイトルのスタートもSPACEだが、押した瞬間はまだプレイ可能になっていないためジャンプしない
		if(input->TriggerKey(DIK_SPACE)){
			// A/Dを押しながらジャンプしたときは、その方向に並走しているレールがあれば直接乗り換える
			// (撃ちながらジャンプで飛び移るのは難しいため、簡単に乗り換えられる操作として用意する)
			Vector3 rightXZ = Normalize(Vector3{cameraRight.x, 0.0f, cameraRight.z});
			Vector3 sideDir = {0.0f, 0.0f, 0.0f};
			if(input->PushKey(DIK_D)) sideDir += rightXZ;
			if(input->PushKey(DIK_A)) sideDir += -rightXZ;

			// A/D同時押しで打ち消し合ったときは方向なしとして、通常のジャンプにする
			if(Length(sideDir) > 0.0f && TrySwitchToSideRail(railEditor,sideDir)){
				return false;
			}

			isOnRail_ = false;
			freePosition_ = basePosition_;
			freeVelocityY_ = kJumpSpeed;
			freeBaseRotation_ = baseRotation_; // 離脱時点の向きをオフレール中のカメラ基準向きとして固定する
		}
		return false;
	}

	// 移動前の高さを覚えておく(この後の着地判定でレールの高さを跨いだ瞬間を検出するのに使う)
	const float previousPositionY = freePosition_.y;

	// オフレール中はカメラ向き基準(XZ平面)でWASD移動する
	Vector3 forwardXZ = Normalize(Vector3{cameraForward.x, 0.0f, cameraForward.z});
	Vector3 rightXZ = Normalize(Vector3{cameraRight.x, 0.0f, cameraRight.z});

	Vector3 moveDir = {0.0f, 0.0f, 0.0f};
	if(input->PushKey(DIK_W)) moveDir += forwardXZ;
	if(input->PushKey(DIK_S)) moveDir += -forwardXZ;
	if(input->PushKey(DIK_D)) moveDir += rightXZ;
	if(input->PushKey(DIK_A)) moveDir += -rightXZ;
	moveDir = Normalize(moveDir);

	freePosition_ += moveDir * kMoveSpeed * deltaTime;

	// 重力を適用してY方向の速度を更新し、位置に反映する
	freeVelocityY_ -= kGravity * deltaTime;
	freePosition_.y += freeVelocityY_ * deltaTime;

	// 落下中のみ着地判定を行う(上昇中に離脱直後の位置へ即座に再着地しないようにする)
	if(freeVelocityY_ > 0.0f){
		return false;
	}

	RailEditor::NearestRailResult nearest = railEditor->FindNearestRail(freePosition_);
	// 距離1つ(球状の判定)だとレールの横や上にいるだけで乗ってしまうため、
	// 「水平方向でレールの真上にいる」ことと「落下でレールの高さを跨いだ」ことの両方を条件にする
	if(nearest.railIndex >= 0){
		// 水平方向(XZ平面)だけで見た、レール最近傍点までの距離
		Vector3 horizontalDiff = {freePosition_.x - nearest.position.x, 0.0f, freePosition_.z - nearest.position.z};
		bool isAboveRail = Length(horizontalDiff) <= kOnRailHorizontalThreshold;

		// 前フレームはレールより上にいて、今フレームでレールの高さ以下まで落ちた瞬間だけ着地とみなす
		bool crossedRailHeight = (previousPositionY > nearest.position.y) && (freePosition_.y <= nearest.position.y);

		if(isAboveRail && crossedRailHeight){
			railEditor->SwitchActiveRail(nearest.railIndex);
			railT_ = nearest.t;
			isRailFinished_ = false; // 着地先レールを最後まで進めるようにする
			isOnRail_ = true;
			freeVelocityY_ = 0.0f;
		}
	}

	// レールから落ちたときのリスタート
	// どのレールにも乗れないまま、地面の高さまで落ちたらリスタートが必要であることを呼び出し側へ伝える
	return !isOnRail_ && freePosition_.y <= groundHeight;
}

// 押した方向のすぐ横を並走しているレールへ直接乗り換える
// 今のレール以外で最も近いレールを探し、「近い」「押した方向の真横にある」「高さが近い」をすべて満たすときだけ乗り換える
bool Player::TrySwitchToSideRail(RailEditor* railEditor,const Vector3& sideDirXZ){
	RailEditor::NearestRailResult nearest = railEditor->FindNearestRail(basePosition_,railEditor->GetActiveRailIndex());
	if(nearest.railIndex < 0){
		return false;
	}

	// 水平方向(XZ平面)だけで見た、乗り換え先レールの最近傍点までの向きと距離
	Vector3 toRailXZ = {nearest.position.x - basePosition_.x, 0.0f, nearest.position.z - basePosition_.z};
	float horizontalDistance = Length(toRailXZ);
	if(horizontalDistance > kSideRailSwitchMaxDistance){
		return false;
	}

	// 押した方向の真横にあるか(前後にずれた位置のレールには乗り換えない)
	if(Dot(Normalize(toRailXZ),sideDirXZ) < kSideRailDirectionThreshold){
		return false;
	}

	// 高さが大きく違うレールには乗り換えない
	if(std::abs(nearest.position.y - basePosition_.y) > kSideRailSwitchMaxHeightDiff){
		return false;
	}

	// 乗り換え先レールの最近傍点からそのまま進行を続ける
	railEditor->SwitchActiveRail(nearest.railIndex);
	railT_ = nearest.t;
	isRailFinished_ = false; // 乗り換え先レールを最後まで進めるようにする
	return true;
}

// 描画処理
void Player::Draw(){
	if(obj_){
		obj_->Draw();
	}
}

// 当たり判定用に、モデルから求めたワールド座標系の境界球を取得
// 行列はUpdateTransform()で更新されるため、その後に呼べば現在の位置・向き・大きさが反映される
Model::BoundingSphere Player::GetHitSphere() const{
	if(!obj_){
		return {position_, 0.0f};
	}
	return obj_->GetWorldBoundingSphere();
}
