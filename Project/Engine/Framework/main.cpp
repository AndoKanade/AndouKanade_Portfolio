#include "Application.h"
#include "D3DResourceLeakChecker.h"
#include "TimeManager.h"
#include <memory>
#include <filesystem>
#include <Windows.h>

// 変更箇所: exeを直接起動してもシェーダーやリソースの相対パス("Engine/..."等)を解決できるように、
// exeの場所から上位ディレクトリとその直下のフォルダをたどり、"Engine"と"resource"フォルダが
// 揃うプロジェクトルート(ビルド出力先とはsibling関係にある)を探してカレントディレクトリに設定する
namespace{
	constexpr int kMaxParentSearchDepth = 8;

	bool IsProjectRoot(const std::filesystem::path& directory){
		return std::filesystem::exists(directory / L"Engine") && std::filesystem::exists(directory / L"resource");
	}

	void SetCurrentDirectoryToProjectRoot(){
		wchar_t modulePath[MAX_PATH] = {};
		GetModuleFileNameW(nullptr,modulePath,MAX_PATH);

		std::filesystem::path directory = std::filesystem::path(modulePath).parent_path();

		for(int depth = 0; depth < kMaxParentSearchDepth; ++depth){
			if(IsProjectRoot(directory)){
				SetCurrentDirectoryW(directory.c_str());
				return;
			}

			// 直下の兄弟フォルダの中にプロジェクトルートがないか確認する
			std::error_code errorCode;
			for(const auto& entry : std::filesystem::directory_iterator(directory,errorCode)){
				if(entry.is_directory() && IsProjectRoot(entry.path())){
					SetCurrentDirectoryW(entry.path().c_str());
					return;
				}
			}

			directory = directory.parent_path();
		}
	}
}

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
	// exeを直接ダブルクリックしても資産パスを解決できるようにする
	SetCurrentDirectoryToProjectRoot();

	// リソースリークチェック用オブジェクト
	D3DResourceLeakChecker leakCheck;

	// アプリケーション（Framework）の生成
	std::unique_ptr<Framework> game = std::make_unique<Application>();

	// --- 初期化処理 ---
	game->Initialize();

	// --- メインループ ---
	while(true){
		// 終了リクエストが来たらループを抜ける
		if(game->IsEndRequest()){
			break;
		}

		// 更新と描画
		game->Update();
		game->Draw();

		// 時間管理マネージャーの更新
		TimeManager::GetInstance()->Update();
	}

	// --- 終了処理 ---
	game->Finalize();

	return 0;
}