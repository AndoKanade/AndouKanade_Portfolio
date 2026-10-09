#include "CelestialSphere.h"
#include "Camera.h"
#include "DXCommon.h"
#include "ModelManager.h"
#include "ModelCommon.h"
#include "GlobalVariables.h"
#include <cassert>

namespace{
	// 全画面を覆う三角形の頂点数
	constexpr UINT kFullScreenVertexCount = 3;

	// 調整項目のキー
	const char* kZenithColorKey = "skyZenithColor";
	const char* kMiddleColorKey = "skyMiddleColor";
	const char* kHorizonColorKey = "skyHorizonColor";
	const char* kStarIntensityKey = "skyStarIntensity";
	const char* kStructureColorKey = "skyStructureColor";
	const char* kStructureVisibilityKey = "skyStructureVisibility";
}

// 初期化処理
void CelestialSphere::Initialize(DXCommon* dxCommon,const std::string& paramGroup){
	dxCommon_ = dxCommon;
	paramGroup_ = paramGroup;

	CreateRootSignature();
	CreateGraphicsPipelineState();

	// 描画パラメータの定数バッファを生成し、書き込み先を保持する
	paramResource_ = dxCommon_->CreateBufferResource(sizeof(ParamForGPU));
	paramResource_->Map(0,nullptr,reinterpret_cast<void**>(&paramData_));
	*paramData_ = {};

	// 太陽の向きと色はSunが設定した平行光源から読み取る
	if(ModelCommon* modelCommon = ModelManager::GetInstance()->GetModelCommon()){
		light_ = modelCommon->GetLightData();
	}

	// 第3引数はデフォルト値。保存済みJSONがあればそちらが優先される(AddItemは未登録キーのみ追加)
	GlobalVariables* gv = GlobalVariables::GetInstance();
	gv->AddItem(paramGroup_,kZenithColorKey,kDefaultZenithColor);     // 真上の空の色(RGB)
	gv->AddItem(paramGroup_,kMiddleColorKey,kDefaultMiddleColor);     // 地平線と真上の間の空の色(RGB)
	gv->AddItem(paramGroup_,kHorizonColorKey,kDefaultHorizonColor);   // 地平線付近の空の色(RGB)
	gv->AddItem(paramGroup_,kStarIntensityKey,kDefaultStarIntensity); // 星の明るさ
	gv->AddItem(paramGroup_,kStructureColorKey,kDefaultStructureColor);           // 遠景の建造物の色(RGB)
	gv->AddItem(paramGroup_,kStructureVisibilityKey,kDefaultStructureVisibility); // 遠景の建造物の見え具合
}

// 更新処理
void CelestialSphere::Update(float deltaTime){
	time_ += deltaTime;

	// 平行光源の向きは光が進む向きなので、逆にして太陽の方向にする
	if(light_){
		paramData_->toSun = Normalize(-light_->direction);
		paramData_->sunColor = {light_->color.x, light_->color.y, light_->color.z};
	}
	paramData_->time = time_;

	// 調整項目から最新の値を取得し、ImGui編集・ホットリロードを即反映する
	GlobalVariables* gv = GlobalVariables::GetInstance();
	paramData_->zenithColor = gv->GetVector3Value(paramGroup_,kZenithColorKey);
	paramData_->middleColor = gv->GetVector3Value(paramGroup_,kMiddleColorKey);
	paramData_->horizonColor = gv->GetVector3Value(paramGroup_,kHorizonColorKey);
	paramData_->starIntensity = gv->GetFloatValue(paramGroup_,kStarIntensityKey);
	paramData_->structureColor = gv->GetVector3Value(paramGroup_,kStructureColorKey);
	paramData_->structureVisibility = gv->GetFloatValue(paramGroup_,kStructureVisibilityKey);
}

// 描画処理
void CelestialSphere::Draw(const Camera& camera){
	// 空はカメラがどこにいても同じに見えるよう、ビュー行列の平行移動を消してから逆行列を求める
	Matrix4x4 viewMatrix = camera.GetViewMatrix();
	viewMatrix.m[3][0] = 0.0f;
	viewMatrix.m[3][1] = 0.0f;
	viewMatrix.m[3][2] = 0.0f;
	paramData_->inverseViewProjection = Inverse(Multiply(viewMatrix,camera.GetProjectionMatrix()));

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(graphicsPipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootConstantBufferView(0,paramResource_->GetGPUVirtualAddress());

	// 頂点バッファは使わず、頂点シェーダーが頂点番号から全画面の三角形を作る
	commandList->DrawInstanced(kFullScreenVertexCount,1,0,0);
}

// ルートシグネチャの生成(描画パラメータの定数バッファ1つだけ)
void CelestialSphere::CreateRootSignature(){
	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.pParameters = rootParameters;
	desc.NumParameters = _countof(rootParameters);

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
	assert(SUCCEEDED(hr));
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

// 描画パイプラインの生成
void CelestialSphere::CreateGraphicsPipelineState(){
	auto vs = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Skybox/CelestialSphere.VS.hlsl",L"vs_6_0");
	auto ps = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Skybox/CelestialSphere.PS.hlsl",L"ps_6_0");

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	// 頂点バッファを使わないので入力レイアウトは空にする
	psoDesc.InputLayout.pInputElementDescs = nullptr;
	psoDesc.InputLayout.NumElements = 0;
	psoDesc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	psoDesc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	// 一番奥に描くので深度テストは通し、深度も書き込まない(後から描くオブジェクトが必ず手前に出る)
	psoDesc.DepthStencilState.DepthEnable = true;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc,IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}
