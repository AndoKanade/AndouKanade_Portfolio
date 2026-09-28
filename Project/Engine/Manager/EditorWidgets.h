#pragma once
#include "MyMath.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/// <summary>
/// エディタ用の共通ImGuiウィジェット(ヘッダーオンリー)
///
/// マウスドラッグ(DragFloat等)ではなく、[-] / [+] ボタンで値を増減する編集UIを提供する。
/// 各関数は値が変化したとき true を返す。
/// USE_IMGUI が無いビルド構成では何もしない(false を返す)。
/// </summary>
namespace EditorWidgets{

	// float を [-] / [+] ボタンで増減する。step が通常の増減幅、fastStep が「大きく動かす」用。
	inline bool ButtonFloat(const char* label,float& value,float step = 0.1f,float fastStep = 1.0f){
#ifdef USE_IMGUI
		bool changed = false;
		ImGui::PushID(label);

		// [--] [-] value [+] [++] の並びで表示
		if(ImGui::Button("--")){ value -= fastStep; changed = true; }
		ImGui::SameLine();
		if(ImGui::Button("-")){ value -= step; changed = true; }
		ImGui::SameLine();
		if(ImGui::Button("+")){ value += step; changed = true; }
		ImGui::SameLine();
		if(ImGui::Button("++")){ value += fastStep; changed = true; }
		ImGui::SameLine();
		ImGui::Text("%s: %.3f",label,value);

		ImGui::PopID();
		return changed;
#else
		(void)label; (void)value; (void)step; (void)fastStep;
		return false;
#endif
	}

	// int を [-] / [+] ボタンで増減する。
	inline bool ButtonInt(const char* label,int& value,int step = 1,int fastStep = 10){
#ifdef USE_IMGUI
		bool changed = false;
		ImGui::PushID(label);

		if(ImGui::Button("--")){ value -= fastStep; changed = true; }
		ImGui::SameLine();
		if(ImGui::Button("-")){ value -= step; changed = true; }
		ImGui::SameLine();
		if(ImGui::Button("+")){ value += step; changed = true; }
		ImGui::SameLine();
		if(ImGui::Button("++")){ value += fastStep; changed = true; }
		ImGui::SameLine();
		ImGui::Text("%s: %d",label,value);

		ImGui::PopID();
		return changed;
#else
		(void)label; (void)value; (void)step; (void)fastStep;
		return false;
#endif
	}

	// -----------------------------------------------------------------------
	// パネル配置ヘルパー
	// 画面(メインビューポート)の左端/右端を基準に、ウィンドウの初期位置とサイズを決める。
	// ImGuiCond_FirstUseEver なので「初回のみ」適用され、以降はユーザーが動かした配置(imgui.iniに保存)が優先される。
	// ビューポートサイズから実行時に計算するので、解像度を変えてもレイアウトが崩れない。
	// 各ウィンドウの ImGui::Begin(...) の直前で呼ぶこと。
	// -----------------------------------------------------------------------

	// 左端に配置(y は画面上端からのオフセット)
	inline void PanelLeft(float y,float width,float height,float margin = 6.0f){
#ifdef USE_IMGUI
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + margin,vp->WorkPos.y + y),ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(width,height),ImGuiCond_FirstUseEver);
#else
		(void)y; (void)width; (void)height; (void)margin;
#endif
	}

	// 右端に配置(width ぶんだけ右端から内側に寄せる)
	inline void PanelRight(float y,float width,float height,float margin = 6.0f){
#ifdef USE_IMGUI
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		float x = vp->WorkPos.x + vp->WorkSize.x - width - margin;
		ImGui::SetNextWindowPos(ImVec2(x,vp->WorkPos.y + y),ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(width,height),ImGuiCond_FirstUseEver);
#else
		(void)y; (void)width; (void)height; (void)margin;
#endif
	}

	// -----------------------------------------------------------------------
	// 固定タイルレイアウト(Unity風)
	// 画面全体をパネルで隙間なく敷き詰めるための各領域(ピクセル)を計算する。
	// 中央 sceneView は背景透過パネルにして、その領域にゲーム画面を合わせる。
	// -----------------------------------------------------------------------
	struct PanelRect{ float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f; };
	struct Layout{
		// ここから変更: 下段に並ぶパネルが4枚(GameScene Debug / Target Editor / Enemy Editor / PostProcess Settings)になったため、
		// 下段を4分割して重ならないようにする
		PanelRect toolbar, hierarchy, railEditor, inspector, globalVars, sceneView;
		PanelRect bottomLeft, bottomCenterLeft, bottomCenterRight, bottomRight;
		// ここまで変更
	};

#ifdef USE_IMGUI
	// メインビューポートを基準に固定レイアウトを計算する
	inline Layout ComputeLayout(){
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		const float ox = vp->WorkPos.x;
		const float oy = vp->WorkPos.y;
		const float W = vp->WorkSize.x;
		const float H = vp->WorkSize.y;

		const float toolbarH = 34.0f; // 上部ツールバーの高さ
		const float leftW = 300.0f;   // 左カラム幅
		const float rightW = 300.0f;  // 右カラム幅
		const float bottomH = 200.0f; // 下段の高さ

		const float contentY = toolbarH;
		const float contentH = H - toolbarH;
		const float half = contentH * 0.5f;
		const float centerX = leftW;
		const float centerW = W - leftW - rightW;

		Layout L;
		L.toolbar     = {ox,                            oy,                    W,                      toolbarH};
		L.hierarchy   = {ox,                            oy + contentY,         leftW,                  half};
		L.railEditor  = {ox,                            oy + contentY + half,  leftW,                  contentH - half};
		L.inspector   = {ox + W - rightW,               oy + contentY,         rightW,                 half};
		L.globalVars  = {ox + W - rightW,               oy + contentY + half,  rightW,                 contentH - half};
		L.sceneView   = {ox + centerX,                  oy + contentY,         centerW,                contentH - bottomH};

		// ここから変更: 下段を4等分して、パネル同士が重なって見えなくなるのを防ぐ
		// 右端のパネルだけは割り算の余りを含めた幅にして、下段に隙間ができないようにする
		const float bottomW = centerW * 0.25f;
		L.bottomLeft        = {ox + centerX,                       oy + H - bottomH, bottomW,                       bottomH};
		L.bottomCenterLeft  = {ox + centerX + bottomW,             oy + H - bottomH, bottomW,                       bottomH};
		L.bottomCenterRight = {ox + centerX + bottomW * 2.0f,      oy + H - bottomH, bottomW,                       bottomH};
		L.bottomRight       = {ox + centerX + bottomW * 3.0f,      oy + H - bottomH, centerW - bottomW * 3.0f,      bottomH};
		// ここまで変更

		return L;
	}

	// 固定パネルを開始する(移動/リサイズ/折りたたみ不可、最前面化なし)。ImGui::End() は呼び出し側で。
	inline bool BeginFixedPanel(const char* title,const PanelRect& r,ImGuiWindowFlags extraFlags = 0){
		ImGui::SetNextWindowPos(ImVec2(r.x,r.y),ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(r.w,r.h),ImGuiCond_Always);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | extraFlags;
		return ImGui::Begin(title,nullptr,flags);
	}
#endif

	// Vector3 を X/Y/Z の3行のボタンエディタで編集する。
	inline bool ButtonVector3(const char* label,Vector3& v,float step = 0.1f,float fastStep = 1.0f){
#ifdef USE_IMGUI
		bool changed = false;
		ImGui::PushID(label);
		ImGui::Text("%s",label);
		changed |= ButtonFloat("X",v.x,step,fastStep);
		changed |= ButtonFloat("Y",v.y,step,fastStep);
		changed |= ButtonFloat("Z",v.z,step,fastStep);
		ImGui::PopID();
		return changed;
#else
		(void)label; (void)v; (void)step; (void)fastStep;
		return false;
#endif
	}

} // namespace EditorWidgets
