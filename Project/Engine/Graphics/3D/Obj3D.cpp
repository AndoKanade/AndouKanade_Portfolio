#include "Obj3D.h"
#include "Obj3dCommon.h"
#include "CameraManager.h"
#include "Camera.h"
#include "Model.h"
#include "TextureManager.h"
#include "SrvManager.h"
#include "Logger.h"
#include "Sprite.h"
#include "MyMath.h"
#include "ModelManager.h"
#include <cassert>
#include <algorithm>

//=============================================================================
// 初期化
//=============================================================================

void Obj3D::Initialize(Obj3dCommon* object3dCommon){
    this->object3dCommon = object3dCommon;
    this->camera = object3dCommon->GetDefaultCamera();

    // マテリアル用リソースの確保と初期設定
    materialResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(Model::Material));
    materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));
    if(materialData){
        materialData->color = {1.0f, 1.0f, 1.0f, 1.0f};
        materialData->enableLighting = 1;
        materialData->alphaCutoff = Model::kDefaultAlphaCutoff;
        materialData->uvTransform = MakeIdentity4x4();
        materialData->shininess = 20.0f;
        materialData->environmentCoefficient = 0.0f;
    }

    transform = {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
    CreateTransformationMatrixData();
}

//=============================================================================
// 更新処理
//=============================================================================

void Obj3D::Update(){
    // 1. カメラ取得
    if(!camera && object3dCommon){
        camera = object3dCommon->GetDefaultCamera();
    }
    if(!camera){
        Logger::Log("Camera is still NULL!!\n");
    }

    // 2. 行列計算 (Local -> World)
    Matrix4x4 localMatrix;
    if(isUseQuaternion_){
        localMatrix = Multiply(Multiply(MakeScaleMatrix(transform.scale),MakeRotateMatrix(quaternion_)),MakeTranslateMatrix(transform.translate));
    } else{
        localMatrix = MakeAffineMatrix(transform.scale,transform.rotate,transform.translate);
    }

    Matrix4x4 worldMatrix = localMatrix;
    if(auto parentPtr = parent_.lock()){
        worldMatrix = Multiply(localMatrix,parentPtr->GetWorldMatrix());
    }

    // 3. GPUバッファ更新
    // WVPはカメラ行列の更新(シーン更新の後)が終わってから合成するため、Draw()で求める
    transformationMatrixData->World = worldMatrix;
    transformationMatrixData->WorldInverseTranspose = Transpose(Inverse(worldMatrix));

    // 4. アニメーションとスキニング更新
    if(isSkinning_){
        animationTime_ = std::fmod(animationTime_ + (1.0f / 60.0f),animation_.duration);
        skeleton_.ApplyAnimation(animation_,animationTime_);
        skeleton_.Update();

        if(auto* commandList = object3dCommon->GetDxCommon()->GetCommandList()){
            skinCluster_.Update(skeleton_,commandList,object3dCommon,skinCluster_.GetSkinningInfoAddress());
        }
    }
}

//=============================================================================
// 描画処理
//=============================================================================

void Obj3D::Draw(){
    if(model == nullptr){
        return;
    }

    auto* commandList = object3dCommon->GetDxCommon()->GetCommandList();
    auto* lightRes = ModelManager::GetInstance()->GetModelCommon()->GetLightResource();
    Camera* activeCamera = CameraManager::GetInstance()->GetActiveCamera();
    uint32_t skyboxSRVIndex = TextureManager::GetInstance()->GetSrvIndex("resource/Skybox/celestialSphere.dds");

    // カメラ合成 (WVP)
    // Update()の時点ではカメラ行列が前フレームのままのため、カメラ更新後の描画時に合成して1フレームの遅れを防ぐ
    Matrix4x4 worldViewProjectionMatrix = transformationMatrixData->World;
    if(Camera* cameraPtr = camera?camera:object3dCommon->GetDefaultCamera()){
        worldViewProjectionMatrix = Multiply(transformationMatrixData->World,cameraPtr->GetViewProjectionMatrix());
    }
    transformationMatrixData->WVP = worldViewProjectionMatrix;

    // パイプライン切り替え
    if(isSkinning_){
        commandList->SetGraphicsRootSignature(object3dCommon->GetSkinningRootSignature());
        commandList->SetPipelineState(object3dCommon->GetSkinningGraphicsPipelineState());
    } else{
        commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
        commandList->SetPipelineState(object3dCommon->GetGraphicsPipelineState());
    }

    // 共通パラメータセット
    commandList->SetGraphicsRootConstantBufferView(0,materialResource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1,transformationMatrixResource->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2,SrvManager::GetInstance()->GetGPUDescriptorHandle(model->GetModelData().material.textureIndex));
    commandList->SetGraphicsRootDescriptorTable(3,SrvManager::GetInstance()->GetGPUDescriptorHandle(skyboxSRVIndex));
    commandList->SetGraphicsRootConstantBufferView(4,lightRes->GetGPUVirtualAddress());

    if(activeCamera) commandList->SetGraphicsRootConstantBufferView(5,activeCamera->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(6,object3dCommon->GetPointLightDataGPU());
    commandList->SetGraphicsRootConstantBufferView(7,object3dCommon->GetSpotLightDataGPU());

    // 描画実行
    if(isSkinning_){
        commandList->SetGraphicsRootShaderResourceView(8,skinCluster_.GetPaletteAddress());
        model->Draw(skyboxSRVIndex,activeCamera?activeCamera->GetGPUVirtualAddress():0,&skinCluster_);
    } else{
        model->Draw(skyboxSRVIndex,activeCamera?activeCamera->GetGPUVirtualAddress():0);
    }
}

//=============================================================================
// セッター・その他ユーティリティ
//=============================================================================

void Obj3D::SetModel(const std::string& filePath){
    model = ModelManager::GetInstance()->FindModel(filePath);
}

void Obj3D::SetParent(const std::weak_ptr<Obj3D>& parent){
    this->parent_ = parent;
}

Model::BoundingSphere Obj3D::GetWorldBoundingSphere() const{
    // モデルが無いときは、現在位置を中心とした大きさ0の球を返す
    if(!model){
        return {transform.translate, 0.0f};
    }

    const Model::BoundingSphere& localSphere = model->GetBoundingSphere();
    const Matrix4x4& world = transformationMatrixData->World;

    // 中心座標をワールド行列で変換する(行ベクトル × 行列の順で掛ける)
    const Vector3& c = localSphere.center;
    Vector3 worldCenter = {
        c.x * world.m[0][0] + c.y * world.m[1][0] + c.z * world.m[2][0] + world.m[3][0],
        c.x * world.m[0][1] + c.y * world.m[1][1] + c.z * world.m[2][1] + world.m[3][1],
        c.x * world.m[0][2] + c.y * world.m[1][2] + c.z * world.m[2][2] + world.m[3][2]
    };

    // 各軸の拡大率はワールド行列の各行の長さで求まる
    // 軸ごとに拡大率が違っても球からはみ出さないよう、最も大きい拡大率を半径に掛ける
    float scaleX = Length({world.m[0][0], world.m[0][1], world.m[0][2]});
    float scaleY = Length({world.m[1][0], world.m[1][1], world.m[1][2]});
    float scaleZ = Length({world.m[2][0], world.m[2][1], world.m[2][2]});
    float maxScale = (std::max)({scaleX, scaleY, scaleZ});

    return {worldCenter, localSphere.radius * maxScale};
}

void Obj3D::CreateTransformationMatrixData(){
    size_t sizeInBytes = (sizeof(TransformationMatrix) + 0xff) & ~0xff;
    transformationMatrixResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeInBytes);
    transformationMatrixResource->Map(0,nullptr,reinterpret_cast<void**>(&transformationMatrixData));

    *transformationMatrixData = {MakeIdentity4x4(), MakeIdentity4x4(), MakeIdentity4x4()};
}

void Obj3D::CreateMaterialData(){
    materialResource = object3dCommon->GetDxCommon()->CreateBufferResource(sizeof(Model::Material));
    materialResource->Map(0,nullptr,reinterpret_cast<void**>(&materialData));
}

void Obj3D::LoadAnimation(const std::string& directoryPath,const std::string& filename){
    if(!model) return;

    animation_ = LoadAnimationFile(directoryPath,filename);
    animationTime_ = 0.0f;

    skeleton_.Create(model->GetRootNode());
    skinCluster_.Initialize(object3dCommon->GetDxCommon(),model->GetModelData(),skeleton_);
    isSkinning_ = true;
}