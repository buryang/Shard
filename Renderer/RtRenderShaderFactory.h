#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/FileArchive.h"
#include "Utils/Hash.h"
#include "RHI/RHIShader.h"
#include "RHI/RHIShaderLibrary.h"
#include "Renderer/RtRenderShader.h"
#include <functional>
#include <shared_mutex>
#include <condition_variable>
#include <semaphore>
#ifdef _WIN32
#include <atlbase.h>
#include <dxc/dxcapi.h>
#include <d3d12shader.h> 
#endif

namespace MetaInit::Renderer
{
	REGIST_PARAM_TYPE(BOOL, SHADER_COMPILE_RETRY, false);
	class MINIT_API RtShaderType
	{
	public:
		using Ptr = RtShaderType*;
		static Map<String, Ptr>	shader_repos_;
		using Iter = decltype(shader_repos_.begin());
		static bool Regist(Ptr shader_type);
		static inline Iter Begin() { return shader_repos_.begin(); }
		static inline Iter End() { return shader_repos_.end(); }
	public:
		RtShaderType(const String& name, const String& file, const String& func, EShaderFrequency freq);
		void Compile(uint32_t permutation_id);
		RtRenderShader::SharedPtr CreateShader(uint32_t permutation_id);
	private:
		String	name_;
		String	hash_name_;
		String	file_path_;
		//FIXME remember to save include file paths to a extra meta file
		String	entry_func_{ "Main" };
		EShaderFrequency	frequency_;
	};

#define REGIST_SHADER_IMPL(name, file, func, freq, shader_class) \
	static RtShaderType shader_class##ShaderType(name, file, function, freq);

	struct ShaderCompileJobIn
	{
		using HashType = Utils::SpookyV2Hash32;
		mutable HashType hash_;
		String	file_;
		String	entry_;
		uint32_t	permutation_;
		EShaderModel	shader_model_;
		SmallVector<String> include_path_;
		SmallVector<String>	defines_;
		SmallVector<String> extra_cli_;
		void ComputeHash()const;
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
			eFailed	= 0x4,
		};
		EJobStat	stat_;
		RtRenderShaderCode	shader_code_;
		String	compile_message_;
		//other data
	};

	class RtShaderCompileWorker
	{
	public:
		using Ptr = RtShaderCompileWorker*;
		explicit RtShaderCompileWorker(ShaderCompileJobIn&& job_in) :compile_input_(std::move(job_in)) {}
		virtual ~RtShaderCompileWorker() {}
		virtual void Compile() = 0;
		FORCE_INLINE const ShaderCompileJobOutput& GetOutput()const { return compile_output_; }
		FORCE_INLINE const ShaderCompileJobIn::HashType GetHash()const { 
			compile_input_.ComputeHash();
			return compile_input_.hash_; 
		}
	protected:
		ShaderCompileJobIn	compile_input_;
		ShaderCompileJobOutput	compile_output_;
	};

#ifdef _WIN32
	class RtShaderDXCCompileWorker final: public RtShaderCompileWorker
	{
	public:
		using Ptr = RtShaderDXCCompileWorker*;
		RtShaderDXCCompileWorker();
		~RtShaderDXCCompileWorker();
		void Compile() override;
		FORCE_INLINE CComPtr<IDxcUtils> GetDxcUtils() { return utils_; }
		FORCE_INLINE CComPtr<IDxcCompiler> GetDxcCompiler() { return compiler_; }
	private:
		static void GenerateCompileCLI(const ShaderCompileJobIn& job, SmallVector<LPCWSTR>& cli);
		static WString ConvertSMToTargetProfile(EShaderFrequency freq, EShaderModel sm);
		void DoCompileJob(const ShaderCompileJobIn& job, ShaderCompileJobOutput& output);
	private:
		CComPtr<IDxcUtils>	utils_;
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
		void FetchOutput(uint32_t work_id, ShaderCompileJobOutput& output)const;
		void WaitAllJobsDone()const;
	private:
		RtGlobalCompileWorkManager() = default;
		DISALLOW_COPY_AND_ASSIGN(RtGlobalCompileWorkManager);
	private:
		struct CompileEntity
		{
			RtShaderCompileWorker::Ptr entity_{ nullptr };
			std::binary_semaphore	done_semaphore_;
		};
		Map<uint32_t, CompileEntity>	pending_works_;
		std::mutex	pending_work_mutex_;
		std::condition_variable	pengding_work_cv_;
		std::atomic_uint32_t	total_works_{ 0u };
		std::atomic_uint32_t	failed_works_{ 0u };
		std::atomic_uint32_t	successed_works_{ 0u };
	};

	struct ShaderKey
	{
		using HashType = Utils::Blake3Hash64;
		HashType	shader_type_;
		uint32_t	permutation_{ 0 };
	};

	class MINIT_API RtShaderCompiledRepo
	{
	public:
		static RtShaderCompiledRepo& Instance() {
			static RtShaderCompiledRepo repo;
			return repo;
		}
		RHI::RHIShader::Ptr	GetRHIShader(const ShaderKey& key);
		RHI::RHIShader::Ptr GetRHIShaderWait(const ShaderKey& key);
		RtRenderShader::SharedPtr GetShader(const ShaderKey& key);
		/*read a shader if not found wait*/
		RtRenderShader::SharedPtr GetShaderWait(const ShaderKey& key);
		//force_rebuild: whether a shader code exist in shader archive or not force to rebuild it
		void StartAllCompileWorkers(bool force_rebuild = false);
		void WaitForAllCompileWorkers()const;
	private:
		RtShaderCompiledRepo() = default;
		DISALLOW_COPY_AND_ASSIGN(RtShaderCompiledRepo);
		void Add(const ShaderKey& sbader_key, RtRenderShader::SharedPtr shader);
		void Remove(const ShaderKey& shader_key);
		void CollectOutDatedShader(Vector<RtRenderShader::SharedPtr>& outdated_shaders)const;
		//void BeginCompileShader();
		//void WaitCompileShaderDone();
	private:
		RtShaderCompileWorker::Ptr	compile_worker_;
		mutable std::shared_mutex	compile_mutex_;
	};

	class MINIT_API RtShaderPipelineRepo
	{
	public:

	private:
		RHI::RHIShaderLibraryInterface::SharedPtr	shader_cache_;

	};
}
