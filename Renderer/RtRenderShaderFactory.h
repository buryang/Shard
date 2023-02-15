#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/FileArchive.h"
#include "Renderer/RtRenderShader.h"
#include <functional>
#include <shared_mutex>
#ifdef _WIN32
#include <atlbase.h>
#include <dxc/dxcapi.h>
#include <d3d12shader.h> 
#endif


namespace MetaInit::Renderer
{
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
		String	file_;
		String	entry_;
		uint32_t	permutation_;
		EShaderModel	shader_model_;
		SmallVector<String> include_path_;
		SmallVector<String>	defines_;
		SmallVector<String> extra_cli_;
	};

	struct ShaderCompileJobOutput
	{
		RtRenderShaderCode shader_code_;
		//other data
	};

	class RtShaderCompileWorker
	{
	public:
		using Ptr = RtShaderCompileWorker*;
		void Submit(const ShaderCompileJobIn& job);
		bool IsAllCompiled()const { return compile_jobs_.empty(); }
		virtual void Compile() = 0;
		virtual ~RtShaderCompileWorker() {}
	protected:
		Vector<ShaderCompileJobIn> compile_jobs_;
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
		void CompileSingleJob(const ShaderCompileJobIn& job, ShaderCompileJobOutput& output);
	private:
		CComPtr<IDxcUtils>	utils_;
		CComPtr<IDxcCompiler> compiler_;
	};
#endif

	class MINIT_API RtShaderCompiledRepo
	{
	public:
		using HashType = uint64_t;
		void Serialize(const String& file) const;
		void UnSerialize(const String& file);
		RtRenderShader::SharedPtr Read(HashType hash)const;
		/*read a shader if not found wait*/
		RtRenderShader::SharedPtr ReadWait(HashType hash)const;
		void WaitForAllCompileWorkers()const;
	private:
		void Add(HashType hash, RtRenderShader::SharedPtr shader);
		void Remove(HashType hash);
	private:
		Map<HashType, RtRenderShader::SharedPtr>	shader_repo_;
		std::shared_mutex	write_mutex_;
		SmallVector<RtShaderCompileWorker::Ptr> compile_workers_;
	};
}
