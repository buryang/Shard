#include "Utils/FileArchive.h"
#include "Renderer/RtRenderShaderFactory.h"

namespace MetaInit::Renderer {

	bool RtShaderType::Regist(Ptr shader_type)
	{
		if (auto iter = shader_repos_.find(shader_type->name_); iter == shader_repos_.end()) {
			shader_repos_.insert({shader_type->name_, shader_type});
		}
		return false;
	}

	void RtShaderCompiledRepo::Serialize(const String& file) const
	{
		WaitForAllCompileWorkers();
		Utils::FileArchive shader_archive(file, Utils::FileArchive::eAppend);

	}

	void RtShaderCompileWorker::Submit(const ShaderCompileJobIn& job)
	{
		compile_jobs_.emplace_back(job);
	}

	void RtShaderCompiledRepo::UnSerialize(const String& file)
	{
		Utils::FileArchive shader_archive(file, Utils::FileArchive::eRead);
	}

	RtRenderShader::SharedPtr RtShaderCompiledRepo::Read(HashType hash)const
	{
		std::shared_lock<std::shared_mutex>(write_mutex_);
		auto iter = shader_repo_.find(hash);
		if (iter != shader_repo_.end()) {
			return iter->second;
		}
		return RtRenderShader::SharedPtr();
	}

	RtRenderShader::SharedPtr RtShaderCompiledRepo::ReadWait(HashType hash)const
	{
		auto ptr = Read(hash);
		if (ptr.get() == nullptr) {
			WaitForAllCompileWorkers();
			ptr = Read(hash);
		}
		return ptr;
	}

	void RtShaderCompiledRepo::Add(HashType hash, RtRenderShader::SharedPtr shader)
	{
		std::unique_lock<std::shared_mutex>(write_mutex_);
		shader_repo_.insert({hash, shader});
	}

	void RtShaderCompiledRepo::Remove(HashType hash)
	{
		std::unique_lock<std::shared_mutex>(write_mutex_);
		shader_repo_.erase(hash);
	}





}
