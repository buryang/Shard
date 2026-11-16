#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace {

constexpr uint32_t kRadix = 256;
constexpr uint32_t kRadixPasses = 4;
constexpr uint32_t kPartSize = 1792;
constexpr uint32_t kHistTileSize = 1024;
constexpr uint32_t kUavCount = 8;
constexpr uint32_t kPassLimit = 4;

enum class KeyMode {
    Uint,
    Int,
    Float,
};

KeyMode g_key_mode = KeyMode::Uint;
bool g_pairs = false;
bool g_extreme = false;

uint32_t TransformKey(uint32_t bits) {
    if (g_key_mode == KeyMode::Int) {
        return bits ^ 0x80000000u;
    }
    if (g_key_mode == KeyMode::Float) {
        const uint32_t mask = static_cast<uint32_t>(-static_cast<int32_t>(bits >> 31)) | 0x80000000u;
        return bits ^ mask;
    }
    return bits;
}

struct Config {
    uint32_t num_keys;
    uint32_t radix_shift;
    uint32_t thread_blocks;
    uint32_t global_hist_blocks;
    uint32_t is_tail;
    uint32_t is_reverse;
    uint32_t global_hist_index;
    uint32_t pass_hist_index;
    uint32_t sort_key_index;
    uint32_t alt_key_index;
    uint32_t sort_payload_index;
    uint32_t alt_payload_index;
    uint32_t counter_index;
};

struct Buffer {
    ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    uint64_t size = 0;
};

void Check(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        std::fprintf(stderr, "%s failed: 0x%08X\n", message, static_cast<unsigned>(hr));
        std::exit(1);
    }
}

std::vector<uint8_t> ReadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::fprintf(stderr, "Cannot open %s\n", path.c_str());
        std::exit(1);
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    return bytes;
}

class GpuSort {
public:
    explicit GpuSort(uint32_t count) : count_(count) {
        thread_blocks_ = (count_ + kPartSize - 1) / kPartSize;
        global_hist_blocks_ = (count_ + kHistTileSize - 1) / kHistTileSize;
        std::printf("CREATE device\n");
        std::fflush(stdout);
        CreateDevice();
        std::printf("CREATE root\n");
        std::fflush(stdout);
        CreateRootSignature();
        std::printf("CREATE pso\n");
        std::fflush(stdout);
        CreatePipelineStates();
        std::printf("CREATE heap\n");
        std::fflush(stdout);
        CreateDescriptorHeap();
        std::printf("CREATE buffers\n");
        std::fflush(stdout);
        CreateBuffers();
        std::printf("CREATE commands\n");
        std::fflush(stdout);
        CreateCommandObjects();
    }

    std::vector<uint32_t> Run(const std::vector<uint32_t>& input, const std::vector<uint32_t>& payload, bool reverse) {
        std::printf("RUN reset\n");
        std::fflush(stdout);
        ResetCommands();
        std::printf("RUN upload\n");
        std::fflush(stdout);
        Upload(input);
        if (g_pairs) {
            UploadPayload(payload);
        }

        std::printf("RUN clear\n");
        std::fflush(stdout);
        ClearBuffer(global_hist_, 0);
        ClearBuffer(pass_hist_, 0);
        ClearBuffer(counter_, 0);

        std::printf("RUN hist\n");
        std::fflush(stdout);
        Dispatch(upfront_pso_, global_hist_blocks_, MakeConfig(0, reverse));
        std::printf("RUN scan\n");
        std::fflush(stdout);
        Dispatch(scan_pso_, kRadixPasses, MakeConfig(0, reverse));

        uint32_t sort_key_index = 2;
        uint32_t alt_key_index = 3;
        uint32_t sort_payload_index = 4;
        uint32_t alt_payload_index = 5;
        DispatchProbe(2, 3);
        Transition(key_b_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        command_list_->CopyBufferRegion(probe_readback_.resource.Get(), 0, key_b_.resource.Get(), 0, sizeof(uint32_t));
        Transition(key_b_, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        for (uint32_t pass = 0; pass < kPassLimit; ++pass) {
            std::printf("RUN pass %u\n", pass);
            std::fflush(stdout);
            Config config = MakeConfig(pass * 8, reverse, sort_key_index, alt_key_index);
            config.sort_payload_index = sort_payload_index;
            config.alt_payload_index = alt_payload_index;
            Dispatch(digit_pso_, thread_blocks_, config);
            std::swap(sort_key_index, alt_key_index);
            std::swap(sort_payload_index, alt_payload_index);
        }

        std::printf("RUN read index=%u\n", sort_key_index);
        std::fflush(stdout);
        std::vector<uint32_t> result(count_);
        ReadBuffer(sort_key_index == 2 ? key_a_ : key_b_, result);
        if (g_pairs) {
            Transition(sort_payload_index == 4 ? payload_a_ : payload_b_,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
            command_list_->CopyBufferRegion(payload_readback_.resource.Get(), 0,
                (sort_payload_index == 4 ? payload_a_ : payload_b_).resource.Get(), 0, count_ * sizeof(uint32_t));
        }
        Transition(pass_hist_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        command_list_->CopyBufferRegion(hist_readback_.resource.Get(), 0, pass_hist_.resource.Get(), 0, pass_hist_.size);
        Transition(counter_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        command_list_->CopyBufferRegion(counter_readback_.resource.Get(), 0, counter_.resource.Get(), 0, counter_.size);
        std::printf("RUN execute\n");
        std::fflush(stdout);
        ExecuteAndWait();
        std::printf("RUN map\n");
        std::fflush(stdout);
        ReadBack(result);
        if (g_pairs) {
            std::vector<uint32_t> payload_result(count_);
            ReadBackTo(payload_readback_, payload_result);
            result.insert(result.end(), payload_result.begin(), payload_result.end());
        }
        ReadBackProbe();
        ReadBackHist();
        ReadBackCounter();
        return result;
    }

    std::vector<uint32_t> RunBitonic(const std::vector<uint32_t>& input, bool reverse, bool bit64) {
        ResetCommands();
        UploadBitonic(input);

        Config config = MakeConfig(0, false);
        command_list_->SetComputeRoot32BitConstants(0, sizeof(Config) / sizeof(uint32_t), &config, 0);
        const uint32_t bitonic_config[5] = { 2u, count_, reverse ? 1u : 0u, 0u, 0u };
        command_list_->SetComputeRoot32BitConstants(1, 5, bitonic_config, 0);
        command_list_->SetPipelineState(bit64 ? bitonic64_pso_.Get() : bitonic32_pso_.Get());
        command_list_->Dispatch(1, 1, 1);
        UavBarrier();

        std::vector<uint32_t> result(input.size());
        ReadBuffer(key_a_, result);
        ExecuteAndWait();
        ReadBack(result);
        return result;
    }

private:
    void UploadBitonic(const std::vector<uint32_t>& input) {
        void* mapped = nullptr;
        const D3D12_RANGE range{ 0, static_cast<SIZE_T>(input.size() * sizeof(uint32_t)) };
        Check(upload_.resource->Map(0, &range, &mapped), "BitonicUpload::Map");
        std::memcpy(mapped, input.data(), input.size() * sizeof(uint32_t));
        upload_.resource->Unmap(0, nullptr);

        Transition(key_a_, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        command_list_->CopyBufferRegion(key_a_.resource.Get(), 0, upload_.resource.Get(), 0, input.size() * sizeof(uint32_t));
        Transition(key_a_, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    void CreateDevice() {
        ComPtr<IDXGIFactory4> factory;
        Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");

        ComPtr<IDXGIAdapter1> adapter;
        for (uint32_t index = 0; factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                break;
            }
        }
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_)), "D3D12CreateDevice");
        ComPtr<ID3D12InfoQueue> info_queue;
        if (SUCCEEDED(device_.As(&info_queue))) {
            info_queue_ = info_queue;
        }
    }

    void CreateRootSignature() {
        D3D12_DESCRIPTOR_RANGE1 ranges[3]{};
        ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 1, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0 };
        ranges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 1, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 1 };
        ranges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 8, 1, 1, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 2 };

        D3D12_ROOT_PARAMETER1 parameters[3]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants.ShaderRegister = 0;
        parameters[0].Constants.RegisterSpace = 0;
        parameters[0].Constants.Num32BitValues = sizeof(Config) / sizeof(uint32_t);
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants.ShaderRegister = 1;
        parameters[1].Constants.RegisterSpace = 0;
        parameters[1].Constants.Num32BitValues = 5;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[2].DescriptorTable.NumDescriptorRanges = 3;
        parameters[2].DescriptorTable.pDescriptorRanges = ranges;
        parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC1 desc{};
        desc.NumParameters = 3;
        desc.pParameters = parameters;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                     D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned{};
        versioned.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        versioned.Desc_1_1 = desc;

        ComPtr<ID3DBlob> blob;
        ComPtr<ID3DBlob> error;
        Check(D3D12SerializeVersionedRootSignature(&versioned, &blob, &error), "SerializeRootSignature");
        Check(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_signature_)), "CreateRootSignature");
    }

    ComPtr<ID3D12PipelineState> CreatePipeline(const char* path) {
        const std::vector<uint8_t> bytecode = ReadFile(path);
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = root_signature_.Get();
        desc.CS.pShaderBytecode = bytecode.data();
        desc.CS.BytecodeLength = bytecode.size();

        ComPtr<ID3D12PipelineState> pipeline;
        Check(device_->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline)), path);
        return pipeline;
    }

    void CreatePipelineStates() {
        upfront_pso_ = CreatePipeline("shaders/CSUpfrontHist.cso");
        scan_pso_ = CreatePipeline("shaders/CSGlobalExclusiveSum.cso");
        digit_pso_ = CreatePipeline("shaders/CSChainedScanDigitBinning.cso");
        probe_pso_ = CreatePipeline("shaders/CSProbe.cso");
        bitonic32_pso_ = CreatePipeline("shaders/CSBitonicPreSort32.cso");
        bitonic64_pso_ = CreatePipeline("shaders/CSBitonicPreSort64.cso");
    }

    void CreateDescriptorHeap() {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 2 + kUavCount;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        Check(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_)), "CreateDescriptorHeap");
        descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    Buffer CreateBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state) {
        D3D12_HEAP_PROPERTIES heap_properties{};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = flags;

        Buffer buffer{};
        buffer.size = size;
        buffer.state = state;
        Check(device_->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&buffer.resource)), "CreateCommittedResource");
        return buffer;
    }

    Buffer CreateBufferWithHeap(D3D12_HEAP_TYPE heap_type, uint64_t size, D3D12_RESOURCE_STATES state) {
        D3D12_HEAP_PROPERTIES heap_properties{};
        heap_properties.Type = heap_type;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Buffer buffer{};
        buffer.size = size;
        buffer.state = state;
        Check(device_->CreateCommittedResource(&heap_properties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&buffer.resource)), "CreateBufferWithHeap");
        return buffer;
    }

    void CreateBuffers() {
        key_a_ = CreateBuffer(count_ * sizeof(uint32_t) * 2, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        key_b_ = CreateBuffer(count_ * sizeof(uint32_t) * 2, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        dummy_ = CreateBuffer(16, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON);
        payload_a_ = CreateBuffer(count_ * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        payload_b_ = CreateBuffer(count_ * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        global_hist_ = CreateBuffer(kRadix * kRadixPasses * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        pass_hist_ = CreateBuffer(static_cast<uint64_t>(thread_blocks_) * kRadix * kRadixPasses * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        counter_ = CreateBuffer(kRadixPasses * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
        readback_ = CreateBufferWithHeap(D3D12_HEAP_TYPE_READBACK, count_ * sizeof(uint32_t) * 2, D3D12_RESOURCE_STATE_COPY_DEST);
        payload_readback_ = CreateBufferWithHeap(D3D12_HEAP_TYPE_READBACK, count_ * sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST);
        hist_readback_ = CreateBufferWithHeap(D3D12_HEAP_TYPE_READBACK, pass_hist_.size, D3D12_RESOURCE_STATE_COPY_DEST);
        probe_readback_ = CreateBufferWithHeap(D3D12_HEAP_TYPE_READBACK, sizeof(uint32_t), D3D12_RESOURCE_STATE_COPY_DEST);
        counter_readback_ = CreateBufferWithHeap(D3D12_HEAP_TYPE_READBACK, counter_.size, D3D12_RESOURCE_STATE_COPY_DEST);
        upload_ = CreateBufferWithHeap(D3D12_HEAP_TYPE_UPLOAD, count_ * sizeof(uint32_t) * 2, D3D12_RESOURCE_STATE_GENERIC_READ);
        payload_upload_ = CreateBufferWithHeap(D3D12_HEAP_TYPE_UPLOAD, count_ * sizeof(uint32_t), D3D12_RESOURCE_STATE_GENERIC_READ);
        zero_upload_ = CreateBufferWithHeap(D3D12_HEAP_TYPE_UPLOAD, pass_hist_.size, D3D12_RESOURCE_STATE_GENERIC_READ);

        void* zero_data = nullptr;
        const D3D12_RANGE zero_range{ 0, static_cast<SIZE_T>(zero_upload_.size) };
        Check(zero_upload_.resource->Map(0, &zero_range, &zero_data), "ZeroUpload::Map");
        std::memset(zero_data, 0, static_cast<size_t>(zero_upload_.size));
        zero_upload_.resource->Unmap(0, nullptr);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.Format = DXGI_FORMAT_R32_TYPELESS;
        uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav.Buffer.NumElements = static_cast<UINT>(pass_hist_.size / sizeof(uint32_t));
        uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Buffer.NumElements = 1;
        srv.Buffer.StructureByteStride = sizeof(uint32_t);

        D3D12_CPU_DESCRIPTOR_HANDLE cpu = heap_->GetCPUDescriptorHandleForHeapStart();
        device_->CreateShaderResourceView(dummy_.resource.Get(), &srv, cpu);
        cpu.ptr += descriptor_size_;
        device_->CreateShaderResourceView(dummy_.resource.Get(), &srv, cpu);

        const Buffer* uav_buffers[kUavCount] = {
            &global_hist_, &pass_hist_, &key_a_, &key_b_, &payload_a_, &payload_b_, &counter_, &counter_
        };
        cpu.ptr += descriptor_size_;
        for (uint32_t index = 0; index < kUavCount; ++index) {
            uav.Buffer.NumElements = static_cast<UINT>((uav_buffers[index]->size + 3) / 4);
            device_->CreateUnorderedAccessView(uav_buffers[index]->resource.Get(), nullptr, &uav, cpu);
            cpu.ptr += descriptor_size_;
        }
    }

    void CreateCommandObjects() {
        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_)), "CreateCommandQueue");
        Check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator_)), "CreateCommandAllocator");
        Check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator_.Get(), nullptr, IID_PPV_ARGS(&command_list_)), "CreateCommandList");
        Check(command_list_->Close(), "CloseInitialCommandList");
        Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)), "CreateFence");
        fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!fence_event_) {
            std::fprintf(stderr, "CreateEvent failed\n");
            std::exit(1);
        }
    }

    void ResetCommands() {
        Check(allocator_->Reset(), "CommandAllocator::Reset");
        Check(command_list_->Reset(allocator_.Get(), nullptr), "CommandList::Reset");
        ID3D12DescriptorHeap* heaps[] = { heap_.Get() };
        command_list_->SetDescriptorHeaps(1, heaps);
        command_list_->SetComputeRootSignature(root_signature_.Get());
        command_list_->SetComputeRootDescriptorTable(2, heap_->GetGPUDescriptorHandleForHeapStart());
    }

    Config MakeConfig(uint32_t radix_shift, bool reverse, uint32_t sort_key_index = 2, uint32_t alt_key_index = 3) const {
        return Config{
            .num_keys = count_,
            .radix_shift = radix_shift,
            .thread_blocks = thread_blocks_,
            .global_hist_blocks = global_hist_blocks_,
            .is_tail = count_ % kPartSize != 0,
            .is_reverse = reverse ? 1u : 0u,
            .global_hist_index = 0,
            .pass_hist_index = 1,
            .sort_key_index = sort_key_index,
            .alt_key_index = alt_key_index,
            .sort_payload_index = 4,
            .alt_payload_index = 5,
            .counter_index = 6,
        };
    }

    void Upload(const std::vector<uint32_t>& input) {
        void* mapped = nullptr;
        const D3D12_RANGE range{ 0, static_cast<SIZE_T>(input.size() * sizeof(uint32_t)) };
        Check(upload_.resource->Map(0, &range, &mapped), "Upload::Map");
        std::memcpy(mapped, input.data(), input.size() * sizeof(uint32_t));
        upload_.resource->Unmap(0, nullptr);

        Transition(key_a_, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        command_list_->CopyBufferRegion(key_a_.resource.Get(), 0, upload_.resource.Get(), 0, input.size() * sizeof(uint32_t));
        Transition(key_a_, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Transition(key_b_, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    void UploadPayload(const std::vector<uint32_t>& payload) {
        void* mapped = nullptr;
        const D3D12_RANGE range{ 0, static_cast<SIZE_T>(payload.size() * sizeof(uint32_t)) };
        Check(payload_upload_.resource->Map(0, &range, &mapped), "PayloadUpload::Map");
        std::memcpy(mapped, payload.data(), payload.size() * sizeof(uint32_t));
        payload_upload_.resource->Unmap(0, nullptr);

        Transition(payload_a_, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        command_list_->CopyBufferRegion(payload_a_.resource.Get(), 0, payload_upload_.resource.Get(), 0, payload.size() * sizeof(uint32_t));
        Transition(payload_a_, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Transition(payload_b_, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    void Transition(Buffer& buffer, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        if (before == after) {
            return;
        }
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = buffer.resource.Get();
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list_->ResourceBarrier(1, &barrier);
        buffer.state = after;
    }

    void UavBarrier() {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = nullptr;
        command_list_->ResourceBarrier(1, &barrier);
    }

    void ClearBuffer(Buffer& buffer, uint32_t value) {
        (void)value;
        Transition(buffer, buffer.state, D3D12_RESOURCE_STATE_COPY_DEST);
        command_list_->CopyBufferRegion(buffer.resource.Get(), 0, zero_upload_.resource.Get(), 0, buffer.size);
        Transition(buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        UavBarrier();
    }

    void Dispatch(const ComPtr<ID3D12PipelineState>& pipeline, uint32_t groups, const Config& config) {
        command_list_->SetPipelineState(pipeline.Get());
        command_list_->SetComputeRoot32BitConstants(0, sizeof(Config) / sizeof(uint32_t), &config, 0);
        command_list_->Dispatch(groups, 1, 1);
        UavBarrier();
    }

    void DispatchProbe(uint32_t src, uint32_t dst) {
        const uint32_t values[2] = { src, dst };
        command_list_->SetPipelineState(probe_pso_.Get());
        command_list_->SetComputeRoot32BitConstants(0, 2, values, 0);
        command_list_->Dispatch(1, 1, 1);
        UavBarrier();
    }

    void ReadBuffer(Buffer& source, std::vector<uint32_t>& output) {
        Transition(source, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        command_list_->CopyBufferRegion(readback_.resource.Get(), 0, source.resource.Get(), 0, output.size() * sizeof(uint32_t));
        UavBarrier();
    }

    void ExecuteAndWait() {
        Check(command_list_->Close(), "CommandList::Close");
        ID3D12CommandList* lists[] = { command_list_.Get() };
        queue_->ExecuteCommandLists(1, lists);
        ++fence_value_;
        Check(queue_->Signal(fence_.Get(), fence_value_), "Fence::Signal");
        if (fence_->GetCompletedValue() < fence_value_) {
            Check(fence_->SetEventOnCompletion(fence_value_, fence_event_), "SetEventOnCompletion");
            WaitForSingleObject(fence_event_, INFINITE);
        }
        if (info_queue_) {
            const UINT64 count = info_queue_->GetNumStoredMessages();
            for (UINT64 index = 0; index < count; ++index) {
                SIZE_T size = 0;
                info_queue_->GetMessage(index, nullptr, &size);
                std::vector<char> storage(size);
                auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
                if (SUCCEEDED(info_queue_->GetMessage(index, message, &size))) {
                    std::fprintf(stderr, "D3D12: %s\n", message->pDescription);
                }
            }
            info_queue_->ClearStoredMessages();
        }
    }

    void ReadBack(std::vector<uint32_t>& output) {
        ReadBackTo(readback_, output);
    }

    void ReadBackTo(Buffer& source, std::vector<uint32_t>& output) {
        void* mapped = nullptr;
        const D3D12_RANGE range{ 0, static_cast<SIZE_T>(output.size() * sizeof(uint32_t)) };
        Check(source.resource->Map(0, &range, &mapped), "Readback::Map");
        std::memcpy(output.data(), mapped, output.size() * sizeof(uint32_t));
        const D3D12_RANGE write_range{ 0, 0 };
        source.resource->Unmap(0, &write_range);
    }

    void ReadBackHist() {
        void* mapped = nullptr;
        const D3D12_RANGE range{ 0, static_cast<SIZE_T>(hist_readback_.size) };
        Check(hist_readback_.resource->Map(0, &range, &mapped), "HistReadback::Map");
        const auto* values = static_cast<const uint32_t*>(mapped);
        for (uint32_t pass = 0; pass < kRadixPasses; ++pass) {
            const uint32_t base = pass * kRadix * thread_blocks_;
            std::printf("PHIST pass=%u %u %u %u %u\n", pass,
                values[base + 0], values[base + 1], values[base + 2], values[base + 3], values[base + 4]);
        }
        const D3D12_RANGE write_range{ 0, 0 };
        hist_readback_.resource->Unmap(0, &write_range);
    }

    void ReadBackProbe() {
        void* mapped = nullptr;
        const D3D12_RANGE range{ 0, sizeof(uint32_t) };
        Check(probe_readback_.resource->Map(0, &range, &mapped), "ProbeReadback::Map");
        std::printf("PROBE value=0x%08X\n", *static_cast<const uint32_t*>(mapped));
        const D3D12_RANGE write_range{ 0, 0 };
        probe_readback_.resource->Unmap(0, &write_range);
    }

    void ReadBackCounter() {
        void* mapped = nullptr;
        const D3D12_RANGE range{ 0, static_cast<SIZE_T>(counter_readback_.size) };
        Check(counter_readback_.resource->Map(0, &range, &mapped), "CounterReadback::Map");
        const auto* values = static_cast<const uint32_t*>(mapped);
        std::printf("COUNTER %u %u %u %u\n", values[0], values[1], values[2], values[3]);
        const D3D12_RANGE write_range{ 0, 0 };
        counter_readback_.resource->Unmap(0, &write_range);
    }

    uint32_t count_;
    uint32_t thread_blocks_ = 0;
    uint32_t global_hist_blocks_ = 0;
    uint32_t descriptor_size_ = 0;
    uint64_t fence_value_ = 0;
    HANDLE fence_event_ = nullptr;
    D3D12_COMMAND_QUEUE_DESC queue_desc_{};

    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12InfoQueue> info_queue_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12CommandAllocator> allocator_;
    ComPtr<ID3D12GraphicsCommandList> command_list_;
    ComPtr<ID3D12Fence> fence_;
    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12DescriptorHeap> heap_;
    ComPtr<ID3D12PipelineState> upfront_pso_;
    ComPtr<ID3D12PipelineState> scan_pso_;
    ComPtr<ID3D12PipelineState> digit_pso_;
    ComPtr<ID3D12PipelineState> probe_pso_;
    ComPtr<ID3D12PipelineState> bitonic32_pso_;
    ComPtr<ID3D12PipelineState> bitonic64_pso_;

    Buffer key_a_;
    Buffer key_b_;
    Buffer dummy_;
    Buffer payload_a_;
    Buffer payload_b_;
    Buffer global_hist_;
    Buffer pass_hist_;
    Buffer counter_;
    Buffer readback_;
    Buffer payload_readback_;
    Buffer hist_readback_;
    Buffer probe_readback_;
    Buffer counter_readback_;
    Buffer upload_;
    Buffer payload_upload_;
    Buffer zero_upload_;
};

bool TestSize(uint32_t count, bool reverse) {
    std::printf("START count=%u reverse=%d\n", count, reverse ? 1 : 0);
    std::fflush(stdout);

    struct TestPair {
        uint32_t key_bits = 0;
        uint32_t sort_key = 0;
        uint32_t payload = 0;
    };

    std::vector<uint32_t> input(count);
    std::vector<uint32_t> payload(count);
    std::vector<TestPair> expected(count);
    std::mt19937 rng(count + 17);
    for (uint32_t index = 0; index < count; ++index) {
        uint32_t bits = rng();
        if (g_key_mode == KeyMode::Float) {
            const int32_t whole = static_cast<int32_t>(rng() % 2000001u) - 1000000;
            const float value = static_cast<float>(whole) + static_cast<float>(rng() & 0xffffu) / 65536.0f;
            bits = std::bit_cast<uint32_t>(value);
        }
        input[index] = bits;
        payload[index] = index;
        expected[index] = { bits, TransformKey(bits), index };
    }
    if (g_extreme && count > 0) {
        input[0] = 0xffffffffu;
        payload[0] = 0;
        expected[0] = { input[0], TransformKey(input[0]), payload[0] };
        input[count - 1] = 0u;
        payload[count - 1] = count - 1;
        expected[count - 1] = { input[count - 1], TransformKey(input[count - 1]), payload[count - 1] };
    }

    std::sort(expected.begin(), expected.end(), [](const TestPair& lhs, const TestPair& rhs) {
        return lhs.sort_key < rhs.sort_key;
    });
    if (reverse) {
        std::reverse(expected.begin(), expected.end());
    }

    GpuSort sorter(count);
    std::printf("DEVICE count=%u\n", count);
    std::fflush(stdout);
    std::vector<uint32_t> actual = sorter.Run(input, payload, reverse);
    std::vector<uint32_t> actual_keys(actual.begin(), actual.begin() + count);
    std::vector<uint32_t> actual_payload;
    if (g_pairs) {
        actual_payload.assign(actual.begin() + count, actual.end());
    }
    std::printf("READ count=%u\n", count);
    std::fflush(stdout);

    bool ok = true;
    for (size_t index = 0; index < actual_keys.size(); ++index) {
        if (actual_keys[index] != expected[index].key_bits) {
            std::fprintf(stderr, "FAIL key count=%u reverse=%d index=%zu actual=0x%08X expected=0x%08X\n",
                count, reverse ? 1 : 0, index, actual_keys[index], expected[index].key_bits);
            ok = false;
            break;
        }
    }
    if (ok && g_pairs) {
        std::vector<uint32_t> expected_payload(count);
        for (uint32_t index = 0; index < count; ++index) {
            expected_payload[index] = expected[index].payload;
        }
        for (uint32_t begin = 0; begin < count;) {
            uint32_t end = begin + 1;
            while (end < count && expected[begin].key_bits == expected[end].key_bits) {
                ++end;
            }
            std::sort(actual_payload.begin() + begin, actual_payload.begin() + end);
            std::sort(expected_payload.begin() + begin, expected_payload.begin() + end);
            begin = end;
        }
        if (actual_payload != expected_payload) {
            for (size_t index = 0; index < actual_payload.size(); ++index) {
                if (actual_payload[index] != expected_payload[index]) {
                    std::fprintf(stderr, "FAIL payload count=%u reverse=%d index=%zu actual=%u expected=%u\n",
                        count, reverse ? 1 : 0, index, actual_payload[index], expected_payload[index]);
                    break;
                }
            }
            ok = false;
        }
    }
    if (!ok) {
        return false;
    }
    std::printf("PASS count=%u reverse=%d\n", count, reverse ? 1 : 0);
    return true;
}

bool TestBitonic(uint32_t count, bool reverse, bool bit64) {
    std::printf("START bitonic count=%u reverse=%d bit64=%d\n", count, reverse ? 1 : 0, bit64 ? 1 : 0);
    std::mt19937 rng(count + 101);
    std::vector<uint32_t> input;
    std::vector<uint32_t> expected_keys(count);
    std::vector<uint32_t> expected_payload(count);
    for (uint32_t index = 0; index < count; ++index) {
        const uint32_t key = rng();
        expected_keys[index] = key;
        expected_payload[index] = index;
        if (bit64) {
            input.push_back(index);
            input.push_back(key);
        } else {
            input.push_back(key);
        }
    }

    std::vector<uint32_t> order(count);
    for (uint32_t index = 0; index < count; ++index) {
        order[index] = index;
    }
    std::sort(order.begin(), order.end(), [&](uint32_t lhs, uint32_t rhs) {
        return expected_keys[lhs] < expected_keys[rhs];
    });
    if (reverse) {
        std::reverse(order.begin(), order.end());
    }

    GpuSort sorter(count);
    std::vector<uint32_t> actual = sorter.RunBitonic(input, reverse, bit64);
    bool ok = true;
    if (bit64) {
        for (uint32_t index = 0; index < count; ++index) {
            if (actual[index * 2 + 1] != expected_keys[order[index]]) {
                std::fprintf(stderr, "FAIL bitonic key count=%u reverse=%d index=%u actual=0x%08X expected=0x%08X\n",
                    count, reverse ? 1 : 0, index, actual[index * 2 + 1], expected_keys[order[index]]);
                ok = false;
                break;
            }
        }
        if (ok) {
            std::vector<uint32_t> actual_payload(count);
            std::vector<uint32_t> expected_payload_sorted(count);
            for (uint32_t index = 0; index < count; ++index) {
                actual_payload[index] = actual[index * 2];
                expected_payload_sorted[index] = expected_payload[order[index]];
            }
            for (uint32_t begin = 0; begin < count;) {
                uint32_t end = begin + 1;
                while (end < count && expected_keys[order[begin]] == expected_keys[order[end]]) {
                    ++end;
                }
                std::sort(actual_payload.begin() + begin, actual_payload.begin() + end);
                std::sort(expected_payload_sorted.begin() + begin, expected_payload_sorted.begin() + end);
                begin = end;
            }
            for (uint32_t index = 0; index < count; ++index) {
                if (actual_payload[index] != expected_payload_sorted[index]) {
                    std::fprintf(stderr, "FAIL bitonic payload count=%u reverse=%d index=%u actual=%u expected=%u\n",
                        count, reverse ? 1 : 0, index, actual_payload[index], expected_payload_sorted[index]);
                    ok = false;
                    break;
                }
            }
        }
    } else {
        for (uint32_t index = 0; index < count; ++index) {
            if (actual[index] != expected_keys[order[index]]) {
                std::fprintf(stderr, "FAIL bitonic key count=%u reverse=%d index=%u actual=0x%08X expected=0x%08X\n",
                    count, reverse ? 1 : 0, index, actual[index], expected_keys[order[index]]);
                ok = false;
                break;
            }
        }
    }
    if (ok) {
        std::printf("PASS bitonic count=%u reverse=%d bit64=%d\n", count, reverse ? 1 : 0, bit64 ? 1 : 0);
    }
    return ok;
}

}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "bitonic") {
        const uint32_t bitonic_sizes[] = { 1, 31, 32, 33, 100, 1024, 1792, 2048 };
        bool bitonic_ok = true;
        for (uint32_t count : bitonic_sizes) {
            bitonic_ok = TestBitonic(count, false, false) && bitonic_ok;
            bitonic_ok = TestBitonic(count, true, false) && bitonic_ok;
            bitonic_ok = TestBitonic(count, false, true) && bitonic_ok;
            bitonic_ok = TestBitonic(count, true, true) && bitonic_ok;
        }
        return bitonic_ok ? 0 : 1;
    }
    if (argc > 1) {
        const std::string key = argv[1];
        g_key_mode = key == "int" ? KeyMode::Int : (key == "float" ? KeyMode::Float : KeyMode::Uint);
    }
    if (argc > 2) {
        g_pairs = std::string(argv[2]) == "pair";
    }
    if (argc > 3) {
        g_extreme = std::string(argv[3]) == "extreme";
    }
    std::printf("MODE key=%s pairs=%d\n",
        g_key_mode == KeyMode::Int ? "int" : (g_key_mode == KeyMode::Float ? "float" : "uint"),
        g_pairs ? 1 : 0);
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
    }
    const uint32_t sizes[] = { 1, 31, 32, 33, 100, 1023, 1024, 1025, 1791, 1792, 1793, 5000 };
    bool ok = true;
    for (uint32_t count : sizes) {
        ok = TestSize(count, false) && ok;
    }
    ok = TestSize(1792, true) && ok;
    ok = TestSize(5000, true) && ok;
    return ok ? 0 : 1;
}
