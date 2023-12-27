#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/FileArchive.h"
#include "Utils/Hash.h"
#include "Utils/Handle.h"
#include "Utils/Reflection.h"
#include "Renderer/RtRenderShader.h"
#include "delegate/delegate/multicastdelegate.h"
#include <functional>
#include <shared_mutex>
#include <condition_variable>
#include <semaphore>
#ifdef _WIN32
#include <atlbase.h>
#include <dxc/dxcapi.h>
#include <d3d12shader.h> 
#endif

namespace Shard::Renderer
{
    REGIST_PARAM_TYPE(BOOL, SHADER_COMPILE_RETRY, false);
    REGIST_PARAM_TYPE(UINT, PSO_COMPILE_BACTH_SIZE, 128);
    REGIST_PARAM_TYPE(STRING, SHADER_CACHE_DIR, "/shaders/compiled/");
    REGIST_PARAM_TYPE(STRING, PSO_CACHE_DIR, "/pso.cache");

    struct ShaderCompileEnv;
    class MINIT_API RtShaderType
    {
    public:
        using Ptr = RtShaderType*;
        using HashType = Utils::Blake3Hash64;
        using CreateShaderFunc = std::function<RtRenderShader::SharedPtr(const RtRenderShaderInitializeInput&)>;
        using CreateShaderFromArchiveFunc = std::function<RtRenderShader::SharedPtr()>;
        using ValidShaderCompileOutputFunc = std::function<bool(const ShaderCompileEnv& env, uint32_t permutation_id, const RtShaderParameterInfosMap& parameter_map)>;
        using ShouldCompilePermutationFunc = std::function<bool(const ShaderCompileEnv& env, uint32_t permutation_id)>;
        static List<Ptr>    shader_type_repos_;
        using Iter = decltype(shader_type_repos_.begin());
        static bool Regist(Ptr shader_type);
        static inline Iter Begin() { return shader_type_repos_.begin(); }
        static inline Iter End() { return shader_type_repos_.end(); }
        static Ptr FindShaderTypeByName(const String& name);
        static Ptr FindShaderTypeByHashName(const HashType& hash);
        static Vector<Ptr> FindShaderTypeByFileName(const String& file_name);
        static Ptr GetShaderType(const HashType& hash);
    public:
        RtShaderType(const String& name, const String& file, const String& func, EShaderFrequency freq);
        const uint32_t GetPermutationCount()const;
        const String& GetName()const;
        const HashType& GetHashName()const;
        const String& GetFileName()const;
        const String& GetEntryName()const;
        const HashType& GetHashFileName()const;
        EShaderFrequency GetFrequency()const;
        void Compile(uint32_t permutation_id);
        FORCE_INLINE void SetCreateShaderFunc(const CreateShaderFunc& func) {
            shader_creator_ = func;
        }
        FORCE_INLINE void SetCreateShaderFromArchiveFunc(const CreateShaderFromArchiveFunc& func) {
            shader_archive_creator_ = func;
        }
        FORCE_INLINE void SetShouldCompilePermutationFunc(const ShouldCompilePermutationFunc& func) {
            should_compile_decider_ = func;
        }
        FORCE_INLINE bool ShouldCompile(const ShaderCompileEnv& env, uint32_t permutation_id)const {
            PCHECK(should_compile_decider_ != nullptr) << "should compile func not set correctly";
            return should_compile_decider_(env, permutation_id);
        }
        template<typename T, typename=void>
        bool BindTraitsFuncDelegate() {
            return false;
        }
        template<typename T>
        requires std::conditional_t<std::is_function_v<typename T::Create>, 
                                    std::conditional_t<std::is_function_v<typename T::CreateFromArchive>, 
                                    std::conditional_t<std::is_function_v<typename T::ShouldCompile>,
                                    std::true_type, std::false_type>, std::false_type>, std::false_type>::value
        bool BindTraitsFuncDelegate<T>() {
            SetCreateShaderFunc(typename T::Create);
            SetCreateShaderFromArchiveFunc(typename T::CreateFromArchive);
            SetShouldCompilePermutationFunc(typename T::ShouldCompile);
            return true;
        }

    private:
        String    name_;
        HashType    hash_name_;
        String    file_name_;
        HashType    file_name_hash_;
        //FIXME remember to save include file paths to a extra meta file
        String    entry_func_{ "Main" };
        EShaderFrequency    frequency_;
        CreateShaderFunc    shader_creator_{ nullptr };
        CreateShaderFromArchiveFunc    shader_archive_creator_{ nullptr };
        ShouldCompilePermutationFunc    should_compile_decider_{ nullptr };
        ValidShaderCompileOutputFunc    shader_compile_output_validator_{ nullptr };
    };

#define REGIST_SHADER_IMPL(name, file, func, freq, shader_class) \
    static Renderer::RtShaderType shader_class##ShaderType(name, file, function, freq);

    struct ShaderCompileEnv
    {
        ShaderPlatform    platform;
        //other related parameters todo 
    };

    struct ShaderCompileJobIn
    {
        using HashType = Utils::SpookyV2Hash32;
        mutable HashType hash_;
        String    file_;
        String    entry_;
        uint32_t    permutation_;
        ShaderCompileEnv    compile_env_;
        EIRBackend    ir_lang_{ EIRBackend::eSPIRV };
        SmallVector<String> include_path_;
        SmallVector<String>    defines_;
        SmallVector<String> extra_cli_;
        void ComputeHash()const;
        void InitByShaderType(const RtShaderType& shader_type, uint32_t permutation_id);
    };

    bool operator==(const ShaderCompileJobIn& lhs, const ShaderCompileJobIn& rhs);
    bool operator!=(const ShaderCompileJobIn& lhs, const ShaderCompileJobIn& rhs);
    
    //only read ANSI encode file
    void TraverseHLSLIncludePath(const String& prefix, const String& file, Vector<String>& paths);

    struct ShaderCompileJobOutput
    {
        enum class EJobStat {
            eFinalized = 0x1,
            eSuccess = 0x2,
            eFailed    = 0x4,
        };
        EJobStat    stat_;
        RtRenderShaderCode    shader_code_;
        String    compile_message_;
        //get output shader parameters
        RtShaderParameterInfosMap    shader_params_;
    };

    class RtShaderCompileWorker
    {
    public:
        using Ptr = RtShaderCompileWorker*;
        explicit RtShaderCompileWorker(ShaderCompileJobIn&& job_in, class RtShaderCompiledFileMap* owner):compile_input_(std::move(job_in)), filemap_owner_(owner) {}
        virtual ~RtShaderCompileWorker() {}
        virtual void Compile() = 0;
        FORCE_INLINE ShaderCompileJobOutput& GetOutput(){ return compile_output_; }
        FORCE_INLINE class RtShaderCompiledFileMap* GetOwner() { return filemap_owner_; }
        FORCE_INLINE const ShaderCompileJobIn::HashType GetHash()const { 
            compile_input_.ComputeHash();
            return compile_input_.hash_; 
        }
    protected:
        ShaderCompileJobIn    compile_input_;
        ShaderCompileJobOutput    compile_output_;
        class RtShaderCompiledFileMap*    filemap_owner_ { nullptr };
    };

#ifdef _WIN32
    class RtShaderDXCCompileWorker final: public RtShaderCompileWorker
    {
    public:
        using Ptr = RtShaderDXCCompileWorker*;
        explicit RtShaderDXCCompileWorker(ShaderCompileJobIn&& job_in, class RtShaderCompiledFileMap* owner);
        ~RtShaderDXCCompileWorker();
        void Compile() override;
        FORCE_INLINE CComPtr<IDxcUtils> GetDxcUtils() { return utils_; }
        FORCE_INLINE CComPtr<IDxcCompiler> GetDxcCompiler() { return compiler_; }
    private:
        static void GenerateCompileCLI(const ShaderCompileJobIn& job, SmallVector<LPCWSTR>& cli);
        static WString ConvertSMToTargetProfile(EShaderFrequency freq, EShaderModel sm);
        void DoCompileJob(const ShaderCompileJobIn& job, ShaderCompileJobOutput& output);
    private:
        CComPtr<IDxcUtils>    utils_;
        CComPtr<IDxcCompiler> compiler_;
    };
#endif

    class RtGlobalCompileWorkManager {
    public:
        static RtGlobalCompileWorkManager& Instance() {
            static RtGlobalCompileWorkManager instance;
            return instance;
        }
        uint32_t Submit(RtShaderCompileWorker::Ptr work);
        uint32_t Wait(uint32_t work_id)const;
        void FetchOutput(uint32_t work_id, ShaderCompileJobOutput& output);
        void FinalizeAllJobs();
        void Clear();
    private:
        RtGlobalCompileWorkManager() = default;
        DISALLOW_COPY_AND_ASSIGN(RtGlobalCompileWorkManager);
    private:
        struct CompileEntity
        {
            RtShaderCompileWorker::Ptr entity_{ nullptr };
            std::binary_semaphore    done_semaphore_;
        };
        Map<uint32_t, CompileEntity>    pending_works_;
        std::mutex    pending_work_mutex_;
        std::condition_variable    pengding_work_cv_;
        std::atomic_uint32_t    total_works_{ 0u };
        std::atomic_uint32_t    failed_works_{ 0u };
        std::atomic_uint32_t    successed_works_{ 0u };
        std::atomic_uint64_t    total_compile_time_{ 0u };
    };

    using ShaderKey = RtRenderShader::HashType;
    using ShaderHandle = Utils::Handle<RtRenderShader>;
    using ShaderHandleManager = Utils::HandleManager<RtRenderShader>;

    class RtShaderContent
    {
        BEGIN_DECLARE_TYPE_LAYOUT_DEF(RtShaderContent);
    public:
        size_t GetShaderCount()const {
            return shaders_.size();
        }
        FORCE_INLINE void AddShader(RtRenderShader::Ptr shader) {
            shaders_.emplace_back(shader);
        }
        FORCE_INLINE RtRenderShader::Ptr GetShader(const ShaderHandle& handle) {
            return shaders_[uint32_t(handle)];
        }
    private:
        LAYOUT_VECTOR_FIELD(,Renderer::RtRenderShader::Ptr, shaders_);
        END_DECLARE_TYPE_LAYOUT_DEF(RtShaderContent);
    };

    //shader map for single hlsl file
    class MINIT_API RtShaderCompiledFileMap
    {
    public:
        using Ptr = RtShaderCompiledFileMap*;
        using SharedPtr = std::shared_ptr<RtShaderCompiledFileMap>;
        using HashType = Utils::Blake3Hash64;
        RHI::RHIShader::Ptr GetRHIShader(const uint32_t index);
        RHI::RHIShader::Ptr    GetRHIShader(const ShaderKey& key);
        uint32_t GetRHIShaderIndex(const ShaderKey& key);
        //check file hash changed
        bool IsReCompileNeeded()const;
        const HashType& GetHash()const;
        const HashType& GetFileNameHash()const;
        const String& GetFileName()const;
        RtRenderShaderArchive::Ptr GetShaderArchive();
        void Serialize(FileArchive& ar);
        void UnSerialize(FileArchive& ar);
        ~RtShaderCompiledFileMap();
    private:
        //how to deal with compile work
        void CreateRHIShader(const ShaderKey& key);
    private:
        String    file_name_;
        HashType    file_name_hash_;
        HashType    hash_;
        mutable HashType    precalc_hash_; //to save pre calc hash for next use
        RtShaderContent    shader_content_;
        RtRenderShaderArchive::Ptr    shader_archive_;
        RHI::RHIShaderLibraryInterface::SharedPtr    shader_library_;
        //handle allocator logic
        Map<ShaderKey, ShaderHandle>    shader_handle_lut_;
        ShaderHandleManager    shader_hande_generator_;
    };

    class MINIT_API RtShaderCompiledRepo
    {
    public:
        using Ptr = RtShaderCompiledRepo*;
        static RtShaderCompiledRepo& Instance() {
            static RtShaderCompiledRepo repo;
            return repo;
        }
        RtShaderCompiledFileMap::Ptr FindOrCreateSection(const RtShaderType::HashType hash_name);
        RtShaderCompiledFileMap::Ptr FindSection(const RtShaderType::HashType hash_name);
        ~RtShaderCompiledRepo();
        void Serialize(FileArchive& ar)const;
        void UnSerialize(FileArchive& ar);
        /*begin and end async compile work*/
        void BeginCompile(const ShaderCompileEnv& compile_env);
        void EndCompile();
        /*compile shader map and wait util finished*/
        void Compile();
    private:
        RtShaderCompiledRepo() = default;
        DISALLOW_COPY_AND_ASSIGN(RtShaderCompiledRepo);
        void Add(const ShaderKey& sbader_key, RtRenderShader::SharedPtr shader);
        void Remove(const ShaderKey& shader_key);
        void CollectOutDatedShader(Vector<RtRenderShader::SharedPtr>& outdated_shaders)const;
        //void BeginCompileShader();
        //void WaitCompileShaderDone();
    private:
        Map<RtShaderCompiledFileMap::HashType, RtShaderCompiledFileMap::Ptr>    shader_sections_;
    };

    class MINIT_API RtPipelineCompiledRepo
    {
    public:
        using PSORepoOnLoadDelegate = DelegateLib::MulticastDelegate<void()>;
        using PSORepoOnSaveDelegate = DelegateLib::MulticastDelegate<void()>;
        using PSORepoPreCompileReportProgressDelegate = DelegateLib::MulticastDelegate<void(float percentage)>;
        static RtPipelineCompiledRepo& Instance() {
            static RtPipelineCompiledRepo instance;
            return instance;
        }
        void Serialize(const String& path);
        void UnSerialize(const String& path);
        bool IsPreCompileNeeded()const;
        void PreCompile(bool async = false);
        float GetPreCompileCompletePercentage()const;
        RtRenderShaderPipeline::SharedPtr GetPipeline(const PipelineStateObjectDesc& desc);
    private:
        struct CompileJob
        {
            //priority of each pso object
            enum EPriority
            {
                eLow,
                eMedium,
                eHigh,
                eNum,
            };
            EPriority    prior_;
            PipelineStateObjectDesc    desc_;
        };
        RtPipelineCompiledRepo() = default;
        DISALLOW_COPY_AND_ASSIGN(RtPipelineCompiledRepo);
        void CompileBatch(const Span<CompileJob>& job_batch);
        void Compile(const CompileJob& job);
    private:
        std::atomic_uint64_t    total_precompile_works_{ 0u };
        std::atomic_uint64_t    finish_precompile_works_{ 0u };
        Set<PipelineStateObjectDesc>    new_pso_;
        Set<PipelineStateObjectDesc>    record_pso_;
        Vector<CompileJob>    compile_jobs_;
        std::shared_mutex    compile_mutex_;
        std::shared_mutex    utility_mutex_;
        //delegate like unreal
        PSORepoPreCompileReportProgressDelegate compile_report_delegate_;
        PSORepoOnLoadDelegate    repo_load_delegate_;
        PSORepoOnSaveDelegate    repo_save_delegate_;
    };
}


