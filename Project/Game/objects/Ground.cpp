#include "Ground.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include <string>

namespace{
	// 地面に使用する板モデルのパス
	const std::string kGroundModelPath = "Plane/plane.obj";

	// 進行方向に直角な横方向を求めるための上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};

	// 横方向の中央のタイルの添字を求めるための係数(個数-1の半分が中央になる)
	constexpr float kHalf = 0.5f;
}

Ground::Ground() = default;
Ground::~Ground() = default;

// 地面タイルの生成
// plane.objはXY平面の板なのでX軸を-90度回して水平にし、
// レール開始地点を基準に進行方向(奥)と横方向へ格子状に並べることで簡易的な地面を表現する
void Ground::Initialize(Obj3dCommon* objCommon,const Vector3& startPosition,const Vector3& forward){
	if(!objCommon){
		return;
	}

	ModelManager::GetInstance()->LoadModel(kGroundModelPath);

	// 進行方向に直角な横方向を求める
	Vector3 right = Normalize(Cross(kWorldUp,forward));

	// タイル1枚の1辺の長さをそのまま並べる間隔にすると、隙間なく敷き詰められる
	const float tileStep = kTilePlaneSize * kTileScale;
	// 横方向は左右対称に並べるため、中央のタイルの添字を基準にずらす
	const float widthCenterIndex = static_cast<float>(kTileCountWidth - 1) * kHalf;

	// 生成数は固定なので、再確保が起きないよう先に確保しておく
	tiles_.reserve(static_cast<size_t>(kTileCountBack + kTileCountForward) * static_cast<size_t>(kTileCountWidth));

	for(int depthIndex = -kTileCountBack; depthIndex < kTileCountForward; ++depthIndex){
		for(int widthIndex = 0; widthIndex < kTileCountWidth; ++widthIndex){
			Vector3 tilePosition = startPosition
				+ forward * (static_cast<float>(depthIndex) * tileStep)
				+ right * ((static_cast<float>(widthIndex) - widthCenterIndex) * tileStep);
			// 高さはレールの起伏に関係なく一定にして、常に足元より下に敷く
			tilePosition.y = kGroundHeight;

			auto tile = std::make_unique<Obj3D>();
			tile->Initialize(objCommon);
			tile->SetModel(kGroundModelPath);
			tile->SetTranslate(tilePosition);
			tile->SetRotate({kRotateX, 0.0f, 0.0f});
			// Z方向は板の厚みに相当するため拡大しない
			tile->SetScale({kTileScale, kTileScale, 1.0f});

			tiles_.push_back(std::move(tile));
		}
	}
}

// 更新処理
// 位置・向き・大きさは生成時に決めているため、カメラ切り替えへの追従と行列の更新だけ毎フレーム行う
void Ground::Update(){
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(!activeCamera){
		return;
	}

	for(auto& tile : tiles_){
		tile->SetCamera(activeCamera);
		tile->Update();
	}
}

// 描画処理
void Ground::Draw(){
	for(auto& tile : tiles_){
		tile->Draw();
	}
}
