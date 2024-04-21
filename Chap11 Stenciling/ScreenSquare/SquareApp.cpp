#include "../../Common/d3dApp.h"
#include "../../Common/d3dUtil.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
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
    BoxApp(const BoxApp& rhs) = delete;
    BoxApp& operator=(const BoxApp& other) = delete;
    ~BoxApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildSquareGeometry();
    void BuildPSO();

private:
    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

    std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
    std::unique_ptr<MeshGeometry> mSquaresGeo = nullptr;
    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;
    ComPtr<ID3DBlob> mvs2ByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mPSO = nullptr, mSquarePSO = nullptr;

    XMMATRIX mWorld = XMMatrixIdentity();
    XMMATRIX mView = XMMatrixIdentity();
    XMMATRIX mProj = XMMatrixIdentity();

    float mTheta = 1.5f * XM_PI;
    float mPhi = XM_PIDIV4;
    float mRadius = 5.0f;
    float mRotateSensitivity = 0.005f;
    float mMoveSensitivity = 0.005f;
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
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

BoxApp::BoxApp(HINSTANCE hInstance) : D3DApp(hInstance) {
    mMainWndCaption = L"Box App";

}
BoxApp::~BoxApp() {}

bool BoxApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;
    //ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildSquareGeometry();
    BuildPSO();

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    FlushCommandQueue();
    OutputDebugString(std::to_wstring(sizeof(Vertex)).c_str());
    return true;
}

void BoxApp::OnResize()
{
    D3DApp::OnResize();

    mProj = XMMatrixPerspectiveFovLH(XM_PIDIV4, AspectRatio(), 1.0f, 1000.0f);
}

void BoxApp::Update(const GameTimer& gt)
{
    //XMMATRIX world = XMLoadFloat4x4(&mWorld);
    //XMMATRIX view = XMMatrixLookAtLH(MathHelper::SphericalToCartesian(mRadius, mTheta, mPhi), 
    //    XMVectorZero(), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    //XMMATRIX proj = XMLoadFloat4x4(&mProj);
    //XMStoreFloat4x4(&mView, view);

    mView = XMMatrixLookAtLH(MathHelper::SphericalToCartesian(mRadius, mTheta, mPhi),
        XMVectorZero(), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(mWorld * mView * mProj));
    mObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer& gt)
{
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    mCommandList->IASetVertexBuffers(0, 1, &mBoxGeo->VertexBufferView());
    mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
    mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());
    mCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["Box"].IndexCount, 1, 0, 0, 0);

    mCommandList->SetPipelineState(mSquarePSO.Get());
    mCommandList->IASetVertexBuffers(0, 1, &mSquaresGeo->VertexBufferView());
    mCommandList->IASetIndexBuffer(&mSquaresGeo->IndexBufferView());
    for (int i = 0; i < 6; i++)
    {
        mCommandList->OMSetStencilRef(i + 1);
        mCommandList->DrawIndexedInstanced(6, 1, 0, i * 4, 0);
    }

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
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

void BoxApp::BuildDescriptorHeaps()//상수 버퍼를 위한 CBV 힙을 생성한다.
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(mCbvHeap.GetAddressOf())));
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
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void BoxApp::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;

    mvsByteCode = d3dUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "VS", "vs_5_0");
    mvs2ByteCode = d3dUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "VS2", "vs_5_0");
    mpsByteCode = d3dUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "PS", "ps_5_0");
    mInputLayout =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

}

void BoxApp::BuildBoxGeometry()
{
    //std::array<Vertex, 8> vertices =
    //{
    //    Vertex({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::White) }),
    //    Vertex({ XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Black) }),
    //    Vertex({ XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(Colors::Red) }),
    //    Vertex({ XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green) }),
    //    Vertex({ XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Blue) }),
    //    Vertex({ XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Yellow) }),
    //    Vertex({ XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(Colors::Cyan) }),
    //    Vertex({ XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(Colors::Magenta) })
    //};

    //std::array<std::uint16_t, 36> indices =
    //{
    //    // front face
    //    0, 1, 2,
    //    0, 2, 3,

    //    // back face
    //    4, 6, 5,
    //    4, 7, 6,

    //    // left face
    //    4, 5, 1,
    //    4, 1, 0,

    //    // right face
    //    3, 2, 6,
    //    3, 6, 7,

    //    // top face
    //    1, 5, 6,
    //    1, 6, 2,

    //    // bottom face
    //    4, 0, 3,
    //    4, 3, 7
    //};
    using GG = GeometryGenerator;
    GG::MeshData grid = GG::CreateGrid(10.0f, 10.0f, 10, 10);
    std::vector<Vertex> vertices(grid.Vertices.size());
    for (int i = 0; i < vertices.size(); i++)
    {
        vertices[i].Pos = grid.Vertices[i].Position;
        //vertices[i].Pos.y += i;
        vertices[i].Color = XMFLOAT4(Colors::Black);
    }
    std::vector<std::uint16_t> indices = grid.GetIndices16();

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

void BoxApp::BuildSquareGeometry()
{
    std::vector<Vertex> vertices(4 * 6);
    XMFLOAT4 colors[6] = {
        XMFLOAT4(Colors::Blue),
        XMFLOAT4(Colors::Green),
        XMFLOAT4(Colors::Red),
        XMFLOAT4(Colors::Yellow),
        XMFLOAT4(Colors::Magenta),
        XMFLOAT4(Colors::Cyan),
    };
    for (int i = 0; i < 6; i++)
    {
        vertices[i * 4 + 0].Pos = XMFLOAT3(-1.0f, -1.0f, 0.5f);
        vertices[i * 4 + 1].Pos = XMFLOAT3(-1.0f, +1.0f, 0.5f);
        vertices[i * 4 + 2].Pos = XMFLOAT3(+1.0f, +1.0f, 0.5f);
        vertices[i * 4 + 3].Pos = XMFLOAT3(+1.0f, -1.0f, 0.5f);
        vertices[i * 4 + 0].Color = vertices[i * 4 + 1].Color
            = vertices[i * 4 + 2].Color = vertices[i * 4 + 3].Color = colors[i];
    }
    std::vector<std::int16_t> indices = { 0, 1, 2, 0, 2, 3 };
    mSquaresGeo = std::make_unique<MeshGeometry>();
    mSquaresGeo->Name = "squareGeo";
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &mSquaresGeo->VertexBufferCPU));
    CopyMemory(mSquaresGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &mSquaresGeo->IndexBufferCPU));
    CopyMemory(mSquaresGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    mSquaresGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, mSquaresGeo->VertexBufferUploader);
    mSquaresGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, mSquaresGeo->IndexBufferUploader);

    mSquaresGeo->VertexByteStride = sizeof(Vertex);
    mSquaresGeo->VertexBufferByteSize = vbByteSize;
    mSquaresGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
    mSquaresGeo->IndexBufferByteSize = ibByteSize;
}

void BoxApp::BuildPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(psoDesc));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();

    psoDesc.VS = {
        mvsByteCode->GetBufferPointer(),
        mvsByteCode->GetBufferSize()
    };

    psoDesc.PS = {
        mpsByteCode->GetBufferPointer(),
        mpsByteCode->GetBufferSize()
    };

    D3D12_DEPTH_STENCIL_DESC dss = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    dss.StencilEnable = true;
    dss.StencilReadMask = 0xff;
    dss.StencilWriteMask = 0xff;
    dss.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
    dss.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_INCR;
    dss.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    dss.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    dss.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
    dss.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_INCR;
    dss.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    dss.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    //psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    //psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = dss;
    
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPSO.GetAddressOf())));

    auto squarePSODesc = psoDesc;
    squarePSODesc.VS =
    {
        mvs2ByteCode->GetBufferPointer(),
        mvs2ByteCode->GetBufferSize()
    };
    squarePSODesc.DepthStencilState.DepthEnable = false;
    squarePSODesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    squarePSODesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    squarePSODesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    squarePSODesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&squarePSODesc, IID_PPV_ARGS(mSquarePSO.GetAddressOf())));
}
