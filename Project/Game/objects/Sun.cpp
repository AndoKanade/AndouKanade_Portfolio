#include "Sun.h"
#include "ModelManager.h"
#include "ModelCommon.h"
#include "GlobalVariables.h"
#include <cmath>

namespace{
	// 度からラジアンへ変換する係数
	constexpr float kDegreeToRadian = 3.14159265f / 180.0f;

	// 方角の基準を水平に求めるための上方向
	constexpr Vector3 kWorldUp = {0.0f, 1.0f, 0.0f};

	// 光の色の不透明度(平行光源の色はRGBだけ使うため常に1)
	constexpr float kLightAlpha = 1.0f;

	// 調整項目のキー
	const char* kElevationKey = "sunElevationDegree";
	const char* kAzimuthKey = "sunAzimuthDegree";
	const char* kIntensityKey = "sunIntensity";
	const char* kColorKey = "sunColor";
}

// 初期化処理
void Sun::Initialize(const std::string& paramGroup,const Vector3& referenceForward){
	paramGroup_ = paramGroup;

	// 3Dオブジェクトの描画で使われている平行光源を取得する
	if(ModelCommon* modelCommon = ModelManager::GetInstance()->GetModelCommon()){
		light_ = modelCommon->GetLightData();
	}

	// レールが上り下りしていても太陽の高さが変わらないよう、基準の向きは水平にする
	referenceForward_ = Normalize(Vector3{referenceForward.x, 0.0f, referenceForward.z});
	referenceRight_ = Normalize(Cross(kWorldUp,referenceForward_));

	// 第3引数はデフォルト値。保存済みJSONがあればそちらが優先される(AddItemは未登録キーのみ追加)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->AddItem(paramGroup_,kElevationKey,kDefaultElevationDegree); // 太陽の高さ(地平線からの角度・度)
	gv->AddItem(paramGroup_,kAzimuthKey,kDefaultAzimuthDegree);     // 太陽の方角(進行方向から右回りの角度・度)
	gv->AddItem(paramGroup_,kIntensityKey,kDefaultIntensity);       // 光の明るさ
	gv->AddItem(paramGroup_,kColorKey,kDefaultColor);               // 光の色(RGB)

	Update();
}

// 更新処理
// 調整項目から最新の値を取得し、ImGui編集・ホットリロードを即反映する
void Sun::Update(){
	if(!light_){
		return;
	}

	GlobalVariables* gv = GlobalVariables::GetInstance();
	const float elevation = gv->GetFloatValue(paramGroup_,kElevationKey) * kDegreeToRadian;
	const float azimuth = gv->GetFloatValue(paramGroup_,kAzimuthKey) * kDegreeToRadian;
	const float intensity = gv->GetFloatValue(paramGroup_,kIntensityKey);
	const Vector3 color = gv->GetVector3Value(paramGroup_,kColorKey);

	// 地面から見た太陽の方向(水平成分は基準の前方向から右回りに方角の分だけ回す)
	const Vector3 horizontal = referenceForward_ * std::cos(azimuth) + referenceRight_ * std::sin(azimuth);
	const Vector3 toSun = horizontal * std::cos(elevation) + kWorldUp * std::sin(elevation);

	// 平行光源の向きは光が進む向きなので、太陽の方向の逆にする
	light_->direction = Normalize(-toSun);
	light_->color = {color.x, color.y, color.z, kLightAlpha};
	light_->intensity = intensity;
}
