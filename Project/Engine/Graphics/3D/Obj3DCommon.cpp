#include "Obj3dCommon.h"
#include "Logger.h"
#include <cassert>
#include <ModelManager.h>

using namespace Microsoft::WRL;

// 初期化処理
void Obj3dCommon::Initialize(DXCommon* dxcommon){
	// DXCommonの存在確認
	assert(dxcommon);
	dxCommon_ = dxcommon;

	// パイプラインステートの生成
	CreateGraphicsPipelineState();
	CreateComputePipelineState();

	// カメラリソース作成 (256バイトアライメント)
	size_t cameraSize = (sizeof(CameraForGPU) + 0xff) & ~0xff;
	cameraResource_ = dxCommon_->CreateBufferResource(cameraSize);
	cameraResource_->Map(0,nullptr,reinterpret_cast<void**>(&cameraData_));
	cameraData_->worldPosition = {0.0f, 0.0f, 0.0f};

	// 平行光源リソース作成 (256バイトアライメント)
	size_t directionalSize = (sizeof(DirectionalLight) + 0xff) & ~0xff;
	directionalLightResource_ = dxCommon_->CreateBufferResource(directionalSize);
	directionalLightResource_->Map(0,nullptr,reinterpret_cast<void**>(&directionalLightData_));
	directionalLightData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
	directionalLightData_->direction = {0.0f, -1.0f, 0.0f};
	directionalLightData_->intensity = 1.0f;

	// 点光源リソース作成 (256バイトアライメント)
	size_t pointSize = (sizeof(PointLight) + 0xff) & ~0xff;
	pointLightResource_ = dxCommon_->CreateBufferResource(pointSize);
	pointLightResource_->Map(0,nullptr,reinterpret_cast<void**>(&pointLightData_));
	pointLightData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
	pointLightData_->position = {0.0f, 2.0f, 0.0f};
	pointLightData_->intensity = 0.0f;
	pointLightData_->radius = 10.0f;
	pointLightData_->decay = 2.0f;

	// スポットライトリソース作成 (256バイトアライメント)
	size_t spotSize = (sizeof(SpotLight) + 0xff) & ~0xff;
	spotLightResource_ = dxCommon_->CreateBufferResource(spotSize);
	spotLightResource_->Map(0,nullptr,reinterpret_cast<void**>(&spotLightData_));
	spotLightData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
	spotLightData_->position = {0.0f, 5.0f, 0.0f};
	spotLightData_->direction = {0.0f, -1.0f, 0.0f};
	spotLightData_->intensity = 0.0f;
	spotLightData_->distance = 20.0f;
	spotLightData_->cosAngle = cosf(30.0f * 3.141592f / 180.0f);
	spotLightData_->cosFalloffStart = cosf(20.0f * 3.141592f / 180.0f);
	spotLightData_->decay = 2.0f;
}

// 描画設定処理
void Obj3dCommon::Draw(){
	auto commandList = dxCommon_->GetCommandList();

	commandList->SetGraphicsRootSignature(rootSignature.Get());
	commandList->SetPipelineState(graphicsPipelineState.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 平行光源設定
	auto lightRes = ModelManager::GetInstance()->GetModelCommon()->GetLightResource();
	commandList->SetGraphicsRootConstantBufferView(4,lightRes->GetGPUVirtualAddress());

	// カメラ設定
	if(cameraResource_){
		commandList->SetGraphicsRootConstantBufferView(5,cameraResource_->GetGPUVirtualAddress());
	}

	// 点光源設定
	if(pointLightResource_){
		commandList->SetGraphicsRootConstantBufferView(6,pointLightResource_->GetGPUVirtualAddress());
	}

	// スポットライト設定
	if(spotLightResource_){
		commandList->SetGraphicsRootConstantBufferView(7,spotLightResource_->GetGPUVirtualAddress());
	}
}

// ルートシグネチャの生成
void Obj3dCommon::CreateRootSignature(){
	HRESULT hr;

	// テクスチャ用デスクリプタレンジ
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 環境マップ用デスクリプタレンジ
	D3D12_DESCRIPTOR_RANGE descriptorRangeEnv[1] = {};
	descriptorRangeEnv[0].BaseShaderRegister = 1;
	descriptorRangeEnv[0].NumDescriptors = 1;
	descriptorRangeEnv[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeEnv[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータの設定
	D3D12_ROOT_PARAMETER rootParameters[8] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRangeEnv;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeEnv);

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Descriptor.ShaderRegister = 3;

	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].Descriptor.ShaderRegister = 4;

	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[7].Descriptor.ShaderRegister = 5;

	// サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
	if(FAILED(hr)){
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));
}

// スキニング用ルートシグネチャの生成
void Obj3dCommon::CreateSkinningRootSignature(){
	HRESULT hr;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE descriptorRangeEnv[1] = {};
	descriptorRangeEnv[0].BaseShaderRegister = 1;
	descriptorRangeEnv[0].NumDescriptors = 1;
	descriptorRangeEnv[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeEnv[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[9] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 1;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRangeEnv;
	rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeEnv);

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Descriptor.ShaderRegister = 3;

	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].Descriptor.ShaderRegister = 4;

	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[7].Descriptor.ShaderRegister = 5;

	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[8].Descriptor.ShaderRegister = 0;
	rootParameters[8].Descriptor.RegisterSpace = 0;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
	if(FAILED(hr)){
		Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&skinningRootSignature));
	assert(SUCCEEDED(hr));
}

// パイプラインステートの生成
void Obj3dCommon::CreateGraphicsPipelineState(){
	HRESULT hr;
	CreateRootSignature();
	CreateSkinningRootSignature();

	ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Obj3D/Object3d.VS.hlsl",L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Obj3D/Object3d.PS.hlsl",L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// マテリアルのα値で半透明に描けるよう、通常のアルファブレンド(描く色×α + 背景×(1-α))を有効にする
	// α値が1のモデルは従来どおり上書きと同じ見た目になる
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	// α値そのものは描く側の値をそのまま書き込む
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = {vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize()};
	graphicsPipelineStateDesc.PS = {pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize()};
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	ComPtr<IDxcBlob> skinningVertexShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Obj3D/SkinningObject3d.VS.hlsl",L"vs_6_0");
	assert(skinningVertexShaderBlob != nullptr);

	std::array<D3D12_INPUT_ELEMENT_DESC,5> skinningInputElementDescs{};
	skinningInputElementDescs[0] = {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
	skinningInputElementDescs[1] = {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
	skinningInputElementDescs[2] = {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
	skinningInputElementDescs[3].SemanticName = "WEIGHT";
	skinningInputElementDescs[3].SemanticIndex = 0;
	skinningInputElementDescs[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	skinningInputElementDescs[3].InputSlot = 0;
	skinningInputElementDescs[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	skinningInputElementDescs[4].SemanticName = "INDEX";
	skinningInputElementDescs[4].SemanticIndex = 0;
	skinningInputElementDescs[4].Format = DXGI_FORMAT_R32G32B32A32_SINT;
	skinningInputElementDescs[4].InputSlot = 0;
	skinningInputElementDescs[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC skinningInputLayoutDesc{};
	skinningInputLayoutDesc.pInputElementDescs = skinningInputElementDescs.data();
	skinningInputLayoutDesc.NumElements = static_cast<UINT>(skinningInputElementDescs.size());

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinningPipelineStateDesc = graphicsPipelineStateDesc;
	skinningPipelineStateDesc.InputLayout = skinningInputLayoutDesc;
	skinningPipelineStateDesc.VS = {skinningVertexShaderBlob->GetBufferPointer(), skinningVertexShaderBlob->GetBufferSize()};
	skinningPipelineStateDesc.pRootSignature = skinningRootSignature.Get();

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&skinningPipelineStateDesc,IID_PPV_ARGS(&skinningGraphicsPipelineState));
	assert(SUCCEEDED(hr));
}

void Obj3dCommon::CreateComputePipelineState(){
	HRESULT hr;

	// 1. RootSignature用デスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE srvRange[1] = {};
	srvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange[0].NumDescriptors = 3;
	srvRange[0].BaseShaderRegister = 0;
	srvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_DESCRIPTOR_RANGE uavRange[1] = {};
	uavRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	uavRange[0].NumDescriptors = 1;
	uavRange[0].BaseShaderRegister = 0;
	uavRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[3] = {};
	// CBV
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	// SRV Table
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.pDescriptorRanges = srvRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	// UAV Table
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].DescriptorTable.pDescriptorRanges = uavRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// --- RootSignatureの生成を先に実行 ---
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc,D3D_ROOT_SIGNATURE_VERSION_1,&signatureBlob,&errorBlob);
	assert(SUCCEEDED(hr));
	hr = dxCommon_->GetDevice()->CreateRootSignature(0,signatureBlob->GetBufferPointer(),signatureBlob->GetBufferSize(),IID_PPV_ARGS(&computeRootSignature));
	assert(SUCCEEDED(hr));

	// --- 2. PSOの作成 ---
	ComPtr<IDxcBlob> computeShaderBlob = dxCommon_->CompileShader(L"Engine/Graphics/Shaders/Obj3D/Skinning.CS.hlsl",L"cs_6_0");
	assert(computeShaderBlob != nullptr);

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePipelineStateDesc{};
	computePipelineStateDesc.CS = {
		.pShaderBytecode = computeShaderBlob->GetBufferPointer(),
		.BytecodeLength = computeShaderBlob->GetBufferSize()
	};
	computePipelineStateDesc.pRootSignature = computeRootSignature.Get();

	hr = dxCommon_->GetDevice()->CreateComputePipelineState(&computePipelineStateDesc,IID_PPV_ARGS(&computePipelineState));
	assert(SUCCEEDED(hr));
}

// UAV生成関数の実装例
void Obj3dCommon::CreateUAV(ID3D12Device* device,ID3D12Resource* resource,UINT numElements,UINT stride,D3D12_CPU_DESCRIPTOR_HANDLE handle){
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = numElements;
	uavDesc.Buffer.StructureByteStride = stride;
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	device->CreateUnorderedAccessView(resource,nullptr,&uavDesc,handle);
}