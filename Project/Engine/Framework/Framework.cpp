#include "Framework.h"
#include "Logger.h"
#include "SceneManager.h" 
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "SoundManager.h"
#include "CameraManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "TimeManager.h"
#include "SpriteCommon.h"
#include "Obj3DCommon.h"
#include "AbstractSceneFactory.h"
#include "GlobalVariables.h"
#include "EditorContext.h"
#include "EditorWidgets.h"
#include "imgui.h"

void Framework::Initialize(){
	// 1. 基盤の初期化（ハードウェア/OSとのやり取り）
	// クラッシュ時のダンプファイル出力設定
	SetUnhandledExceptionFilter(Logger::ExportDump);

	// WinAPIの初期化
	// ウィンドウサイズは kClientWidth/kClientHeight に一元化(スワップチェーン等と必ず一致させるため)
	winApi_ = std::make_unique<WinAPI>();
	winApi_->Initialize(L"Andou_Kanade_就職作品",WinAPI::kClientWidth,WinAPI::kClientHeight);

	// DirectXの初期化
	dxCommon_ = std::make_unique<DXCommon>();
	dxCommon_->Initialize(winApi_.get());

	// 入力システムの初期化
	input_ = std::make_unique<Input>();
	input_->Initialize(winApi_.get());

	// 2. ゲーム固有システムの初期化
	InitializeGameSystems();

	// 3. 描画共通設定の初期化
	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Initialize(dxCommon_.get());

	object3dCommon_ = std::make_unique<Obj3dCommon>();
	object3dCommon_->Initialize(dxCommon_.get());

	// 4. 調整項目(GlobalVariables)の読み込み
	// シーンがデフォルト値を登録(AddItem)する前に読み込んでおくことで、
	// 保存済みの値がデフォルト値で上書きされないようにする
	GlobalVariables::GetInstance()->LoadFiles();
}

void Framework::Update(){
	// --- ウィンドウメッセージ処理 ---
	// ウィンドウが閉じられたら終了リクエストを送る
	if(winApi_->ProcessMessage()){
		endRequest_ = true;
	}

	// --- 入力情報の更新 ---
	input_->Update();

	// 変更箇所: ImGui無し(Release等)でもF2キーでPlay/Stopを切り替えられるようにする
	if(input_->TriggerKey(DIK_F2)){
		EditorContext::GetInstance()->TogglePlayMode();
	}

	// --- ImGui 受付開始 ---
	// 注意: シーン更新前に Begin() を呼び出す必要がある
#ifdef USE_IMGUI
	ImGuiManager::GetInstance()->Begin();

	// 調整項目のホットリロード監視 + 編集UI(ImGui有効時のみ)
	GlobalVariables::GetInstance()->Update();

	// エディタの固定タイルレイアウト。ツールバーは常時、中央Sceneパネルは編集モードのみ。
	{
		EditorContext* ec = EditorContext::GetInstance();
		EditorWidgets::Layout layout = EditorWidgets::ComputeLayout();

		// --- 上部ツールバー(Play/Stop) 両モードで表示 ---
		EditorWidgets::BeginFixedPanel("##Toolbar",layout.toolbar,ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
		if(ec->IsPlayMode()){
			if(ImGui::Button("Stop")){ ec->SetPlayMode(false); }
			ImGui::SameLine();
			ImGui::TextDisabled("PLAY MODE");
		} else{
			if(ImGui::Button("Play")){ ec->SetPlayMode(true); }
			ImGui::SameLine();
			ImGui::TextDisabled("EDIT MODE");
		}
		ImGui::End();

		// --- 中央 Scene パネル(背景透過)。内側領域をゲームのビューポートとして記録する ---
		if(!ec->IsPlayMode()){
			EditorWidgets::BeginFixedPanel("Scene",layout.sceneView,ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			// Begin直後のカーソル位置=内側左上(スクリーン座標)、残り領域=内側サイズ
			ImVec2 innerPos = ImGui::GetCursorScreenPos();
			ImVec2 innerSize = ImGui::GetContentRegionAvail();
			ec->SetSceneViewRect(innerPos.x,innerPos.y,innerSize.x,innerSize.y);
			ImGui::End();
		} else{
			// Playモードは全画面。Scene領域を無効化
			ec->ClearSceneViewRect();
		}
	}
#endif

	// --- シーン更新処理 ---
	SceneManager::GetInstance()->Update();

	// --- ImGui 受付終了 ---
#ifdef USE_IMGUI
	ImGuiManager::GetInstance()->End();
#endif

	// --- カメラの更新 ---
	CameraManager::GetInstance()->Update();
}

void Framework::Finalize(){
	// --- マネージャーの終了処理 ---
	// 依存関係を考慮して順次解放
	SceneManager::GetInstance()->Finalize();

#ifdef USE_IMGUI
	ImGuiManager::GetInstance()->Finalize();
#endif

	SoundManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	CameraManager::GetInstance()->Finalize();
	SrvManager::GetInstance()->Finalize();

	// --- 基盤システムの終了 ---
	// unique_ptrにより各システムは自動的に解放される
}