#include "Application.h"
#include "Obj3DCommon.h"
#include "SceneFactory.h"
#include "SceneManager.h"
#include "ImGuiManager.h"
#include "SrvManager.h"
#include "SoundManager.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "ParticleManager.h"
#include "TimeManager.h"
#include "TextureManager.h"
#include "PostProcess.h"
#include "RenderTexture.h"
#include "EditorWidgets.h"
#include "EditorContext.h"
#include "WinAPI.h"

Application* Application::instance_ = nullptr;

// -------------------------------------------------
// コンストラクタ・デストラクタ
// -------------------------------------------------
Application::Application(){
	instance_ = this;
}

Application::~Application() = default;

// -------------------------------------------------
// 初期化処理
// -------------------------------------------------
void Application::Initialize(){
	// 1. 基底クラス(Framework)の初期化
	Framework::Initialize();
	dxCommon_->InitDepthShaderResourceView();

	// 2. ポストプロセス関連のリソース初期化
	postProcess_ = std::make_unique<PostProcess>();
	postProcess_->Initialize(dxCommon_.get());

	renderTexture_ = std::make_unique<RenderTexture>();

	// RTV/SRVハンドルの取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon_->AllocateRtvDescriptor();
	uint32_t srvIndex = SrvManager::GetInstance()->Allocate();
	D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCpu = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGpu = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex);

	// オフスクリーン用テクスチャの生成
	renderTexture_->Create(
		dxCommon_->GetDevice(),
		WinAPI::kClientWidth,
		WinAPI::kClientHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		// 最適化クリア値は DXCommon::PreDraw のクリア色と一致させること
		// (不一致だとD3D12デバッグレイヤーが WARNING #820 を出し、GBV有効下では例外化してクラッシュする)
		{0.1f, 0.25f, 0.5f, 1.0f},
		rtvHandle,
		srvHandleCpu,
		srvHandleGpu
	);

	// 3. シーン工場の生成
	sceneFactory_ = std::make_unique<SceneFactory>();

	// 4. シーンマネージャのセットアップ
	SceneManager* sceneManager = SceneManager::GetInstance();
	sceneManager->SetFactory(sceneFactory_.get());
	sceneManager->SetCommonPtr(object3dCommon_.get(),input_.get(),spriteCommon_.get());

	// 5. 最初のシーンを開始(タイトルはゲームシーン内の開始演出として表示する)
	sceneManager->ChangeScene("GAME");

	// 6. ポストプロセス用マスク画像のロード
	TextureManager::GetInstance()->LoadTexture("resource/noise0.png");
	TextureManager::GetInstance()->LoadTexture("resource/noise1.png");
	currentMaskPath_ = "resource/noise0.png";
}

void Application::InitializeGameSystems(){
	SrvManager::GetInstance()->Initialize(dxCommon_.get());
#ifdef USE_IMGUI
	ImGuiManager::GetInstance()->Initialize(winApi_.get(),dxCommon_.get());
#endif
	SoundManager::GetInstance()->Initialize();
	TextureManager::GetInstance()->Initialize(dxCommon_.get(),SrvManager::GetInstance());
	ModelManager::GetInstance()->Initialize(dxCommon_.get());
	CameraManager::GetInstance()->Update();
	ParticleManager::GetInstance()->Initialize(dxCommon_.get(),SrvManager::GetInstance());
	TimeManager::GetInstance()->Initialize();
}

// -------------------------------------------------
// 終了処理
// -------------------------------------------------
void Application::Finalize(){
	Framework::Finalize();
}

// -------------------------------------------------
// 更新処理
// -------------------------------------------------
void Application::Update(){
	// 1. 基底クラスの更新
	Framework::Update();

	// 2. Dissolveアニメーション処理
	if(isDissolving_){
		dissolveTimer_ += 1.0f / 60.0f;
		float threshold = dissolveTimer_ / kDissolveDuration;

		if(threshold >= 1.0f){
			threshold = 1.0f;
			isDissolving_ = false;
		}
		postProcess_->SetDissolveThreshold(threshold);
	}

	// 3. グリッチ演出タイマー更新
	if(isGlitchActive_){
		glitchTimer_ -= 1.0f / 60.0f;
		if(glitchTimer_ <= 0.0f){
			isGlitchActive_ = false;
			currentPPType_ = PostProcess::Type::PostProcess;
		}
	}

	// 4. Random用時間更新処理
	static float time = 0.0f;
	time += 1.0f / 60.0f;
	postProcess_->SetRandomTime(time);
}

// -------------------------------------------------
// 描画処理
// -------------------------------------------------
void Application::Draw(){
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// 1. RenderTextureへの描画 (パス1：オフスクリーン)
	dxCommon_->PreDraw(renderTexture_.get());
	SrvManager::GetInstance()->PreDraw();

	if(object3dCommon_){
		object3dCommon_->Draw();
	}
	SceneManager::GetInstance()->Draw();

	// 2. Swapchainへの描画 (パス2：ポストプロセス適用)
	dxCommon_->PreDraw(nullptr);

	// リソースバリアの設定：描画ターゲットからシェーダーリソース読み込み状態へ
	D3D12_RESOURCE_BARRIER barriers[2] = {};
	barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[0].Transition.pResource = renderTexture_->GetResource();
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[1].Transition.pResource = dxCommon_->GetDepthStencilResource();
	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	commandList->ResourceBarrier(2,barriers);

	// 追加 Editモードのときは、ゲーム画面を中央「Scene」パネルの領域に合わせて描く。
	// PostProcess::Drawはビューポートを自前で設定しないので、ここでビューポートを差し替えるだけで
	// 全画面三角形がその矩形にマッピングされる。16:9を維持してレターボックス。
	// Sceneパネルが無い(Playモード/非エディタ)ときは PreDraw の全画面ビューポートのまま。
	{
		EditorContext* ec = EditorContext::GetInstance();
		const EditorRect& sr = ec->GetSceneViewRect();
		if(!ec->IsPlayMode() && sr.w > 0.0f && sr.h > 0.0f){
			const float targetAspect = static_cast<float>(WinAPI::kClientWidth) / static_cast<float>(WinAPI::kClientHeight);
			const float rectAspect = sr.w / sr.h;
			float vw,vh;
			if(rectAspect > targetAspect){
				// 領域が横長 → 高さを合わせて左右をレターボックス
				vh = sr.h;
				vw = vh * targetAspect;
			} else{
				// 領域が縦長 → 幅を合わせて上下をレターボックス
				vw = sr.w;
				vh = vw / targetAspect;
			}
			D3D12_VIEWPORT sceneViewport{};
			sceneViewport.TopLeftX = sr.x + (sr.w - vw) * 0.5f;
			sceneViewport.TopLeftY = sr.y + (sr.h - vh) * 0.5f;
			sceneViewport.Width = vw;
			sceneViewport.Height = vh;
			sceneViewport.MinDepth = 0.0f;
			sceneViewport.MaxDepth = 1.0f;
			commandList->RSSetViewports(1,&sceneViewport);
		}
	}

	// 3. ポストプロセス用テクスチャ切り替え (Dissolve対応)
	D3D12_GPU_DESCRIPTOR_HANDLE secondarySRV = dxCommon_->GetDepthSrvHandleGpu();
	if(currentPPType_ == PostProcess::Type::Dissolve){
		secondarySRV = TextureManager::GetInstance()->GetSrvHandleGPU(currentMaskPath_);
	}

	// ポストプロセスの実行
	postProcess_->Draw(commandList,renderTexture_->GetSrvHandleGpu(),secondarySRV,currentPPType_);

	// 状態を元に戻す
	barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	commandList->ResourceBarrier(2,barriers);

	// 4. UI描画 (ImGui)
#ifdef USE_IMGUI
	ImGuiManager::GetInstance()->Draw();
#endif

	// 5. 描画後処理
	dxCommon_->PostDraw();
}

// -------------------------------------------------
// デバッグUI表示
// -------------------------------------------------
void Application::ShowPostProcessUI(){
#ifdef USE_IMGUI
	// 固定タイルレイアウトの下段・右に配置
	EditorWidgets::BeginFixedPanel("PostProcess Settings",EditorWidgets::ComputeLayout().bottomRight);

	const char* typeNames[] = {"Default", "BoxFilter", "Grayscale", "Vignette", "GaussianBlur", "LuminanceOutline", "DepthOutline", "RadialBlur", "Dissolve", "Random", "Glitch"};
	int currentIdx = static_cast<int>(currentPPType_);

	if(ImGui::Combo("Filter Type",&currentIdx,typeNames,IM_ARRAYSIZE(typeNames))){
		currentPPType_ = static_cast<PostProcess::Type>(currentIdx);
	}

	ImGui::Separator();

	// 各フィルターごとの詳細設定UI
	if(currentPPType_ == PostProcess::Type::Vignette){
		static float intensity = 0.5f,scale = 0.8f;
		if(ImGui::DragFloat("Intensity",&intensity,0.01f,0.0f,1.0f)) postProcess_->SetVignetteIntensity(intensity);
		if(ImGui::DragFloat("Scale",&scale,0.01f,0.0f,2.0f)) postProcess_->SetVignetteScale(scale);
	} else if(currentPPType_ == PostProcess::Type::BoxFilter || currentPPType_ == PostProcess::Type::GaussianBlur){
		static int k = 1;
		if(ImGui::SliderInt((currentPPType_ == PostProcess::Type::BoxFilter)?"Kernel Size":"Blur Strength",&k,0,10)){
			postProcess_->SetKernelSize(k);
		}
	} else if(currentPPType_ == PostProcess::Type::RadialBlur){
		static float center[2] = {0.5f, 0.5f},width = 0.01f;
		if(ImGui::DragFloat2("Center",center,0.01f,0.0f,1.0f)) postProcess_->SetRadialBlurCenter({center[0], center[1]});
		if(ImGui::DragFloat("Width",&width,0.001f,0.0f,0.1f)) postProcess_->SetRadialBlurWidth(width);
	} else if(currentPPType_ == PostProcess::Type::Dissolve){
		static float threshold = 0.0f,edgeWidth = 0.03f,edgeColor[3] = {1.0f, 0.4f, 0.3f};
		if(isDissolving_) threshold = dissolveTimer_ / kDissolveDuration;

		if(ImGui::SliderFloat("Threshold",&threshold,0.0f,1.0f)) postProcess_->SetDissolveThreshold(threshold);
		if(ImGui::Button("Start Animation")){ isDissolving_ = true; dissolveTimer_ = 0.0f; }
		ImGui::SameLine();
		if(ImGui::Button("Reset")){ isDissolving_ = false; dissolveTimer_ = 0.0f; postProcess_->SetDissolveThreshold(0.0f); }

		if(ImGui::SliderFloat("Edge Width",&edgeWidth,0.0f,0.2f)) postProcess_->SetDissolveEdgeWidth(edgeWidth);
		if(ImGui::ColorEdit3("Edge Color",edgeColor)) postProcess_->SetDissolveEdgeColor({edgeColor[0], edgeColor[1], edgeColor[2]});

		const char* masks[] = {"resource/noise0.png", "resource/noise1.png"};
		static int currentMaskIndex = 0;
		if(ImGui::Combo("Select Mask",&currentMaskIndex,masks,IM_ARRAYSIZE(masks))) currentMaskPath_ = masks[currentMaskIndex];
	} else if(currentPPType_ == PostProcess::Type::Glitch){
		static float intensity = 0.5f;
		if(ImGui::DragFloat("Intensity",&intensity,0.01f,0.0f,1.0f)) postProcess_->SetRandomIntensity(intensity);
	}
	ImGui::End();
#endif
}

void Application::TriggerGlitch(){
	currentPPType_ = PostProcess::Type::Glitch;
	isGlitchActive_ = true;
	glitchTimer_ = kGlitchDuration;
}