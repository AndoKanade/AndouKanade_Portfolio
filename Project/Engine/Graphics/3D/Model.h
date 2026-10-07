#pragma once

class SkinCluster;

#include "ModelCommon.h"
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl.h>
#include <map>
#include "MyMath.h"
#include "Skeleton.h"
#include <assimp/scene.h>

struct aiNode;

class Model{
public:
	// 頂点データ
	struct VertexData{
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
		Vector4 weight;
		int32_t index[4];
	};

	// マテリアル読み込みデータ
	struct MaterialData{
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	struct VertexWeightData{
		float weight;
		uint32_t vertexIndex;
	};
	struct JointWeightData{
		Matrix4x4 inverseBindPoseMatrix;
		std::vector<VertexWeightData> vertexWeights;
	};

	// モデルデータ全体
	struct ModelData{
		std::vector<VertexData> vertices;
		std::vector<uint32_t> indices;
		MaterialData material;
		Node rootNode;
		std::map<std::string,JointWeightData> skinClusterData;
	};

	// マテリアル定数バッファ
	struct Material{
		Vector4 color;
		int32_t enableLighting;
		// テクスチャのアルファがこの値以下のピクセルは描画しない(半透明にしたいものは0にする)
		float alphaCutoff;
		float padding[2];
		Matrix4x4 uvTransform;
		float shininess;
		float environmentCoefficient;
	};

	// アルファテストの既定のしきい値(半分以上透けているピクセルは描画しない)
	static constexpr float kDefaultAlphaCutoff = 0.5f;

	// 当たり判定用の境界球(モデルの全頂点を包む球)
	struct BoundingSphere{
		Vector3 center = {0.0f, 0.0f, 0.0f}; // 球の中心座標
		float radius = 0.0f;                 // 球の半径

		// 球同士が重なっているかどうか(中心間距離が半径の和以下なら重なっている)
		// 平方根の計算を避けるため、距離の2乗と半径の和の2乗で比較する
		bool IsHit(const BoundingSphere& other) const{
			Vector3 diff = center - other.center;
			float radiusSum = radius + other.radius;
			return Dot(diff,diff) <= radiusSum * radiusSum;
		}
	};

public:
	// 初期化
	void Initialize(ModelCommon* modelCommon,const std::string& directorypath,const std::string& filename);

	// 描画
	void Draw(uint32_t skyboxTextureIndex,D3D12_GPU_VIRTUAL_ADDRESS cameraAddress,SkinCluster* skinCluster = nullptr);
	// テクスチャのセット
	void SetTexture(const std::string& texturefilePath);

	/// <summary>
	/// 頂点バッファの中身を書き換える(頂点を動かすアニメーション用)
	/// 頂点数は読み込み時と同じであること。インデックスは変わらない前提
	/// </summary>
	/// <param name="vertices">書き込む頂点データ</param>
	void UpdateVertices(const std::vector<VertexData>& vertices);

	// RootNodeの取得
	const Node& GetRootNode() const{ return modelData.rootNode; }

	// .mtlファイルの読み込み
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,const std::string& filename);

	// .objファイルの読み込み
	static ModelData LoadModelFile(const std::string& directoryPath,const std::string& filename);

	const ModelData& GetModelData() const{ return modelData; }

	// モデル座標系での境界球を取得
	const BoundingSphere& GetBoundingSphere() const{ return boundingSphere_; }

	/// <summary>
	/// 境界球を、指定した位置・拡大率でワールド座標系に置いたときの球を取得する
	/// 回転は考慮しないため、球のモデルなど回転しても形が変わらないもの向け
	/// </summary>
	/// <param name="translate">ワールド座標</param>
	/// <param name="scale">拡大率(全軸共通)</param>
	BoundingSphere GetBoundingSphere(const Vector3& translate,float scale) const{
		return {translate + boundingSphere_.center * scale, boundingSphere_.radius * scale};
	}

private:
	// 全頂点を包む境界球を計算する(読み込み時に一度だけ行う)
	void CalculateBoundingSphere();

	// 頂点バッファの作成
	void CreateVertexData();

	// インデックスバッファの作成
	void CreateIndexData();

	// マテリアルバッファの作成
	void CreateMaterialData();

	// Assimpのノードを読み込む
	static Node ReadNode(aiNode* node);

private:
	// 共通リソースへのポインタ
	ModelCommon* modelCommon_ = nullptr;

	// CPU側のモデルデータ
	ModelData modelData;

	// モデル座標系での境界球(当たり判定用)
	BoundingSphere boundingSphere_;

	// 頂点バッファ関連リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	VertexData* vertexData = nullptr;

	// インデックスバッファ関連リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource;
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};

	// マテリアルバッファ関連リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;
};