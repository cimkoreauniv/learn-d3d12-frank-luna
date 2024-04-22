#include "../../Common/d3dApp.h"
#include <DirectXColors.h>
#include <iostream>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class VecLenApp : public D3DApp
{
public:
    VecLenApp(HINSTANCE hInstance);
    ~VecLenApp();

    virtual bool Initialize()override;

private:
    //virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;
    ComPtr<ID3D12Resource> mReadBackBuffer = nullptr, mOutputBuffer = nullptr;
    ComPtr<ID3D12Resource> mInputBuffer = nullptr, mInputUploadBuffer = nullptr;
    ComPtr<ID3D12PipelineState> mPSO = nullptr;
    ComPtr<ID3D12RootSignature> mRootSignature;
    ComPtr<ID3D12DescriptorHeap> mSrvUavDescriptorHeap = nullptr;
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
        VecLenApp theApp(hInstance);
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

VecLenApp::VecLenApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

VecLenApp::~VecLenApp()
{
}

bool VecLenApp::Initialize()
{
    if (!D3DApp::Initialize())
        return false;
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    XMFLOAT3 v[64];
    for (int i = 0; i < 64; i++)
    {
        XMVECTOR v1 = MathHelper::RandUnitVec3() * MathHelper::RandF(1.0f, 10.0f);
        XMStoreFloat3(&v[i], v1);
        OutputDebugString((std::to_wstring(XMVectorGetX(XMVector3Length(v1))) + L"\n").c_str());
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 2;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mSrvUavDescriptorHeap)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    mInputBuffer = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), v, sizeof(XMFLOAT3) * 64, mInputUploadBuffer);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
    srvDesc.Buffer.NumElements = 64;
    srvDesc.Buffer.StructureByteStride = 0;
    md3dDevice->CreateShaderResourceView(mInputBuffer.Get(), &srvDesc, hDescriptor);
    hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(256, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(mOutputBuffer.GetAddressOf())
    ));
    
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
    uavDesc.Buffer.NumElements = 64;

    md3dDevice->CreateUnorderedAccessView(mOutputBuffer.Get(), nullptr, &uavDesc, hDescriptor);

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(256),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(mReadBackBuffer.GetAddressOf())
    ));

    D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
    ComPtr<ID3DBlob> mcsByteCode = d3dUtil::CompileShader(L"Shaders/2.hlsl", nullptr, "CS", "cs_5_0");
    computePSODesc.CS =
    {
        mcsByteCode->GetBufferPointer(),
        mcsByteCode->GetBufferSize()
    };
    CD3DX12_DESCRIPTOR_RANGE srvTable;
    srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE uavTable;
    uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    slotRootParameter[0].InitAsDescriptorTable(1, &srvTable);
    slotRootParameter[1].InitAsDescriptorTable(1, &uavTable);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter);
    ComPtr<ID3DBlob> serializedRootSig = nullptr, errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRootSig, &errorBlob);
    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
    computePSODesc.pRootSignature = mRootSignature.Get();

    ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(mPSO.ReleaseAndGetAddressOf())));

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(1, cmdsLists);

    FlushCommandQueue();

    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));
    mCommandList->SetComputeRootSignature(mRootSignature.Get());
    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvUavDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(1, descriptorHeaps);
    mCommandList->SetComputeRootDescriptorTable(0, mSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    mCommandList->SetComputeRootDescriptorTable(1, 
        CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, mCbvSrvUavDescriptorSize));

    mCommandList->Dispatch(1, 1, 1);

    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));

    mCommandList->CopyResource(mReadBackBuffer.Get(), mOutputBuffer.Get());

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists2[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(1, cmdsLists2);
    FlushCommandQueue();

    float* data = nullptr;
    mReadBackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&data));

    FILE* fp = fopen("results.txt", "wt");
    for (int i = 0; i < 64; i++)
        fprintf(fp, "%f\n", data[i]);
    fclose(fp);
    mReadBackBuffer->Unmap(0, nullptr);
    return true;
}

void VecLenApp::Update(const GameTimer& gt)
{

}

void VecLenApp::Draw(const GameTimer& gt)
{
    ThrowIfFailed(mDirectCmdListAlloc->Reset());

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    auto transition1 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &transition1);

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    auto transition2 = CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &transition2);

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(1, cmdLists);

    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    FlushCommandQueue();
}