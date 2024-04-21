#include "../../Common/d3dApp.h"
#include "../../Common/d3dUtil.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#pragma warning(disable:4996)
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct RenderItem
{
    RenderItem() = default;

    XMFLOAT4X4 World = MathHelper::Identity4x4();

    int NumFramesDirty = gNumFrameResources;

    UINT ObjCBIndex = -1;

    Material* mat = nullptr;
    MeshGeometry* Geo = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer :int
{
    Opaque = 0,
    Count
};

class LitColumnsApp : public D3DApp
{
public:
    LitColumnsApp(HINSTANCE hInstance);
    LitColumnsApp(const LitColumnsApp& rhs) = delete;
    LitColumnsApp& operator=(const LitColumnsApp& other) = delete;
    ~LitColumnsApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void UpdateCamera(const GameTimer& gt);
    void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
    void UpdateMainPassCB(const GameTimer& gt);

    //requirements
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildPSOs();//shaders
    void BuildShapeGeometry();
    void BuildSkullGeometry();
    void BuildFrameResources();//renderitem
    void BuildMaterials();
    void BuildRenderItems();//shapegeometry
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

private:
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    std::vector<std::unique_ptr<RenderItem>> mAllRitems;

    std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

    PassConstants mMainPassCB;

    UINT mPassCbvOffset = 0;

    bool mIsWireFrame = false;

    XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f * XM_PI;
    float mPhi = 0.2f * XM_PI;
    float mRadius = 15.0f;
    float mRadiusMin = 5.0f, mRadiusMax = 150.0f;
    float mRotateSensitivity = 0.25f;
    float mMoveSensitivity = 0.05f;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        LitColumnsApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

LitColumnsApp::LitColumnsApp(HINSTANCE hInstance) : D3DApp(hInstance) {
    mMainWndCaption = L"Land and Waves App";
}
LitColumnsApp::~LitColumnsApp() {}

bool LitColumnsApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
    BuildSkullGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();
    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    FlushCommandQueue();
    //OutputDebugString(L"Initialize Complete\n");
    return true;
}

void LitColumnsApp::OnResize()
{
    D3DApp::OnResize();

    XMStoreFloat4x4(&mProj, XMMatrixPerspectiveFovLH(XM_PIDIV4, AspectRatio(), 1.0f, 1000.0f));
}


void LitColumnsApp::Update(const GameTimer& gt)
{
    UpdateCamera(gt);

    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    UpdateObjectCBs(gt);
    UpdateMainPassCB(gt);
    UpdateMaterialCBs(gt);
}

void LitColumnsApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    mCurrFrameResource->Fence = ++mCurrentFence;
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void LitColumnsApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;
    SetCapture(mhMainWnd);
}

void LitColumnsApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void LitColumnsApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if (btnState & MK_LBUTTON)
    {
        mTheta -= XMConvertToRadians(mRotateSensitivity * (x - mLastMousePos.x));
        mPhi -= XMConvertToRadians(mRotateSensitivity * (y - mLastMousePos.y));
        mPhi = MathHelper::Clamp(mPhi, 0.001f, 0.999f * XM_PI);
    }
    else if (btnState & MK_RBUTTON)
    {
        int dx = x - mLastMousePos.x;
        int dy = y - mLastMousePos.y;
        mRadius += mMoveSensitivity * (dx - dy);
        mRadius = MathHelper::Clamp(mRadius, mRadiusMin, mRadiusMax);
    }
    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void LitColumnsApp::UpdateCamera(const GameTimer& gt)
{
    XMVECTOR eyePos = MathHelper::SphericalToCartesian(mRadius, mTheta, mPhi);
    XMStoreFloat4x4(&mView, XMMatrixLookAtLH(eyePos,
        XMVectorZero(), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
    XMStoreFloat3(&mEyePos, eyePos);
}

void LitColumnsApp::UpdateObjectCBs(const GameTimer& gt)
{
    auto currObjectCB = mCurrFrameResource->ObjectCB.get();
    for (auto& e : mAllRitems)
    {
        if (e->NumFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e->World);

            ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

            currObjectCB->CopyData(e->ObjCBIndex, objConstants);

            e->NumFramesDirty--;
        }
    }
}

void LitColumnsApp::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
    for (auto& e : mMaterials)
    {
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstants.FresnelR0 = mat->FresnelR0;
            matConstants.Roughness = mat->Roughness;
            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);
            mat->NumFramesDirty--;
        }
    }
}

void LitColumnsApp::UpdateMainPassCB(const GameTimer& gt)
{
    XMMATRIX view = XMLoadFloat4x4(&mView);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);

    XMMATRIX viewProj = XMMatrixMultiply(view, proj);
    XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
    mMainPassCB.EyePosW = mEyePos;
    mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
    mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
    mMainPassCB.NearZ = 1.0f;
    mMainPassCB.FarZ = 1000.0f;
    mMainPassCB.TotalTime = gt.TotalTime();
    mMainPassCB.DeltaTime = gt.DeltaTime();
    mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

    for (int i = 0; i < 10; i++)
    {
        mMainPassCB.Lights[i].FalloffStart = 0.3f;
        mMainPassCB.Lights[i].FalloffEnd = 10.0f;
        mMainPassCB.Lights[i].Strength = { 1.0f, 1.0f, 1.0f };
        mMainPassCB.Lights[i].Direction = { 0.0f, -1.0f, 0.0f };
        mMainPassCB.Lights[i].SpotPower = 2.0f * i;
    }
    for (int i = 0; i < 5; ++i)
    {
        mMainPassCB.Lights[2 * i].Position = { -5.0f, 3.5f, -10.0f + 5.0f * i };
        mMainPassCB.Lights[2 * i + 1].Position = { 5.0f, 3.5f, -10.0f + 5.0f * i };
    }

    auto currPassCB = mCurrFrameResource->PassCB.get();
    currPassCB->CopyData(0, mMainPassCB);
}

void LitColumnsApp::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsConstantBufferView(2);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    
    ComPtr<ID3DBlob> serializedRootSig = nullptr, errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void LitColumnsApp::BuildRenderItems()
{
    auto skullRItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skullRItem->World, XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f));
    skullRItem->ObjCBIndex = 0;
    skullRItem->Geo = mGeometries["skullGeo"].get();
    skullRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    skullRItem->IndexCount = skullRItem->Geo->DrawArgs["skull"].IndexCount;
    skullRItem->StartIndexLocation = skullRItem->Geo->DrawArgs["skull"].StartIndexLocation;
    skullRItem->BaseVertexLocation = skullRItem->Geo->DrawArgs["skull"].BaseVertexLocation;
    skullRItem->mat = mMaterials["skullMat"].get();
    mAllRitems.push_back(std::move(skullRItem));

    auto boxRItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&boxRItem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
    boxRItem->ObjCBIndex = 1;
    boxRItem->Geo = mGeometries["shapeGeo"].get();
    boxRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxRItem->IndexCount = boxRItem->Geo->DrawArgs["box"].IndexCount;
    boxRItem->StartIndexLocation = boxRItem->Geo->DrawArgs["box"].StartIndexLocation;
    boxRItem->BaseVertexLocation = boxRItem->Geo->DrawArgs["box"].BaseVertexLocation;
    boxRItem->mat = mMaterials["stone0"].get();
    mAllRitems.push_back(std::move(boxRItem));

    auto gridRItem = std::make_unique<RenderItem>();
    gridRItem->World = MathHelper::Identity4x4();
    gridRItem->ObjCBIndex = 2;
    gridRItem->Geo = mGeometries["shapeGeo"].get();
    gridRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRItem->IndexCount = gridRItem->Geo->DrawArgs["grid"].IndexCount;
    gridRItem->StartIndexLocation = gridRItem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRItem->BaseVertexLocation = gridRItem->Geo->DrawArgs["grid"].BaseVertexLocation;
    gridRItem->mat = mMaterials["tile0"].get();
    mAllRitems.push_back(std::move(gridRItem));

    UINT objCBIndex = 3;
    for (int i = 0; i < 5; ++i)
    {
        auto leftCylRItem = std::make_unique<RenderItem>();
        auto rightCylRItem = std::make_unique<RenderItem>();
        auto leftSphereRItem = std::make_unique<RenderItem>();
        auto rightSphereRItem = std::make_unique<RenderItem>();
        XMStoreFloat4x4(&leftCylRItem->World, XMMatrixTranslation(-5.0f, 1.5f, -10.0f + 5.0f * i));
        leftCylRItem->ObjCBIndex = objCBIndex++;
        leftCylRItem->Geo = mGeometries["shapeGeo"].get();
        leftCylRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftCylRItem->IndexCount = leftCylRItem->Geo->DrawArgs["cylinder"].IndexCount;
        leftCylRItem->StartIndexLocation = leftCylRItem->Geo->DrawArgs["cylinder"].StartIndexLocation;
        leftCylRItem->BaseVertexLocation = leftCylRItem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
        leftCylRItem->mat = mMaterials["bricks0"].get();

        XMStoreFloat4x4(&rightCylRItem->World, XMMatrixTranslation(+5.0f, 1.5f, -10.0f + 5.0f * i));
        rightCylRItem->ObjCBIndex = objCBIndex++;
        rightCylRItem->Geo = mGeometries["shapeGeo"].get();
        rightCylRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightCylRItem->IndexCount = rightCylRItem->Geo->DrawArgs["cylinder"].IndexCount;
        rightCylRItem->StartIndexLocation = rightCylRItem->Geo->DrawArgs["cylinder"].StartIndexLocation;
        rightCylRItem->BaseVertexLocation = rightCylRItem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
        rightCylRItem->mat = mMaterials["bricks0"].get();

        XMStoreFloat4x4(&leftSphereRItem->World, XMMatrixTranslation(-5.0f, 3.5f, -10.0f + 5.0f * i));
        leftSphereRItem->ObjCBIndex = objCBIndex++;
        leftSphereRItem->Geo = mGeometries["shapeGeo"].get();
        leftSphereRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        leftSphereRItem->IndexCount = leftSphereRItem->Geo->DrawArgs["sphere"].IndexCount;
        leftSphereRItem->StartIndexLocation = leftSphereRItem->Geo->DrawArgs["sphere"].StartIndexLocation;
        leftSphereRItem->BaseVertexLocation = leftSphereRItem->Geo->DrawArgs["sphere"].BaseVertexLocation;
        leftSphereRItem->mat = mMaterials["stone0"].get();

        XMStoreFloat4x4(&rightSphereRItem->World, XMMatrixTranslation(5.0f, 3.5f, -10.0f + 5.0f * i));
        rightSphereRItem->ObjCBIndex = objCBIndex++;
        rightSphereRItem->Geo = mGeometries["shapeGeo"].get();
        rightSphereRItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rightSphereRItem->IndexCount = rightSphereRItem->Geo->DrawArgs["sphere"].IndexCount;
        rightSphereRItem->StartIndexLocation = rightSphereRItem->Geo->DrawArgs["sphere"].StartIndexLocation;
        rightSphereRItem->BaseVertexLocation = rightSphereRItem->Geo->DrawArgs["sphere"].BaseVertexLocation;
        rightSphereRItem->mat = mMaterials["stone0"].get();

        mAllRitems.push_back(std::move(leftCylRItem));
        mAllRitems.push_back(std::move(rightCylRItem));
        mAllRitems.push_back(std::move(leftSphereRItem));
        mAllRitems.push_back(std::move(rightSphereRItem));
    }

    for (auto& e : mAllRitems)
        mRitemLayer[(int)RenderLayer::Opaque].push_back(e.get());
}


void LitColumnsApp::BuildFrameResources()
{
    for (int i = 0; i < gNumFrameResources; ++i)
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
}

void LitColumnsApp::BuildMaterials()
{
    auto bricks0 = std::make_unique<Material>();
    bricks0->Name = "bricks0";
    bricks0->MatCBIndex = 0;
    bricks0->DiffuseSrvHeapIndex = 0;
    bricks0->DiffuseAlbedo = XMFLOAT4(Colors::ForestGreen);
    bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    bricks0->Roughness = 0.1f;

    auto stone0 = std::make_unique<Material>();
    stone0->Name = "stone0";
    stone0->MatCBIndex = 1;
    stone0->DiffuseSrvHeapIndex = 1;
    stone0->DiffuseAlbedo = XMFLOAT4(Colors::LightSteelBlue);
    stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stone0->Roughness = 0.3f;

    auto tile0 = std::make_unique<Material>();
    tile0->Name = "tile0";
    tile0->MatCBIndex = 2;
    tile0->DiffuseSrvHeapIndex = 2;
    tile0->DiffuseAlbedo = XMFLOAT4(Colors::LightGray);
    tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
    tile0->Roughness = 0.2f;

    auto skullMat = std::make_unique<Material>();
    skullMat->Name = "skullMat";
    skullMat->MatCBIndex = 3;
    skullMat->DiffuseSrvHeapIndex = 3;
    skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    skullMat->Roughness = 0.3f;

    mMaterials["bricks0"] = std::move(bricks0);
    mMaterials["stone0"] = std::move(stone0);
    mMaterials["tile0"] = std::move(tile0);
    mMaterials["skullMat"] = std::move(skullMat);
}

void LitColumnsApp::BuildShadersAndInputLayout()
{
    mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders/Default_5.hlsl", nullptr, "VS", "vs_5_1");
    mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders/Default_5.hlsl", nullptr, "PS", "ps_5_1");
    mInputLayout =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
}

void LitColumnsApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePSODesc;
    ZeroMemory(&opaquePSODesc, sizeof(opaquePSODesc));
    opaquePSODesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    opaquePSODesc.pRootSignature = mRootSignature.Get();

    opaquePSODesc.VS = {
        mShaders["standardVS"]->GetBufferPointer(),
        mShaders["standardVS"]->GetBufferSize()
    };

    opaquePSODesc.PS = {
        mShaders["opaquePS"]->GetBufferPointer(),
        mShaders["opaquePS"]->GetBufferSize()
    };

    opaquePSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePSODesc.SampleMask = UINT_MAX;
    opaquePSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePSODesc.NumRenderTargets = 1;
    opaquePSODesc.RTVFormats[0] = mBackBufferFormat;
    opaquePSODesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    opaquePSODesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    opaquePSODesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePSODesc, IID_PPV_ARGS(mPSOs["opaque"].GetAddressOf())));

    /*D3D12_GRAPHICS_PIPELINE_STATE_DESC wireframePSODesc = opaquePSODesc;
    wireframePSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&wireframePSODesc, IID_PPV_ARGS(mPSOs["opaque_wireframe"].GetAddressOf())));*/
}

void LitColumnsApp::BuildShapeGeometry()
{
    using GG = GeometryGenerator;
    GG::MeshData box = GG::CreateBox(1.5f, 0.5f, 1.5f, 3);
    GG::MeshData grid = GG::CreateGrid(20.0f, 30.0f, 60, 40);
    GG::MeshData sphere = GG::CreateSphere(0.5f, 20, 20);
    GG::MeshData cylinder = GG::CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    UINT boxVertexOffset = 0;
    UINT gridVertexOffset = (UINT)box.Vertices.size();
    UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
    UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

    UINT boxIndexOffset = 0;
    UINT gridIndexOffset = (UINT)box.Indices32.size();
    UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

    SubmeshGeometry boxSubmesh, gridSubmesh, sphereSubmesh, cylinderSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.BaseVertexLocation = boxVertexOffset;
    boxSubmesh.StartIndexLocation = boxIndexOffset;

    gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    auto totalVertexCount = box.Vertices.size() + grid.Vertices.size() + sphere.Vertices.size() + cylinder.Vertices.size();
    std::vector<Vertex> vertices(totalVertexCount);

    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Normal = box.Vertices[i].Normal;
    }
    for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = grid.Vertices[i].Position;
        vertices[k].Normal = grid.Vertices[i].Normal;
    }
    for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = sphere.Vertices[i].Position;
        vertices[k].Normal = sphere.Vertices[i].Normal;
    }
    for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cylinder.Vertices[i].Position;
        vertices[k].Normal = cylinder.Vertices[i].Normal;
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    geo->DrawArgs["box"] = boxSubmesh;
    geo->DrawArgs["grid"] = gridSubmesh;
    geo->DrawArgs["sphere"] = sphereSubmesh;
    geo->DrawArgs["cylinder"] = cylinderSubmesh;

    mGeometries[geo->Name] = std::move(geo);
}

void LitColumnsApp::BuildSkullGeometry()
{
    FILE* fp = fopen("Models/skull.txt", "rt");
    if (fp == NULL)
    {
        MessageBox(0, L"Skull file not found.", 0, 0);
        return;
    }
    char ignore[100];
    UINT vcount, tcount;
    fscanf(fp, "%s %d", ignore, &vcount);
    fscanf(fp, "%s %d", ignore, &tcount);
    while (fgetc(fp) != '{');
    std::vector<Vertex> vertices(vcount);
    for (UINT i = 0; i < vcount; i++)
    {
        fscanf(fp, "%f %f %f %f %f %f", &vertices[i].Pos.x, &vertices[i].Pos.y, &vertices[i].Pos.z,
            &vertices[i].Normal.x, &vertices[i].Normal.y, &vertices[i].Normal.z);
    }
    while (fgetc(fp) != '{');
    std::vector<std::int32_t> indices(tcount * 3);
    for (UINT i = 0; i < tcount; i++)
        fscanf(fp, "%d %d %d", &indices[3 * i], &indices[3 * i + 1], &indices[3 * i + 2]);

    fclose(fp);

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "skullGeo";
    ThrowIfFailed(D3DCreateBlob(vbByteSize, geo->VertexBufferCPU.GetAddressOf()));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
    ThrowIfFailed(D3DCreateBlob(ibByteSize, geo->IndexBufferCPU.GetAddressOf()));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexBufferByteSize = vbByteSize;
    geo->VertexByteStride = sizeof(Vertex);
    geo->IndexFormat = DXGI_FORMAT_R32_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry skullSubmesh;
    skullSubmesh.BaseVertexLocation = 0;
    skullSubmesh.IndexCount = (UINT)indices.size();
    skullSubmesh.StartIndexLocation = 0U;

    geo->DrawArgs["skull"] = skullSubmesh;
    mGeometries[geo->Name] = std::move(geo);
}

void LitColumnsApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

    auto objectCB = mCurrFrameResource->ObjectCB->Resource();
    auto matCB = mCurrFrameResource->MaterialCB->Resource();
    for (auto ri : ritems)
    {
        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

        auto objCBAddress = objectCB->GetGPUVirtualAddress();
        objCBAddress += ri->ObjCBIndex * objCBByteSize;

        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

        auto matCBAddress = matCB->GetGPUVirtualAddress();
        matCBAddress += ri->mat->MatCBIndex * matCBByteSize;

        cmdList->SetGraphicsRootConstantBufferView(1, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}