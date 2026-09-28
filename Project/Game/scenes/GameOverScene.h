#pragma once

#include "BaseScene.h"
#include "MyMath.h"
#include <memory>
#include <string>

// --- 前方宣言 ---
class Input;
class Obj3dCommon;
class Sprite;
class SpriteCommon;

/// <summary>
/// ゲームオーバー画面シーン
/// プレイヤーの体力が0になった際に表示する
/// </summary>
class GameOverScene : public BaseScene{
public:
	// --- コンストラクタ・デストラクタ ---
	GameOverScene();
	~GameOverScene() override;

	// --- BaseScene オーバーライド ---
	void Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon) override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// --- メンバ変数：外部依存 (借りてくるもの) ---
	Obj3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;

	// --- メンバ変数：内部リソース (所有するもの) ---
	std::unique_ptr<Sprite> backgroundSprite_;
};
