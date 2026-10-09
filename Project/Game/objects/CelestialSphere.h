#pragma once

#include "MyMath.h"
#include <d3d12.h>
#include <wrl.h>
#include <string>

// 前方宣言
class Camera;
class DXCommon;
struct DirectionalLight;

/// <summary>
/// 天球(ゲームの背景の空)
/// 画面全体を覆う三角形を一番奥に描き、ピクセルごとの視線の向きから空の色をシェーダーで計算する。
/// 地平線から真上へのグラデーション(霧で白んだ水色→少し濃い青緑)・太陽の方向の霞んだ光のにじみ・霧にかすむ遠景の建造物のシルエット・高いところほど見える星のまたたき(初期値では消している)を表現する。
/// 太陽の向きと色は3Dオブジェクト共通の平行光源(Sunが設定したもの)から取るため、光の当たり方と空の太陽の位置が一致する。
/// 空の色と星の明るさはGlobalVariablesに登録し、ImGuiの編集や外部ファイルの書き換えを毎フレーム反映する。
/// </summary>
class CelestialSphere{
public:
	/// <summary>
	/// 初期化処理(描画パイプラインと定数バッファの生成・調整項目の登録)
	/// </summary>
	/// <param name="dxCommon">DirectX共通処理</param>
	/// <param name="paramGroup">調整項目を登録するGlobalVariablesのグループ名</param>
	void Initialize(DXCommon* dxCommon,const std::string& paramGroup);

	/// <summary>
	/// 更新処理(経過時間・太陽・調整項目の最新値を定数バッファへ書き込む)
	/// </summary>
	/// <param name="deltaTime">1フレームの経過時間(秒)</param>
	void Update(float deltaTime);

	/// <summary>
	/// 描画処理(他のオブジェクトより先に描く)
	/// カメラが動いた直後の向きで空を描くため、カメラの行列は描画時に反映する
	/// </summary>
	/// <param name="camera">描画に使うカメラ</param>
	void Draw(const Camera& camera);

private:
	// シェーダーへ渡す描画パラメータ(CelestialSphere.hlsliのCelestialSphereParamと並びを合わせる)
	struct ParamForGPU{
		Matrix4x4 inverseViewProjection;
		Vector3 toSun;
		float time;
		Vector3 zenithColor;
		float starIntensity;
		Vector3 middleColor;
		float padding0;
		Vector3 horizonColor;
		float padding1;
		Vector3 sunColor;
		float padding2;
		Vector3 structureColor;
		float structureVisibility;
	};

	void CreateRootSignature();
	void CreateGraphicsPipelineState();

private:
	DXCommon* dxCommon_ = nullptr;

	// 描画パイプライン
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

	// 描画パラメータの定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> paramResource_;
	ParamForGPU* paramData_ = nullptr;

	// 太陽の向きと色を読み取る平行光源(3Dオブジェクト共通で使われているもの)
	DirectionalLight* light_ = nullptr;

	// 調整項目を登録しているグループ名
	std::string paramGroup_;

	// 経過時間(秒)
	float time_ = 0.0f;

	// 空の色の初期値は、スプラトゥーンの参考映像のような白っぽく霞んだ水色に合わせる(シェーダー内の色はリニア値)
	// 真上の空の色の初期値(少し濃い青緑)
	static constexpr Vector3 kDefaultZenithColor = {0.17f, 0.35f, 0.4f};
	// 地平線と真上の間の空の色の初期値(くすんだ水色)
	static constexpr Vector3 kDefaultMiddleColor = {0.28f, 0.51f, 0.56f};
	// 地平線付近の空の色の初期値(霧で白んだ明るい水色)
	static constexpr Vector3 kDefaultHorizonColor = {0.51f, 0.75f, 0.77f};
	// 遠景の建造物の太陽側の面の色の初期値(霧の中で白っぽく見える灰色)
	static constexpr Vector3 kDefaultStructureColor = {0.72f, 0.78f, 0.78f};
	// 遠景の建造物の見え具合の初期値(0で消す)
	static constexpr float kDefaultStructureVisibility = 1.0f;
	// 星の明るさの初期値(明るい空には星が合わないため0で消しておく。夜空にしたいときに上げる)
	static constexpr float kDefaultStarIntensity = 0.0f;
};
