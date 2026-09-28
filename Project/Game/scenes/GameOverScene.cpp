#include "GameOverScene.h"

// エンジン/システム関連
#include "Input.h"
#include "SceneManager.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "TextureManager.h"
#include "WinAPI.h"

// ImGui (マクロ定義がある場合のみ)
#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif

// 定数定義 (ファイルパスやパラメータ)
namespace{
	// 背景演出用のテクスチャ(専用素材が無いためクリア画面と同じ丸テクスチャを流用)
	const std::string kBackgroundTexture = "resource/circle.png";
	// 背景スプライトのサイズ
	const float kBackgroundSpriteSize = 200.0f;
	// ゲームオーバー演出の背景色(赤がかった色で「失敗」を表現)
	const Vector4 kBackgroundColor = {1.0f, 0.3f, 0.3f, 0.6f};
}

// コンストラクタ
GameOverScene::GameOverScene() = default;

// デストラクタ
GameOverScene::~GameOverScene() = default;

// 初期化処理
void GameOverScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	// メンバ変数の保持
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// --- リソースのロード ---
	TextureManager::GetInstance()->LoadTexture(kBackgroundTexture);

	// --- スプライト生成と設定 ---
	backgroundSprite_ = std::make_unique<Sprite>();
	backgroundSprite_->Initialize(spriteCommon_,kBackgroundTexture);
	backgroundSprite_->SetAnchorPoint({0.5f, 0.5f}); // 中心を基準点にする
	backgroundSprite_->SetSize({kBackgroundSpriteSize, kBackgroundSpriteSize});
	backgroundSprite_->SetPosition({
		static_cast<float>(WinAPI::kClientWidth) * 0.5f,
		static_cast<float>(WinAPI::kClientHeight) * 0.5f
		});
	backgroundSprite_->SetColor(kBackgroundColor);
}

// 終了処理
void GameOverScene::Finalize(){
	// unique_ptrにより自動解放されるため処理なし
}

// 更新処理
void GameOverScene::Update(){
	// スプライトの更新
	if(backgroundSprite_){
		backgroundSprite_->Update();
	}

	// "GAME OVER"の文字表現(専用のフォント描画機能が無いため、ImGuiでテキスト表示する)
#ifdef USE_IMGUI
	{
		ImGui::SetNextWindowPos({static_cast<float>(WinAPI::kClientWidth) * 0.5f - 100.0f, 80.0f});
		ImGui::Begin("GameOver",nullptr,ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
		ImGui::Text("GAME OVER");
		ImGui::Text("Press SPACE to return to Title");
		ImGui::End();
	}
#endif

	// シーン遷移 (スペースキーでタイトルへ)
	if(input_->TriggerKey(DIK_SPACE)){
		SceneManager::GetInstance()->ChangeScene("TITLE");
	}
}

// 描画処理
void GameOverScene::Draw(){
	// 2Dスプライト描画
	if(spriteCommon_ && backgroundSprite_){
		spriteCommon_->Draw(); // 描画前処理
		backgroundSprite_->Draw();
	}
}
