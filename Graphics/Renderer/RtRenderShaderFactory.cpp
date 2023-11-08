#include "Utils/FileArchive.h"
#include "Utils/SimpleJobSystem.h"
#include "Core/EngineGlobalParams.h"
#include "RHI/RHIShaderLibrary.h"
#include "Renderer/RtRenderShaderFactory.h"
#include <filesystem>
#include <string>
 
using Path = std::filesystem::path;
namespace fs = std::filesystem;

namespace Shard::Renderer {

	enum EVkShiftFlags
	{
		eNone,
		eSampler = 0x1,
		eTexture = 0x2,
		eBuffer = 0x4,
		eUniform = 0x8,
		eAll = 0xFFFF,
	};
	REGIST_PARAM_TYPE(UINT, RENDER_COMPILE_WORKERS, 128);
	REGIST_PARAM_TYPE(STRING, RENDER_SHADER_DIR, "/shaders");
	REGIST_PARAM_TYPE(UINT, COMPILE_VK_SHIFT_FLAGS, EVkShiftFlags::eNone);
	REGIST_PARAM_TYPE(UINT, COMPILE_VK_BOFFSET_S, 0);
	REGIST_PARAM_TYPE(UINT, COMPILE_VK_SET_S, 0);
	REGIST_PARAM_TYPE(UINT, COMPILE_VK_BOFFSET_T, 0);
	REGIST_PARAM_TYPE(UINT, COMPILE_VK_SET_T, 0);
	REGIST_PARAM_TYPE(UINT, COMPILE_VK_BOFFSET_B, 0);
	REGIST_PARAM_TYPE(UINT, COMPILE_VK_SET_B, 0);
	REGIST_PARAM_TYPE(UINT, COMPILE_VK_BOFFSET_U, 0);
	REGIST_PARAM_TYPE(UINT, COMPILE_VK_SET_U, 0);

	//class layout define here
	IMPLEMENT_TYPE_LAYOUT_DEF(RtShaderContent);

	bool RtShaderType::Regist(Ptr shader_type)
	{
		auto iter = eastl::find_if(Begin(), End(), [](const auto* val) { return val->hash_name_ >= shader_type->hash_name_; });
		if (iter == End() || (*iter)->hash_name_ != shader_type->hash_name_) {
			shader_type_repos_.insert(iter, shader_type);
		}
		return false;
	}

	RtShaderType::Ptr RtShaderType::FindShaderTypeByName(const String& name)
	{
		const auto hash_name = Utils::CalcBlake3HashForBytes<RtShaderType::HashType::GetHashSize()>(reinterpret_cast<const uint8_t*>(name.data()), name.size());
		const auto iter = eastl::find(Begin(), End(), hash_name);
		if (iter != End() && (*iter)->GetName() == name) {
			return *iter;
		}
		return nullptr;
	}

	//whether hash unique?
	RtShaderType::Ptr RtShaderType::FindShaderTypeByHashName(const HashType& hash) 
	{
		const auto iter = eastl::find(Begin(), End(), hash);
		if (iter != End()) 
		{
			return *iter;
		}
		return nullptr;
	}

	Vector<RtShaderType::Ptr> RtShaderType::FindShaderTypeByFileName(const String& file_name)
	{
		Vector<RtShaderType::Ptr> shader_types;
		for (auto iter = Begin();  iter != End(); ++iter) {
			if ((*iter)->GetFileName() == file_name) {
				shader_types.push_back(*iter);
			}
		}
		return shader_types;
	}

	RtShaderType::Ptr RtShaderType::GetShaderType(const HashType& hash)
	{
		auto* shader_type = FindShaderTypeByHashName(hash);
		if (shader_type != nullptr) {
			return shader_type;
		}
		LOG(ERROR) << fmt::format("try to get type :{} not exsit", hash.ToString());
		return nullptr;
	}

	RtShaderType::RtShaderType(const String& name, const String& file, const String& func, EShaderFrequency freq) :name_(name), file_name_(file), entry_func_(func), frequency_(freq)
	{
	}

	const uint32_t RtShaderType::GetPermutationCount() const
	{
		return uint32_t();
	}

	void RtShaderCompiledRepo::Serialize(Utils::FileArchive& archive) const
	{
		archive << shader_sections_.size();
		for (const auto& section : shader_sections_) {
			section.second->Serialize(archive);
		}
	}

	void RtShaderCompiledRepo::UnSerialize(Utils::FileArchive& archive)
	{
		size_t sections_count{ 0 };
		archive << sections_count;
		for (auto n = 0; n < sections_count; ++n) {
			auto* section = new RtShaderCompiledFileMap;
			section->Serialize(archive);
			const auto hash_name = section->GetFileNameHash();
			shader_sections_.insert({ hash_name, section });
		}
	}

	void RtShaderCompiledRepo::BeginCompile(const ShaderCompileEnv& compile_env) {
		for (auto& [key, sec] : shader_sections_) {
			if (sec->IsReCompileNeeded()) {
				shader_sections_.erase(key);
			}
		}
		decltype(shader_sections_) candidate_sections;
		//traverse shader types to do compile extra work 
		for (auto iter = RtShaderType::Begin(); iter != RtShaderType::End(); ++iter) {
			//check wether need compile file
			const auto* shader_type = *iter;
			const auto& section_file_name = shader_type->GetHashFileName();
			RtShaderCompiledFileMap::Ptr shader_section = nullptr;
			if (auto* section = FindSection(section_file_name); section != nullptr) {
				continue;
			}
			else
			{
				if (auto sec_iter = candidate_sections.find(section_file_name); sec_iter != candidate_sections.end()) {
					shader_section = sec_iter->second;
				}
				else
				{
					shader_section = new RtShaderCompiledFileMap;
					candidate_sections.insert({section_file_name, shader_section });
				}
			}
			//iterator permutation ids
			for (auto n = 0; n < shader_type->GetPermutationCount(); ++n) {
				if (shader_type->ShouldCompile(compile_env, n)) {
					ShaderCompileJobIn job;
					job.InitByShaderType(*shader_type, n);
#ifdef _WIN32
					auto* compile_task = new RtShaderDXCCompileWorker(job, shader_section);
#else
					static_assert(0 && "not implemented other platform");
#endif
					RtGlobalCompileWorkManager::Instance().Submit(compile_task);
				}
			}
		}
		candidate_sections.insert(shader_sections_.begin(), shader_sections_.end());
		std::swap(shader_sections_, candidate_sections);
	}

	void RtShaderCompiledRepo::EndCompile() {
		//sync compile work
		RtGlobalCompileWorkManager::Instance().FinalizeAllJobs();
	}

	void RtShaderCompiledRepo::Compile() {
		ShaderCompileEnv env; //todo how to set env parameters
		BeginCompile(env);
		EndCompile();
	}

	static void UpdateHasherOfFileStream(std::ifstream& stream, blake3_hasher& hasher) {
		stream.seekg(0, stream.end);
		const auto stream_size = stream.tellg();
		stream.seekg(0, stream.beg);
		std::unique_ptr<uint8_t[]> buffer{new uint8_t[stream_size]};
		stream.read(reinterpret_cast<char*>(buffer.get()), stream_size);
		blake3_hasher_update(&hasher, buffer.get(), stream_size);
	}

	bool RtShaderCompiledFileMap::IsReCompileNeeded()const {
		const auto shader_dir = fs::current_path() / GET_PARAM_TYPE_VAL(STRING, RENDER_SHADER_DIR).c_str();
		const auto file_path =  shader_dir/file_name_.c_str();
		if (!fs::exists(file_path)){
			return true;
		}
		Vector<String> include_paths;
		TraverseHLSLIncludePath(shader_dir.c_str(), file_name_, include_paths);
		include_paths.emplace_back(file_path.c_str());
		HashType curr_hash;
		blake3_hasher hasher;
		blake3_hasher_init(&hasher);
		for (const auto& path : include_paths) {
			std::ifstream include_file(path.c_str(), std::ios::binary);
			if (include_file.is_open()) {
					return false;
			}
			UpdateHasherOfFileStream(include_file, hasher);
		}
		blake3_hasher_finalize(&hasher, curr_hash.GetBytes(), curr_hash.GetHashSize());
		precalc_hash_ = curr_hash;
		return curr_hash != hash_;
	}

	RtShaderCompiledFileMap::Ptr RtShaderCompiledRepo::FindOrCreateSection(const RtShaderType::HashType hash_name)
	{
		const auto* shader_type = RtShaderType::FindShaderTypeByHashName(hash_name);
		if (shader_type == nullptr) {
			return nullptr;
		}
		const auto section_name = shader_type->GetHashFileName();
		if (const auto& iter = shader_sections_.find(section_name); iter != shader_sections_.end()) {
			return iter->second;
		}
		auto* section = new RtShaderCompiledFileMap();//todo
		shader_sections_.insert({section_name, section});
		return section;
	}

	RtShaderCompiledRepo::~RtShaderCompiledRepo()
	{
		for (auto& iter : shader_sections_) {
			delete iter.second;
		}
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

#ifdef _WIN32
	RtShaderDXCCompileWorker::~RtShaderDXCCompileWorker()
	{
	}

	void RtShaderDXCCompileWorker::Compile()
	{
		DoCompileJob(compile_input_, compile_output_);
	}
#endif

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

	void ShaderCompileJobIn::InitByShaderType(const RtShaderType& shader_type, uint32_t permutation_id) {
		file_ = shader_type.GetFileName();
		entry_ = shader_type.GetEntryName();
		permutation_ = permutation_id;
		if (GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_SHIFT_FLAGS) != EVkShiftFlags::eNone)
		{
			String shift_cli;
			if (GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_SHIFT_FLAGS) & EVkShiftFlags::eSampler)
			{
				shift_cli += fmt::format("-fvk-s-shift {} {}", GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_BOFFSET_S), GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_SET_S));
			}
			if (GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_SHIFT_FLAGS) & EVkShiftFlags::eTexture)
			{
				shift_cli += fmt::format("-fvk-t-shift {} {}", GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_BOFFSET_T), GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_SET_T));
			}
			if (GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_SHIFT_FLAGS) & EVkShiftFlags::eBuffer)
			{
				shift_cli += fmt::format("-fvk-b-shift {} {}", GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_BOFFSET_B), GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_SET_B));
			}
			if (GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_SHIFT_FLAGS) & EVkShiftFlags::eUniform)
			{
				shift_cli += fmt::format("-fvk-u-shift {} {}", GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_BOFFSET_U), GET_PARAM_TYPE_VAL(UINT, COMPILE_VK_SET_U));
			}
			extra_cli_.emplace_back(shit_cli);
		}
		//todo other work
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

	void RtGlobalCompileWorkManager::FetchOutput(uint32_t work_id, ShaderCompileJobOutput& output)
	{
		//get output data
		CompileEntity* compile_context = nullptr;
		compile_context = &pending_works_[work_id];
		compile_context.done_semaphore_.acquire();
		output = std::move(compile_context->entity_->GetOutput());
		//remove work
		{
			std::unique_lock<std::mutex> lock(work_mutex_);
			delete compile_context->entity_;
			pending_works_.erase(work_id);
		}
	}

	void RtGlobalCompileWorkManager::FinalizeAllJobs()
	{
		for (auto& [id, context] : pending_works_ ) {
			context.done_semaphore_.acquire();
			const auto& output = context.entity_->GetOutput();
			if (output.stat_ == EJobStat::eSuccess) {
				successed_works_++;
				//save shader code
				auto* shader_archive = context.entity_->GetOwner();
				if (nullptr != shader_archice) {
					shader_archive->AddShaderCode(output.shader_code_);
				}
			}
			else
			{
				failed_works_++;
			}
		}
		LOG(INFO) << std::format("Compile Stat:\nTotal:\t{}\n"
			"Success:\t{}\nFailed:\t{}\n",
			total_works_, successed_works_, failed_works_);
		Clear();
	}

	void RtGlobalCompileWorkManager::Clear() {
		std::unique_lock<std::mutex> lock(work_mutex_);
		pending_works_.clear();
		total_works_ = 0u;
		successed_works_ = 0u;
		failed_works_ = 0u;
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

	void RtPipelineCompiledRepo::Serialize(const String& path)
	{
		const auto* cpath = path.c_str();
		if (!fs::exists(path.c_str())) {
			cpath = fs:
		}
		Utils::FileArchive pso_ar(cpath, Utils::FileArchive::EArchiveMode::eWrite);
		{
			//read logic
		}
		if (repo_load_delegate_) {
			repo_load_delegate_();
		}
	}

	void RtPipelineCompiledRepo::UnSerialize(const String& path)
	{
		Utils::FileArchive pso_ar(path.c_str(), Utils::FileArchive::EArchiveMode::eWrite);
		//todo fixme
		for (const pso : new_pso_) {
			pso_ar << pso;
		}
		if (repo_save_delegate_) {
			repo_save_delegate_();
		}
	}

	void RtPipelineCompiledRepo::PreCompile(bool async)
	{
		const auto batch_size = GET_PARAM_TYPE_VAL(UINT, PSO_COMPILE_BACTH_SIZE);
		const auto batch_num = std::ceil(record_pso_.size() / batch_size);
		if (!async) {
			for (auto n = 0; n < batch_num; ++n) {
				const auto batch_begin = n * batch_size;
				const auto batch_real_size = batch_begin + batch_size > record_pso_.size() ? record_pso_.size() - batch_begin : batch_size;
				CompileBatch({ record_pso_.data()+n*batch_begin, batch_real_size });
				//report compile progress
				float ratio = float(n) / batch_num;
				finish_precompile_works_.fetch_add(1u);
				if (compile_report_delegate_) {
					compile_report_delegate_(ratio);
				}
			}
		}
		else
		{
			//begin async compile work
			for (auto n = 0; n < batch_num; ++n) {
				const auto batch_begin = n * batch_size;
				const auto batch_real_size = batch_begin + batch_size > record_pso_.size() ? record_pso_.size() - batch_begin : batch_size;
				Span<CompileJob> compile_batch{ record_pso_.data() + n * batch_begin, batch_real_size };
				const auto compile_batch_func = [this, compile_batch]() {
					CompileBatch(compile_batch);
					{
						std::unique_lock<std::shared_mutex> lock(utility_mutex_);
						auto proc_num = finish_precompile_works_.fetch_add(1u);
						float ratio = float(proc_num) / batch_num;
						if (compile_report_delegate_) {
							compile_report_delegate_(ratio);//todo fix sync work
						}
					}
				};
				Utils::Schedule(compile_batch_func);
			}
		}
	}

	RtRenderShaderPipeline::SharedPtr Shard::Renderer::RtPipelineCompiledRepo::GetPipeline(const PipelineStateObjectDesc& desc)
	{
		RtRenderShaderPipeline::SharedPtr  pso(new RtRenderShaderPipeline(desc));
		return pso;
	}

	void RtPipelineCompiledRepo::CompileBatch(const Span<CompileJob>& job_batch)
	{
		for (const auto& job : job_batch) {
			Compile(job);
		}
	}

	void RtPipelineCompiledRepo::Compile(const CompileJob& job)
	{
		//todo
		const auto pso_desc_to_rhi = [this, &](const PipelineStateObjectDesc& desc){
			RHI::RHIPipelineStateObjectInitializer initializer;
			auto shader_library = RHIGlobalEntity::Instance()->GetOrCreateShaderLibrary();
			if (desc.type_ == PipelineStateObjectDesc::EPSOType::eGFX) {
				for (auto n = 0; n < EShaderFrequency::eGFXNum; ++n) {
					const auto shader_hash = desc.stages_[n];
					if (!shader_hash.IsZero()) {
						auto rhi_shader = shader_library->GetRHIShader(shader_hash);
						switch (EShaderFrequency(n)) {
						case EShaderFrequency::eVertex:
							initializer.gfx_.vertex_shader_ = rhi_shader;
							break;
						case EShaderFrequency::eHull:
							initializer.gfx_.hull_shader_ = rhi_shader;
							break;
						case EShaderFrequency::eGeometry:
							initializer.gfx_.geometry_shader_ = rhi_shader;
							break;
						case EShaderFrequency::eDomain:
							initializer.gfx_.domain_shader_ = rhi_shader;
							break;
						case EShaderFrequency::eFrag:
							initializer.gfx_.pixel_shader_ = rhi_shader;
							break;
						default:
							LOG(ERROR) << "fault desc input";
						}
					}
				}
				initializer.type_ = RHIPipelineStateObjectInitializer::EType::eGFX;
				initializer.gfx_.vertex_input_state_ = desc.gfx_desc_.vertex_input_state_;
				initializer.gfx_.blend_state_ = desc.gfx_desc_.blend_state_;
				initializer.gfx_.depth_stencil_state_ = desc.gfx_desc_.depth_stencil_state_;
				initializer.gfx_.rasterization_state_ = desc.gfx_desc_.rasterization_state_;
				initializer.gfx_.primitive_topology_ = descgfx_desc_..primitive_topology_;
			}
			else if (desc.type == PipelineStateObjectDesc::EPSOType::eCompute) {
				initializer.type_ = RHIPipelineStateObjectInitializer::EType::eCompute;
				PCHECK(!desc.compute_.compute_shader_.IsZero()) << "set an invalid compute shader";
				initializer.compute_.compute_shader_ = shader_library->GetRHIShader(desc.compute_.compute_shader_);
			}
			else if (desc.type == PipelineStateObjectDesc::EPSOType::RayTrace) {
				//todo
			}
			return initializer;
		};

		const auto rhi_initializer = pso_desc_to_rhi(job.desc_);
		auto pos_rhi = RHIGlobalEntity::Instance()->GetOrGetOrCreatePSOLibrary()->GetOrCreatePipeLine(rhi_initializer); //todo 
		PCHECK(pso_rhi != nullptr) << "compile pso failed";
	}
}
