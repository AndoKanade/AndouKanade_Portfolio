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
/// 地平線から真上へのグラデーション(夕焼け色→青空→星空の藍色)・太陽の円盤と光のにじみ・高いところほど見える星のまたたきを表現する。
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

	// 真上の空の色の初期値(星が映える深い藍色。シェーダー内の色はリニア値)
	static constexpr Vector3 kDefaultZenithColor = {0.008f, 0.015f, 0.07f};
	// 地平線と真上の間の空の色の初期値(青空の色)
	static constexpr Vector3 kDefaultMiddleColor = {0.12f, 0.25f, 0.65f};
	// 地平線付近の空の色の初期値(雲海の上の夕焼けのような暖色)
	static constexpr Vector3 kDefaultHorizonColor = {0.95f, 0.6f, 0.38f};
	// 星の明るさの初期値(0で星を消す)
	static constexpr float kDefaultStarIntensity = 1.0f;
};
