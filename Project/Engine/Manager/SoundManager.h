#pragma once

// --- 標準ライブラリ ---
#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <atomic>

// --- DirectX / Windows関連 ---
#include <wrl.h>
#include <xaudio2.h>

// --- Media Foundation関連 ---
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

// --- ライブラリリンク設定 ---
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

/// <summary>
/// 音声データ構造体
/// フォーマット情報と、デコード済みの波形データを保持する
/// </summary>
struct SoundData{
	WAVEFORMATEX wfex;			// 波形フォーマット
	std::vector<BYTE> pBuffer;	// 波形データのバッファ (自動メモリ管理)
	unsigned int bufferSize;	// バッファのサイズ
};

/// <summary>
/// サウンド管理クラス (シングルトン)
/// MP3/WAVファイルのロード、再生、停止、一時停止などを管理する
/// </summary>
class SoundManager{
public: // --- シングルトンインスタンス取得 ---

	static SoundManager* GetInstance();

public: // --- システム初期化・終了 ---

	/// <summary>
	/// 初期化 (XAudio2, MediaFoundationの起動)
	/// </summary>
	void Initialize();

	/// <summary>
	/// 終了処理 (データの解放, エンジンの終了)
	/// </summary>
	void Finalize();

	/// <summary>
	/// 更新 (毎フレーム呼ぶ)
	/// 再生中に出力デバイスが失われていたら、XAudio2を作り直して既定のデバイスへ切り替える
	/// </summary>
	void Update();

public: // --- 音声ロード・再生制御 ---

	/// <summary>
	/// 音声ファイルをロード (MP3, WAVなど)
	/// </summary>
	/// <param name="filename">ファイルパス (キーとして使用)</param>
	void SoundLoadFile(const std::string& filename);

	/// <summary>
	/// 音声を再生
	/// </summary>
	/// <param name="filename">ファイルパス</param>
	/// <param name="volume">音量 (0.0=無音, 1.0=最大)</param>
	/// <param name="loop">trueで無限ループ</param>
	void PlayAudio(const std::string& filename,float volume = 1.0f,bool loop = false);

	/// <summary>
	/// 音声を停止 (停止後は最初から再生になる)
	/// </summary>
	void StopAudio(const std::string& filename);

	/// <summary>
	/// 一時停止 (再生位置を保持したまま止める)
	/// </summary>
	void PauseAudio(const std::string& filename);

	/// <summary>
	/// 再開 (一時停止した位置から再生する)
	/// </summary>
	void ResumeAudio(const std::string& filename);

	/// <summary>
	/// 再生中かどうかを確認
	/// </summary>
	bool IsPlaying(const std::string& filename);

	/// <summary>
	/// 再生中の音声の音量を変更 (再生していなければ何もしない)
	/// </summary>
	/// <param name="filename">ファイルパス</param>
	/// <param name="volume">音量 (0.0=無音, 1.0=最大)</param>
	void SetVolume(const std::string& filename,float volume);

private: // --- コンストラクタ・デストラクタ (外部からの生成禁止) ---
	SoundManager() = default;
	~SoundManager() = default;
	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

private: // --- 内部で使う型 ---

	/// <summary>
	/// XAudio2のエンジンからの通知を受け取るクラス
	/// 出力デバイスが失われたときなどに OnCriticalError が呼ばれる。
	/// 通知はXAudio2の内部スレッドから届き、その中ではエンジンを作り直せないため、フラグを立てるだけにする。
	/// </summary>
	class EngineCallback : public IXAudio2EngineCallback{
	public:
		explicit EngineCallback(std::atomic<bool>* criticalErrorFlag) : criticalErrorFlag_(criticalErrorFlag){}

		void STDMETHODCALLTYPE OnProcessingPassStart() override{}
		void STDMETHODCALLTYPE OnProcessingPassEnd() override{}
		// 出力デバイスが失われた(抜かれた・無効にされた)ときに呼ばれる
		void STDMETHODCALLTYPE OnCriticalError(HRESULT) override{ *criticalErrorFlag_ = true; }

	private:
		std::atomic<bool>* criticalErrorFlag_;
	};

	// 再生中の音1つ分
	struct ActiveVoice{
		IXAudio2SourceVoice* voice = nullptr;
		bool isLoop = false;   // 無限ループ再生か(デバイスを作り直したときに再生し直すのに使う)
		bool isPaused = false; // 一時停止中か(作り直した後も一時停止のままにするのに使う)
	};

private: // --- 内部ヘルパー関数 ---

	// 音声データのメモリ解放
	void Unload(SoundData* soundData);

	// XAudio2のエンジンとマスターボイスを作成する(失敗したら音なしの状態にしてfalseを返す)
	bool CreateEngine();

	// 再生中のボイス・マスターボイス・XAudio2のエンジンをすべて破棄する
	void DestroyEngine();

private: // --- メンバ変数 ---

	// XAudio2本体
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	// マスターボイス (最終的な出力先)
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	// 音を出せる状態か
	// 出力デバイスが無い・他のアプリに排他モードで使われているときはfalseになり、再生しても何もしない
	bool isAvailable_ = false;

	// 出力デバイスが失われたか(XAudio2の内部スレッドから書き込まれるためatomicにする)
	std::atomic<bool> hasCriticalError_ = false;
	// エンジンからの通知の受け取り先
	EngineCallback engineCallback_{&hasCriticalError_};

	// ロード済み音声データの管理コンテナ [キー:ファイル名]
	std::map<std::string,SoundData> soundDatas_;

	// 再生中のソースボイス管理コンテナ [キー:ファイル名]
	std::map<std::string,ActiveVoice> activeVoices_;
};