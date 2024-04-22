#include "../Common/d3dApp.h"
#include "../Common/d3dUtil.h"
#include "../Common/UploadBuffer.h"
#include <string>
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class BoxApp : public D3DApp
{
public:
    BoxApp(HINSTANCE hInstance);
    BoxApp(const BoxApp &rhs) = delete;
    BoxApp &operator=(const BoxApp &other) = delete;
    ~BoxApp();

    virtual bool Initialize() override;

private:
    virtual void OnResize() override;
    virtual void Update(const GameTimer &gt) override;
    virtual void Draw(const GameTimer &gt) override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildPSO();

private:
    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    ComPtr<ID3D12Resource> mMsaaRenderTarget = nullptr, mMsaaDepthStencil = nullptr;
    ComPtr<ID3D12DescriptorHeap> mMsaaRTVHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> mMsaaDSVHeap = nullptr;

    XMMATRIX mWorld = XMMatrixIdentity();
    XMMATRIX mView = XMMatrixIdentity();
    XMMATRIX mProj = XMMatrixIdentity();

    float mTheta = 1.5f * XM_PI;
    float mPhi = XM_PIDIV4;
    float mRadius = 5.0f;
    float mRotateSensitivity = 0.005f;
    float mMoveSensitivity = 0.005f;
    const UINT sampleLevel = 8;
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
        BoxApp theApp(hInstance);
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (DxException &e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

BoxApp::BoxApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
    mMainWndCaption = L"Box App";
}
BoxApp::~BoxApp() {}

bool BoxApp::Initialize()
{
    if (!InitMainWindow())
        return false;

    if (!InitDirect3D())
        return false;

    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList *cmdLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    FlushCommandQueue();
    OnResize();
    return true;
}

void BoxApp::OnResize()
{
    D3DApp::OnResize();

    mProj = XMMatrixPerspectiveFovLH(XM_PIDIV4, AspectRatio(), 1.0f, 1000.0f);

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC msaaRTDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        mBackBufferFormat,
        (UINT)mScreenViewport.Width,
        (UINT)mScreenViewport.Height,
        1,
        1,
        sampleLevel);
    msaaRTDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE msaaOptimizedClearValue = {};
    msaaOptimizedClearValue.Format = mBackBufferFormat;
    CopyMemory(msaaOptimizedClearValue.Color, &Colors::LightSteelBlue, sizeof(float) * 4);

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &msaaRTDesc,
        D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
        &msaaOptimizedClearValue,
        IID_PPV_ARGS(mMsaaRenderTarget.ReleaseAndGetAddressOf())));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = mBackBufferFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

    md3dDevice->CreateRenderTargetView(mMsaaRenderTarget.Get(), &rtvDesc, mMsaaRTVHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        mDepthStencilFormat,
        (UINT)mScreenViewport.Width,
        (UINT)mScreenViewport.Height,
        1,
        1,
        sampleLevel);
    depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = mDepthStencilFormat;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(mMsaaDepthStencil.ReleaseAndGetAddressOf())));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = mDepthStencilFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;

    md3dDevice->CreateDepthStencilView(
        mMsaaDepthStencil.Get(),
        &dsvDesc,
        mMsaaDSVHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::Update(const GameTimer &gt)
{
    // XMMATRIX world = XMLoadFloat4x4(&mWorld);
    // XMMATRIX view = XMMatrixLookAtLH(MathHelper::SphericalToCartesian(mRadius, mTheta, mPhi),
    //     XMVectorZero(), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    // XMMATRIX proj = XMLoadFloat4x4(&mProj);
    // XMStoreFloat4x4(&mView, view);

    mView = XMMatrixLookAtLH(MathHelper::SphericalToCartesian(mRadius, mTheta, mPhi),
                             XMVectorZero(), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(mWorld * mView * mProj));
    mObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer &gt)
{
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    if (m4xMsaaState)
    {
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                mMsaaRenderTarget.Get(),
                D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            mCommandList->ResourceBarrier(1, &barrier);

            auto rtvDescriptor = mMsaaRTVHeap->GetCPUDescriptorHandleForHeapStart();
            auto dsvDescriptor = mMsaaDSVHeap->GetCPUDescriptorHandleForHeapStart();

            mCommandList->OMSetRenderTargets(1, &rtvDescriptor, true, &dsvDescriptor);

            mCommandList->ClearRenderTargetView(rtvDescriptor, Colors::LightSteelBlue, 0, nullptr);
            mCommandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        }
    }
    else
    {
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                                                                  D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            mCommandList->ResourceBarrier(1, &barrier);
        }
        mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
        mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

        mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
    }
    ID3D12DescriptorHeap *descriptorHeaps[] = {mCbvHeap.Get()};
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
    mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    mCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["Box"].IndexCount, 1, 0, 0, 0);

    if (m4xMsaaState)
    {
        {
            D3D12_RESOURCE_BARRIER barriers[2] =
                {
                    CD3DX12_RESOURCE_BARRIER::Transition(
                        mMsaaRenderTarget.Get(),
                        D3D12_RESOURCE_STATE_RENDER_TARGET,
                        D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
                    CD3DX12_RESOURCE_BARRIER::Transition(
                        CurrentBackBuffer(),
                        D3D12_RESOURCE_STATE_PRESENT,
                        D3D12_RESOURCE_STATE_RESOLVE_DEST)};
            mCommandList->ResourceBarrier(2, barriers);
        }
        mCommandList->ResolveSubresource(CurrentBackBuffer(), 0, mMsaaRenderTarget.Get(), 0, mBackBufferFormat);
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                CurrentBackBuffer(),
                D3D12_RESOURCE_STATE_RESOLVE_DEST,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            mCommandList->ResourceBarrier(1, &barrier);
        }
    }

    {
        D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                                                              D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        mCommandList->ResourceBarrier(1, &barrier);
    }
    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList *cmdLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if (btnState & MK_LBUTTON)
    {
        mTheta -= mRotateSensitivity * (x - mLastMousePos.x);
        mPhi -= mRotateSensitivity * (y - mLastMousePos.y);
        mPhi = MathHelper::Clamp(mPhi, 0.001f, 0.999f * XM_PI);
    }
    else if (btnState & MK_RBUTTON)
    {
        int dx = x - mLastMousePos.x;
        int dy = y - mLastMousePos.y;
        mRadius += mMoveSensitivity * (dx - dy);
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 10.0f);
    }
    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void BoxApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
                                                   IID_PPV_ARGS(mCbvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc,
                                                   IID_PPV_ARGS(&mMsaaRTVHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc,
                                                   IID_PPV_ARGS(&mMsaaDSVHeap)));
}

void BoxApp::BuildConstantBuffers()
{
    mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();

    int boxCBufIndex = 0;
    cbAddress += (UINT64)boxCBufIndex * objCBByteSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = objCBByteSize * 1;

    md3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr, errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                             serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
        ::OutputDebugStringA((char *)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
                                                  IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void BoxApp::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;

    mvsByteCode = d3dUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "VS", "vs_5_0");
    mpsByteCode = d3dUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "PS", "ps_5_0");
    mInputLayout =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
}

void BoxApp::BuildBoxGeometry()
{
    std::array<Vertex, 8> vertices =
        {
            Vertex({XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White)}),
            Vertex({XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black)}),
            Vertex({XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red)}),
            Vertex({XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green)}),
            Vertex({XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue)}),
            Vertex({XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow)}),
            Vertex({XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan)}),
            Vertex({XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta)})};

    std::array<std::uint16_t, 36> indices =
        {
            // front face
            0, 1, 2,
            0, 2, 3,

            // back face
            4, 6, 5,
            4, 7, 6,

            // left face
            4, 5, 1,
            4, 1, 0,

            // right face
            3, 2, 6,
            3, 6, 7,

            // top face
            1, 5, 6,
            1, 6, 2,

            // bottom face
            4, 0, 3,
            4, 3, 7};

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->Name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
    CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
    CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
                                                            mCommandList.Get(), vertices.data(), vbByteSize, mBoxGeo->VertexBufferUploader);
    mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
                                                           mCommandList.Get(), indices.data(), ibByteSize, mBoxGeo->IndexBufferUploader);

    mBoxGeo->VertexByteStride = sizeof(Vertex);
    mBoxGeo->VertexBufferByteSize = vbByteSize;
    mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mBoxGeo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    mBoxGeo->DrawArgs["Box"] = submesh;
}

void BoxApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(psoDesc));
    psoDesc.InputLayout = {mInputLayout.data(), (UINT)mInputLayout.size()};
    psoDesc.pRootSignature = mRootSignature.Get();

    psoDesc.VS = {
        mvsByteCode->GetBufferPointer(),
        mvsByteCode->GetBufferSize()};

    psoDesc.PS = {
        mpsByteCode->GetBufferPointer(),
        mpsByteCode->GetBufferSize()};

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = sampleLevel;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPSO.GetAddressOf())));
}
