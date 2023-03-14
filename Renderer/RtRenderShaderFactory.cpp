#include "Utils/FileArchive.h"
#include "Utils/SimpleJobSystem.h"
#include "Core/RenderGlobalParams.h"
#include "Renderer/RtRenderShaderFactory.h"
#include <filesystem>
#include <string>
 
using Path = std::filesystem::path;
namespace MetaInit::Renderer {

	REGIST_PARAM_TYPE(UINT, RENDER_COMPILE_WORKERS, 128);

	bool RtShaderType::Regist(Ptr shader_type)
	{
		if (auto iter = shader_repos_.find(shader_type->name_); iter == shader_repos_.end()) {
			shader_repos_.insert({shader_type->name_, shader_type});
		}
		return false;
	}

	void RtShaderCompiledRepo::Serialize(Utils::FileArchive& archive) const
	{
	}

	void RtShaderCompiledRepo::UnSerialize(Utils::FileArchive& archive)
	{
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

	void RtShaderCompiledRepo::StartAllCompileWorkers(bool force_rebuild)
	{

	}

	void RtShaderCompiledRepo::WaitForAllCompileWorkers() const
	{
		//to do 
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

	void RtShaderDXCCompileWorker::Compile()
	{
		DoCompileJob(compile_input_, compile_output_);
	}

	void ShaderCompileJobIn::ComputeHash() const
	{
		if (hash_.IsZero()) {
			folly::hash::SpookyHashV2 hasher;
			hasher.Init(0u, 1024u);
			hasher.Update(file_.data(), file_.size());
			hasher.Update(entry_.data(), entry_.size());
			hasher.Update(&permutation_, sizeof(permutation_));
			hasher.Update(&shader_model_, sizeof(shader_model_));
			uint32_t include_size = include_path_.size();
			hasher.Update(&include_size, sizeof(include_size));
			for (auto n = 0; n < include_size; ++n) {
				hasher.Update(include_path_[n].data(), include_path_[n].size());
			}
			uint32_t macro_size = defines_.size();
			hasher.Update(&macro_size, sizeof(macro_size));
			for (auto n = 0; n < macro_size; ++n) {
				hasher.Update(defines_[n].data(), defines_[n].size());
			}
			uint32_t extra_size = extra_cli_.size();
			hasher.Update(&extra_size, sizeof(extra_size));
			for (auto n = 0l n < extra_size; ++n) {
				hasher.Update(extra_cli_[n].data(), extra_cli_[n].size());
			}
			uint64_t hash0, hash1;
			hasher.Final(&hash0, &hash1);
			hash_.FromBinary(&hash0);
		}
	}

	uint32_t RtGlobalCompileWorkManager::Submit(RtShaderCompileWorker::Ptr work)
	{
		std::unique_lock<std::mutex> lock(work_mutex_);
		work_cv_.wait(lock, [&]() { return pending_workers_.size() < GET_PARAM_TYPE_VAL(UINT, RENDER_COMPILE_WORKERS); });
		pending_workers_.insert({ total_works_, work });
		auto& compile_context = pending_workers_[total_works_];
		const auto compile_job = [compile_context]() {
			try {
				compile_context.entity_->Compile();
			}
			catch (...) {
				comile_context.done_semaphore_.notify_one();
			}
		};
		Utils::Schedule(compile_job);
		total_works_++;
		return total_works_-1;
	}

	uint32_t RtGlobalCompileWorkManager::Wait(uint32_t work_id) const
	{
		CompileEntity* compile_context = nullptr;
		bool is_retry_pass = false;
		uint32_t curr_work_id = work_id;
		do {
			{
				std::shared_lock<std::mutex> lock(work_mutex_);
				compile_context = &pending_works_[curr_work_id];
				assert(compile_context->entity_ != nullptr);
			}
			compile_context->done_semaphore_.acquire(); //wait compile work to end
			const auto& compile_output = compile_context->entity_->GetOuptut();
			if (compile_output->stat_ == EJobStat::eSuccess) {
				success_works_++;
			}
			else 
			{
				failed_works_++;
				if(!is_retry_pass && GET_PARAM_TYPE_VAL(SHADER_COMPILE_RETRY)) {
					//retry compile
					{
						//remove failed work
						std::unique_lock<std::mutex> lock(work_mutex_);
						pending_works_.erase(curr_work_id);
					}
					curr_work_id = const_cast<RtGlobalCompileWorkManager*>(this)->Submit(compile_context->entity_);
					is_retry_pass = true;
				}
			}

		} while (is_retry_pass);
		return curr_work_id;
	}

	void RtGlobalCompileWorkManager::FetchOutput(uint32_t work_id, ShaderCompileJobOutput& output) const
	{
		//get output data
		{
			std::shared_lock<std::mutex> lock(work_mutex_);
			compile_context = pending_works_[work_id];
			output = compile_context.entity_->GetOutput();
		}
		//remove work
		{
			std::unique_lock<std::mutex> lock(work_mutex_);
			pending_works_.erase(work_id);
		}
	}

	void RtGlobalCompileWorkManager::WaitAllJobsDone() const
	{
		std::shared_lock<std::mutex> lock(work_mutex_);
		for (auto& [id, context] : pending_works_ ) {
			context.done_semaphore_.acquire();
			const auto& output = context.entity_->GetOutput();
			if (output.stat_ == EJobStat::eSuccess) {
				successed_works_++;
			}
			else
			{
				failed_works_++;
			}
		}
		LOG(INFO) << std::format("Compile Stat:\nTotal:\t{}\n"
							"Success:\t{}\nFailed:\t{}\n", 
							total_works_, successed_works_, failed_works_);
	}

	
	void TraverseHLSLIncludePath(const String& prefix, const String& file, Vector<String>& paths)
	{
		String code_line;
		auto& file_path = Path(prefix) / Path(file);
		ifstream hlsl_file(file_path.string());
		while (getline(hlsl_file, code_line)) {
			
			if (const auto header_pos = code_line.find("#include"); header_pos != String::npos) {
				const auto path_begin = code_line.find_first_of("\"", headr_pos+8);
				const auto path_end = sub_str.find_first_of("\"", path_begin+1);
				const auto& path = code_line.substr(path_begin, path_end);
				if (path.empty()) {
					continue;
				}
				if (std::filesystem::exists(path)) {
					paths.emplace_back(path);
					TraverseHLSLIncludePath(prefix, path, paths);
				}
			}
		}
	}

}
