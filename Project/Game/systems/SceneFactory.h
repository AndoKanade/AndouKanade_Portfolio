#pragma once

#include "AbstractSceneFactory.h"
#include <memory> // std::unique_ptr用
#include <string> // std::string用

/// <summary>
/// 具体的なシーン生成工場クラス
/// アプリケーション固有のシーン（GameScene, ClearSceneなど）を生成する役割を持ちます。
/// </summary>
class SceneFactory : public AbstractSceneFactory{
public:
    // デストラクタ
    virtual ~SceneFactory() = default;

    /// <summary>
    /// シーン生成関数
    /// 文字列(sceneName)に対応するシーンクラスを生成して返します。
    /// </summary>
    /// <param name="sceneName">生成するシーンの名前（例: "GAME", "CLEAR"）</param>
    /// <returns>生成されたシーンのユニークポインタ。該当なしの場合はnullptr</returns>
    std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;
};