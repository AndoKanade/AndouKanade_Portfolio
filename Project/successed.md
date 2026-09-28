# 実装記録

日付ごとに、実際に実装・完了した内容を記録する。

---

## 2026-09-13

### 目標

- 09/08まで: 敵が自機に向かって弾を撃ってくるようにする
- 09/13まで: 自機と敵の弾との当たり判定を実装する
- 余裕があれば: 自機に体力を実装し、弾が当たると体力を減らす

### 1. 敵の弾の発射

対象ファイル: `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp`

- Enemy クラス内に敵弾（`struct Bullet`）を追加した。
- 弾は `Initialize` で最大数ぶん（`kMaxBulletCount = 16`）まとめて生成し、以降は `isAlive` フラグで使い回す方式にした。発射のたびに `make_unique` するのを避けるため。
- プレイヤーを検知している間だけ、`kShotInterval`（1.2秒）ごとに1発発射する。非検知中は発射せず、待ち時間を戻して次の検知直後に即撃ちされないようにした。
- 弾は発射した瞬間のプレイヤー座標へ向かう等速直線運動（追尾なし・重力なし）。発射位置は敵の中心から `kBulletSpawnUpOffset`（0.5）ぶん上。
- 弾は `kBulletLifeTime`（4秒）で未使用に戻り、再利用される。
- 敵が撃破されても、発射済みの弾は飛び続けるようにした（`UpdateBullets` を本体の生存チェックより先に呼ぶ構成）。
- `Reset()` で弾と発射間隔も初期化されるため、Play開始時に前回の弾を持ち越さない。

追加した定数（すべて `Enemy.h` 内の `static constexpr`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kMaxBulletCount` | 16 | 同時に存在できる弾の最大数 |
| `kShotInterval` | 1.2f | 発射間隔（秒） |
| `kBulletSpeed` | 18.0f | 弾の移動速度（1秒あたり） |
| `kBulletLifeTime` | 4.0f | 弾の生存時間（秒） |
| `kBulletScale` | 0.2f | 弾の表示スケール |
| `kBulletSpawnUpOffset` | 0.5f | 発射位置を敵の中心より上へずらす量 |

### 2. 自機と敵弾の当たり判定

対象ファイル: `Game/objects/Enemy.cpp` / `Game/scenes/GameScene.cpp`

- `Enemy::CheckHitToPlayer(playerPosition, playerHitRadius)` を追加。発射済みの弾とプレイヤーの中心間距離で判定し、命中した弾を未使用に戻して命中数を返す。
- GameScene 側では、雑魚敵の更新直後にこの判定を呼ぶようにした。
- 命中した弾は無敵時間中でもその場で消滅させ、すり抜けて後から当たることがないようにした。

### 3. 自機の体力

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

- `playerHp_`（最大 `kPlayerMaxHp_` = 3）と、被弾後の無敵時間 `playerInvincibleTimer_` を追加した。
- 被弾すると体力を1減らし、`kPlayerInvincibleTime_`（1秒）の無敵時間を設定する。同一フレームで複数の弾が当たっても減少は1回分だけ。
- 被弾位置に火花パーティクル（`ParticleManager::EmitSpark`）を発生させ、当たったことが見た目で分かるようにした。
- Play開始時のリセット処理で、体力と無敵時間も初期状態へ戻すようにした。

追加した定数（`GameScene.h`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kPlayerMaxHp_` | 3 | 体力の最大値 |
| `kPlayerHitRadius_` | 1.0f | 敵弾が命中したとみなす距離 |
| `kPlayerInvincibleTime_` | 1.0f | 被弾後の無敵時間（秒） |

### 4. デバッグ表示

対象ファイル: `Game/scenes/GameScene.cpp`

ImGui の "Rail Branch Debug" ウィンドウに以下を追加した。

- `Enemy Bullets`: 発射中の敵弾の数
- `Player HP`: 現在の体力 / 最大体力
- `Player Invincible`: 無敵時間の残り（秒）

### ビルド確認

`MyGameEngine.sln` を Debug / x64 でビルドし、エラー・警告なしで成功することを確認した。

### 残課題

- 体力が0になっても現状は何も起きない（体力の減少が止まるだけ）。SceneFactory に GAME_OVER 相当のシーンが無いため、ゲームオーバー処理を入れるには別途シーンの追加が必要。
- 体力や被弾状態の HUD 表示（スプライト）が未実装で、現状は ImGui のデバッグ表示のみ。
- 敵弾のパラメータ（発射間隔・速度など）が `Enemy.h` の定数固定で、GlobalVariables による実行中の調整に対応していない。

---

## 2026-09-19

### 目標

- 09/16まで: 体力が0になったときにシーンを切り替えて GAMEOVER シーンを作る
- 09/20まで: 敵の配置をエディターで置けるようにする
- 余裕があれば: 敵にも体力を入れる

### 1. ゲームオーバーシーン

対象ファイル: `Game/scenes/GameOverScene.h` / `Game/scenes/GameOverScene.cpp`（新規）、`Game/systems/SceneFactory.cpp`、`Game/scenes/GameScene.cpp`

- ClearScene と同じ構成で GameOverScene を新規作成した。専用素材が無いため、背景は既存の `resource/circle.png` を赤系の色（`kBackgroundColor` = 1.0, 0.3, 0.3, 0.6）で表示している。
- 文字表示は専用のフォント描画機能が無いため、ClearScene と同様に ImGui で "GAME OVER" と操作説明を表示している。
- SPACE キーでタイトルへ戻る。
- SceneFactory に `"GAMEOVER"` の分岐を追加した。
- GameScene 側では、敵弾の被弾処理の直後に判定を追加した。Playモード中に `playerHp_` が0以下になると `sceneManager_->ChangeScene("GAMEOVER")` を呼ぶ。

前回の残課題だった「体力が0になっても何も起きない」状態が解消された。

### 2. 敵の配置エディター

対象ファイル: `Game/Editor/EnemyEditor.h` / `Game/Editor/EnemyEditor.cpp`（新規）、`Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

TargetEditor と同じ操作感になるよう、同じ構成で新規作成した。

- Editモード中のみ動作する（Playモード中は即 return してドラッグ状態も解除する）。
- `Add Enemy`: 現在のカメラ前方 `kSpawnDistance`（15.0）の位置に敵を1体追加する。追加した敵はそのまま選択状態になる。
- Sceneビュー上で敵を左ドラッグして移動できる。掴んだ瞬間のレイ方向を法線とする平面上を動かすため、画面に平行な移動になる。掴める距離は `kPickRadius`（1.0）。
- `Delete Enemy`: 選択中の敵を削除する（未選択のときは押せない）。
- 敵ごとに `Position` / `Patrol Dir`（往復移動の方向） / `Max HP` をボタンUIで編集できる。往復方向が0ベクトルに近くなったときは既定値 (1, 0, 0) に戻す。
- `Save` / `Load` で `resource/enemy/enemy.json` へ保存・再読込できる。保存形式は `position` / `patrolDirection` / `maxHp`。古い保存データに `patrolDirection` や `maxHp` が無くても既定値で補って読めるようにした。
- 配置された敵の一覧を表示し、クリックで選択できるようにした。

GameScene 側の変更:

- 単体の `std::unique_ptr<Enemy> enemy_` を `std::vector<std::unique_ptr<Enemy>> enemies_` に変更した。
- `SyncEnemiesFromEditor()` を追加した。的（`SyncTargetsFromEditor`）と同じ方式で、**体数が変わったときだけ** Enemy の実体を生成・削除し、毎フレームの生成が発生しないようにした。座標・往復方向・最大HPは毎フレーム上書きするので、エディターでの編集が即座に反映される。
- 保存データが無い初回起動時のみ、従来どおりレール中間地点（`kEnemySpawnRailT_` = 0.5）の少し上へ1体を自動配置する。
- Play開始時のリセットで全ての敵を `Reset()` するようにした。
- 敵弾とプレイヤーの当たり判定は全ての敵に対して行い、命中数を合計してから体力を1減らす。複数の敵が同時に撃っていても、当たった弾はすべて消える。

### 3. 敵の体力

対象ファイル: `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp` / `Game/scenes/GameScene.cpp`

- Enemy に `hp_` / `maxHp_` を追加した。`Reset()` で最大値まで回復する。
- `TakeDamage(damage)` を追加した。撃破済みの敵には効果が無く、体力が0になったときだけ `isAlive_` を false にして true を返す。
- `Initialize()` に `maxHp` 引数を追加し、配置エディターからの編集反映用に `SetBasePosition` / `SetPatrolDirection` / `SetMaxHp` を追加した。`SetMaxHp` は `kMinMaxHp`（1）を下回らないよう制限する（0以下だと生成直後に撃破された状態になってしまうため）。
- `Kill()` は体力も0にして、表示と状態が食い違わないようにした。
- GameScene のプレイヤー弾の判定を、1発で撃破する方式から `kBulletDamageToEnemy_`（1）ぶん体力を減らす方式に変更した。当たるたびに火花パーティクルが出て、体力が0になったときだけ撃破される。

追加した定数:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `Enemy::kDefaultMaxHp` | 3 | 体力の最大値の初期値 |
| `Enemy::kMinMaxHp` | 1 | 最大HPとして設定できる下限 |
| `EnemyEditor::kDefaultMaxHp` | 3 | Add Enemy で追加したときの最大HP |
| `EnemyEditor::kMinMaxHp` | 1 | エディターで設定できる最大HPの下限 |
| `EnemyEditor::kSpawnDistance` | 15.0f | 新規追加時にカメラ前方へ置く距離 |
| `EnemyEditor::kPickRadius` | 1.0f | マウスで敵を掴めるとみなす距離 |
| `GameScene::kBulletDamageToEnemy_` | 1 | プレイヤーの弾1発が敵に与えるダメージ量 |

### 4. エディタパネルのレイアウト調整

対象ファイル: `Engine/Manager/EditorWidgets.h`、`Game/Editor/TargetEditor.cpp`、`Game/Editor/EnemyEditor.cpp`

Enemy Editor を追加した時点で、下段に置くパネルが4枚（GameScene Debug / Target Editor / Enemy Editor / PostProcess Settings）になり、同じ位置に重なって見えなくなる問題があった。

- `Layout` に `bottomCenterLeft` / `bottomCenterRight` を追加し、`ComputeLayout()` で下段を4等分するようにした。右端のパネルだけは割り算の余りを含めた幅にして、下段に隙間ができないようにしている。
- Target Editor を `bottomCenterLeft`、Enemy Editor を `bottomCenterRight` に割り当てた。GameScene Debug（`bottomLeft`）と PostProcess Settings（`bottomRight`）は割り当て名の変更なしで、それぞれ左端・右端に配置される。

### 5. デバッグ表示

対象ファイル: `Game/scenes/GameScene.cpp`

ImGui の "Rail Branch Debug" ウィンドウの敵の表示を、複数体に対応した内容へ変更した。

- `Enemies`: 配置されている体数と生存数
- `Enemy Bullets`: 全ての敵の発射中の弾の合計数
- `Enemy n HP`: 敵ごとの現在HP / 最大HP と検知状態

生存数と弾の合計は、体ごとの表示と同じループでまとめて数えるようにした。

### その他

- `MyGameEngine.vcxproj` / `MyGameEngine.vcxproj.filters` に新規4ファイル（EnemyEditor.h/.cpp、GameOverScene.h/.cpp）を登録した。
- `CLAUDE.md` のロードマップと「現状の課題」を現状に合わせて更新した（1〜4を完了、解決済み項目を 3.3 へ移動）。

### ビルド確認

`MyGameEngine.vcxproj` を Debug / x64 でビルドし、エラー・警告なしで成功することを確認した。

### 残課題

- 敵の配置は `Save` を押さないと次回起動時に反映されない（自動保存は未対応）。
- 体力・敵のHP・撃破数などの HUD 表示（スプライト）が未実装で、現状は ImGui のデバッグ表示のみ。前回からの持ち越し。
- ゲームオーバー画面の "GAME OVER" 表示も、クリア画面と同じく ImGui での代用のまま。
- プレイヤーの弾は発射のたびに `make_unique<Obj3D>` している。敵弾は使い回しているので、同じ方式に揃えたい。
- 敵の体力以外のパラメータ（移動速度・検知範囲・発射間隔など）は `Enemy.h` の定数固定のままで、敵ごとの設定には対応していない。

---

## 2026-09-27

### 目標

- 09/24まで: レールから落ちたときにリスタートできるようにする
- 09/27まで: 敵の攻撃を増やす

### 1. レールから落ちたときのリスタート

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

- Play開始時のリセット処理を `ResetPlayState()` として切り出し、Playに入った瞬間とリスタートで共通して使うようにした。処理内容は従来のリセットと同じ（railT_ を0に戻す、レールを0番へ、的を全て復活、弾をクリア、敵を `Reset()`、体力と無敵時間を初期化）。
- オフレール中の落下判定で、どのレールにも着地できないまま一定の高さより下まで落ちたら `ResetPlayState()` を呼び、レール先頭からやり直すようにした。
- 判定の高さは専用の定数を持たず、後述の地面の高さ `kGroundHeight_` をそのまま使っている。地面の高さを変えれば判定も一緒に動く。

チェックポイント（落ちる直前のレール位置）からの復帰にはしていない。今回はレール先頭からのやり直しで実装した。

### 2. 敵の攻撃の追加（3方向拡散ショット・追尾弾）

対象ファイル: `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp`

- `AttackType`（`Single` / `Spread3` / `Homing`）を追加し、プレイヤーを検知して1回撃つごとに `kAttackOrder` の順番で切り替えるようにした。単発 → 3方向拡散 → 追尾弾 → 単発 … とローテーションする。
- **3方向拡散ショット**: プレイヤー方向を中心に、Y軸回転で左右へ `kSpreadAngle` ずつ角度をつけた3発を同時発射する。上下の角度は中央の弾と同じにしている。
- **追尾弾**: 発射後も毎フレーム進行方向をプレイヤー方向へ補間して向きを変える。補間の割合は `kHomingTurnRate * deltaTime`（1.0を超えないよう上限を設定）で、速さは `Bullet::speed` に保持して変えない。避けられる余地を残すため、通常弾より遅い `kHomingBulletSpeed` にした。
- `Bullet` に `speed` と `isHoming` を追加した。`Reset()` で両方とも初期化し、再利用した弾が前回の設定を引きずらないようにした。
- 発射間隔を攻撃の種類ごとに分け、`GetCurrentShotInterval()` で取得するようにした。弾数の多い拡散、避けにくい追尾は間隔を長めにしている。
- 発射処理は `FireCurrentAttack()`（種類ごとの振り分け）と `FireBullet(spawnPosition, direction, speed, isHoming)`（未使用の弾を1発使う）に分けた。弾は従来どおり `Initialize` で16発まとめて生成して使い回している。

追加した定数（すべて `Enemy.h` 内の `static constexpr`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kAttackOrderCount` | 3 | 攻撃を切り替える順番の要素数 |
| `kAttackOrder` | Single, Spread3, Homing | 攻撃を切り替える順番 |
| `kSpreadBulletCount` | 3 | 3方向拡散ショットで同時に発射する弾数 |
| `kSpreadAngle` | 0.21f | 隣の弾との間につける角度（ラジアン、約12度） |
| `kSpreadShotInterval` | 2.0f | 3方向拡散ショットの発射間隔（秒） |
| `kHomingBulletSpeed` | 12.0f | 追尾弾の移動速度（1秒あたり） |
| `kHomingTurnRate` | 1.5f | 追尾弾が1秒あたりに向きを補正する割合 |
| `kHomingShotInterval` | 2.5f | 追尾弾の発射間隔（秒） |

### 3. 簡易的な地面の描画

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

奥行きが分かりにくかったため、`resource/Plane/plane.obj` を使って地面を表現した。

- `CreateGroundTiles()` を追加し、レールの座標を基準に板モデルを格子状に敷き詰める。レールエディターの初期化後に呼ぶ必要がある。
- plane.obj は XY 平面の 2x2 の板（法線 +Z）なので、X軸を -90 度回して法線を上向きにしている。
- レール開始地点（t=0）の進行方向を「奥」として、奥へ 8 枚・手前へ 1 枚・横へ 5 列を並べる。並べる間隔はタイル1枚の1辺の長さそのままにしてあるため、繋ぎ目に隙間ができない。計45枚で 横100 × 奥180 の広さになり、現在のレール（x = -10.2 〜 40.7）は全域が地面の上に収まる。
- 位置・向き・大きさは生成時に決め打ちし、毎フレームはアクティブカメラの同期と行列更新だけ行う。
- テクスチャは plane.mtl の uvChecker がそのまま出るため、マス目で奥行きが分かる。

追加した定数（`GameScene.h`）:

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kGroundTilePlaneSize_` | 2.0f | 板モデル1枚の1辺の長さ |
| `kGroundTileScale_` | 10.0f | 板モデルに掛ける表示スケール（1タイル20x20になる） |
| `kGroundTileCountForward_` | 8 | 奥（進行方向）へ並べるタイル数 |
| `kGroundTileCountBack_` | 1 | 手前へ並べるタイル数 |
| `kGroundTileCountWidth_` | 5 | 横方向へ並べるタイル数 |
| `kGroundHeight_` | -3.0f | 地面を敷くY座標（落下リスタートの判定にも使う） |
| `kGroundRotateX_` | -π/2 | 板を水平にするためのX軸回転 |

### 4. 着地判定の精密化

対象ファイル: `Game/Editor/RailEditor.h` / `Game/Editor/RailEditor.cpp` / `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

まだレールに着いていないのに乗ってしまう問題があったため、判定方法を変更した。

- 従来は最近傍点までの3D距離が `kOnRailDistanceThreshold_`（2.0）以内かどうかの1条件だった。球状の判定なのでレールの横や上にいるだけで乗ってしまっていた。
- `NearestRailResult` に最近傍点のワールド座標 `position` を追加した。`FindNearestRail()` 内で既に計算している値をそのまま返すため、呼び出し側での再計算は発生しない。
- 着地条件を2つの AND に変更した。
  1. 水平方向（XZ平面）での最近傍点までの距離が `kOnRailHorizontalThreshold_` 以内（レールの真上にいる）
  2. 前フレームはレールより上、今フレームでレールの高さ以下（落下でレール面を跨いだ瞬間）
- 条件2は跨いだ瞬間のみ成立するため、許容距離を広げても「まだ着いてないのに乗る」現象は起きない。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kOnRailHorizontalThreshold_` | 1.0f | 水平方向の許容距離（`kOnRailDistanceThreshold_` = 2.0f を置き換え） |

### 5. レール間分岐移動（矢印キー操作）の削除

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

レールの乗り換えはジャンプしてプレイヤーを移動させ、着地判定で行う方式にしたため、矢印キーでの分岐操作を削除した。

- `hasPendingBranch_` / `pendingBranchTargetRailIndex_` / `pendingBranchTargetPointIndex_` を削除。
- 制御点の分岐設定を調べる分岐検知処理と、LEFT/RIGHT キーでの乗り移り処理を削除。
- クリア判定を `isRailFinished_` のみに変更した（分岐待ちの条件が無くなったため）。
- ImGui のデバッグ表示から Pending Branch / Required Key の行を削除。
- カメラ右方向ベクトル `cameraRight` の計算は、オフレール中の WASD 移動で使っているため残している。

RailEditor 側の分岐設定 UI（Inspector の Branch Target Rail/Point）と `rail.json` への保存は残しているが、実行時には読まれない状態になった。

### 6. レール・弾の色分け

対象ファイル: `Game/Editor/RailEditor.cpp` / `Game/objects/Enemy.cpp` / `Game/scenes/GameScene.cpp`

どのレールに乗っているか、どちらの弾かを見た目で判別できるようにした。

- レールごとの色分けパレット（白・水色・ピンク・黄緑）と分岐先ハイライト色を廃止し、**今乗っているレールを黄色、それ以外を黒**の2色にした。制御点の球と曲線の両方に適用される。着地してアクティブレールが切り替わると、黄色が乗り移り先へ移る。
- **敵の弾を赤、自機の弾を青**にした。マテリアルは `Obj3D` ごとに持っているため、同じ sphere.obj を使っている的やレールの制御点には影響しない。敵弾は初期化時に16発生成する方式なので、色の設定も生成時の1回だけで済む。

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `RailEditor.cpp::kActiveRailColor` | 1.0, 0.9, 0.1, 1.0 | 今乗っているレールの色（黄色） |
| `RailEditor.cpp::kInactiveRailColor` | 0.0, 0.0, 0.0, 1.0 | 乗っていないレールの色（黒） |
| `Enemy.cpp::kEnemyBulletColor` | 1.0, 0.2, 0.2, 1.0 | 敵の弾の色（赤） |
| `GameScene.cpp::kPlayerBulletColor` | 0.2, 0.4, 1.0, 1.0 | 自機の弾の色（青） |

### 7. カメラデバッグマーカーの削除

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp`

不要になったため、カメラの位置・向きを可視化するマーカー一式を削除した。

- `cameraMarker_` / `cameraFacingMarker_` / `showCameraDebugMarkers_` を削除。
- 生成・位置追従・描画、F3 キーでの表示トグル、ImGui の `Show Main Camera Markers` チェックボックスを削除。
- 向きマーカー専用の調整項目だった GlobalVariables の `noseOffset` も登録・取得ごと削除した（`resource/GlobalVariables/GameScene.json` にキーは残っているが参照されない）。
- 球モデルの読み込みは的や弾でも使うため残している。

### 8. コードの整理

対象ファイル: `Game/scenes/GameScene.h` / `Game/scenes/GameScene.cpp` / `Game/objects/Enemy.h` / `Game/objects/Enemy.cpp` / `Game/Editor/RailEditor.h` / `Game/Editor/RailEditor.cpp`

処理内容は変えず、呼び出し元の無いコードと古くなったコメントを削除した。

削除した不要なコード:

| 場所 | 削除したもの | 理由 |
| --- | --- | --- |
| `GameScene.h` | `Application* app_` | どこからも参照されていない |
| `GameScene.cpp` | `#include "SoundManager.h"` / `#include "Logger.h"` | このファイルで未使用 |
| `Enemy.h` / `Enemy.cpp` | `Kill()` | 呼び出し元が無い（撃破は `TakeDamage()` 経由のみ） |
| `RailEditor.h` / `.cpp` | `FindNearestTOnRail()` / `GetDistanceToRail()` | 呼び出し元が無い（着地判定は `FindNearestRail()` を使用） |
| `RailEditor.h` / `.cpp` | `GetTFromControlPointIndex()` / `GetBranchAt()` / `GetControlPointPosition()` / `BranchInfo` | 矢印キーの分岐移動を削除したことで呼び出し元が無くなった |

修正した古いコメント:

- 「矢印キーはレール分岐操作に使用するため」→ 矢印キー操作が無くなったため修正。
- ロードマップの番号を参照していた「(優先度1で実装)」を削除。
- 「フェンスや地面等のオブジェクト」→ このシーンにフェンスは無く、地面タイルは毎フレーム `SetCamera()` を呼ぶため誤りだったので修正。
- 機能削除の経緯を説明していたコメントを削除し、現在の挙動の説明だけを残した。
- `Enemy.h` の `Bullet` 構造体の行末コメントの桁揃えを修正。`kShotInterval` / `kBulletSpeed` を「単発」「単発・3方向拡散用」と明示。

### その他

- `CLAUDE.md` の 4.3 を更新した。「変更箇所にはわかりやすい目印をつけること」を、「変更箇所を示す目印コメント（「ここから追加」「ここまで追加」など）は書かないこと。処理の意図を説明するコメントのみ記述する」に変更した。

### ビルド確認

`MyGameEngine.vcxproj` を Debug / x64 でビルドし、エラー・警告なしで成功することを確認した。

### 残課題

- リスタートはレール先頭からのやり直しのみ。チェックポイント（落ちる直前のレール位置）からの復帰は未実装。レールを離れた瞬間の `GetActiveRailIndex()` と `railT_` を保存しておけば対応できる。
- 敵の攻撃の種類は全ての敵で共通の固定ローテーション。敵ごとに攻撃を選べるようにするには EnemyEditor と `enemy.json` への追加が必要。
- 地面は描画のみで当たり判定は無い。また奥行き方向はレール開始地点の進行方向で固定なので、途中で大きく曲がるレールを作ると地面から外れる（レールを一定間隔でサンプリングしてタイルを敷く方式にすれば対応できる）。
- ImGui のウィンドウ名が分岐機能削除後も `"Rail Branch Debug"` のまま。
- RailEditor の分岐設定 UI と `rail.json` の `branchTargetRailIndex` / `branchTargetPointIndex` は残っているが、実行時に読まれない状態。
- プレイヤーの弾は発射のたびに `make_unique<Obj3D>` している。敵弾は使い回しているので、同じ方式に揃えたい。前回からの持ち越し。
- 体力・敵のHP・撃破数などの HUD 表示（スプライト）が未実装で、現状は ImGui のデバッグ表示のみ。前回からの持ち越し。
- ゲームオーバー画面・クリア画面の文字表示は ImGui での代用のまま。前回からの持ち越し。
