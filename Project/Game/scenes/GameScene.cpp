#include "GameScene.h"
#include "CameraManager.h"
#include "ImGuiManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Skybox.h"
#include "SkyboxCommon.h"
#include "Input.h"
#include "Obj3dCommon.h"
#include "SpriteCommon.h"
#include "WinAPI.h"
#include "Application.h"
#include "GlobalVariables.h"
#include "EditorWidgets.h"
#include "EditorContext.h"

// レールエディター
#include "Editor/RailEditor.h"
// ステージ開始演出(タイトル → カメラの回り込み → カウントダウン)
#include "Title/StartSequence.h"
// ゲームオブジェクト
#include "objects/Ground.h"
#include "objects/Sun.h"
#include "objects/Player.h"
#include "objects/PlayerBulletManager.h"
#include "objects/TargetManager.h"
#include "objects/EnemyManager.h"
#include "objects/InkEffectManager.h"
#include "objects/ComboCounter.h"
#include "objects/StageResult.h"
// カメラ
#include "Camera/RailCamera.h"
#include "Camera/DebugTopCamera.h"
// UI
#include "UI/Reticle.h"
#include "UI/StageHUD.h"
#include "UI/SpeedLines.h"

namespace{
	// スカイボックスのテクスチャパス
	const std::string kSkyboxTexture = "resource/Skybox/rostock_laage_airport_4k.dds";
	// GlobalVariablesのグループ名(GameSceneの調整項目)
	const char* kGameSceneGroup = "GameScene";

	// プレイ用カメラの名前
	const char* kDefaultCameraName = "default";
	// プレイ用カメラの初期位置
	constexpr Vector3 kDefaultCameraPosition = {0.0f, 0.0f, -30.0f};

	// 撃破演出(パーティクル)に使用するテクスチャパス
	const std::string kHitParticleTexture = "resource/circle.png";
	// ParticleManager::EmitSpark()が内部で使用するグループ名と合わせる必要がある
	const char* kHitParticleGroupName = "Spark";

	// 弾が画面中央のレティクルへ向かって飛ぶよう、カメラの前方のこの距離の点を狙って撃ち出す
	// (カメラはプレイヤーの後ろ上にあるため、カメラと同じ向きで平行に撃つとレティクルより下にずれる)
	constexpr float kAimConvergeDistance = 30.0f;
}

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

// シーンの初期化
void GameScene::Initialize(Obj3dCommon* object3dCommon,Input* input,SpriteCommon* spriteCommon){
	object3dCommon_ = object3dCommon;
	input_ = input;
	spriteCommon_ = spriteCommon;

	// カメラの生成と設定
	CameraManager::GetInstance()->CreateCamera(kDefaultCameraName,object3dCommon_->GetDxCommon()->GetDevice());
	auto* defaultCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName);
	defaultCamera->SetTranslate(kDefaultCameraPosition);
	CameraManager::GetInstance()->SetActiveCamera(kDefaultCameraName);
	object3dCommon_->SetDefaultCamera(CameraManager::GetInstance()->GetActiveCamera());

	// テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(kSkyboxTexture);

	// スカイボックスの生成と初期化
	skyboxCommon_ = std::make_unique<SkyboxCommon>();
	skyboxCommon_->Initialize(object3dCommon_->GetDxCommon());
	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(skyboxCommon_.get(),kSkyboxTexture);

	// レールエディターの生成と初期化
	railEditor_ = std::make_unique<RailEditor>();
	railEditor_->Initialize(object3dCommon_);

	// 着地できる範囲の可視化に、着地判定で使っている許容距離をそのまま渡す
	railEditor_->SetLandingRangeRadius(Player::kOnRailHorizontalThreshold);

	// 簡易的な地面の生成(レール開始地点を基準に並べるため、レールエディターの初期化後に行う)
	ground_ = std::make_unique<Ground>();
	ground_->Initialize(object3dCommon_,railEditor_->GetPositionOnRail(0.0f),railEditor_->GetForwardOnRail(0.0f),kGameSceneGroup);

	// 調整項目(GlobalVariables)のグループを作成する
	// 各クラスがこのグループに自分の調整項目を登録する
	// ImGuiの "Global Variables" ウィンドウから実行中に編集・保存でき、
	// resource/GlobalVariables/GameScene.json を外部で書き換えると自動反映される(ホットリロード)
	GlobalVariables::GetInstance()->CreateGroup(kGameSceneGroup);

	// 太陽の生成(方角はレール開始地点の進行方向を基準にする)
	sun_ = std::make_unique<Sun>();
	sun_->Initialize(kGameSceneGroup,railEditor_->GetForwardOnRail(0.0f));

	// プレイヤーの生成
	player_ = std::make_unique<Player>();
	player_->Initialize(object3dCommon_,kGameSceneGroup);

	// レール追従カメラの生成
	railCamera_ = std::make_unique<RailCamera>();
	railCamera_->Initialize(kGameSceneGroup);

	// 俯瞰用のデバッグカメラを生成
	debugTopCamera_ = std::make_unique<DebugTopCamera>();
	debugTopCamera_->Initialize(object3dCommon_);

	// プレイヤーの弾の生成
	bulletManager_ = std::make_unique<PlayerBulletManager>();
	bulletManager_->Initialize(object3dCommon_);

	// 的の生成(保存済みの配置が無い初回起動時はレール沿いに自動配置する)
	targetManager_ = std::make_unique<TargetManager>();
	targetManager_->Initialize(object3dCommon_,railEditor_.get(),kGameSceneGroup);

	// 雑魚敵の生成(保存済みの配置が無い初回起動時はレールの中間地点に自動配置する)
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(object3dCommon_,railEditor_.get());

	// 画面中央固定のレティクルを生成
	reticle_ = std::make_unique<Reticle>();
	reticle_->Initialize(spriteCommon_);

	// インクのしぶき・的の破片・閃光の演出(使い回す3Dオブジェクトをここでまとめて生成する)
	inkEffect_ = std::make_unique<InkEffectManager>();
	inkEffect_->Initialize(object3dCommon_);

	// 的を続けて壊したときの連鎖数
	comboCounter_ = std::make_unique<ComboCounter>();

	// プレイ中の画面表示(体力・壊した的の数・連鎖数)
	stageHUD_ = std::make_unique<StageHUD>();
	stageHUD_->Initialize(spriteCommon_);

	// レールを進んでいる間に出すスピード線
	speedLines_ = std::make_unique<SpeedLines>();
	speedLines_->Initialize(spriteCommon_);

	// 的の撃破時に発生させる火花パーティクルのグループを事前に生成しておく
	TextureManager::GetInstance()->LoadTexture(kHitParticleTexture);
	ParticleManager::GetInstance()->CreateParticleGroup(kHitParticleGroupName,kHitParticleTexture);

	// ステージ開始演出の生成(Playに入った瞬間にタイトルから始める)
	startSequence_ = std::make_unique<StartSequence>();
	startSequence_->Initialize(object3dCommon_,spriteCommon_,input_);
}

// シーンの終了処理
void GameScene::Finalize(){
	// Play中にTABでカーソルを隠したままシーンが切り替わると、次のシーンでもカーソルが消えたままになるため表示に戻す
	if(!isCursorVisible_){
		isCursorVisible_ = true;
		::ShowCursor(TRUE);
	}
}

// ゲームを初期状態(レール先頭)から始め直す
// Playに入った瞬間と、レールから落ちたときのリスタートで共通して使う
void GameScene::ResetPlayState(){
	// レール位置・オンレール状態・体力を初期状態に戻す
	player_->Reset();
	// 照準を正面に戻す
	railCamera_->Reset();
	// 的を復活させる
	targetManager_->Reset();
	// 開始時に残っている弾もリセットする
	bulletManager_->Clear();

	// レールを0番へ戻し、インクで塗った区間も消す
	if(railEditor_){
		railEditor_->SwitchActiveRail(0);
		railEditor_->ClearPaint();
	}

	// 出ている演出・連鎖数・スピード線も消す
	inkEffect_->Reset();
	comboCounter_->Reset();
	speedLines_->Reset();

	// 雑魚敵も撃破前の初期状態(体力満タン)から始める
	enemyManager_->Reset();
}

// シーンの更新処理
void GameScene::Update(){
	// スカイボックスの更新
	if(skybox_){
		skybox_->Update(*CameraManager::GetInstance()->GetActiveCamera());
	}

	// パーティクルの更新(発生タイミング・経過時間の進行。カメラ行列は描画時に反映する)
	ParticleManager::GetInstance()->Update();

	// 地面タイルの更新(霧のUVスクロールも行う)
	ground_->Update(kDeltaTime);

	// 太陽の光の向き・色・明るさを調整項目の最新値で更新する
	sun_->Update();

	// レティクルの更新
	reticle_->Update();

	// レールエディターの更新
	if(railEditor_){
		railEditor_->Update();
	}

	// 的・敵の配置エディターの更新(配置モード中のドラッグ移動と、編集結果の反映もここで処理される)
	targetManager_->UpdateEditor();
	enemyManager_->UpdateEditor();

	// レール進行・カメラ・プレイヤー・敵・弾・的の更新
	if(railEditor_){
		UpdateGameplay();
	}

	// ImGuiが無い環境でもレールの制御点を確認できるよう、F1キーで俯瞰デバッグカメラを切り替える
	debugTopCamera_->HandleToggleKey(input_,railEditor_.get());

	// ImGuiが無い環境でも表示切り替えができるよう、キー操作で各種デバッグ表示をトグルする
	if(input_ && input_->TriggerKey(DIK_F4) && railEditor_){
		railEditor_->ToggleShowControlPointModels();
	}
	if(input_ && input_->TriggerKey(DIK_F5) && railEditor_){
		railEditor_->ToggleShowCurve();
	}

	// 俯瞰デバッグカメラが有効な間は、ImGui無しでもWASD(平面移動)+QE(高さ)で自由に動かせるようにする
	debugTopCamera_->UpdateMove(input_);

#ifdef USE_IMGUI
	// Playモード中は編集用パネルをすべて隠す(クリーンな実行画面にするため)
	if(!EditorContext::GetInstance()->IsPlayMode()){
		ShowEditorPanels();
	}
#endif
}

// マウスカーソルの表示/非表示切り替え
// Editモードでは常に表示する(ImGui操作にカーソルが必要なため、Play中に消していても強制的に戻す)
// Playモード中はTABキーで切り替えられるようにする(照準はマウスの移動量のみで行うため、カーソル表示が邪魔になることがある)
void GameScene::UpdateCursorVisibility(bool isPlayMode){
	if(!isPlayMode){
		if(!isCursorVisible_){
			isCursorVisible_ = true;
			::ShowCursor(TRUE);
		}
	} else if(input_ && input_->TriggerKey(DIK_TAB)){
		isCursorVisible_ = !isCursorVisible_;
		::ShowCursor(isCursorVisible_?TRUE:FALSE);
	}
}

// プレイ中のゲーム処理
void GameScene::UpdateGameplay(){
	// EditモードではゲームのシミュレーションをUnityのように止める(Playを押して初めて動く)。
	// カメラ位置や的の配置反映などの「表示」は両モードで行い、進行/入力/射撃だけをPlay限定にする。
	const bool isPlayMode = EditorContext::GetInstance()->IsPlayMode();

	// Playに入った瞬間、ゲームを初期状態から開始する(UnityのPlayと同じく毎回リセットして始まる)。
	// これでPlayを押すとレール先頭=編集で見えていた画から始まる。
	// 開始演出もタイトルから始め、Editモードへ戻ったときは止める。
	// 落下リスタートでは ResetPlayState() だけを呼ぶため、タイトルを挟まずにすぐ再開する
	if(isPlayMode && !wasPlayMode_){
		// 俯瞰デバッグカメラがONのままだとプレイ用の視点にならないため、Playに入ったら自動でOFFにする
		debugTopCamera_->Disable();
		ResetPlayState();
		startSequence_->Start(skipTitle_);
	} else if(!isPlayMode && wasPlayMode_){
		startSequence_->Stop();
	} else if(isPlayMode && input_ && input_->TriggerKey(DIK_R)){
		// 確認用の一時的な処理:Play中にRキーでタイトルから最初からやり直す
		ResetPlayState();
		startSequence_->Start(skipTitle_);
	}
	wasPlayMode_ = isPlayMode;

	// 開始演出の更新(タイトル中のSPACE入力・カメラの回り込み・カウントダウン)
	if(isPlayMode){
		startSequence_->Update(kDeltaTime);
	}

	// レール進行・操作・射撃・勝敗判定は、開始演出が終わってプレイ可能になってから動かす
	const bool isGameplayActive = isPlayMode && startSequence_->IsPlayable();

	// マウスカーソルの表示/非表示切り替え
	UpdateCursorVisibility(isPlayMode);

	// レール進行度を時間で進める
	player_->UpdateRailProgress(railEditor_.get(),isGameplayActive,kDeltaTime);

	// オンレール/オフレールの状態に応じて、カメラ・プレイヤーの基準位置と基準向きを求める
	player_->UpdateBasePose(railEditor_.get());

	// レールを進んでいる間は、通った区間をインクの色で塗る
	// 開始演出中・Edit中・終点に着いた後は進まないため、レールを進んでいる間に含めない
	const bool isRidingRail = isGameplayActive && player_->IsOnRail() && !player_->HasReachedGoal();
	if(isRidingRail){
		railEditor_->PaintActiveRail(player_->GetRailT());
	}

	// カメラを基準位置の後ろ上に置き、照準の入力を反映する
	railCamera_->Update(player_->GetBasePosition(),player_->GetBaseRotation(),isGameplayActive,input_,kDeltaTime);
	const Vector3& cameraPos = railCamera_->GetPosition();
	const Vector3& cameraForward = railCamera_->GetForward();

	// クリア判定
	// アクティブなレールが最後まで到達したらクリアとする(オフレール中は判定しない)
	// 切り替えは次フレームに行われるため、以降の判定(ゲームオーバー・落下リスタートなど)で上書きされないようここで抜ける
	if(isGameplayActive && player_->HasReachedGoal()){
		// クリア画面で結果を表示できるよう、シーンが作り直される前に書き込んでおく
		StageResult::destroyedTargetCount = targetManager_->GetDestroyedCount();
		StageResult::totalTargetCount = targetManager_->GetTotalCount();
		StageResult::maxCombo = comboCounter_->GetMaxCombo();
		sceneManager_->ChangeScene("CLEAR");
		return;
	}

#ifdef USE_IMGUI
	// レールの状態確認用デバッグ表示(Playモード中も含めて常に表示する)
	ShowStatusWindow();
#endif

	// プレイヤーをレールの上に立たせ、照準の方向へ向ける
	// 座標は雑魚敵の検知判定・開始演出にも使う
	player_->UpdateTransform(railCamera_->GetRotation(),startSequence_->GetPlayerHopHeight());
	const Vector3& playerPos = player_->GetPosition();

	// レールを進んでいる間は、足元から後ろへインクのしぶきを飛ばして勢いを見せる
	if(isRidingRail){
		inkEffect_->EmitRailTrail(playerPos,-railEditor_->GetForwardOnRail(player_->GetRailT()));
	}

	// 開始演出中は、プレイ用カメラをプレイヤー中心に回転させた位置・向きでカメラを上書きする
	// (カメラの行列はシーン更新の後にまとめて更新されるため、ここで上書きしてもこのフレームの描画に反映される)
	// タイトルロゴも同じくプレイヤーの位置・向きを基準に配置する
	if(isPlayMode && startSequence_->IsControllingCamera()){
		startSequence_->UpdateLogo(playerPos,railCamera_->GetRotation());

		Vector3 directedCameraPos;
		Vector3 directedCameraRot;
		startSequence_->CalculateCamera(playerPos,cameraPos,railCamera_->GetRotation(),directedCameraPos,directedCameraRot);
		if(Camera* mainCamera = CameraManager::GetInstance()->GetCamera(kDefaultCameraName)){
			mainCamera->SetTranslate(directedCameraPos);
			mainCamera->SetRotate(directedCameraRot);
		}
	}

	// 雑魚敵の更新
	// Edit中・開始演出中はゲームを静止させるため、経過時間を0にして表示更新のみ行わせる
	enemyManager_->Update(playerPos,isGameplayActive?kDeltaTime:0.0f);

	// 敵弾とプレイヤーの当たり判定
	// プレイ可能な間のみ判定する。連続被弾で一瞬に体力が尽きないよう、被弾後は一定時間無敵にする
	if(isGameplayActive){
		// 無敵時間の経過を進める
		player_->UpdateInvincible(kDeltaTime);

		// 命中した弾は無敵中でもここで消滅させ、すり抜けて後から当たらないようにする
		// プレイヤーの行列はこのフレームの配置処理でUpdate()済み
		int hitCount = enemyManager_->CheckHitToPlayer(player_->GetHitSphere());

		// 同時に複数当たっても体力の減少は1回分だけにする
		if(hitCount > 0){
			player_->TakeDamage();
		}
	}

	// ゲームオーバー判定
	// 体力が0になったらゲームオーバー画面へ遷移する
	// クリア判定と同じく、以降の落下リスタートなどの処理を行わないようここで抜ける
	if(isGameplayActive && player_->IsDead()){
		sceneManager_->ChangeScene("GAMEOVER");
		return;
	}

	// プレイヤーの自立(ジャンプ+WASD移動)
	// どのレールにも乗れないまま地面の高さまで落ちたら、レール先頭からやり直す
	if(isGameplayActive){
		bool needsRestart = player_->UpdateMovement(input_,railEditor_.get(),cameraForward,railCamera_->GetRight(),ground_->GetHeight(),kDeltaTime);
		if(needsRestart){
			ResetPlayState();
		}
	}

	// 弾の発射処理(左クリックした瞬間に1発だけ発射する)
	bool shootTriggered = isGameplayActive && input_ && input_->TriggerMouseButton(kShootMouseButton);
	if(shootTriggered){
		// プレイヤーの手元から、レティクルの先の点へ向けて撃ち出す
		Vector3 muzzlePos = player_->GetMuzzlePosition();
		Vector3 aimPoint = cameraPos + cameraForward * kAimConvergeDistance;
		Vector3 shootDirection = Normalize(aimPoint - muzzlePos);
		bulletManager_->Fire(muzzlePos,shootDirection);

		// 手元から撃つ向きへインクを少し飛び散らせる
		inkEffect_->EmitInkSplash(muzzlePos,shootDirection,kMuzzleSplashCount);
	}

	// 弾の移動更新と生存時間チェック
	bulletManager_->Move(kDeltaTime);

	// 連鎖が途切れるまでの時間を進める(Edit中・開始演出中は止めておく)
	comboCounter_->Update(isGameplayActive?kDeltaTime:0.0f);

	// 的の照準判定と弾との当たり判定
	bool isAimingAtAnyTarget = targetManager_->Update(cameraPos,cameraForward,*bulletManager_);

	// 壊れた的ごとに、破片・しぶき・閃光を出して連鎖数を増やす
	for(const Vector3& destroyedPos : targetManager_->GetDestroyedPositions()){
		inkEffect_->EmitTargetBreak(destroyedPos);
		comboCounter_->AddHit();
	}

	// 雑魚敵への被弾判定
	enemyManager_->CheckHitByBullets(*bulletManager_);

	// 命中または生存時間切れで消えた弾をリストから削除し、残った弾の描画用の行列を更新する
	bulletManager_->RemoveDeadBullets();
	bulletManager_->UpdateTransforms();

	// 狙えているときはレティクル中心を赤くする
	reticle_->SetAiming(isAimingAtAnyTarget);

	// 演出の更新(このフレームで出した分も含めて行列を求めるため、出す処理がすべて終わってから行う)
	inkEffect_->Update(kDeltaTime);

	// スピード線はレールを進んでいる間だけ出す
	speedLines_->Update(isRidingRail,kDeltaTime);

	// 画面表示の更新
	stageHUD_->Update(player_->GetHp(),player_->GetMaxHp(),targetManager_->GetDestroyedCount(),targetManager_->GetTotalCount(),*comboCounter_);
}

#ifdef USE_IMGUI
// レール・プレイヤー・敵の状態確認用のデバッグウィンドウ
void GameScene::ShowStatusWindow(){
	ImGui::Begin("Rail Branch Debug");
	ImGui::Text("Rail Count: %d",railEditor_->GetRailCount());
	ImGui::Text("Active Rail Index: %d",railEditor_->GetActiveRailIndex());
	ImGui::Text("Active Rail Point Count: %d",railEditor_->GetControlPointCount());
	ImGui::Text("Rail T: %.3f",player_->GetRailT());
	ImGui::Text("Current Point Index: %d",railEditor_->GetControlPointIndexFromT(player_->GetRailT()));

	// オンレール判定・自由移動のデバッグ表示
	ImGui::Text("On Rail: %s",player_->IsOnRail()?"true":"false");
	if(!player_->IsOnRail()){
		ImGui::Text("Free Velocity Y: %.2f",player_->GetFreeVelocityY());
	}
	ImGui::Text("Jump Key: SPACE");

	// 雑魚敵のデバッグ表示
	enemyManager_->ShowDebugInfo();

	// プレイヤーの体力・無敵時間のデバッグ表示
	ImGui::Text("Player HP: %d / %d",player_->GetHp(),player_->GetMaxHp());
	ImGui::Text("Player Invincible: %.2f",player_->GetInvincibleTimer());
	ImGui::End();
}

// Editモード中の編集用パネル
void GameScene::ShowEditorPanels(){
	Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
	if(!activeCamera){
		return;
	}

	EditorWidgets::Layout L = EditorWidgets::ComputeLayout();
	// デバッグ用のメインウィンドウ(下段・左)
	EditorWidgets::BeginFixedPanel("GameScene Debug",L.bottomLeft);

	// ONにすると、Playを押したときにタイトル演出を飛ばしてすぐプレイを始める(デバッグ用)
	ImGui::Checkbox("Skip Title",&skipTitle_);

	// カメラ設定のUI
	if(ImGui::CollapsingHeader("Camera Settings")){
		Vector3 camPos = activeCamera->GetTranslate();
		if(ImGui::DragFloat3("Camera Pos",&camPos.x,0.1f)){
			activeCamera->SetTranslate(camPos);
		}

		Vector3 camRot = activeCamera->GetRotate();
		if(ImGui::DragFloat3("Camera Rotate",&camRot.x,0.01f)){
			activeCamera->SetRotate(camRot);
		}

		// 俯瞰デバッグカメラの切り替えボタン(ONにした瞬間だけ制御点全体にフィットさせる)
		debugTopCamera_->ShowToggleCheckbox(railEditor_.get());
	}

	// ライティング設定のUI
	if(ImGui::CollapsingHeader("Lighting")){
		if(PointLight* pData = object3dCommon_->GetPointLightData()){
			ImGui::Text("Point Light");
			ImGui::ColorEdit4("Point Color",&pData->color.x);
			ImGui::DragFloat3("Point Pos",&pData->position.x,0.1f);
			ImGui::DragFloat("Point Intensity",&pData->intensity,0.1f,0.0f,100.0f);
		}
		if(SpotLight* sData = object3dCommon_->GetSpotLightData()){
			ImGui::Text("Spot Light");
			ImGui::ColorEdit4("Spot Color",&sData->color.x);
		}
	}

	ModelManager::GetInstance()->UpdateLightGui();
	ImGui::End();

	Application::GetInstance()->ShowPostProcessUI();

	// シーン階層(Hierarchy)の最小版(左・上段に配置)
	EditorWidgets::BeginFixedPanel("Hierarchy",L.hierarchy);
	targetManager_->ShowHierarchy();
	ImGui::End();

	// Inspectorの最小版(右・上段に配置)
	EditorWidgets::BeginFixedPanel("Inspector",L.inspector);
	targetManager_->ShowInspector();
	ImGui::End();
}
#endif

// シーンの描画処理
void GameScene::Draw(){
	object3dCommon_->Draw();

	// 雲海の雲の部分を描画(他のオブジェクトより先に描く)
	ground_->Draw();

	// レールエディターの描画
	if(railEditor_){
		railEditor_->Draw();
	}

	// プレイヤー(人型モデル)を描画
	player_->Draw();

	// タイトルロゴを描画(タイトル中・カメラの回り込み中のみ)
	if(startSequence_){
		startSequence_->Draw3D();
	}

	// 雑魚敵を描画(撃破済みのときはEnemy側で描画をスキップする)
	enemyManager_->Draw();

	// 生存している的だけ描画
	targetManager_->Draw();

	// 発射中の弾を描画
	bulletManager_->Draw();

	// インクのしぶき・的の破片・閃光を描画
	inkEffect_->Draw();

	// 雲の上の霧を描画(半透明なので、奥にあるオブジェクトが透けて見えるよう3Dオブジェクトの最後に描く)
	ground_->DrawFog();

	// 撃破演出パーティクルの描画(3Dオブジェクトの後、2Dレティクルの前に描画する)
	if(Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera()){
		ParticleManager::GetInstance()->Draw(activeCamera);
	}

	// 画面中央固定のレティクルを描画
	// タイトル中・カメラの回り込み中は照準を使わないため表示しない
	bool isTitleShowing = startSequence_ && startSequence_->IsControllingCamera();

	// スピード線はレティクル・画面表示より奥に見えるよう先に描く
	speedLines_->Draw();

	if(!isTitleShowing){
		reticle_->Draw();
	}

	// 体力・壊した的の数・連鎖数はPlay中だけ表示する(タイトル中・カメラの回り込み中は出さない)
	if(EditorContext::GetInstance()->IsPlayMode() && !isTitleShowing){
		stageHUD_->Draw();
	}

	// 開始演出のタイトル表示(PRESS SPACE)を最前面に描画する
	if(startSequence_){
		startSequence_->Draw2D();
	}
}
