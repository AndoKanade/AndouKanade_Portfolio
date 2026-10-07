#pragma once

// --- 標準ライブラリ ---
#include <list>
#include <random>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>

// --- DirectX / Math ---
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"

// --- エンジン内ヘッダー ---
#include "DXCommon.h"
#include "SrvManager.h"

// 前方宣言
class Camera;

// =============================================================================
// データ構造体
// =============================================================================

struct Particle{
    Vector3 translate;
    float padding1;

    Vector3 scale;
    float lifeTime;

    Vector3 velocity;
    float currentTime;

    Vector4 color;

    Vector2 uvOffset;
    uint32_t particleType;
    float padding2;
};

struct ParticleForGPU{
    Vector3 translate;
    float padding1;

    Vector3 scale;
    float lifeTime;

    Vector3 velocity;
    float currentTime;

    Vector4 color;

    Vector2 uvOffset;
    uint32_t particleType;
    float padding2;
};

struct PerView{
    Matrix4x4 viewProjection;
    Matrix4x4 billboardMatrix;
};

struct EmitterSphere{
    Vector3 translate;
    float radius;
    uint32_t count;
    float frequency;
    float frequencyTime;
    uint32_t emit;
    Vector4 color;
    Vector3 velocity;
    float lifeTime;
    uint32_t particleType;
};

struct PerFrame{
    float time;
    float deltaTime;
};

struct ParticleGroup{
    // リソース情報
    std::string textureFilePath;
    uint32_t textureSrvIndex;
    static const uint32_t kNumMaxInstance = 1024;

    // GPUリソース
    uint32_t instancingSrvIndex;
    uint32_t instancingUavIndex;
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
    ParticleForGPU* instancingData = nullptr;
    uint32_t numInstance = 0;

    // エミッター関連
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource;
    EmitterSphere* emitterData = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource;
    uint32_t freeListIndexUavIndex;
    Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource;
    uint32_t freeListUavIndex;

    Microsoft::WRL::ComPtr<ID3D12Resource> indexStagingResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> listStagingResource;


    // フラグ
    bool isRing = false;
    bool isCylinder = false;
    bool isShockwave = false;
    bool isSpark = false;
    bool isSmoke = false;
    bool isCharge = false;
    bool isAura = false;
    bool isWarp = false;
};

// =============================================================================
// クラス定義
// =============================================================================

class ParticleManager{
private:
    ParticleManager() = default;
    ~ParticleManager() = default;

public:
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    static ParticleManager* GetInstance();

    // ライフサイクル
    void Initialize(DXCommon* dxCommon,SrvManager* srvManager);
    void Finalize();
    void Update();
    void Draw(Camera* camera);

    // パーティクル操作
    void CreateParticleGroup(const std::string& name,const std::string& textureFilePath,
        bool isRing = false,bool isCylinder = false,bool isShockwave = false,
        bool isSpark = false,bool isSmoke = false,bool isCharge = false,
        bool isAura = false,bool isWarp = false);

    void Emit(const std::string& name,const Transform& emitterTransform,uint32_t count,
        const Vector4& color,const Vector3& velocity,float lifeTime);

    void EmitShockwave(const Vector3& position);
    void EmitSpark(const Vector3& position);
    void EmitSmoke(const Vector3& position);
    void EmitCharge(const Vector3& position);
    void EmitAura(const Vector3& position);
    void EmitWarp();

private:
    // 内部処理・ヘルパー
    void CreateGraphicsPipeline();
    void CreateComputePipeline();
    void CreateModel();
    void CreateRingModel();
    void CreateCylinderModel();
    Particle MakeNewParticle(const Vector3& translate);

    struct VertexData{
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // メンバ変数
    DXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::mt19937 randomEngine_;
    std::unordered_map<std::string,std::unique_ptr<ParticleGroup>> particleGroups_;

    // グラフィックスパイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;

    // コンピュートパイプライン
    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> emitComputeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitComputePipelineState_;

    // モデル用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> ringVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW ringVertexBufferView_{};
    uint32_t ringVertexCount_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> cylinderVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW cylinderVertexBufferView_{};
    uint32_t cylinderVertexCount_ = 0;

    // 定数バッファ用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
    PerView* perViewData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;
    PerFrame* perFrameData_ = nullptr;
};