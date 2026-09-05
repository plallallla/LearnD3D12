#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <GlfwWindow.hpp>
#include <Utility.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.hpp>

#include <wrl/client.h>

#include <dxgi1_6.h>
#include <directx/d3d12sdklayers.h>
#include <directx/d3dx12.h>
#include <DirectXMath.h>

#define FrameCount 2
#define WIDTH 1280
#define HEIGHT 720

using namespace Microsoft::WRL;
using namespace DirectX;

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed here!");
    }
}

void enable_debug_layer(UINT& flag)
{
    ComPtr<ID3D12Debug> debug_controller;
    if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)))) return;
    debug_controller->EnableDebugLayer();
    flag |= DXGI_CREATE_FACTORY_DEBUG;
}

ComPtr<IDXGIFactory4> create_factory(UINT flag)
{
    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(flag, IID_PPV_ARGS(&factory)));
    return factory;
}

void GetHardwareAdapter(
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
                requestHighPerformanceAdapter ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE :
                DXGI_GPU_PREFERENCE_UNSPECIFIED,
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

ComPtr<ID3D12Device> create_device(UINT flag, ComPtr<IDXGIFactory4> factory)
{
    ComPtr<ID3D12Device> device;
    ThrowIfFailed(CreateDXGIFactory2(flag, IID_PPV_ARGS(&factory)));
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    GetHardwareAdapter(factory.Get(), &hardwareAdapter);
    ThrowIfFailed(D3D12CreateDevice(
        hardwareAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device)
    ));
    return device;
}

ComPtr<ID3D12CommandQueue> create_command_queue(ComPtr<ID3D12Device> device)
{
    ComPtr<ID3D12CommandQueue> command_queue;
    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&command_queue)));
    return command_queue;
}

ComPtr<IDXGISwapChain3> create_swap_chain(HWND h, ComPtr<IDXGIFactory4> factory, ComPtr<ID3D12CommandQueue> command_queue)
{
    ComPtr<IDXGISwapChain3> ret_sc3;
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
        command_queue.Get(),
        h,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    ));
    ThrowIfFailed(factory->MakeWindowAssociation(h, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain.As(&ret_sc3));
    return ret_sc3;
}

ComPtr<ID3D12DescriptorHeap> create_descriptor_heap(ComPtr<ID3D12Device> device, UINT ct, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shader_visible)
{
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = ct;
    desc.Type = type;
    desc.Flags = shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));
    return heap;
}

void generate_sphere(std::vector<float>& vertices, std::vector<UINT>& indices)
{
    const unsigned int X_SEGMENTS = 64;
    const unsigned int Y_SEGMENTS = 64;
    const float PI = 3.14159265359f;

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT2> uvs;
    std::vector<XMFLOAT3> normals;

    for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
    {
        float xSegment = (float)x / (float)X_SEGMENTS;
        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
        {
            float ySegment = (float)y / (float)Y_SEGMENTS;
            float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
            float yPos = std::cos(ySegment * PI);
            float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

            positions.emplace_back(xPos, yPos, zPos);
            uvs.emplace_back(xSegment, 1.0f - ySegment);
            normals.emplace_back(xPos, yPos, zPos);
        }
    }

    for (unsigned int x = 0; x < X_SEGMENTS; ++x)
    {
        for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
        {
            UINT i0 = x * (Y_SEGMENTS + 1) + y;
            UINT i1 = (x + 1) * (Y_SEGMENTS + 1) + y;
            UINT i2 = i0 + 1;
            UINT i3 = i1 + 1;

            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);

            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i3);
        }
    }

    vertices.reserve(positions.size() * 8);
    for (size_t i = 0; i < positions.size(); ++i)
    {
        vertices.push_back(positions[i].x);
        vertices.push_back(positions[i].y);
        vertices.push_back(positions[i].z);
        vertices.push_back(normals[i].x);
        vertices.push_back(normals[i].y);
        vertices.push_back(normals[i].z);
        vertices.push_back(uvs[i].x);
        vertices.push_back(uvs[i].y);
    }
}

struct PbrSphereContext
{
    static constexpr UINT PbrTextureCount = 5;

    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;

    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthStencil;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;
    UINT m_srvDescriptorSize;
    UINT m_frameIndex;

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    UINT m_indexCount;

    ComPtr<ID3D12Resource> m_constantBuffer;
    UINT8* m_cbvDataBegin;

    ComPtr<ID3D12Resource> m_textures[PbrTextureCount];

    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    struct Constant
    {
        XMFLOAT4X4 mvp;
        XMFLOAT4X4 model;
        XMFLOAT4X4 normal;
        XMFLOAT4 eye;
        XMFLOAT4 lightPos;
        XMFLOAT4 lightColor;
    };

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

    PbrSphereContext(HWND h)
    {
        UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
        enable_debug_layer(dxgiFactoryFlags);
#endif

        m_factory = create_factory(dxgiFactoryFlags);
        m_device = create_device(dxgiFactoryFlags, m_factory);
        m_commandQueue = create_command_queue(m_device);
        m_swapChain = create_swap_chain(h, m_factory, m_commandQueue);

        m_rtvHeap = create_descriptor_heap(m_device, FrameCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{m_rtvHeap->GetCPUDescriptorHandleForHeapStart()};
        for (UINT n = 0; n < FrameCount; ++n)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
            dsvHeapDesc.NumDescriptors = 1;
            dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

            D3D12_CLEAR_VALUE depthClearValue = {};
            depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
            depthClearValue.DepthStencil.Depth = 1.0f;

            CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_D32_FLOAT, WIDTH, HEIGHT, 1, 1, 1, 0,
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

            CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &depthDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &depthClearValue,
                IID_PPV_ARGS(&m_depthStencil)));

            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            m_device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
        }

        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

        {
            D3D12_STATIC_SAMPLER_DESC sampler = {};
            sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            sampler.MaxLOD = D3D12_FLOAT32_MAX;
            sampler.ShaderRegister = 0;
            sampler.RegisterSpace = 0;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            D3D12_DESCRIPTOR_RANGE srvRange = {};
            srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            srvRange.NumDescriptors = PbrTextureCount;
            srvRange.BaseShaderRegister = 0;
            srvRange.RegisterSpace = 0;
            srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_ROOT_PARAMETER rootParameters[2];
            rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            rootParameters[0].Descriptor.ShaderRegister = 0;
            rootParameters[0].Descriptor.RegisterSpace = 0;
            rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
            rootParameters[1].DescriptorTable.pDescriptorRanges = &srvRange;
            rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init(
                _countof(rootParameters), rootParameters,
                1, &sampler,
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

            ComPtr<ID3DBlob> signature;
            ComPtr<ID3DBlob> error;
            ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
            ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        }

        {
            auto vertexShaderData = ReadCSO(std::string{SHADER_DIR} + "/pbr_sphere_VSMain.cso");
            auto pixelShaderData  = ReadCSO(std::string{SHADER_DIR} + "/pbr_sphere_PSMain.cso");

            D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
            psoDesc.pRootSignature = m_rootSignature.Get();
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderData.data(), vertexShaderData.size());
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderData.data(), pixelShaderData.size());
            psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            psoDesc.SampleDesc.Count = 1;
            ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
        }

        m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(WIDTH), static_cast<float>(HEIGHT));
        m_scissorRect = CD3DX12_RECT(0, 0, WIDTH, HEIGHT);

        ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
        ThrowIfFailed(m_commandList->Close());

        {
            std::vector<float> sphereVertices;
            std::vector<UINT> sphereIndices;
            generate_sphere(sphereVertices, sphereIndices);
            m_indexCount = static_cast<UINT>(sphereIndices.size());

            {
                const UINT vertexBufferSize = static_cast<UINT>(sphereVertices.size() * sizeof(float));
                CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
                auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
                ThrowIfFailed(m_device->CreateCommittedResource(
                    &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_vertexBuffer)));

                UINT8* pData;
                CD3DX12_RANGE readRange(0, 0);
                ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pData)));
                memcpy(pData, sphereVertices.data(), vertexBufferSize);
                m_vertexBuffer->Unmap(0, nullptr);

                m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
                m_vertexBufferView.StrideInBytes = sizeof(float) * 8;
                m_vertexBufferView.SizeInBytes = vertexBufferSize;
            }

            {
                const UINT indexBufferSize = static_cast<UINT>(sphereIndices.size() * sizeof(UINT));
                CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
                auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
                ThrowIfFailed(m_device->CreateCommittedResource(
                    &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                    D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_indexBuffer)));

                void* pData;
                CD3DX12_RANGE readRange(0, 0);
                ThrowIfFailed(m_indexBuffer->Map(0, &readRange, &pData));
                memcpy(pData, sphereIndices.data(), indexBufferSize);
                m_indexBuffer->Unmap(0, nullptr);

                m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
                m_indexBufferView.SizeInBytes = indexBufferSize;
                m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
            }
        }

        {
            size_t cbSize = (sizeof(Constant) + 255) & (~255);
            CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
            auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_constantBuffer)));

            CD3DX12_RANGE readRange(0, 0);
            ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvDataBegin)));
        }

        m_srvHeap = create_descriptor_heap(m_device, PbrTextureCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
        m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        LoadTextures();

        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
        WaitForPreviousFrame();
    }

    void LoadTextures()
    {
        const char* texFilenames[PbrTextureCount] = {
            "rusted_iron/albedo.png",
            "rusted_iron/ao.png",
            "rusted_iron/metallic.png",
            "rusted_iron/normal.png",
            "rusted_iron/roughness.png"
        };

        ComPtr<ID3D12CommandAllocator> uploadAllocator;
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&uploadAllocator)));
        ComPtr<ID3D12GraphicsCommandList> uploadList;
        ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, uploadAllocator.Get(), nullptr, IID_PPV_ARGS(&uploadList)));

        for (UINT i = 0; i < PbrTextureCount; ++i)
        {
            std::string fullPath = std::string{PBR_TEXTURE_DIR} + "/" + texFilenames[i];

            int width, height, channels;
            unsigned char* imgData = stbi_load(fullPath.c_str(), &width, &height, &channels, 4);
            if (!imgData)
            {
                throw std::runtime_error("Failed to load texture: " + fullPath);
            }

            CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
            CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &defaultHeapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_textures[i])));

            ComPtr<ID3D12Resource> uploadHeap;
            CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
            const UINT64 uploadSize = GetRequiredIntermediateSize(m_textures[i].Get(), 0, 1);
            auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadHeap)));

            D3D12_SUBRESOURCE_DATA texData = {};
            texData.pData = imgData;
            texData.RowPitch = width * 4;
            texData.SlicePitch = texData.RowPitch * height;
            UpdateSubresources(uploadList.Get(), m_textures[i].Get(), uploadHeap.Get(), 0, 0, 1, &texData);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_textures[i].Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            uploadList->ResourceBarrier(1, &barrier);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
                m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
                i, m_srvDescriptorSize);
            m_device->CreateShaderResourceView(m_textures[i].Get(), &srvDesc, srvHandle);

            stbi_image_free(imgData);
        }

        ThrowIfFailed(uploadList->Close());
        ID3D12CommandList* ppCommandLists[] = { uploadList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        ComPtr<ID3D12Fence> uploadFence;
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&uploadFence)));
        HANDLE uploadEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        UINT64 uploadFenceValue = 1;
        m_commandQueue->Signal(uploadFence.Get(), uploadFenceValue);
        if (uploadFence->GetCompletedValue() < uploadFenceValue)
        {
            uploadFence->SetEventOnCompletion(uploadFenceValue, uploadEvent);
            WaitForSingleObject(uploadEvent, INFINITE);
        }
        CloseHandle(uploadEvent);
    }

    void update_constant_buffer(float rotationAngle)
    {
        float aspectRatio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);

        XMVECTOR eye = XMVectorSet(0.0f, 0.0f, -3.0f, 1.0f);
        XMVECTOR target = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 100.0f);
        XMMATRIX world = XMMatrixRotationY(rotationAngle);

        XMMATRIX wvp = world * view * proj;

        Constant cb;
        XMStoreFloat4x4(&cb.mvp, wvp);
        XMStoreFloat4x4(&cb.model, world);

        XMMATRIX world3x3 = XMMATRIX(
            XMVectorSetW(world.r[0], 0.0f),
            XMVectorSetW(world.r[1], 0.0f),
            XMVectorSetW(world.r[2], 0.0f),
            XMVectorSetW(world.r[3], 1.0f));
        XMMATRIX invWorld3x3 = XMMatrixInverse(nullptr, world3x3);
        XMMATRIX normalMatrix = XMMatrixTranspose(invWorld3x3);
        XMStoreFloat4x4(&cb.normal, normalMatrix);

        XMStoreFloat4(&cb.eye, eye);
        XMStoreFloat4(&cb.lightPos, XMVectorSet(5.0f, 5.0f, -5.0f, 1.0f));
        XMStoreFloat4(&cb.lightColor, XMVectorSet(5.0f, 5.0f, 5.0f, 1.0f));

        memcpy(m_cbvDataBegin, &cb, sizeof(cb));
    }

    void populate_command_list(float rotationAngle)
    {
        update_constant_buffer(rotationAngle);

        ThrowIfFailed(m_commandAllocator->Reset());
        ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

        m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
        m_commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffer->GetGPUVirtualAddress());

        ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

        m_commandList->RSSetViewports(1, &m_viewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &barrier);

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), (INT)m_frameIndex, m_rtvDescriptorSize);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
        m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        m_commandList->IASetIndexBuffer(&m_indexBufferView);
        m_commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_renderTargets[m_frameIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &barrier);

        ThrowIfFailed(m_commandList->Close());
    }

    void render(float rotationAngle)
    {
        populate_command_list(rotationAngle);

        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        ThrowIfFailed(m_swapChain->Present(1, 0));

        WaitForPreviousFrame();
    }
};

int main()
{
    GlfwWindow window{WIDTH, HEIGHT, "PBR Sphere"};
    PbrSphereContext context{window._hwnd};

    float rotationAngle = 0.0f;
    while (!glfwWindowShouldClose(window._window))
    {
        rotationAngle += 0.005f;
        context.render(rotationAngle);
        glfwPollEvents();
    }
    return 0;
}