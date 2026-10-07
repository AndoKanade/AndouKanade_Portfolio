#pragma once

/// <summary>
/// クリア画面へ渡すプレイの結果
/// シーンは切り替えのたびに作り直されるため、GameScene がクリア直前に書き込み、ClearScene が読み出す。
/// </summary>
struct StageResult{
	// 壊した的の数
	inline static int destroyedTargetCount = 0;
	// 配置されていた的の総数
	inline static int totalTargetCount = 0;
	// 一番長く続いた連鎖数
	inline static int maxCombo = 0;
};
