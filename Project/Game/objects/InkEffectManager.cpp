#include "InkEffectManager.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include <cmath>

namespace{
	// しぶき・閃光に使うモデル
	const std::string kDropletModelPath = "Sphere/sphere.obj";
	// 的の破片に使うモデル(薄い三角柱)
	const std::string kShardModelPath = "InkShard/shard.obj";

	// プレイヤーのインクの色(黄色)
	constexpr Vector4 kInkColor = {1.0f, 0.85f, 0.1f, 1.0f};
	// 閃光の色(白)
	constexpr Vector4 kFlashColor = {1.0f, 1.0f, 0.9f, 1.0f};
	// 的の破片の色(的の外側から順に、緑・オレンジ・白・黒)
	constexpr Vector4 kShardColors[] = {
		{0.43f, 0.80f, 0.27f, 1.0f},
		{1.0f, 0.37f, 0.18f, 1.0f},
		{0.94f, 0.96f, 0.94f, 1.0f},
		{0.18f, 0.18f, 0.2f, 1.0f},
	};
	constexpr size_t kShardColorCount = sizeof(kShardColors) / sizeof(kShardColors[0]);

	// 上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};
}

InkEffectManager::InkEffectManager() = default;
InkEffectManager::~InkEffectManager() = default;

// 初期化処理
// 演出に使う3Dオブジェクトはここでまとめて生成し、以降は使い回す
void InkEffectManager::Initialize(Obj3dCommon* objCommon){
	ModelManager::GetInstance()->LoadModel(kDropletModelPath);
	ModelManager::GetInstance()->LoadModel(kShardModelPath);

	// 種類ごとに決まった数だけ生成する
	auto createPool = [objCommon](std::vector<Piece>& pool,size_t count,Kind kind,const std::string& modelPath){
		pool.resize(count);
		for(Piece& piece : pool){
			piece.obj = std::make_unique<Obj3D>();
			piece.obj->Initialize(objCommon);
			piece.obj->SetModel(modelPath);
			piece.kind = kind;
		}
	};
	createPool(droplets_,kDropletCount,Kind::Droplet,kDropletModelPath);
	createPool(shards_,kShardCount,Kind::Shard,kShardModelPath);
	createPool(flashes_,kFlashCount,Kind::Flash,kDropletModelPath);

	// 閃光は周りの明るさに関係なく光って見えるよう、ライティングを切っておく
	for(Piece& flash : flashes_){
		if(Model::Material* material = flash.obj->GetMaterial()){
			material->enableLighting = 0;
		}
	}

	std::random_device seedGenerator;
	randomEngine_.seed(seedGenerator());
}

// 出ている演出をすべて消す
void InkEffectManager::Reset(){
	for(std::vector<Piece>* pool : {&droplets_, &shards_, &flashes_}){
		for(Piece& piece : *pool){
			piece.isAlive = false;
		}
	}
}

// 使い回し用の配列から次に使う1つを取り出す
// 配列を順番に回して使うため、空きが無いときは一番古いものが上書きされる
InkEffectManager::Piece& InkEffectManager::Spawn(std::vector<Piece>& pool,size_t& nextIndex){
	Piece& piece = pool[nextIndex];
	nextIndex = (nextIndex + 1) % pool.size();

	piece.isAlive = true;
	piece.lifeTime = 0.0f;
	piece.rotate = {0.0f, 0.0f, 0.0f};
	piece.angularVelocity = {0.0f, 0.0f, 0.0f};
	return piece;
}

// 指定した範囲の乱数を返す
float InkEffectManager::RandomRange(float min,float max){
	std::uniform_real_distribution<float> distribution(min,max);
	return distribution(randomEngine_);
}

// 各軸が -1〜1 の乱数のベクトルを返す
Vector3 InkEffectManager::RandomVector(){
	return {RandomRange(-1.0f,1.0f), RandomRange(-1.0f,1.0f), RandomRange(-1.0f,1.0f)};
}

// 的を壊したときの演出
// 的の色の破片とインクのしぶきを周りへ飛び散らせ、中心に一瞬だけ閃光を出す
void InkEffectManager::EmitTargetBreak(const Vector3& position){
	for(int i = 0; i < kBreakShardCount; ++i){
		Piece& shard = Spawn(shards_,nextShard_);
		shard.position = position;
		shard.velocity = Normalize(RandomVector()) * RandomRange(kBreakShardSpeedMin,kBreakShardSpeedMax) + kWorldUp * kBreakShardUpSpeed;
		shard.angularVelocity = RandomVector() * kBreakShardSpinMax;
		shard.color = kShardColors[static_cast<size_t>(i) % kShardColorCount];
		shard.baseScale = RandomRange(kBreakShardScaleMin,kBreakShardScaleMax);
		shard.maxLifeTime = RandomRange(kBreakShardLifeMin,kBreakShardLifeMax);
	}

	for(int i = 0; i < kBreakDropletCount; ++i){
		Piece& droplet = Spawn(droplets_,nextDroplet_);
		droplet.position = position;
		droplet.velocity = Normalize(RandomVector()) * RandomRange(kBreakDropletSpeedMin,kBreakDropletSpeedMax) + kWorldUp * kBreakDropletUpSpeed;
		droplet.color = kInkColor;
		droplet.baseScale = RandomRange(kBreakDropletScaleMin,kBreakDropletScaleMax);
		droplet.maxLifeTime = RandomRange(kBreakDropletLifeMin,kBreakDropletLifeMax);
	}

	Piece& flash = Spawn(flashes_,nextFlash_);
	flash.position = position;
	flash.velocity = {0.0f, 0.0f, 0.0f};
	flash.color = kFlashColor;
	flash.baseScale = kFlashScale;
	flash.maxLifeTime = kFlashLife;
}

// 指定した向きへ飛び散るインクのしぶき
void InkEffectManager::EmitInkSplash(const Vector3& position,const Vector3& direction,int count){
	for(int i = 0; i < count; ++i){
		Piece& droplet = Spawn(droplets_,nextDroplet_);
		droplet.position = position;
		droplet.velocity = direction * RandomRange(kSplashSpeedMin,kSplashSpeedMax) + RandomVector() * kSplashSpread;
		droplet.color = kInkColor;
		droplet.baseScale = RandomRange(kSplashScaleMin,kSplashScaleMax);
		droplet.maxLifeTime = RandomRange(kSplashLifeMin,kSplashLifeMax);
	}
}

// レールを進んでいる間、足元から後ろへ飛ぶインクのしぶき
// 毎フレーム1粒ずつ出し続けることで、レールの上を滑っている勢いを見せる
void InkEffectManager::EmitRailTrail(const Vector3& position,const Vector3& backward){
	Piece& droplet = Spawn(droplets_,nextDroplet_);
	droplet.position = position;
	droplet.velocity = backward * kTrailBackSpeed + kWorldUp * kTrailUpSpeed + RandomVector() * kTrailSpread;
	droplet.color = kInkColor;
	droplet.baseScale = RandomRange(kTrailScaleMin,kTrailScaleMax);
	droplet.maxLifeTime = RandomRange(kTrailLifeMin,kTrailLifeMax);
}

// 移動・回転・縮小・寿命の更新
void InkEffectManager::Update(float deltaTime){
	for(std::vector<Piece>* pool : {&droplets_, &shards_, &flashes_}){
		for(Piece& piece : *pool){
			if(!piece.isAlive) continue;

			piece.lifeTime += deltaTime;
			if(piece.lifeTime >= piece.maxLifeTime){
				piece.isAlive = false;
				continue;
			}

			// 種類ごとの重力を速度に加えてから位置を進める
			if(piece.kind == Kind::Droplet){
				piece.velocity.y -= kDropletGravity * deltaTime;
			} else if(piece.kind == Kind::Shard){
				piece.velocity.y -= kShardGravity * deltaTime;
			}
			piece.position += piece.velocity * deltaTime;
			piece.rotate += piece.angularVelocity * deltaTime;

			UpdatePieceTransform(piece);
		}
	}
}

// 1つ分の行列と色を更新する
void InkEffectManager::UpdatePieceTransform(Piece& piece){
	// 寿命に対する経過の割合(0〜1)
	float lifeRate = piece.lifeTime / piece.maxLifeTime;
	Vector4 color = piece.color;
	Vector3 scale = {piece.baseScale, piece.baseScale, piece.baseScale};
	Vector3 rotate = piece.rotate;

	if(piece.kind == Kind::Flash){
		// 閃光は一気に広がりながら透明になって消える
		float expand = Lerp(1.0f,kFlashExpandScale,lifeRate);
		scale = scale * expand;
		color.w = 1.0f - lifeRate;
	} else{
		// しぶき・破片は寿命の後半で小さくなりながら消える
		if(lifeRate > kShrinkStartRate){
			float shrink = 1.0f - (lifeRate - kShrinkStartRate) / (1.0f - kShrinkStartRate);
			scale = scale * shrink;
		}

		// しぶきは飛んでいる向きに伸ばして、勢いのある液体に見せる
		float speed = Length(piece.velocity);
		if(piece.kind == Kind::Droplet && speed > kMinStretchSpeed){
			Vector3 direction = piece.velocity / speed;
			float horizontalLength = std::sqrt(direction.x * direction.x + direction.z * direction.z);
			// 球モデルのZ軸を速度の向きへ回す(Y軸回転で左右、X軸回転で上下を合わせる)
			rotate = {std::atan2(-direction.y,horizontalLength), std::atan2(direction.x,direction.z), 0.0f};
			float stretch = 1.0f + speed * kDropletStretchPerSpeed;
			if(stretch > kDropletMaxStretch){
				stretch = kDropletMaxStretch;
			}
			scale.z *= stretch;
		}
	}

	piece.obj->SetTranslate(piece.position);
	piece.obj->SetRotate(rotate);
	piece.obj->SetScale(scale);
	if(Model::Material* material = piece.obj->GetMaterial()){
		material->color = color;
	}
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		piece.obj->SetCamera(activeCamera);
	}
	piece.obj->Update();
}

// 出ている演出を描画する
void InkEffectManager::Draw(){
	for(std::vector<Piece>* pool : {&droplets_, &shards_, &flashes_}){
		for(Piece& piece : *pool){
			if(piece.isAlive){
				piece.obj->Draw();
			}
		}
	}
}
