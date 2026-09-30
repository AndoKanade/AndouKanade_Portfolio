#pragma once

/// <summary>
/// エディタの状態を保持するシングルトン(ヘッダーオンリー)
///
/// Unityの「Sceneビュー(編集) ↔ Gameビュー(実行)」のような2モードを管理する。
///  ・Editモード: ゲーム画面を中央に縮小表示し、まわりに編集パネルを並べて配置/パラメータを調整する
///  ・Playモード: 縮小をやめて全画面表示にし、編集パネルやギズモを隠したクリーンな実行画面にする
///
/// 描画側(Application)・パネル側(各シーン/エディタ)から IsPlayMode() を参照して振る舞いを切り替える。
/// </summary>
// ピクセル単位の矩形(Sceneパネルの領域を描画側へ渡すのに使う)
struct EditorRect{
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
};

class EditorContext{
public:
	static EditorContext* GetInstance(){
		static EditorContext instance;
		return &instance;
	}

	// 実行(Play)モードかどうか
	bool IsPlayMode() const{ return isPlayMode_; }
	void SetPlayMode(bool play){ isPlayMode_ = play; }
	void TogglePlayMode(){ isPlayMode_ = !isPlayMode_; }

	// Editモードでゲーム画面を縮小表示する倍率(1.0で原寸)。※Sceneパネル方式では未使用だが将来用に残す
	float GetEditViewScale() const{ return editViewScale_; }
	void SetEditViewScale(float scale){ editViewScale_ = scale; }

	// 中央「Scene」パネルの内側領域(ピクセル)。ImGui側が毎フレーム設定し、描画側(Application)が
	// ゲームのビューポートをこの矩形に合わせる。w/hが0のときは「Sceneパネル無し=全画面」とみなす。
	const EditorRect& GetSceneViewRect() const{ return sceneViewRect_; }
	void SetSceneViewRect(float x,float y,float w,float h){ sceneViewRect_ = {x, y, w, h}; }
	void ClearSceneViewRect(){ sceneViewRect_ = {}; }

private:
	EditorContext() = default;
	~EditorContext() = default;
	EditorContext(const EditorContext&) = delete;
	EditorContext& operator=(const EditorContext&) = delete;

#ifdef USE_IMGUI
	bool isPlayMode_ = false;     // 起動時は編集モード
#else
	bool isPlayMode_ = true;      // ImGuiが無い(Release)ときは編集UIを使えないため、起動時から実行モードにする
#endif
	float editViewScale_ = 0.6f;  // 編集モードの縮小率(将来用)
	EditorRect sceneViewRect_{};  // 中央Sceneパネルの領域
};
