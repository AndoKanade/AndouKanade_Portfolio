# Game フォルダ クラス仕様書

最終更新: 2026-10-07

`Game/` 以下の各クラスについて、「何ができるか」「外から何を呼べるか」「中でどう動いているか」をまとめる。
エンジン側（`Engine/`）のクラスは対象外。

---

## 目次

1. [全体構成](#1-全体構成)
2. [systems（シーン管理）](#2-systemsシーン管理)
3. [scenes（シーン）](#3-scenesシーン)
4. [Title（ステージ開始演出）](#4-titleステージ開始演出)
5. [objects（ゲームオブジェクト）](#5-objectsゲームオブジェクト)
6. [Camera（カメラ）](#6-cameraカメラ)
7. [UI](#7-ui)
8. [Editor（配置エディター）](#8-editor配置エディター)
9. [保存ファイル一覧](#9-保存ファイル一覧)
10. [操作キー一覧](#10-操作キー一覧)

---

## 1. 全体構成

```
Game/
├── systems/   シーンの生成・切り替えの仕組み
├── scenes/    GameScene（本編）、ClearScene、GameOverScene
├── Title/     GameScene 内で動くステージ開始演出（タイトル → 回り込み → カウントダウン）
├── objects/   Player / PlayerBulletManager / TargetManager / Enemy / EnemyManager / Ground
├── Camera/    RailCamera（プレイ用カメラ）/ DebugTopCamera（俯瞰デバッグカメラ）
├── UI/        Reticle（画面中央のレティクル）
└── Editor/    RailEditor / TargetEditor / EnemyEditor（Editモード中の配置ツール）
```

### シーンの流れ

```
起動 ─→ GAME ─(レール終端に到達)─→ CLEAR ─(SPACE)─→ GAME
          │
          └─(体力0)─→ GAMEOVER ─(SPACE)─→ GAME
```

- タイトルは独立したシーンではなく、GameScene の中の開始演出（`StartSequence`）として表示する。
- GAME へ切り替わるたびに GameScene が作り直され、Play モードならタイトルから始まる。

### Edit モードと Play モード

エンジン側の `EditorContext` が持つモード。起動時は Edit モードで、画面上部の Play / Stop ボタンで切り替える。

| モード | GameScene の動き |
| --- | --- |
| Edit | ゲームは静止する。各エディターのパネルが表示され、レール・的・敵を配置できる |
| Play | 入った瞬間に `ResetPlayState()` で初期化し、開始演出をタイトルから始める。編集パネルは隠れる |

---

## 2. systems（シーン管理）

### BaseScene（`systems/BaseScene.h`）

全シーンの基底クラス（抽象クラス）。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, Input*, SpriteCommon*)` | 純粋仮想。シーンの初期化 |
| `Finalize()` | 純粋仮想。シーンの終了処理 |
| `Update()` | 純粋仮想。毎フレームの更新 |
| `Draw()` | 純粋仮想。毎フレームの描画 |
| `SetSceneManager(SceneManager*)` | シーン遷移を依頼するための SceneManager を受け取る |

- 派生クラスは `sceneManager_`（protected）を使って `ChangeScene()` を呼べる。

### AbstractSceneFactory（`systems/AbstractSceneFactory.h`）

シーン生成のインターフェース。`CreateScene(const std::string&)` を純粋仮想で持つ。

### SceneFactory（`systems/SceneFactory.h/.cpp`）

シーン名の文字列から実際のシーンを生成する。

| シーン名 | 生成されるクラス |
| --- | --- |
| `"GAME"` | GameScene |
| `"CLEAR"` | ClearScene |
| `"GAMEOVER"` | GameOverScene |
| 上記以外 | `nullptr` |

### SceneManager（`systems/SceneManager.h/.cpp`）

シングルトン。現在のシーンを 1 つだけ保持し、更新・描画・切り替えを行う。

| 関数 | 内容 |
| --- | --- |
| `GetInstance()` | インスタンスの取得 |
| `SetFactory(AbstractSceneFactory*)` | シーン生成に使う工場を登録する |
| `SetCommonPtr(Obj3dCommon*, Input*, SpriteCommon*)` | 各シーンに渡す共通リソースを登録する |
| `ChangeScene(const std::string&)` | 次のシーンを予約する。実際の切り替えは次の `Update()` の冒頭 |
| `Update()` | 予約があれば「旧シーンの Finalize → 新シーンの生成 → SetSceneManager → Initialize」を行ってから、現在のシーンを更新する |
| `Draw()` | 現在のシーンを描画する |
| `Finalize()` | 現在のシーンを終了・解放する |

- 切り替えのたびにシーンは破棄・再生成されるため、シーンをまたいで状態は残らない。

---

## 3. scenes（シーン）

### GameScene（`scenes/GameScene.h/.cpp`）

本編。レールに沿ってカメラとプレイヤーが進み、的と敵を撃つ。
プレイヤー・カメラ・弾・的・敵などの処理はそれぞれのクラスに任せ、GameScene は「呼び出す順番」「Edit / Play モードの切り替え」「開始演出とのつなぎ」「勝敗判定」「デバッグ表示」を担当する。

#### 持っているもの

| メンバ | 内容 |
| --- | --- |
| `skybox_` | 背景のスカイボックス |
| `railEditor_` | レールのデータと編集ツール（RailEditor） |
| `ground_` | 簡易的な地面（Ground） |
| `player_` | プレイヤー（Player） |
| `railCamera_` | レール追従カメラ（RailCamera） |
| `debugTopCamera_` | 俯瞰デバッグカメラ（DebugTopCamera） |
| `bulletManager_` | プレイヤーの弾（PlayerBulletManager） |
| `targetManager_` | 的（TargetManager） |
| `enemyManager_` | 雑魚敵（EnemyManager） |
| `startSequence_` | ステージ開始演出（StartSequence）。タイトルロゴもこの中で表示する |
| `reticle_` | 画面中央のレティクル（Reticle） |

#### 初期化（Initialize）

1. カメラ `"default"`（プレイ用）を作る。
2. スカイボックス、RailEditor、Ground を生成する。地面はレールの開始地点を基準に並べる。
3. GlobalVariables にグループ `GameScene` を作る。調整項目は各クラスが自分の Initialize で登録する。
4. Player、RailCamera、DebugTopCamera（カメラ `"debug_top"` を作る）、PlayerBulletManager を生成する。
5. TargetManager / EnemyManager を生成する。保存データが無い初回起動時だけ自動配置する。
6. Reticle、撃破演出用パーティクルのグループ、StartSequence を生成する。

#### 毎フレームの更新（Update）の流れ

1. スカイボックス・パーティクル・地面・レティクル・各エディターを更新する。的・敵のエディターの編集結果は、TargetManager / EnemyManager の `UpdateEditor()` で的・敵のリストに反映する。
2. `UpdateGameplay()` で次の処理を順番に行う。
   1. Play に入った瞬間は `ResetPlayState()` と `startSequence_->Start()` を呼ぶ。Edit に戻った瞬間は `startSequence_->Stop()` を呼ぶ。
   2. Play 中は `startSequence_->Update()` を呼び、`isGameplayActive`（Play 中かつ開始演出が終わってプレイ可能）を求める。
   3. マウスカーソルの表示を切り替える（`UpdateCursorVisibility()`）。
   4. `Player::UpdateRailProgress()` でレールを進め、`Player::UpdateBasePose()` で基準位置・基準向きを求める。
   5. `RailCamera::Update()` でカメラをプレイヤーの後ろ上に置き、照準の入力を反映する。
   6. クリア判定と、状態確認用のデバッグウィンドウ（`ShowStatusWindow()`）。
   7. `Player::UpdateTransform()` でプレイヤーをレールの上に立たせ、照準の方向へ向ける。開始演出中は、StartSequence が計算した位置・向きでカメラを上書きする。
   8. 敵の更新、敵弾とプレイヤーの当たり判定、ゲームオーバー判定。
   9. `Player::UpdateMovement()` でジャンプ・WASD 移動・着地判定を行う。地面まで落ちたら `ResetPlayState()` を呼ぶ。
   10. 射撃、弾の移動、的・敵と弾の当たり判定、弾の削除と行列の更新、レティクルの色の更新。
3. デバッグ用のキー操作（F1 / F4 / F5）と、Edit モード中の ImGui パネル（`ShowEditorPanels()`）を処理する。

- 4〜10 のうち入力・移動・当たり判定・勝敗判定は、`isGameplayActive` が true のときだけ動く。Edit 中・開始演出中は表示の更新だけ行う。

#### ゲームの仕様

| 項目 | 内容 |
| --- | --- |
| 射撃 | 左クリックで、プレイヤーの手元（`Player::GetMuzzlePosition()`）から、カメラの前方 30.0 の点（レティクルの先）へ向けて弾を発射する |
| 落下リスタート | どのレールにも乗れずに地面の高さまで落ちたら `ResetPlayState()` を呼ぶ。開始演出は挟まない |
| クリア | オンレール中にレールの終端に着いたら CLEAR へ移る |
| ゲームオーバー | プレイヤーの体力が 0 になったら GAMEOVER へ移る |
| 当たり判定 | すべてモデルの頂点から求めた境界球（`Model::BoundingSphere`）同士の重なりで判定する。プレイヤー・敵本体はワールド行列で変換した球（`Obj3D::GetWorldBoundingSphere()`）、弾・的は現在座標と表示スケールから求めた球を使う |

- レール移動・ジャンプ・体力などプレイヤーの仕様は Player、カメラと照準の仕様は RailCamera、弾の仕様は PlayerBulletManager、的の仕様は TargetManager を参照。

#### 主な関数

| 関数 | 内容 |
| --- | --- |
| `ResetPlayState()` | Player・RailCamera・TargetManager・EnemyManager を初期状態に戻し、弾を消し、アクティブレールを 0 番にする |
| `UpdateGameplay()` | レール進行・カメラ・プレイヤー・敵・弾・的・勝敗判定を、決まった順番で呼び出す |
| `UpdateCursorVisibility(isPlayMode)` | Edit 中はカーソルを常に表示し、Play 中は TAB で表示を切り替える |
| `ShowStatusWindow()` | 「Rail Branch Debug」ウィンドウを表示する（ImGui がある構成のみ） |
| `ShowEditorPanels()` | 「GameScene Debug」「Hierarchy」「Inspector」パネルを表示する（ImGui がある構成のみ） |

#### GlobalVariables の調整項目（グループ `GameScene`）

`resource/GlobalVariables/GameScene.json` に保存され、実行中の ImGui 編集と外部ファイルの書き換えが反映される。
グループは GameScene が作り、各項目は担当するクラスが登録・取得する。

| キー | 初期値 | 登録するクラス | 内容 |
| --- | --- | --- | --- |
| `railSpeed` | 0.05 | Player | レール全体の進行速度 |
| `cameraHeightOffset` | 0.8 | RailCamera | カメラの回転の中心をレールより上に置く量 |
| `cameraDistance` | 3.0 | RailCamera | カメラをプレイヤーの後ろへ離す距離 |
| `cameraFollowSharpness` | 8.0 | RailCamera | レールの向きへカメラが追従する速さ（大きいほど遅れが小さい） |
| `mouseSensitivity` | 0.0004 | RailCamera | マウス照準の感度 |
| `aimYawLimit` | 0.6 | RailCamera | 照準の左右の可動範囲（ラジアン） |
| `aimPitchLimit` | 0.5 | RailCamera | 照準の上下の可動範囲（ラジアン） |
| `aimHitAngle` | 0.09 | TargetManager | レティクルが的を捉えたとみなす角度（ラジアン） |

#### ImGui 表示

| ウィンドウ | 表示するモード | 内容 |
| --- | --- | --- |
| Rail Branch Debug | 常時 | レールの本数、アクティブレール、進行度、オンレール状態、敵の体力（EnemyManager が表示）、プレイヤーの体力など |
| GameScene Debug | Edit のみ | カメラ座標・回転の編集、俯瞰カメラの切り替え（DebugTopCamera が表示）、ライティング |
| Hierarchy | Edit のみ | 的の一覧。クリックで選択できる（TargetManager が表示） |
| Inspector | Edit のみ | 選択中の的の座標・生存フラグの編集（TargetManager が表示） |

### ClearScene（`scenes/ClearScene.h/.cpp`）

レールの終端に到達したときの画面。

- 背景に `resource/circle.png` を緑系の色で表示する。
- 「STAGE CLEAR!」と操作説明を ImGui で表示する（専用のフォント描画が無いため）。
- SPACE で GAME へ戻る（タイトルから始まる）。

### GameOverScene（`scenes/GameOverScene.h/.cpp`）

プレイヤーの体力が 0 になったときの画面。ClearScene と同じ構成。

- 背景に `resource/circle.png` を赤系の色で表示する。
- 「GAME OVER」と操作説明を ImGui で表示する。
- SPACE で GAME へ戻る（タイトルから始まる）。

---

## 4. Title（ステージ開始演出）

GameScene の中で「タイトル → カメラの回り込み → カウントダウン → プレイ」を進めるクラス群。
GameScene が直接触るのは StartSequence だけで、残りのクラスは StartSequence の内部で使う。

> 2026-09-30 実装。ビルドは確認済みで、実際の画面での見た目はまだ確認していない。

### StartSequence（`Title/StartSequence.h/.cpp`）

開始演出の段階を管理する。

#### 段階（Phase）

| 段階 | 内容 | 次の段階へ進む条件 |
| --- | --- | --- |
| `None` | 何もしない（Edit モード中） | `Start()` が呼ばれる |
| `Title` | プレイヤーを正面から映し、カメラを左右にゆっくり揺らす。タイトルロゴ（3D モデル）と PRESS SPACE を表示する | SPACE を押す |
| `CameraTurn` | カメラがプレイヤーの背後へ回り込む。タイトル表示は回り込みの進行に合わせて薄くなる | 回り込みが終わる |
| `Countdown` | 3, 2, 1 を表示する | GO になる |
| `Playing` | プレイ中。GO の表示が消えるまでカウントダウンの更新を続ける | なし |

#### 関数

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, SpriteCommon*, Input*)` | TitleLogo と TitleUI を生成する |
| `Start()` | `Title` から始める。カメラワーク・カウントダウン・タイトル表示も初期状態に戻す |
| `Stop()` | `None` に戻す |
| `Update(float deltaTime)` | 段階ごとの処理と、次の段階への切り替えを行う |
| `UpdateLogo(pivot, gameplayRot)` | タイトルロゴをプレイヤーの位置・向きに合わせて配置する |
| `Draw3D()` | `Title` と `CameraTurn` の間だけタイトルロゴを描画する |
| `Draw2D()` | `Title` と `CameraTurn` の間だけ PRESS SPACE を描画する |
| `IsPlayable()` | `Playing` なら true。GameScene はこれを見てゲーム処理を動かす |
| `IsControllingCamera()` | `Title` か `CameraTurn` なら true。GameScene はこれを見てカメラを上書きし、レティクルを隠す |
| `CalculateCamera(pivot, gameplayPos, gameplayRot, outPos, outRot)` | 演出中のカメラ座標・向きを求める（TitleCameraDirector に任せる） |

- スタートとジャンプはどちらも SPACE だが、押した瞬間はまだ `Playing` ではないためジャンプしない。

### TitleCameraDirector（`Title/TitleCameraDirector.h/.cpp`）

タイトル中と回り込み中のカメラワークを計算する。

#### 計算方法

- プレイ用カメラ（レール追従カメラ）の位置を、プレイヤーを中心に Y 軸まわりに回転させて求める。向きのヨーにも同じ角度を足すため、常にプレイ時と同じ構図でプレイヤーを映す。
- タイトル中は回転角を 180°（正面）にし、`sin` で左右に揺らす。カメラとプレイヤーの距離は 0.6 倍にして、少し寄った画にする。
- 回り込み中は、開始した瞬間の角度から 0°（プレイ用カメラ）まで、距離の倍率を 0.6 から 1.0 まで、`Easing::EaseInOutCubic` で補間する。
- 回り込みが終わった時点で、プレイ用カメラと完全に一致する。

#### 関数

| 関数 | 内容 |
| --- | --- |
| `Reset()` | タイトル開始時の状態に戻す |
| `UpdateIdle(float)` | タイトル中の揺れを進める |
| `BeginTurn()` | 現在の揺れの角度から回り込みを始める |
| `UpdateTurn(float)` | 回り込みを進める |
| `IsTurnFinished()` | 回り込みが終わったら true |
| `GetTurnProgress()` | 回り込みの進行度（0〜1、イージング前） |
| `Calculate(...)` | 演出中のカメラ座標・向きを求める |

#### 調整用の定数

| 定数 | 値 | 内容 |
| --- | --- | --- |
| `kTurnDuration` | 2.0 | 回り込みにかける時間（秒） |
| `kTitleDistanceScale` | 0.6 | タイトル中のカメラ距離の倍率 |
| `kGameplayDistanceScale` | 1.0 | 回り込み終了時の距離の倍率 |
| `kSwayAmplitude` | 0.08 | タイトル中の揺れ幅（ラジアン、約 5°） |
| `kSwaySpeed` | 0.8 | 揺れの速さ |

### TitleLogo（`Title/TitleLogo.h/.cpp`）

タイトルロゴの 3D モデル（`resource/Title/title.obj`、立体文字）を表示する。

- タイトル中のカメラから見て、プレイヤーの奥の上空に、文字の正面をカメラへ向けて置く。
- プレイ用カメラ（プレイヤーの約 6 後ろ）よりさらに後ろに置くため、カメラが回り込み終わると自然に画面外へ外れる。
- 上下の傾きは使わず、常に直立させる。
- モデルの原点は文字の左下なので、文字列の中心が指定位置に来るようにずらして置く。
- SPACE を押すと、回り込むカメラにかぶらないよう消える。消え方は `logoHideMode` で選び、消え終わるまでの時間は消え方ごとに決まっている（下記 LogoHidePattern）。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*)` | モデルの読み込みと、調整項目の登録 |
| `Update(pivot, gameplayRotate)` | プレイヤーの位置とプレイ用カメラのヨーを基準に配置する |
| `Draw()` | 描画。消え終わった後は描画しない |
| `Reset()` | 表示状態に戻す |
| `BeginHide()` | 縮めて消す演出を始める |
| `UpdateHide(float)` | 縮めて消す演出を進める |

#### 調整項目（GlobalVariables、グループ `Title`）

Edit モードの Global Variables パネル、または `resource/GlobalVariables/Title.json` の書き換えで変更できる（JSON の書き換えは Play 中も反映される）。

| キー | 初期値 | 内容 |
| --- | --- | --- |
| `logoOffset` | (0, 4, 10) | 文字列の中心を置く位置。プレイヤーから見て x: 右、y: 上、z: 後ろ |
| `logoScale` | 1.5 | 表示スケール |
| `logoHideMode` | 3 | スタート後の消え方の番号（LogoHidePattern の表を参照）。範囲外の番号は JumpAway になる。SPACE を押した時点の値を使う |

### LogoHidePattern（`Title/LogoHidePattern.h/.cpp`）

タイトルロゴの消え方の種類と、その計算。TitleLogo から使う。

- `CalculateLogoHideEffect(type, progress)`：進行度（0〜1）から、ロゴの大きさの倍率・位置のずれ・追加の回転・α 値を求める。
- `GetLogoHideDuration(type)`：消え方ごとの、消え終わるまでの時間。動きの多い消え方ほど長い。
- `ToLogoHideType(int, defaultType)`：設定値の番号を種類に変換する。範囲外の番号は `defaultType`（TitleLogo では JumpAway）になる。

| 番号 | 種類 | 消え方 | 時間 |
| --- | --- | --- | --- |
| 1 | Squash | 縦に潰れて横線になり、横にも縮んで消える | 0.5 秒 |
| 2 | PopOut | 一瞬ふくらんでから、勢いよく縮む | 0.5 秒 |
| 3 | JumpAway | しゃがんで溜めてから、放物線を描いて奥へジャンプしていく（初期値） | 1.1 秒 |

### TitleUI（`Title/TitleUI.h/.cpp`）

タイトル中の 2D 表示。専用の素材が無いため、既存のテクスチャを仮に使う。

| 表示 | 仮テクスチャ | 位置・大きさ | 動き |
| --- | --- | --- | --- |
| PRESS SPACE | `resource/Title/pressSpace.png` | 画面上から 80% の高さ、360 × 60 | 濃さ 0.2〜1 で点滅＋フェード |

| 関数 | 内容 |
| --- | --- |
| `Initialize(SpriteCommon*)` | テクスチャの読み込みとスプライトの生成 |
| `Reset()` | 点滅の位相と表示の濃さを初期状態に戻す |
| `Update(float deltaTime, float visibility)` | 点滅を進め、`visibility`（0〜1）を全体の濃さとして掛ける |
| `Draw()` | 描画。濃さが 0 以下なら何もしない |

### StartCountdown（`Title/StartCountdown.h/.cpp`）

プレイ開始前のカウントダウン。専用の素材が無いため、今は ImGui で画面中央に大きく表示する。

- 1 秒ごとに 3 → 2 → 1 と表示し、3 秒経つと「GO!」になる。
- GO は 0.8 秒表示してから消える。

| 関数 | 内容 |
| --- | --- |
| `Start()` | 3 から始める |
| `Stop()` | 非表示に戻す |
| `Update(float)` | 時間を進めて表示する（GO が消えるまで呼び続ける） |
| `IsCountFinished()` | GO になったら true |

### Easing（`Engine/Base/Easing.h`）

※ `Engine/` にあるが、Title のために追加したので記載する。

| 関数 | 内容 |
| --- | --- |
| `Easing::EaseInOutCubic(float t)` | ゆっくり動き出し、中盤で加速し、最後にゆっくり止まる 3 次関数 |
| `Easing::EaseInCubic(float t)` | ゆっくり動き出して、だんだん加速する |
| `Easing::EaseOutCubic(float t)` | 勢いよく動き出して、だんだん減速して止まる |
| `Easing::EaseInBack(float t)` | 一度逆方向へ少し戻ってから、加速して進む |

---

## 5. objects（ゲームオブジェクト）

### Enemy（`objects/Enemy.h/.cpp`）

雑魚敵。決まった方向に往復し、プレイヤーを見つけると撃ってくる。

#### 動き

| 項目 | 内容 |
| --- | --- |
| 見た目 | `human/sneakWalk.gltf`（赤色）、スケール 0.4 |
| 往復移動 | 基準座標を中心に、往復方向へ片側 5.0 まで、速度 3.0 で往復する |
| 検知 | プレイヤーとの距離が 25.0 以内なら検知中とし、プレイヤーの方を向く。検知していない間は進行方向を向く |
| 攻撃 | 検知中だけ撃つ。1 回撃つごとに、単発 → 3 方向拡散 → 追尾弾 の順で切り替える |
| 体力 | 最大値は配置エディターで敵ごとに設定する（下限 1）。0 になると撃破され、描画されなくなる |
| 撃破後 | 発射済みの弾は消えずに飛び続ける |

#### 攻撃の種類

| 種類 | 内容 | 弾速 | 発射間隔 |
| --- | --- | --- | --- |
| 単発（Single） | 発射した瞬間のプレイヤー方向へ直進する | 18.0 | 1.2 秒 |
| 3 方向拡散（Spread3） | プレイヤー方向を中心に、左右へ約 12° ずつ開いた 3 発を同時に撃つ | 18.0 | 2.0 秒 |
| 追尾弾（Homing） | 発射後も毎フレーム、プレイヤー方向へ少しずつ向きを変える | 12.0 | 2.5 秒 |

- 弾（赤色）は初期化時に 16 発まとめて生成し、使い回す。寿命は 4 秒。
- 弾は敵の中心より 0.5 上から発射する。

#### 関数

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, basePosition, patrolDirection, maxHp)` | モデルと弾 16 発の生成、初期状態への設定 |
| `Update(playerPosition, deltaTime)` | 弾の更新、往復移動、検知、発射、向きの更新。`deltaTime` が 0 なら動かずに表示だけ更新する |
| `Draw()` | 本体（生存中のみ）と、発射中の弾を描画する |
| `Reset()` | 位置・向き・体力・弾・攻撃の順番を初期状態に戻す |
| `TakeDamage(int)` | 体力を減らす。この攻撃で撃破されたら true を返す |
| `CheckHitToPlayer(playerSphere)` | 敵弾とプレイヤーの当たり判定。弾の境界球とプレイヤーの境界球が重なったら弾を消し、その数を返す |
| `SetBasePosition` / `SetPatrolDirection` / `SetMaxHp` | 配置エディターの編集を反映する |
| `GetPosition` / `IsAlive` / `GetHp` / `GetMaxHp` | 状態の取得 |
| `GetHitSphere()` | 本体のモデルから求めたワールド座標系の境界球を取得する（当たり判定用） |
| `IsDetectingPlayer` / `GetActiveBulletCount` | デバッグ表示用の取得 |

### Player（`objects/Player.h/.cpp`）

プレイヤー。レールに沿って自動で進み、ジャンプでレールを離れると WASD で自由に動ける。体力を持つ。

#### 仕様

| 項目 | 内容 |
| --- | --- |
| 見た目 | `human/walk.gltf`（青色）、スケール 0.3 |
| レール移動 | `railT_`（0〜1）を「制御点ごとの Speed × `railSpeed` × 経過時間」で進める。終端（1.0）で止まる |
| 表示位置 | 基準位置（オンレール中はレール上の点、オフレール中は自由移動の座標）にそのまま立たせる |
| 体の向き | カメラの左右の向き（照準の方向）。レールが傾いていても体は直立させる |
| 弾を撃ち出す位置 | 表示位置から 0.35 上（胸の高さ）。`GetMuzzlePosition()` で取得する |
| ジャンプ | SPACE でレールを離れる。上向きの初速は 6.0、重力は 9.8。離れた瞬間の向きをオフレール中の基準向きとして固定する |
| オフレール移動 | WASD で、カメラ基準の水平方向へ 8.0 で動く |
| 着地 | 落下中に「水平距離が `kOnRailHorizontalThreshold`（1.0）以内」かつ「前フレームより下へレールの高さを跨いだ」ら、そのレールに乗り移る |
| 体力 | 最大 3。敵弾が当たると 1 減り、1 秒間は無敵になる。被弾位置に火花パーティクルを出す |

#### 関数

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, paramGroup)` | モデルの生成と、調整項目 `railSpeed` の登録 |
| `Reset()` | レール先頭・オンレール・体力満タンの状態に戻す |
| `UpdateRailProgress(railEditor, isGameplayActive, deltaTime)` | プレイ可能かつオンレール中、かつ終端未到達のときだけレールを進める |
| `UpdateBasePose(railEditor)` | オンレール / オフレールに応じて、カメラ・プレイヤーの共通の基準位置と基準向きを求める |
| `UpdateTransform(cameraRotation, hopHeight)` | 表示位置と体の向きを求め、モデルの行列を更新する。タイトル中の跳ね（`hopHeight`）は見た目だけに反映する |
| `UpdateInvincible(deltaTime)` | 無敵時間を進める |
| `TakeDamage()` | 無敵中でなければ体力を 1 減らし、無敵時間を始める |
| `UpdateMovement(input, railEditor, cameraForward, cameraRight, groundHeight, deltaTime)` | ジャンプ・WASD 移動・着地判定。地面まで落ちてリスタートが必要になったら true を返す |
| `GetHitSphere()` | モデルから求めたワールド座標系の境界球を取得する（当たり判定用） |
| `GetPosition` / `GetBasePosition` / `GetBaseRotation` | 表示位置・基準位置・基準向きの取得 |
| `GetMuzzlePosition()` | 弾を撃ち出す位置の取得 |
| `HasReachedGoal` / `IsDead` | クリア・ゲームオーバーの判定用 |
| `GetRailT` / `IsOnRail` / `GetFreeVelocityY` / `GetHp` / `GetMaxHp` / `GetInvincibleTimer` | デバッグ表示用の取得 |

### PlayerBulletManager（`objects/PlayerBulletManager.h/.cpp`）

プレイヤーの弾の管理。

| 項目 | 内容 |
| --- | --- |
| 見た目 | `Sphere/sphere.obj`（青色）、スケール 0.15 |
| 弾道 | 速度 40、重力 9.8、寿命 2 秒 |
| ダメージ | 敵 1 体に 1 発あたり 1 |
| 生成 | 発射のたびに `make_unique<Obj3D>` で生成している（使い回しは未対応） |

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*)` | 弾のモデルを読み込む |
| `Fire(position, direction)` | 弾を 1 発発射する |
| `Move(deltaTime)` | 移動と生存時間の更新 |
| `CheckHit(sphere)` | 指定した境界球に重なる弾を 1 発探し、見つかればその弾を消して true を返す |
| `RemoveDeadBullets()` | 命中・寿命切れの弾をリストから削除する |
| `UpdateTransforms()` | 描画用の行列を更新する |
| `Draw()` / `Clear()` | 描画 / すべての弾を消す |
| `GetDamage()` | 弾 1 発のダメージ量を取得する |

### TargetManager（`objects/TargetManager.h/.cpp`）

的の管理。TargetEditor の配置データを的のリストに反映し、照準判定・弾との当たり判定・描画を行う。

| 項目 | 内容 |
| --- | --- |
| 見た目 | `Sphere/sphere.obj`、スケール 0.4（狙えているときは 0.6） |
| 照準判定 | 「カメラ → 的」の向きとカメラの前方ベクトルのなす角が `aimHitAngle` 以内なら狙えている。的が大きくなり、レティクルの中心ドットが赤くなる |
| 命中 | 弾と的の境界球が重なったら撃破し、火花パーティクルを出す |
| 初回の自動配置 | 保存データが無いとき、レール上の 16 か所に左右交互・上下・奥行きをずらして配置する |

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, railEditor, paramGroup)` | 調整項目 `aimHitAngle` の登録、TargetEditor の生成、初回の自動配置 |
| `UpdateEditor()` | TargetEditor を更新し、個数・座標を的のリストに反映する。個数が変わったときだけ生成・削除する |
| `Update(cameraPosition, cameraForward, bullets)` | 照準判定・弾との当たり判定・行列の更新。いずれかの的を狙えていれば true を返す |
| `Reset()` / `Draw()` | 全ての的を復活させる / 生存している的を描画する |
| `ShowHierarchy()` / `ShowInspector()` | Hierarchy / Inspector パネルの中身を表示する（ImGui がある構成のみ） |

### EnemyManager（`objects/EnemyManager.h/.cpp`）

雑魚敵の管理。EnemyEditor の配置データを敵のリストに反映し、更新・当たり判定・描画をまとめて行う。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, railEditor)` | EnemyEditor の生成。保存データが無いときは、レールの中間地点（t = 0.5）の少し上（1.0）に 1 体置く |
| `UpdateEditor()` | EnemyEditor を更新し、座標・往復方向・体力を敵のリストに反映する。体数が変わったときだけ生成・削除する |
| `Update(playerPosition, deltaTime)` | 全ての敵を更新する |
| `CheckHitToPlayer(playerSphere)` | 全ての敵の弾とプレイヤーの当たり判定。命中した弾の合計数を返す |
| `CheckHitByBullets(bullets)` | プレイヤーの弾と敵本体の当たり判定。当たった敵の体力を減らし、火花パーティクルを出す |
| `Reset()` / `Draw()` | 全ての敵を初期状態に戻す / 描画する |
| `ShowDebugInfo()` | 生存数・弾の数・体ごとの体力を表示する（ImGui がある構成のみ） |

### Ground（`objects/Ground.h/.cpp`）

雲海の地面。継ぎ目なく並べられる雲海のテクスチャを貼った `CloudSea/cloudSea.obj` を X 軸に -90° 回して水平にし、レールの開始地点を基準に 45 枚（奥 8・手前 1・横 5）並べる。1 枚の 1 辺は 20、高さは -3.0 で一定。雲の色をテクスチャのまま見せるため、ライティングは無効にしている。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*, startPosition, forward)` | 地面タイルを生成する |
| `Update()` / `Draw()` | 行列の更新 / 描画 |
| `GetHeight()` | 地面の高さを取得する（落下リスタートの判定に使う） |

---

## 6. Camera（カメラ）

### RailCamera（`Camera/RailCamera.h/.cpp`）

レール追従カメラ（三人称視点のプレイ用カメラ）。計算した位置・向きをカメラ `"default"` に反映する。

| 項目 | 内容 |
| --- | --- |
| 位置 | プレイヤーの基準位置から `cameraHeightOffset` だけ上の点を中心に、カメラの向きの後ろ側へ `cameraDistance` だけ離した位置。照準を動かすとプレイヤーの周りを回り込む |
| 向き | 基準向きへ `cameraFollowSharpness` で少し遅れて追従させた向き＋マウスの照準オフセット。リセット直後は遅れなしで合わせる |
| 照準 | マウス移動量 × `mouseSensitivity`。左右は `aimYawLimit`、上下は `aimPitchLimit` の範囲に制限する |

| 関数 | 内容 |
| --- | --- |
| `Initialize(paramGroup)` | 調整項目（`cameraHeightOffset` / `cameraDistance` / `cameraFollowSharpness` / `mouseSensitivity` / `aimYawLimit` / `aimPitchLimit`）の登録 |
| `Reset()` | 照準オフセットを 0 に戻し、次の更新で追従の遅れなしにレールの向きへ合わせる |
| `Update(basePosition, baseRotation, isInputEnabled, input, deltaTime)` | 位置・向き・前方ベクトル・右方向ベクトルを計算し、カメラに反映する |
| `GetPosition` / `GetRotation` / `GetForward` / `GetRight` | 計算結果の取得 |

### DebugTopCamera（`Camera/DebugTopCamera.h/.cpp`）

レール全体を真上から見下ろす俯瞰デバッグカメラ（カメラ `"debug_top"`）。

| 関数 | 内容 |
| --- | --- |
| `Initialize(Obj3dCommon*)` | カメラを生成し、高さ 30 から真下を向かせる |
| `HandleToggleKey(input, railEditor)` | F1 で切り替える。ON にした瞬間に、制御点全体が映る高さ（半径 × 2.2 ＋ 10）に合わせる |
| `UpdateMove(input)` | 有効な間、WASD で平面移動、QE で高さを移動する（速度 10） |
| `ShowToggleCheckbox(railEditor)` | ImGui の切り替え用チェックボックス（ImGui がある構成のみ） |
| `IsEnabled()` | 有効かどうかを取得する |

---

## 7. UI

### Reticle（`UI/Reticle.h/.cpp`）

画面中央固定のレティクル。外枠（`Reticle/reticleOutline.png`）と中心ドット（`Reticle/reticle.png`）の 2 枚構成で、どちらも 48 × 48。

| 関数 | 内容 |
| --- | --- |
| `Initialize(SpriteCommon*)` | スプライトを生成し、画面中央に置く |
| `Update()` | 行列の更新 |
| `SetAiming(bool)` | 狙えているときは中心ドットを赤、それ以外は白にする |
| `Draw()` | スプライト共通の描画前処理を行ってから描画する |

---

## 8. Editor（配置エディター）

3 つとも Edit モード中だけ動く。Play モード中は `Update()` の冒頭で何もせずに戻る。

### RailEditor（`Editor/RailEditor.h/.cpp`）

レールのデータを持ち、編集もできる。GameScene はこのクラスからレール上の位置・向きを受け取る。

#### レールの仕組み

- レールは複数本持てる。ゲームで使うのは「アクティブなレール」1 本。
- 1 本のレールは制御点（ControlPoint）の配列で、各制御点は次の値を持つ。
  - `position`：座標
  - `rotation`：Z だけをロール（バンク）として使う
  - `speed`：その区間の速度倍率
  - 分岐設定：保存はされるが、今の実行時には読まれない
- 位置は Catmull-Rom スプラインで補間する。
- 向き（ピッチ・ヨー）は進行方向から逆算し、ロールは制御点間を線形補間する。
- 描画では、アクティブなレールを黄色、それ以外を黒で表示する。

#### ImGui パネル「Rail Editor」

| 操作 | 内容 |
| --- | --- |
| レール一覧 | クリックでアクティブなレールを切り替える |
| New Rail | レールを 1 本追加し、アクティブにする |
| Add Control Point | アクティブなレールの末尾に制御点を追加する |
| Save / Load | 全レールを JSON に保存する / JSON から読み込む |
| Show Control Point Models / Show Rail Curve | 制御点の球 / 曲線の表示切り替え |
| Show Landing Range | 着地できる範囲（レールの左右、緑の点列）の表示切り替え |
| 制御点一覧 | クリックで選択し、Position / Rotation / Speed / 分岐設定を編集する。Reset で初期値に戻す。Delete で削除する（最後の 1 点は削除できない） |

#### 主な関数（GameScene から使うもの）

| 関数 | 内容 |
| --- | --- |
| `GetPositionOnRail(t)` | 進行度 t の座標 |
| `GetRotationOnRail(t)` | 進行度 t の向き（ピッチ・ヨー・ロール） |
| `GetForwardOnRail(t)` | 進行度 t の進行方向（正規化済み） |
| `GetSpeedOnRail(t)` | 進行度 t の速度倍率（制御点の Speed を補間したもの） |
| `FindNearestRail(worldPos)` | 全レールから最も近いレール・進行度・距離・最近傍点を求める。粗いサンプリング 200 点のあと、三分探索で 20 回絞り込む |
| `SwitchActiveRail(index)` | アクティブなレールを切り替える |
| `GetControlPointsCenter()` / `GetControlPointsRadius()` | 俯瞰カメラを自動で合わせるための、制御点全体の中心と半径 |
| `SetLandingRangeRadius(radius)` | 着地範囲の表示に使う許容距離を受け取る |
| `ToggleShowControlPointModels()` / `ToggleShowCurve()` | キー操作用の表示切り替え |
| `SaveToJson()` / `LoadFromJson()` | 保存・読み込み。起動時は自動で読み込む |

### TargetEditor（`Editor/TargetEditor.h/.cpp`）

的の配置データを持ち、Scene ビュー上で編集できる。

#### ImGui パネル「Target Editor」

| 操作 | 内容 |
| --- | --- |
| Placement Mode | ON の間だけ、マウスでのドラッグ移動を受け付ける |
| Add Target | カメラの前方 15.0 の位置に的を追加し、選択する |
| Delete Target | 選択中の的を削除する |
| Save / Load | `resource/target/target.json` への保存・読み込み |
| マウスドラッグ | Scene ビュー上で的を左ドラッグして動かす。レイから 1.0 以内の的を掴み、画面に平行な面の上で動かす |

- 選択中の的の座標は、GameScene の Inspector パネルからも編集できる（`GetTargetPositionRef()`）。
- 起動時に保存データを自動で読み込む。

### EnemyEditor（`Editor/EnemyEditor.h/.cpp`）

敵の配置データを持ち、Scene ビュー上で編集できる。操作感は TargetEditor と同じ。

#### 1 体分のデータ（EnemyPoint）

| 項目 | 内容 |
| --- | --- |
| `position` | 往復移動の中心座標 |
| `patrolDirection` | 往復する方向。長さがほぼ 0 になったら (1, 0, 0) に戻す |
| `maxHp` | 体力の最大値。初期値 3、下限 1 |

#### ImGui パネル「Enemy Editor」

| 操作 | 内容 |
| --- | --- |
| Placement Mode | ON の間だけ、マウスでのドラッグ移動を受け付ける |
| Add Enemy | カメラの前方 15.0 の位置に敵を追加し、選択する |
| Delete Enemy | 選択中の敵を削除する |
| Save / Load | `resource/enemy/enemy.json` への保存・読み込み |
| 敵一覧 | クリックで選択する |
| 選択中の敵 | Position / Patrol Dir / Max HP をボタン UI で編集する |
| マウスドラッグ | TargetEditor と同じ |

- 古い保存データに `patrolDirection` や `maxHp` が無くても、初期値で補って読み込める。

---

## 9. 保存ファイル一覧

| ファイル | 保存するクラス | 内容 | 反映のしかた |
| --- | --- | --- | --- |
| `resource/rail/rail.json`、`rail1.json`、`rail2.json` … | RailEditor | レールごとの制御点 | 起動時に自動で読み込む。以降は Save / Load ボタン |
| `resource/target/target.json` | TargetEditor | 的の座標 | 同上 |
| `resource/enemy/enemy.json` | EnemyEditor | 敵の座標・往復方向・最大 HP | 同上 |
| `resource/GlobalVariables/GameScene.json` | GlobalVariables（エンジン） | GameScene の調整項目 | 実行中の変更が自動で反映される |
| `resource/GlobalVariables/Title.json` | GlobalVariables（エンジン） | タイトルロゴの位置・大きさ | 同上 |

---

## 10. 操作キー一覧

### タイトル中（Play モード）

| キー | 内容 |
| --- | --- |
| SPACE | スタート（カメラの回り込みを始める） |

### プレイ中

| キー | 内容 |
| --- | --- |
| マウス移動 | 照準 |
| 左クリック | 射撃 |
| SPACE | ジャンプ（レールを離れる） |
| WASD | オフレール中の水平移動 |
| TAB | マウスカーソルの表示切り替え |

### デバッグ（モード問わず）

| キー | 内容 |
| --- | --- |
| F1 | 俯瞰デバッグカメラの切り替え。ON にした瞬間に、制御点全体が映るよう自動で合わせる |
| WASD / Q E | 俯瞰カメラ中の平面移動 / 高さの移動 |
| F4 | 制御点の球の表示切り替え |
| F5 | レール曲線の表示切り替え |

### クリア画面・ゲームオーバー画面

| キー | 内容 |
| --- | --- |
| SPACE | GAME へ戻る（タイトルから始まる） |
