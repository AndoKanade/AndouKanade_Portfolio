#include "Enemy.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include <cmath>
#include <string>

namespace{
	// 雑魚敵の表示に使用するモデル(プレイヤーと区別できるよう別のモーションのモデルを使う)
	const std::string kEnemyModelPath = "human/sneakWalk.gltf";

	// 敵弾の表示に使用するモデル
	const std::string kEnemyBulletModelPath = "Sphere/sphere.obj";

	// 敵弾の色(自機の弾が青なので、区別できるよう赤にする)
	constexpr Vector4 kEnemyBulletColor = {1.0f, 0.2f, 0.2f, 1.0f};

	// 雑魚敵の体の色(自機と区別できるよう、弾と同系統の赤にする)
	constexpr Vector4 kEnemyColor = {1.0f, 0.3f, 0.3f, 1.0f};
}

Enemy::Enemy() = default;
Enemy::~Enemy() = default;

// 初期化処理
void Enemy::Initialize(Obj3dCommon* objCommon,const Vector3& basePosition,const Vector3& patrolDirection,int maxHp){
	// 描画用モデルの読み込みと3Dオブジェクトの生成
	ModelManager::GetInstance()->LoadModel(kEnemyModelPath);
	obj_ = std::make_unique<Obj3D>();
	obj_->Initialize(objCommon);
	obj_->SetModel(kEnemyModelPath);

	// 体の色は生成時に一度設定するだけでよいため、ここで赤にしておく
	if(Model::Material* material = obj_->GetMaterial()){
		material->color = kEnemyColor;
	}

	basePosition_ = basePosition;
	patrolDirection_ = Normalize(patrolDirection);

	// 体力の最大値を設定する(Reset()で現在の体力がこの値まで回復する)
	SetMaxHp(maxHp);

	// 敵弾の生成
	// 発射のたびに生成すると無駄な処理が毎フレーム発生するため、ここで最大数ぶんまとめて作り、以降は使い回す
	ModelManager::GetInstance()->LoadModel(kEnemyBulletModelPath);
	bulletModel_ = ModelManager::GetInstance()->FindModel(kEnemyBulletModelPath);
	bullets_.resize(kMaxBulletCount);
	for(auto& bullet : bullets_){
		bullet.obj = std::make_unique<Obj3D>();
		bullet.obj->Initialize(objCommon);
		bullet.obj->SetModel(kEnemyBulletModelPath);
		bullet.obj->SetScale({kBulletScale, kBulletScale, kBulletScale});

		// 弾の色は生成時に一度設定するだけでよいため、ここで赤にしておく
		if(Model::Material* material = bullet.obj->GetMaterial()){
			material->color = kEnemyBulletColor;
		}
	}

	Reset();
}

// 初期状態(生存・移動位置の中心)へ戻す
void Enemy::Reset(){
	position_ = basePosition_;
	patrolOffset_ = 0.0f;
	patrolSign_ = 1.0f;
	rotationY_ = 0.0f;
	isAlive_ = true;
	isDetectingPlayer_ = false;

	// 体力を最大値まで戻す
	hp_ = maxHp_;

	// 発射済みの弾をすべて未使用に戻し、発射間隔も初期化する
	for(auto& bullet : bullets_){
		bullet.isAlive = false;
		bullet.lifeTime = 0.0f;

		// 追尾情報も消して、再利用した弾が前回の設定を引きずらないようにする
		bullet.isHoming = false;
		bullet.speed = 0.0f;
	}

	// 攻撃の種類も先頭(単発)に戻してから、その種類の発射間隔で待ち時間を初期化する
	attackIndex_ = 0;
	shotTimer_ = GetCurrentShotInterval();
}

// 体力を減らす(撃破されたときtrueを返す)
bool Enemy::TakeDamage(int damage){
	// 撃破済みの敵はこれ以上体力が減らない
	if(!isAlive_){
		return false;
	}

	hp_ -= damage;
	if(hp_ <= 0){
		hp_ = 0;
		isAlive_ = false;
		return true;
	}

	return false;
}

// 往復移動の中心座標を設定する(現在の往復位置を保ったまま移動させる)
void Enemy::SetBasePosition(const Vector3& basePosition){
	basePosition_ = basePosition;
	position_ = basePosition_ + patrolDirection_ * patrolOffset_;
}

// 往復移動の方向を設定する(内部で正規化する)
void Enemy::SetPatrolDirection(const Vector3& patrolDirection){
	patrolDirection_ = Normalize(patrolDirection);
	position_ = basePosition_ + patrolDirection_ * patrolOffset_;
}

// 体力の最大値を設定する(現在の体力が最大値を超える場合は最大値に合わせる)
void Enemy::SetMaxHp(int maxHp){
	// 0以下だと生成直後に撃破された状態になってしまうため下限で制限する
	maxHp_ = (maxHp < kMinMaxHp)?kMinMaxHp:maxHp;
	if(hp_ > maxHp_){
		hp_ = maxHp_;
	}
}

// 更新処理
void Enemy::Update(const Vector3& playerPosition,float deltaTime){
	// 撃破済みでも発射済みの弾は飛び続けさせるため、弾の更新は本体より先に行う
	// 追尾弾の向き補正にプレイヤー座標が必要なため、そのまま渡す
	UpdateBullets(playerPosition,deltaTime);

	// 撃破済みのときは移動も向きの更新も行わない
	if(!isAlive_){
		return;
	}

	// 固定パターンの移動
	// 中心座標からのずれを進行向きに応じて増減させ、可動範囲の端に達したら折り返す
	patrolOffset_ += patrolSign_ * kMoveSpeed * deltaTime;
	if(patrolOffset_ > kPatrolRange){
		patrolOffset_ = kPatrolRange;
		patrolSign_ = -1.0f;
	} else if(patrolOffset_ < -kPatrolRange){
		patrolOffset_ = -kPatrolRange;
		patrolSign_ = 1.0f;
	}
	position_ = basePosition_ + patrolDirection_ * patrolOffset_;

	// プレイヤーの検知判定(検知範囲内かどうか)
	Vector3 toPlayer = playerPosition - position_;
	isDetectingPlayer_ = (Length(toPlayer) <= kDetectionRange);

	// 弾の発射処理
	// プレイヤーを検知している間だけ、一定間隔でプレイヤーへ向けて撃つ
	// 1回撃つごとに攻撃の種類を次へ進め、単発・3方向拡散・追尾弾を順番に使う
	if(isDetectingPlayer_){
		shotTimer_ -= deltaTime;
		if(shotTimer_ <= 0.0f){
			FireCurrentAttack(playerPosition);

			// 次の攻撃へ切り替え、その攻撃の発射間隔で待ち時間を設定する
			attackIndex_ = (attackIndex_ + 1) % kAttackOrderCount;
			shotTimer_ = GetCurrentShotInterval();
		}
	} else{
		// 非検知中は撃たない。次に検知した直後に即撃ちされないよう、待ち時間を戻しておく
		shotTimer_ = GetCurrentShotInterval();
	}

	// 向きの決定
	// 検知中はプレイヤーの方向、非検知中は移動している方向を向く
	Vector3 facingDirection = isDetectingPlayer_?toPlayer:(patrolDirection_ * patrolSign_);

	// XZ平面での向き(Y軸回転)を求める。真上・真下方向しか成分が無い場合は前フレームの向きを維持する
	if(std::fabs(facingDirection.x) > 1e-5f || std::fabs(facingDirection.z) > 1e-5f){
		rotationY_ = std::atan2(facingDirection.x,facingDirection.z);
	}

	// 描画用オブジェクトへ反映する
	if(obj_){
		obj_->SetTranslate(position_);
		obj_->SetRotate({0.0f, rotationY_, 0.0f});
		obj_->SetScale({kScale, kScale, kScale});

		// アクティブカメラが切り替わっても正しく描画されるよう毎フレーム同期
		if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
			obj_->SetCamera(activeCamera);
		}

		obj_->Update();
	}
}

// 描画処理(撃破済みのときは本体を描画しない。発射済みの弾は残っていれば描画する)
void Enemy::Draw(){
	if(isAlive_ && obj_){
		obj_->Draw();
	}

	// 発射中の弾を描画する
	for(auto& bullet : bullets_){
		if(bullet.isAlive && bullet.obj){
			bullet.obj->Draw();
		}
	}
}

// 弾の更新処理(移動・追尾の向き補正・寿命切れの判定・描画用トランスフォームの更新)
void Enemy::UpdateBullets(const Vector3& playerPosition,float deltaTime){
	// アクティブカメラは全弾で共通なので、ループの外で一度だけ取得する
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();

	for(auto& bullet : bullets_){
		// 未使用の弾は移動も描画更新も不要
		if(!bullet.isAlive){
			continue;
		}

		// 追尾弾の向き補正
		// 現在の進行方向をプレイヤー方向へ少しずつ寄せることで、急に折れ曲がらない緩やかな追尾にする
		if(bullet.isHoming){
			Vector3 toPlayer = playerPosition - bullet.position;
			if(Length(toPlayer) > 1e-5f){
				Vector3 currentDirection = Normalize(bullet.velocity);
				Vector3 targetDirection = Normalize(toPlayer);

				// 1フレームで補正する割合(1.0を超えると一気に向き切り替わってしまうため上限で制限する)
				float turnRatio = kHomingTurnRate * deltaTime;
				if(turnRatio > 1.0f){
					turnRatio = 1.0f;
				}

				// 速さは保ったまま向きだけを変える
				Vector3 newDirection = Normalize(currentDirection + (targetDirection - currentDirection) * turnRatio);
				bullet.velocity = newDirection * bullet.speed;
			}
		}

		// 等速直線運動で前進させる(プレイヤーが避けやすいよう重力は掛けない)
		bullet.position += bullet.velocity * deltaTime;

		// 何にも当たらなかった弾は一定時間で未使用に戻し、再利用できるようにする
		bullet.lifeTime += deltaTime;
		if(bullet.lifeTime >= kBulletLifeTime){
			bullet.isAlive = false;
			continue;
		}

		// 描画用オブジェクトへ反映する
		if(bullet.obj){
			bullet.obj->SetTranslate(bullet.position);
			if(activeCamera){
				bullet.obj->SetCamera(activeCamera);
			}
			bullet.obj->Update();
		}
	}
}

// 現在の攻撃の種類に応じた発射処理を行う
void Enemy::FireCurrentAttack(const Vector3& playerPosition){
	// 発射位置と、そこからプレイヤーへ向かう方向は全ての攻撃で共通なので先に求める
	Vector3 spawnPosition = GetBulletSpawnPosition();

	Vector3 toPlayer = playerPosition - spawnPosition;
	if(Length(toPlayer) < 1e-5f){
		return; // プレイヤーと発射位置がほぼ同じ場合は方向が定まらないため撃たない
	}
	Vector3 baseDirection = Normalize(toPlayer);

	switch(kAttackOrder[attackIndex_]){
	case AttackType::Spread3:
	{
		// プレイヤー方向を中心に、Y軸回転で左右へ角度をつけた弾を同時発射する
		// 中央の弾を基準(添字kSpreadBulletCount / 2)として、そこからのずれ分だけ角度をつける
		const int centerIndex = kSpreadBulletCount / 2;
		for(int i = 0; i < kSpreadBulletCount; ++i){
			float angle = static_cast<float>(i - centerIndex) * kSpreadAngle;

			// Y軸回転でXZ平面上の向きだけを回す(上下の角度は中央の弾と同じにする)
			float cosAngle = std::cos(angle);
			float sinAngle = std::sin(angle);
			Vector3 direction = {
				baseDirection.x * cosAngle + baseDirection.z * sinAngle,
				baseDirection.y,
				-baseDirection.x * sinAngle + baseDirection.z * cosAngle
			};

			FireBullet(spawnPosition,direction,kBulletSpeed,false);
		}
		break;
	}
	case AttackType::Homing:
		// 発射後もプレイヤーを追い続ける弾を1発だけ撃つ
		FireBullet(spawnPosition,baseDirection,kHomingBulletSpeed,true);
		break;

	case AttackType::Single:
	default:
		// 発射した瞬間のプレイヤー位置へ向かって直進する弾を1発撃つ
		FireBullet(spawnPosition,baseDirection,kBulletSpeed,false);
		break;
	}
}

// 現在の攻撃の種類に応じた発射間隔(秒)を取得する
float Enemy::GetCurrentShotInterval() const{
	switch(kAttackOrder[attackIndex_]){
	case AttackType::Spread3:
		return kSpreadShotInterval;
	case AttackType::Homing:
		return kHomingShotInterval;
	case AttackType::Single:
	default:
		return kShotInterval;
	}
}

// 弾の発射位置(敵の中心より少し上=胸の高さ)を取得する
Vector3 Enemy::GetBulletSpawnPosition() const{
	Vector3 spawnPosition = position_;
	spawnPosition.y += kBulletSpawnUpOffset;
	return spawnPosition;
}

// 指定した方向へ弾を1発発射する
void Enemy::FireBullet(const Vector3& spawnPosition,const Vector3& direction,float speed,bool isHoming){
	// 未使用の弾を探して使い回す(全弾使用中のときは発射しない)
	for(auto& bullet : bullets_){
		if(bullet.isAlive){
			continue;
		}

		bullet.position = spawnPosition;
		bullet.velocity = Normalize(direction) * speed;
		bullet.speed = speed;
		bullet.isHoming = isHoming;
		bullet.lifeTime = 0.0f;
		bullet.isAlive = true;
		return;
	}
}

// 発射済みの弾とプレイヤーの当たり判定
int Enemy::CheckHitToPlayer(const Model::BoundingSphere& playerSphere){
	int hitCount = 0;

	// 弾のモデルが無いときは境界球を求められないため判定しない
	if(!bulletModel_){
		return hitCount;
	}

	for(auto& bullet : bullets_){
		if(!bullet.isAlive){
			continue;
		}

		// 発射したばかりの弾は描画用オブジェクトの行列がまだ更新されていないため、
		// 弾の現在座標と表示スケールから境界球を求める(弾は球なので回転は考慮しなくてよい)
		Model::BoundingSphere bulletSphere = bulletModel_->GetBoundingSphere(bullet.position,kBulletScale);

		// 弾とプレイヤーの境界球が重なっていれば命中とみなし、その弾を未使用に戻す
		if(bulletSphere.IsHit(playerSphere)){
			bullet.isAlive = false;
			++hitCount;
		}
	}

	return hitCount;
}

// 当たり判定用に、モデルから求めたワールド座標系の境界球を取得
// 本体の行列はUpdate()の最後で更新されるため、Update()後に呼べば現在の位置・向き・大きさが反映される
Model::BoundingSphere Enemy::GetHitSphere() const{
	if(!obj_){
		return {position_, 0.0f};
	}
	return obj_->GetWorldBoundingSphere();
}

// 発射済みで生存している弾の数を取得(デバッグ表示用)
int Enemy::GetActiveBulletCount() const{
	int count = 0;
	for(const auto& bullet : bullets_){
		if(bullet.isAlive){
			++count;
		}
	}
	return count;
}