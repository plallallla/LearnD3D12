#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include <GlfwWindow.hpp>
#include <Utility.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.hpp>

#include <wrl/client.h>

#include <dxgi1_6.h>
#include <directx/d3d12sdklayers.h>
#include <directx/d3dx12.h>

#define FrameCount 2
#define WIDTH 1280
#define HEIGHT 720

using namespace Microsoft::WRL;

struct ScreenVertex
{
    float position[2];
    float uv[2];
};

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed here!");
    }
}

struct D3D12Context
{
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;

    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;
    UINT m_frameIndex;

    // Compute shader resources (PingPong).
    ComPtr<ID3D12Resource> m_pingPong[2];
    ComPtr<ID3D12Resource> m_blurParamsBuffer;
    ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
    UINT m_cbvSrvUavDescriptorSize;
    UINT m_texWidth;
    UINT m_texHeight;
    INT m_pingPongIndex;
    UINT m_frameCount;

    // Compute pipeline.
    ComPtr<ID3D12RootSignature> m_computeRootSignature;
    ComPtr<ID3D12PipelineState> m_computePSO;

    // Graphics pipeline (fullscreen quad).
    ComPtr<ID3D12RootSignature> m_graphicsRootSignature;
    ComPtr<ID3D12PipelineState> m_graphicsPSO;
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    // Synchronization objects.
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    void WaitForPreviousFrame()
    {
        const UINT64 fence = m_fenceValue;
        ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
        m_fenceValue++;

        if (m_fence->GetCompletedValue() < fence)
        {
            ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle()
    {
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_srvUavHeap->GetGPUDescriptorHandleForHeapStart(), 0, m_cbvSrvUavDescriptorSize);
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(INT index)
    {
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_srvUavHeap->GetGPUDescriptorHandleForHeapStart(), 1 + index, m_cbvSrvUavDescriptorSize);
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandle(INT index)
    {
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_srvUavHeap->GetGPUDescriptorHandleForHeapStart(), 3 + index, m_cbvSrvUavDescriptorSize);
    }

    D3D12Context(HWND h)
    {
        UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif

        // factory + device
        ComPtr<IDXGIFactory4> factory;
        ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);
        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)));

        // command queue
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

        // swap chain
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = WIDTH;
        swapChainDesc.Height = HEIGHT;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = FrameCount;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        swapChainDesc.Flags = 0;
        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(factory->CreateSwapChainForHwnd(
            m_commandQueue.Get(),
            h,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain));
        ThrowIfFailed(factory->MakeWindowAssociation(h, DXGI_MWA_NO_ALT_ENTER));
        ThrowIfFailed(swapChain.As(&m_swapChain));
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        // rtv heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT n = 0; n < FrameCount; ++n)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }

        // command allocator
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

        // load image with stb_image
        std::string imagePath = std::string{DISPATCH_TEXTURE_DIR} + "/Lena.png";
        int imgWidth = 0;
        int imgHeight = 0;
        int imgChannels = 0;
        unsigned char* imageData = stbi_load(imagePath.c_str(), &imgWidth, &imgHeight, &imgChannels, 4);
        if (!imageData)
        {
            throw std::runtime_error("Failed to load image: " + imagePath);
        }
        m_texWidth = static_cast<UINT>(imgWidth);
        m_texHeight = static_cast<UINT>(imgHeight);

        // PingPong textures (default heap, UAV capable).
        for (int i = 0; i < 2; ++i)
        {
            D3D12_RESOURCE_DESC texDesc = {};
            texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texDesc.Width = m_texWidth;
            texDesc.Height = static_cast<UINT>(m_texHeight);
            texDesc.DepthOrArraySize = 1;
            texDesc.MipLevels = 1;
            texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texDesc.SampleDesc.Count = 1;
            texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &texDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&m_pingPong[i])));
        }

        // CBV/SRV/UAV descriptor heap (shader visible).
        // Layout: [0] CBV blurParams, [1] SRV pingPong[0], [2] SRV pingPong[1], [3] UAV pingPong[0], [4] UAV pingPong[1]
        D3D12_DESCRIPTOR_HEAP_DESC srvUavHeapDesc = {};
        srvUavHeapDesc.NumDescriptors = 5;
        srvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvUavHeapDesc, IID_PPV_ARGS(&m_srvUavHeap)));
        m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // blur params constant buffer (upload heap, 256-byte aligned).
        {
            CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(256);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_blurParamsBuffer)));
        }

        {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = m_blurParamsBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = 256;
            m_device->CreateConstantBufferView(&cbvDesc, m_srvUavHeap->GetCPUDescriptorHandleForHeapStart());
        }

        CD3DX12_CPU_DESCRIPTOR_HANDLE srvUavHandle(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart());
        srvUavHandle.Offset(1, m_cbvSrvUavDescriptorSize);
        for (int i = 0; i < 2; ++i)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            m_device->CreateShaderResourceView(m_pingPong[i].Get(), &srvDesc, srvUavHandle);
            srvUavHandle.Offset(1, m_cbvSrvUavDescriptorSize);
        }
        for (int i = 0; i < 2; ++i)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = 0;
            m_device->CreateUnorderedAccessView(m_pingPong[i].Get(), nullptr, &uavDesc, srvUavHandle);
            srvUavHandle.Offset(1, m_cbvSrvUavDescriptorSize);
        }

        // shaders
        auto vertexShaderData = ReadCSO(std::string{SHADER_DIR} + "/dispatch_VSMain.cso");
        auto pixelShaderData = ReadCSO(std::string{SHADER_DIR} + "/dispatch_PSMain.cso");
        auto computeShaderData = ReadCSO(std::string{SHADER_DIR} + "/dispatch_CSMain.cso");

        // compute root signature: CBV(b0) + SRV(t0) + UAV(u0)
        {
            CD3DX12_DESCRIPTOR_RANGE cbvRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
            CD3DX12_DESCRIPTOR_RANGE srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            CD3DX12_DESCRIPTOR_RANGE uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
            CD3DX12_ROOT_PARAMETER rootParams[3];
            rootParams[0].InitAsDescriptorTable(1, &cbvRange, D3D12_SHADER_VISIBILITY_ALL);
            rootParams[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_ALL);
            rootParams[2].InitAsDescriptorTable(1, &uavRange, D3D12_SHADER_VISIBILITY_ALL);

            CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
            rootSigDesc.Init(3, rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_computeRootSignature)));
        }

        // graphics root signature: SRV(t0) + static linear clamp sampler(s0)
        {
            CD3DX12_DESCRIPTOR_RANGE srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
            CD3DX12_ROOT_PARAMETER rootParams[1];
            rootParams[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

            CD3DX12_STATIC_SAMPLER_DESC samplerDesc = {};
            samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc.ShaderRegister = 0;
            samplerDesc.RegisterSpace = 0;
            samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
            rootSigDesc.Init(1, rootParams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_graphicsRootSignature)));
        }

        // compute PSO
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC computePSODesc = {};
            computePSODesc.pRootSignature = m_computeRootSignature.Get();
            computePSODesc.CS = CD3DX12_SHADER_BYTECODE(computeShaderData.data(), static_cast<UINT>(computeShaderData.size()));
            ThrowIfFailed(m_device->CreateComputePipelineState(&computePSODesc, IID_PPV_ARGS(&m_computePSO)));
        }

        // graphics PSO
        {
            D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
            psoDesc.pRootSignature = m_graphicsRootSignature.Get();
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderData.data(), static_cast<UINT>(vertexShaderData.size()));
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderData.data(), static_cast<UINT>(pixelShaderData.size()));
            psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState.DepthEnable = FALSE;
            psoDesc.DepthStencilState.StencilEnable = FALSE;
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            psoDesc.SampleDesc.Count = 1;
            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_graphicsPSO)));
        }

        // viewport preserves the image aspect ratio inside the window.
        {
            const float imageAspect = static_cast<float>(m_texWidth) / static_cast<float>(m_texHeight);
            const float windowAspect = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
            float vpWidth = static_cast<float>(WIDTH);
            float vpHeight = static_cast<float>(HEIGHT);
            if (imageAspect > windowAspect)
            {
                vpHeight = vpWidth / imageAspect;
            }
            else
            {
                vpWidth = vpHeight * imageAspect;
            }
            const float vpX = (WIDTH - vpWidth) / 2.0f;
            const float vpY = (HEIGHT - vpHeight) / 2.0f;
            m_viewport = CD3DX12_VIEWPORT(vpX, vpY, vpWidth, vpHeight);
            m_scissorRect = CD3DX12_RECT(static_cast<LONG>(vpX), static_cast<LONG>(vpY), static_cast<LONG>(vpX + vpWidth), static_cast<LONG>(vpY + vpHeight));
        }

        // command list
        ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
        ThrowIfFailed(m_commandList->Close());

        // fullscreen quad vertex buffer
        {
            ScreenVertex quadVertices[] =
            {
                { { -1.0f,  1.0f }, { 0.0f, 0.0f } },
                { {  1.0f,  1.0f }, { 1.0f, 0.0f } },
                { { -1.0f, -1.0f }, { 0.0f, 1.0f } },
                { { -1.0f, -1.0f }, { 0.0f, 1.0f } },
                { {  1.0f,  1.0f }, { 1.0f, 0.0f } },
                { {  1.0f, -1.0f }, { 1.0f, 1.0f } }
            };

            const UINT vertexBufferSize = sizeof(quadVertices);
            CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_vertexBuffer)));

            UINT8* pVertexDataBegin = nullptr;
            CD3DX12_RANGE readRange(0, 0);
            ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
            memcpy(pVertexDataBegin, quadVertices, vertexBufferSize);
            m_vertexBuffer->Unmap(0, nullptr);

            m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
            m_vertexBufferView.StrideInBytes = sizeof(ScreenVertex);
            m_vertexBufferView.SizeInBytes = vertexBufferSize;
        }

        // fence
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // upload the image into pingPong[0] synchronously.
        UploadImage(imageData);
        stbi_image_free(imageData);

        m_pingPongIndex = 0;
        m_frameCount = 0;
        WaitForPreviousFrame();
    }

    void UploadImage(const unsigned char* imageData)
    {
        UINT64 totalBytes = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        const D3D12_RESOURCE_DESC texDesc = m_pingPong[0]->GetDesc();
        m_device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, nullptr, nullptr, &totalBytes);

        ComPtr<ID3D12Resource> uploadBuffer;
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBytes);
        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer)));

        UINT8* mappedData = nullptr;
        ThrowIfFailed(uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
        const UINT rowBytes = m_texWidth * 4;
        for (UINT y = 0; y < m_texHeight; ++y)
        {
            memcpy(mappedData + y * footprint.Footprint.RowPitch, imageData + y * rowBytes, rowBytes);
        }
        uploadBuffer->Unmap(0, nullptr);

        ThrowIfFailed(m_commandAllocator->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = m_pingPong[0].Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = uploadBuffer.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;

        m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        // pingPong[0]: COPY_DEST -> NON_PIXEL_SHADER_RESOURCE
        // pingPong[1]: COPY_DEST -> NON_PIXEL_SHADER_RESOURCE
        const CD3DX12_RESOURCE_BARRIER barriers[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(m_pingPong[0].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(m_pingPong[1].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        };
        m_commandList->ResourceBarrier(2, barriers);

        ThrowIfFailed(m_commandList->Close());

        ID3D12CommandList* commandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, commandLists);

        const UINT64 fence = m_fenceValue;
        ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
        m_fenceValue++;
        if (m_fence->GetCompletedValue() < fence)
        {
            ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    void populate_command_list()
    {
        const INT input = m_pingPongIndex;
        const INT output = 1 - m_pingPongIndex;

        ThrowIfFailed(m_commandAllocator->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

        // Bind the shader-visible CBV/SRV/UAV heap before referencing it via descriptor tables.
        ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvUavHeap.Get() };
        m_commandList->SetDescriptorHeaps(1, descriptorHeaps);

        // --- compute pass ---
        const auto pingPongToUav = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pingPong[output].Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        m_commandList->ResourceBarrier(1, &pingPongToUav);

        m_commandList->SetPipelineState(m_computePSO.Get());
        m_commandList->SetComputeRootSignature(m_computeRootSignature.Get());
        m_commandList->SetComputeRootDescriptorTable(0, cbvHandle());
        m_commandList->SetComputeRootDescriptorTable(1, srvHandle(input));
        m_commandList->SetComputeRootDescriptorTable(2, uavHandle(output));
        m_commandList->Dispatch((m_texWidth + 15) / 16, (m_texHeight + 15) / 16, 1);

        // UAV barrier (output: UNORDERED_ACCESS -> PIXEL_SHADER_RESOURCE for the draw).
        const auto pingPongToPixelSrv = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pingPong[output].Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &pingPongToPixelSrv);

        // --- graphics pass ---
        const auto backBufferToRtv = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &backBufferToRtv);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), (INT)m_frameIndex, m_rtvDescriptorSize);
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        m_commandList->SetPipelineState(m_graphicsPSO.Get());
        m_commandList->SetGraphicsRootSignature(m_graphicsRootSignature.Get());
        m_commandList->SetGraphicsRootDescriptorTable(0, srvHandle(output));
        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);
        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        m_commandList->DrawInstanced(6, 1, 0, 0);

        // output texture back to NON_PIXEL_SHADER_RESOURCE for the next compute read.
        const auto pingPongToNonPixelSrv = CD3DX12_RESOURCE_BARRIER::Transition(
            m_pingPong[output].Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &pingPongToNonPixelSrv);

        const auto backBufferToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &backBufferToPresent);

        ThrowIfFailed(m_commandList->Close());

        // swap PingPong buffers so next frame blurs the current result.
        m_pingPongIndex = output;
    }

    void render()
    {
        // Update blur radius: abs(sin(t)) * maxRadius, oscillates between 0 and 8.
        const float t = static_cast<float>(m_frameCount) * 0.05f;
        const INT blurRadius = static_cast<INT>(fabsf(sinf(t)) * 8.0f + 0.5f);
        m_frameCount++;

        struct BlurParams
        {
            INT blurRadius;
        };
        BlurParams params;
        params.blurRadius = blurRadius;
        UINT8* mappedData = nullptr;
        CD3DX12_RANGE readRange(0, 0);
        ThrowIfFailed(m_blurParamsBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
        memcpy(mappedData, &params, sizeof(BlurParams));
        m_blurParamsBuffer->Unmap(0, nullptr);

        populate_command_list();

        ID3D12CommandList* commandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, commandLists);

        ThrowIfFailed(m_swapChain->Present(1, 0));

        WaitForPreviousFrame();
    }

    inline static void GetHardwareAdapter(
        IDXGIFactory1* pFactory,
        IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = true)
    {
        *ppAdapter = nullptr;

        ComPtr<IDXGIAdapter1> adapter;

        ComPtr<IDXGIFactory6> factory6;
        if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
        {
            for (
                UINT adapterIndex = 0;
                SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                    adapterIndex,
                    requestHighPerformanceAdapter ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                    IID_PPV_ARGS(&adapter)));
                ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    continue;
                }

                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        if (adapter.Get() == nullptr)
        {
            for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    continue;
                }

                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        *ppAdapter = adapter.Detach();
    }
};

int main()
{
    GlfwWindow window{WIDTH, HEIGHT, "Dispatch (PingPong blur)"};
    D3D12Context device{window._hwnd};

    while (!glfwWindowShouldClose(window._window))
    {
        device.render();
        glfwPollEvents();
    }

    return 0;
}
