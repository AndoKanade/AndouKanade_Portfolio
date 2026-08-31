#include "Camera.h"
#include "WinAPI.h"

// コンストラクタ
Camera::Camera()
	: transform({
		{1.0f, 1.0f, 1.0f},   // scale
		{0.0f, 0.0f, 0.0f},   // rotate
		{0.0f, 0.0f, -10.0f}  // translate (画面に映るように手前に引く)
		})
	,fov(1.0472f) // 視野角60度(望遠気味に見えていた0.45rad(約26度)から変更)
	,aspectRatio(float(WinAPI::kClientWidth) / float(WinAPI::kClientHeight))
	,nearClip(0.1f)
	,farClip(100.0f)
	,worldMatrix(MakeAffineMatrix(transform.scale,transform.rotate,transform.translate))
	,viewMatrix(Inverse(worldMatrix))
	,projectionMatrix(MakePerspectiveFovMatrix(fov,aspectRatio,nearClip,farClip))
	,viewProjectionMatrix(Multiply(viewMatrix,projectionMatrix)){}

void Camera::Initialize(ID3D12Device* device){
	// 1. リソースの作成
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = (sizeof(CameraForGPU) + 255) & ~255; // 256バイトアライメント
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource));

	// 2. マップしてアドレスを取得
	resource->Map(0,nullptr,reinterpret_cast<void**>(&data));
}

// 更新処理
void Camera::Update(){
	// ワールド行列の計算
	worldMatrix = MakeAffineMatrix(transform.scale,transform.rotate,transform.translate);

	// ビュー行列の計算 (ワールド行列の逆行列)
	viewMatrix = Inverse(worldMatrix);

	// プロジェクション行列の計算
	projectionMatrix = MakePerspectiveFovMatrix(fov,aspectRatio,nearClip,farClip);

	// 合成行列の計算 (View * Projection)
	viewProjectionMatrix = Multiply(viewMatrix,projectionMatrix);

	if(data){
		data->worldPosition = transform.translate;
	}
}