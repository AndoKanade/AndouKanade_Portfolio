#include "Ground.h"
#include "CloudWave.h"
#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "Camera.h"
#include "GlobalVariables.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace{
	// 雲に使用するモデルのパス(継ぎ目なく並べられる起伏をつけた板に雲海のテクスチャを貼ったもの)
	const std::string kCloudModelPath = "CloudSea/cloudSurface.obj";
	// 霧に使用するモデルのパス(雲と同じ格子の板に、継ぎ目なく並べられるもやのテクスチャを貼ったもの)
	const std::string kFogModelPath = "CloudSea/fogSurface.obj";

	// 起伏に陰影をつけるため、雲はライティングを有効にする
	constexpr int32_t kEnableLighting = 1;
	// 霧はテクスチャの色そのままに見せたいため、ライティングを無効にする
	constexpr int32_t kDisableLighting = 0;

	// 進行方向に直角な横方向を求めるための上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};

	// 横方向の中央のタイルの添字を求めるための係数(個数-1の半分が中央になる)
	constexpr float kHalf = 0.5f;

	// UVのずらし量を0〜1の範囲に収めるための周期(テクスチャは1ごとに繰り返す)
	constexpr float kUvPeriod = 1.0f;
	// UVは2次元なので、UV変換行列の奥行き方向は拡大しない
	constexpr float kUvDepthScale = 1.0f;
	// 霧の模様は回転させない
	constexpr Vector3 kUvNoRotation = {0.0f, 0.0f, 0.0f};

	// 霧の層の色(白いもや。不透明度は層ごとに決める)
	constexpr Vector3 kFogColor = {1.0f, 1.0f, 1.0f};

	// 霧の層1枚分の設定
	struct FogLayerSetting{
		float heightOffset;     // 雲の表面から浮かせる高さ(雲と同じ形でうねるので、どこでも雲から同じだけ離れる)
		float alpha;            // 不透明度(テクスチャの透明度に掛け合わせる)
		float uvScale;          // テクスチャの繰り返し回数(タイルの境目で模様がつながるよう整数にする)
		Vector2 uvStartOffset;  // 模様の初期のずらし量(層ごとに模様が重ならないようにする)
		Vector2 scrollSpeed;    // 流れる速さ(1秒あたりにずらすUV量)
	};

	// 霧の層(下から順)
	// 下ほど濃く上ほど薄くして厚みのあるもやに見せ、層ごとに模様の細かさ・流れる向き・速さを変えて立体的に動かす
	// 4層を重ねた合計の濃さが高すぎると下の雲の起伏が見えなくなるため、各層の不透明度は低めにして雲が透けて見えるようにする
	constexpr std::array<FogLayerSetting,4> kFogLayers = {{
		{0.15f, 0.25f, 1.0f, {0.00f, 0.00f}, {0.03f, 0.05f}},
		{0.40f, 0.17f, 2.0f, {0.37f, 0.21f}, {-0.04f, 0.03f}},
		{0.70f, 0.11f, 1.0f, {0.62f, 0.84f}, {0.05f, -0.02f}},
		{1.05f, 0.06f, 2.0f, {0.15f, 0.53f}, {-0.02f, -0.04f}},
	}};
}

Ground::Ground() = default;
Ground::~Ground() = default;

// 雲と霧のタイルの生成
// 雲は起伏の高さを板の厚み方向の拡大率で調整し、霧の層は雲と同じ拡大率にして雲の形に沿わせる
void Ground::Initialize(Obj3dCommon* objCommon,const Vector3& startPosition,const Vector3& forward,const std::string& paramGroup){
	if(!objCommon){
		return;
	}

	// 霧の濃さの倍率を調整項目に登録する(第3引数はデフォルト値。保存済みJSONがあればそちらが優先される)
	// ImGuiの "Global Variables" ウィンドウから実行中に変更でき、次のUpdate()で全層の不透明度に反映される
	paramGroup_ = paramGroup;
	GlobalVariables::GetInstance()->AddItem(paramGroup_,"fogDensity",kDefaultFogDensity);

	ModelManager::GetInstance()->LoadModel(kCloudModelPath);
	ModelManager::GetInstance()->LoadModel(kFogModelPath);

	// 雲と霧の起伏を時間とともにうねらせる準備をする
	// 霧も雲と同じ形で動かし、雲の山が霧を突き抜けて境目がくっきり出ないようにする
	cloudWave_ = std::make_unique<CloudWave>();
	cloudWave_->Initialize({ModelManager::GetInstance()->FindModel(kCloudModelPath),ModelManager::GetInstance()->FindModel(kFogModelPath)});

	// 雲のタイルを生成する
	CreateTiles(objCommon,kCloudModelPath,startPosition,forward,kGroundHeight,kCloudWaveHeight,cloudTiles_);
	for(auto& tile : cloudTiles_){
		if(Model::Material* material = tile->GetMaterial()){
			// 起伏に光の当たり方の差を出して立体感をつける
			material->enableLighting = kEnableLighting;
			// 雲がつやつやして見えないよう、鏡面反射をほとんど出さない
			material->shininess = kCloudShininess;
		}
	}

	// 霧の層を下から順に生成する
	fogLayers_.resize(kFogLayers.size());
	for(size_t layerIndex = 0; layerIndex < kFogLayers.size(); ++layerIndex){
		const FogLayerSetting& setting = kFogLayers[layerIndex];
		FogLayer& layer = fogLayers_[layerIndex];
		layer.uvOffset = setting.uvStartOffset;

		CreateTiles(objCommon,kFogModelPath,startPosition,forward,kGroundHeight + setting.heightOffset,kCloudWaveHeight,layer.tiles);
		for(auto& tile : layer.tiles){
			if(Model::Material* material = tile->GetMaterial()){
				material->enableLighting = kDisableLighting;
				material->color = {kFogColor.x, kFogColor.y, kFogColor.z, setting.alpha};
				// ふちのぼかしを残すため、アルファテストで半透明部分を捨てないようにする
				material->alphaCutoff = kFogAlphaCutoff;
			}
		}
	}
}

// 板モデルのタイルを格子状に並べて生成する
// 板モデルはXY平面の板なのでX軸を-90度回して水平にし、
// 基準位置から進行方向(奥)と横方向へ並べることで簡易的な地面を表現する
void Ground::CreateTiles(Obj3dCommon* objCommon,const std::string& modelPath,const Vector3& startPosition,const Vector3& forward,
	float height,float thicknessScale,std::vector<std::unique_ptr<Obj3D>>& tiles){
	// 進行方向に直角な横方向を求める
	Vector3 right = Normalize(Cross(kWorldUp,forward));

	// タイル1枚の1辺の長さをそのまま並べる間隔にすると、隙間なく敷き詰められる
	const float tileStep = kTilePlaneSize * kTileScale;
	// 横方向は左右対称に並べるため、中央のタイルの添字を基準にずらす
	const float widthCenterIndex = static_cast<float>(kTileCountWidth - 1) * kHalf;

	// 生成数は固定なので、再確保が起きないよう先に確保しておく
	tiles.reserve(static_cast<size_t>(kTileCountBack + kTileCountForward) * static_cast<size_t>(kTileCountWidth));

	for(int depthIndex = -kTileCountBack; depthIndex < kTileCountForward; ++depthIndex){
		for(int widthIndex = 0; widthIndex < kTileCountWidth; ++widthIndex){
			Vector3 tilePosition = startPosition
				+ forward * (static_cast<float>(depthIndex) * tileStep)
				+ right * ((static_cast<float>(widthIndex) - widthCenterIndex) * tileStep);
			// 高さはレールの起伏に関係なく一定にして、常に足元より下に敷く
			tilePosition.y = height;

			auto tile = std::make_unique<Obj3D>();
			tile->Initialize(objCommon);
			tile->SetModel(modelPath);
			tile->SetTranslate(tilePosition);
			tile->SetRotate({kRotateX, 0.0f, 0.0f});
			// Z方向は板の厚み(起伏の高さ)に相当するため、横の広がりとは別の拡大率にする
			tile->SetScale({kTileScale, kTileScale, thicknessScale});

			tiles.push_back(std::move(tile));
		}
	}
}

// 更新処理
// 位置・向き・大きさは生成時に決めているため、カメラ切り替えへの追従と行列の更新、雲と霧のうねり、霧のUVスクロールだけ毎フレーム行う
void Ground::Update(float deltaTime){
	// 雲と霧の起伏をうねらせる(全タイルが同じモデルを使うので、モデルの頂点を1回書き換えれば全タイルに反映される)
	if(cloudWave_){
		cloudWave_->Update(deltaTime);
	}

	UpdateTiles(cloudTiles_);

	// 調整項目から霧の濃さの倍率を取得(ImGui編集/ホットリロードが即反映される)
	const float fogDensity = GlobalVariables::GetInstance()->GetFloatValue(paramGroup_,"fogDensity");

	for(size_t layerIndex = 0; layerIndex < fogLayers_.size(); ++layerIndex){
		const FogLayerSetting& setting = kFogLayers[layerIndex];
		FogLayer& layer = fogLayers_[layerIndex];

		// 霧のテクスチャを少しずつずらして流れているように見せる
		// ずらし量が大きくなり続けると精度が落ちるため、テクスチャの繰り返し周期で0〜1に戻す
		layer.uvOffset.x = std::fmod(layer.uvOffset.x + setting.scrollSpeed.x * deltaTime,kUvPeriod);
		layer.uvOffset.y = std::fmod(layer.uvOffset.y + setting.scrollSpeed.y * deltaTime,kUvPeriod);

		// 層の全タイルで同じ値にすることで、タイルの境目でも模様がつながる
		const Matrix4x4 uvTransform = MakeAffineMatrix(
			{setting.uvScale, setting.uvScale, kUvDepthScale},
			kUvNoRotation,
			{layer.uvOffset.x, layer.uvOffset.y, 0.0f});

		// 層ごとの不透明度に濃さの倍率を掛ける(下ほど濃く上ほど薄い関係は保ったまま、全体の濃さだけを変える)
		const float fogAlpha = std::clamp(setting.alpha * fogDensity,kFogAlphaMin,kFogAlphaMax);

		for(auto& tile : layer.tiles){
			if(Model::Material* material = tile->GetMaterial()){
				material->uvTransform = uvTransform;
				material->color.w = fogAlpha;
			}
		}

		UpdateTiles(layer.tiles);
	}
}

// タイルをアクティブなカメラに追従させて行列を更新する
void Ground::UpdateTiles(std::vector<std::unique_ptr<Obj3D>>& tiles){
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(!activeCamera){
		return;
	}

	for(auto& tile : tiles){
		tile->SetCamera(activeCamera);
		tile->Update();
	}
}

// 雲の描画処理
void Ground::Draw(){
	for(auto& tile : cloudTiles_){
		tile->Draw();
	}
}

// 霧の描画処理
// 上から見下ろすカメラでは下の層ほど奥になるため、下の層から順に描いて半透明を正しく重ねる
void Ground::DrawFog(){
	for(auto& layer : fogLayers_){
		for(auto& tile : layer.tiles){
			tile->Draw();
		}
	}
}
