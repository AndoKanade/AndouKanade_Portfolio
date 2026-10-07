#include "EnemyManager.h"
#include "Enemy.h"
#include "PlayerBulletManager.h"
#include "ParticleManager.h"
#include "ImGuiManager.h"
#include "Editor/RailEditor.h"
#include "Editor/EnemyEditor.h"

EnemyManager::EnemyManager() = default;
EnemyManager::~EnemyManager() = default;

// 初期化処理
void EnemyManager::Initialize(Obj3dCommon* objCommon,const RailEditor* railEditor){
	objCommon_ = objCommon;

	// 敵の配置エディターの生成と初期化(保存済みJSONがあればここで読み込まれる)
	editor_ = std::make_unique<EnemyEditor>();
	editor_->Initialize();

	// 保存済みの配置が無い初回起動時のみ、従来どおりレール中間地点への自動配置で初期データを作る
	if(railEditor && editor_->GetEnemyCount() == 0){
		CreateDefaultPlacement(railEditor);
	}

	// 配置エディターの内容をシーンの敵リストへ反映する
	SyncFromEditor();
}

// 保存済みの配置が無いときの自動配置
// レールの中間地点の少し上に置き、レールに対して直角な方向へ往復させる
void EnemyManager::CreateDefaultPlacement(const RailEditor* railEditor){
	constexpr Vector3 kEnemyWorldUp = {0.0f, 1.0f, 0.0f};

	Vector3 enemyBasePos = railEditor->GetPositionOnRail(kSpawnRailT);
	Vector3 enemyRailForward = railEditor->GetForwardOnRail(kSpawnRailT);
	Vector3 enemyPatrolDir = Normalize(Cross(kEnemyWorldUp,enemyRailForward));

	editor_->AddEnemyAt(enemyBasePos + kEnemyWorldUp * kUpOffset,enemyPatrolDir,EnemyEditor::kDefaultMaxHp);

	// 自動配置直後は何も選択していない状態にしておく
	editor_->SetSelectedIndex(-1);
}

// 配置エディターの更新(配置モード中のドラッグ移動もここで処理される)
void EnemyManager::UpdateEditor(){
	if(!editor_){
		return;
	}

	editor_->Update();
	// 編集結果(追加・削除・ドラッグ移動・パラメータ変更)をシーンの敵リストへ反映する
	SyncFromEditor();
}

// 配置エディターの内容を敵のリストに反映する
// 的と同じく、体数が変わったときだけ敵の実体を生成・削除し、毎フレームの生成を避ける
void EnemyManager::SyncFromEditor(){
	if(!editor_){
		return;
	}

	size_t editorCount = static_cast<size_t>(editor_->GetEnemyCount());

	// 足りない分の敵を生成する(生成時のみモデル・弾の初期化を行う)
	while(enemies_.size() < editorCount){
		EnemyEditor::EnemyPoint point = editor_->GetEnemyPoint(static_cast<int>(enemies_.size()));

		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize(objCommon_,point.position,point.patrolDirection,point.maxHp);
		enemies_.push_back(std::move(enemy));
	}

	// 多すぎる分の敵を末尾から削除する
	while(enemies_.size() > editorCount){
		enemies_.pop_back();
	}

	// 座標・往復方向・体力をエディターの配置内容で上書きする
	for(size_t i = 0; i < enemies_.size(); ++i){
		EnemyEditor::EnemyPoint point = editor_->GetEnemyPoint(static_cast<int>(i));
		enemies_[i]->SetBasePosition(point.position);
		enemies_[i]->SetPatrolDirection(point.patrolDirection);
		enemies_[i]->SetMaxHp(point.maxHp);
	}
}

// 全ての敵の更新
// 固定パターンでの往復移動とプレイヤー検知による向きの変更を行う
void EnemyManager::Update(const Vector3& playerPosition,float deltaTime){
	for(auto& enemy : enemies_){
		enemy->Update(playerPosition,deltaTime);
	}
}

// 全ての敵の弾とプレイヤーの当たり判定
// 複数の敵が同時に撃っていても、当たった弾はすべて消すためここでは合計だけ数える
int EnemyManager::CheckHitToPlayer(const Model::BoundingSphere& playerSphere){
	int hitCount = 0;
	for(auto& enemy : enemies_){
		hitCount += enemy->CheckHitToPlayer(playerSphere);
	}
	return hitCount;
}

// 雑魚敵への被弾判定
// 的と同じく、敵と生存している弾の境界球が重なっていればヒットとする
// 敵には体力があるため、1発で撃破せず体力を減らし、0になったときだけ撃破される
void EnemyManager::CheckHitByBullets(PlayerBulletManager& bullets){
	for(auto& enemy : enemies_){
		if(!enemy->IsAlive()) continue;

		// 敵の境界球は弾ごとに変わらないため、弾の判定の前に一度だけ求める
		if(bullets.CheckHit(enemy->GetHitSphere())){
			// 被弾位置に火花パーティクルを発生させ、当たったことを見た目で分かるようにする
			ParticleManager::GetInstance()->EmitSpark(enemy->GetPosition());
			enemy->TakeDamage(bullets.GetDamage());
		}
	}
}

// 全ての敵を初期状態に戻す
void EnemyManager::Reset(){
	for(auto& enemy : enemies_){
		enemy->Reset();
	}
}

// 全ての敵を描画する
void EnemyManager::Draw(){
	for(auto& enemy : enemies_){
		enemy->Draw();
	}
}

#ifdef USE_IMGUI
// 雑魚敵のデバッグ表示(配置エディターで複数体置けるため、体ごとに体力も表示する)
void EnemyManager::ShowDebugInfo() const{
	// 生存数と敵弾の総数は、体ごとの表示と同じループでまとめて数える
	int aliveEnemyCount = 0;
	int enemyBulletCount = 0;
	for(const auto& enemy : enemies_){
		if(enemy->IsAlive()){
			++aliveEnemyCount;
		}
		enemyBulletCount += enemy->GetActiveBulletCount();
	}

	ImGui::Text("Enemies: %d (Alive %d)",static_cast<int>(enemies_.size()),aliveEnemyCount);
	ImGui::Text("Enemy Bullets: %d",enemyBulletCount);

	for(size_t i = 0; i < enemies_.size(); ++i){
		ImGui::Text("Enemy %d HP: %d / %d  Detecting: %s",
			static_cast<int>(i),
			enemies_[i]->GetHp(),
			enemies_[i]->GetMaxHp(),
			enemies_[i]->IsDetectingPlayer()?"true":"false");
	}
}
#endif
