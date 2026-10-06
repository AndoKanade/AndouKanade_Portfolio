#include "SoundManager.h"
#include <cassert>
#include <iostream>

// --- 高速化・プロパティ操作用 ---
#include <propvarutil.h> 
#pragma comment(lib, "propsys.lib")

// ==========================================================================
// シングルトンインスタンス取得
// ==========================================================================
SoundManager* SoundManager::GetInstance(){
	static SoundManager instance;
	return &instance;
}

// ==========================================================================
// 初期化
// ==========================================================================
void SoundManager::Initialize(){
	HRESULT result;

	// 1. Media Foundationの初期化 (ローカルファイル再生用設定)
	result = MFStartup(MF_VERSION,MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(result));

	// 2. XAudio2エンジンの作成
	result = XAudio2Create(&xAudio2_,0,XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	// 3. マスターボイスの作成
	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(result));
}

// ==========================================================================
// 終了処理
// ==========================================================================
void SoundManager::Finalize(){
	// 再生中ボイスの破棄
	for(auto& pair : activeVoices_){
		if(pair.second){
			pair.second->DestroyVoice();
		}
	}
	activeVoices_.clear();

	// データの解放
	for(auto& pair : soundDatas_){
		Unload(&pair.second);
	}
	soundDatas_.clear();

	// XAudio2解放
	xAudio2_.Reset();

	// Media Foundation終了
	MFShutdown();
}

// ==========================================================================
// 音声ファイルロード (MP3/WAV対応 + reserve最適化)
// ==========================================================================
void SoundManager::SoundLoadFile(const std::string& filename){
	// 既にロード済みなら何もしない
	if(soundDatas_.find(filename) != soundDatas_.end()){
		return;
	}

	HRESULT hr;

	// 1. ファイル名(string)をワイド文字(wstring)に変換
	int size_needed = MultiByteToWideChar(CP_UTF8,0,&filename[0],(int)filename.size(),NULL,0);
	std::wstring wFilename(size_needed,0);
	MultiByteToWideChar(CP_UTF8,0,&filename[0],(int)filename.size(),&wFilename[0],size_needed);

	// 2. SourceReader (読み込み用クラス) の作成
	IMFSourceReader* pMFSourceReader = nullptr;
	hr = MFCreateSourceReaderFromURL(wFilename.c_str(),NULL,&pMFSourceReader);
	assert(SUCCEEDED(hr));

	// 3. メディアタイプの選択 (PCMへの変換設定)
	IMFMediaType* pMFMediaType = nullptr;
	MFCreateMediaType(&pMFMediaType);
	pMFMediaType->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio);
	pMFMediaType->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_PCM);

	hr = pMFSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,nullptr,pMFMediaType);
	assert(SUCCEEDED(hr));
	pMFMediaType->Release();
	pMFMediaType = nullptr;

	// 4. 最終的なフォーマットを取得
	hr = pMFSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,&pMFMediaType);
	assert(SUCCEEDED(hr));

	WAVEFORMATEX* waveFormat = nullptr;
	UINT32 waveFormatSize = 0;
	MFCreateWaveFormatExFromMFMediaType(pMFMediaType,&waveFormat,&waveFormatSize);

	// -------------------------------------------------------------------
	// ★ 高速化処理：全体のデータ量を予測してメモリを予約(reserve)する
	// -------------------------------------------------------------------
	std::vector<BYTE> mediaData;
	PROPVARIANT var;
	PropVariantInit(&var);

	// メディアソース全体のプロパティから「再生時間(Duration)」を取得
	hr = pMFSourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,MF_PD_DURATION,&var);

	if(SUCCEEDED(hr)){
		// MF_PD_DURATION は 100ナノ秒単位
		LONGLONG duration = var.hVal.QuadPart;
		double durationInSeconds = (double)duration / 10000000.0;

		// 予測サイズ = 時間(秒) × 1秒あたりのバイト数
		size_t estimatedSize = (size_t)(durationInSeconds * waveFormat->nAvgBytesPerSec);

		// reserve() でメモリ確保だけ先に行う (再割り当て防止)
		mediaData.reserve(estimatedSize);

		PropVariantClear(&var);
	}
	// -------------------------------------------------------------------

	// 5. データの読み込みループ
	while(true){
		IMFSample* pMFSample = nullptr;
		DWORD dwFlags = 0;
		hr = pMFSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,nullptr,&dwFlags,nullptr,&pMFSample);
		assert(SUCCEEDED(hr));

		if(dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) break;

		if(pMFSample){
			IMFMediaBuffer* pMFMediaBuffer = nullptr;
			pMFSample->ConvertToContiguousBuffer(&pMFMediaBuffer);

			BYTE* pBuffer = nullptr;
			DWORD cbCurrentLength = 0;
			pMFMediaBuffer->Lock(&pBuffer,nullptr,&cbCurrentLength);

			// vectorにデータを追記 (reserve済みなので高速)
			size_t oldSize = mediaData.size();
			mediaData.resize(oldSize + cbCurrentLength);
			memcpy(&mediaData[oldSize],pBuffer,cbCurrentLength);

			pMFMediaBuffer->Unlock();
			pMFMediaBuffer->Release();
			pMFSample->Release();
		}
	}

	// 6. SoundData構造体に格納
	SoundData soundData = {};
	soundData.wfex = *waveFormat;
	soundData.pBuffer = mediaData;
	soundData.bufferSize = (unsigned int)mediaData.size();
	soundDatas_[filename] = soundData;

	// 後始末
	CoTaskMemFree(waveFormat);
	pMFMediaType->Release();
	pMFSourceReader->Release();
}

// ==========================================================================
// データの解放 (内部用)
// ==========================================================================
void SoundManager::Unload(SoundData* soundData){
	soundData->pBuffer.clear();
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

// ==========================================================================
// 音声再生 (PlayAudio)
// ==========================================================================
void SoundManager::PlayAudio(const std::string& filename,float volume,bool loop){
	auto it = soundDatas_.find(filename);
	assert(it != soundDatas_.end());

	// 二重再生防止：同じファイルが再生中なら停止して再利用
	if(activeVoices_.find(filename) != activeVoices_.end()){
		StopAudio(filename);
	}

	SoundData& soundData = it->second;
	HRESULT result;
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoice,&soundData.wfex);
	assert(SUCCEEDED(result));

	pSourceVoice->SetVolume(volume);

	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer.data();
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;
	buf.LoopCount = loop?XAUDIO2_LOOP_INFINITE:0;

	result = pSourceVoice->SubmitSourceBuffer(&buf);
	assert(SUCCEEDED(result));

	result = pSourceVoice->Start();
	assert(SUCCEEDED(result));

	activeVoices_[filename] = pSourceVoice;
}

// ==========================================================================
// 停止 (StopAudio)
// ==========================================================================
void SoundManager::StopAudio(const std::string& filename){
	auto it = activeVoices_.find(filename);
	if(it != activeVoices_.end()){
		it->second->Stop();
		it->second->FlushSourceBuffers();
		it->second->DestroyVoice();
		activeVoices_.erase(it);
	}
}

// ==========================================================================
// 一時停止 (PauseAudio)
// ==========================================================================
void SoundManager::PauseAudio(const std::string& filename){
	auto it = activeVoices_.find(filename);
	if(it != activeVoices_.end()){
		it->second->Stop();
	}
}

// ==========================================================================
// 再開 (ResumeAudio)
// ==========================================================================
void SoundManager::ResumeAudio(const std::string& filename){
	auto it = activeVoices_.find(filename);
	if(it != activeVoices_.end()){
		it->second->Start();
	}
}

// ==========================================================================
// 再生中チェック
// ==========================================================================
bool SoundManager::IsPlaying(const std::string& filename){
	return activeVoices_.find(filename) != activeVoices_.end();
}

// ==========================================================================
// 音量変更 (SetVolume)
// ==========================================================================
void SoundManager::SetVolume(const std::string& filename,float volume){
	auto it = activeVoices_.find(filename);
	if(it != activeVoices_.end()){
		it->second->SetVolume(volume);
	}
}