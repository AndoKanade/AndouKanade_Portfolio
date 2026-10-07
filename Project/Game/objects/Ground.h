#pragma once

#include "MyMath.h"
#include <memory>
#include <vector>

// 前方宣言
class Obj3D;
class Obj3dCommon;

/// <summary>
/// 簡易的な地面
/// 板モデルをレール開始地点を基準に格子状に並べて表現する。
/// 位置・向き・大きさは生成時に決め打ちするため、毎フレームは行列の更新だけ行う。
/// </summary>
class Ground{
public:
	Ground();
	~Ground();

	/// <summary>
	/// 地面タイルを生成する
	/// </summary>
	/// <param name="objCommon">3Dオブジェクト共通設定</param>
	/// <param name="startPosition">並べる基準の位置(レール開始地点)</param>
	/// <param name="forward">並べる基準の進行方向(レール開始地点での向き)</param>
	void Initialize(Obj3dCommon* objCommon,const Vector3& startPosition,const Vector3& forward);

	// 更新処理(カメラ切り替えへの追従と行列の更新)
	void Update();

	// 描画処理
	void Draw();

	// 地面を敷いている高さを取得(落下リスタートの判定に使う)
	float GetHeight() const{ return kGroundHeight; }

private:
	// 地面タイル
	std::vector<std::unique_ptr<Obj3D>> tiles_;

	// 板モデル1枚の1辺の長さ(plane.objは-1〜1の2x2なのでこの値になる)
	static constexpr float kTilePlaneSize = 2.0f;
	// 板モデルに掛ける表示スケール(1タイルの1辺はkTilePlaneSize倍された長さになる)
	static constexpr float kTileScale = 10.0f;
	// レール開始地点から奥(進行方向)へ並べるタイル数
	static constexpr int kTileCountForward = 8;
	// レール開始地点から手前(進行方向の逆)へ並べるタイル数
	static constexpr int kTileCountBack = 1;
	// 横方向へ並べるタイル数(左右対称にするため奇数にする)
	static constexpr int kTileCountWidth = 5;
	// 地面を敷くY座標(レールの起伏に関係なく一定の高さにする)
	static constexpr float kGroundHeight = -3.0f;
	// plane.objは+Z向きの板なので、X軸を-90度回して法線を上向き(+Y)にする
	static constexpr float kRotateX = -3.14159265f * 0.5f;
};
