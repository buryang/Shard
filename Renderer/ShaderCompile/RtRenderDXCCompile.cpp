#include "Utils/FileArchive.h"
#include "Renderer/RtRenderShaderFactory.h"
#include <fstream>
#include <filesystem>

namespace MetaInit::Renderer {
#ifdef _WIN32
	class UniqueIncludeMetaHandler : public IDxcIncludeHandler
	{
	public:
		UniqueIncludeMetaHandler(const String& meta_path, RtShaderDXCCompileWorker::Ptr worker) : 
												meta_includes_file_(meta_path), compile_worker_(worker)
		{
			compile_worker_->GetDxcUtils()->CreateDefaultIncludeHandler(&include_handle_);
		}
		~UniqueIncludeMetaHandler() {
			//save unique include to meta file
			std::fstream meta_stream(meta_includes_file_.c_str(), std::ios::out);
			if (meta_stream.is_open()) {
				for (const auto& path : unique_includes_) {
					meta_stream << path.c_str() << std::endl;
				}
			}
		}
		HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
		{
			//pFIleName to path
			const auto include_path = "";
			*ppIncludeSource = nullptr;
#if 0
			CComPtr<IDxcBlobEncoding> encoding;
			if (unique_includes_.find(include_path) != unique_includes_.end()) {
				static constexpr char null_str[] = "";
				compile_worker_->GetDxcUtils()->CreateBlob(null_str, 1, CP_UTF8, &encoding);
				*ppIncludeSource = encoding.Detach();
			}
			auto hr = compile_worker_->GetDxcUtils()->LoadFile(pFilename, nullptr, &encoding);
			if (SUCCEEDED(hr)) {
				unique_includes_.insert(include_path);
				*ppIncludeSource = encoding.Detach();
			}
#else
			if (unique_includes_.find(include_path) == unique_includes_.end() &&
					std::filesystem::exists(include_path)) 
			{
				unique_includes_.insert(include_path);
			}

			auto hr = include_handle_->LoadSource(pFilename, ppIncludeSource);
#endif

			return hr;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
		{
			//copy from wickedengine
			include_handle_->QueryInterface(riid, ppvObject);
		}

		ULONG STDMETHODCALLTYPE AddRef(void) override { return 0; }
		ULONG STDMETHODCALLTYPE Release(void) override { return 0; }
	private:
		Set<String>	unique_includes_;
		const String meta_includes_file_; //save all include file to a meta file
		RtShaderDXCCompileWorker::Ptr compile_worker_;
		CComPtr<IDxcIncludeHandler> include_handle_;
	};

	RtShaderDXCCompileWorker::RtShaderDXCCompileWorker()
	{
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils_));
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_));
	}

	RtShaderDXCCompileWorker::~RtShaderDXCCompileWorker()
	{
	}

	void RtShaderDXCCompileWorker::GenerateCompileCLI(const ShaderCompileJobIn& job, SmallVector<LPCWSTR>& cli)
	{
		cli.clear();
		cli.emplace_back(Utils::StringConvertHelper::StringToWString(job.file_).c_str()); //shader file path
		cli.emplace_back(L"-E");
		cli.emplace_back(Utils::StringConvertHelper::StringToWString(job.entry_).c_str()); //entry function name
		for (const auto& def : job.defines_) {
			cli.emplace_back(L"-D");
			cli.emplace_back(def);
		}
	}

	WString RtShaderDXCCompileWorker::ConvertSMToTargetProfile(EShaderFrequency freq, EShaderModel sm)
	{
		switch (freq) {
		case EShaderFrequency::eVertex:
			switch (sm){
			case EShaderModel::eSM_6_0:
				return L"vs_6_0";
			case EShaderModel::eSM_6_1:
				return L"vs_6_1";
			case EShaderModel::eSM_6_2:
				return L"vs_6_2";
			case EShaderModel::eSM_6_3:
				return L"vs_6_3";
			case EShaderModel::eSM_6_4:
				return L"vs_6_4";
			case EShaderModel::eSM_6_5:
				return L"vs_6_5";
			case EShaderModel::eSM_6_6:
				return L"vs_6_6";
			case EShaderModel::eSM_6_7:
				return L"vs_6_7";
			default:
				//todo
				return L"";
			}
		case EShaderFrequency::eFrag:
			switch (sm) {
			case EShaderModel::eSM_6_0:
				return L"ps_6_0";
			case EShaderModel::eSM_6_1:
				return L"ps_6_1";
			case EShaderModel::eSM_6_2:
				return L"ps_6_2";
			case EShaderModel::eSM_6_3:
				return L"ps_6_3";
			case EShaderModel::eSM_6_4:
				return L"ps_6_4";
			case EShaderModel::eSM_6_5:
				return L"ps_6_5";
			case EShaderModel::eSM_6_6:
				return L"ps_6_6";
			case EShaderModel::eSM_6_7:
				return L"ps_6_7";
			default:
				//todo
				return L"";
			}
		case EShaderFrequency::eCompute:
			switch (sm) {
			case EShaderModel::eSM_6_0:
				return L"cs_6_0";
			case EShaderModel::eSM_6_1:
				return L"cs_6_1";
			case EShaderModel::eSM_6_2:
				return L"cs_6_2";
			case EShaderModel::eSM_6_3:
				return L"cs_6_3";
			case EShaderModel::eSM_6_4:
				return L"cs_6_4";
			case EShaderModel::eSM_6_5:
				return L"cs_6_5";
			case EShaderModel::eSM_6_6:
				return L"cs_6_6";
			case EShaderModel::eSM_6_7:
				return L"cs_6_7";
			default:
				//todo
				return L"";
			}
		default:
			LOG(ERROR) << "there's not such a stage";
			return L"";
		}
	}

	void RtShaderDXCCompileWorker::DoCompileJob(const ShaderCompileJobIn& job, ShaderCompileJobOutput& output)
	{
		SmallVector<LPCWSTR*> cli;
		CComPtr<IDxcBlobEncoding> ssource_ptr{ nullptr };
		utils_->LoadFile(Utils::StringConvertHelper::StringToWString(job.file_).c_str(), nullptr, &ssource_ptr); //to fixme
		DxcBuffer ssource{ .Ptr = ssource_ptr->GetBufferPointer(),
							.Size = ssource_ptr->GetBufferSize(), .Encoding = DXC_CP_ACP };
		CComPtr<IDxcResult> result{ nullptr };
		UniqueIncludeMetaHandler meta_handler{job.file_ + ".meta", this};
		compiler_->Compile(&ssource, cli.data(), cli.size(), &meta_handler, IID_PPV_ARGS(&result));

		HRESULT hr_stat;
		result->GetStatus(&hr_stat);
		if (FAILED(hr_stat)) {
			output.stat_ = EJobStat::eFailed;
			CComPtr<IDxcBlobUtf8> errors;
			result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
			if (errors != nullptr && errors->GetStringLength()) {
				ostringstream strstream;
				strstream << "compile " << job.file_.c_str() << "warning and errors: " << errors->GetStringPointer();
				output.compile_message_ = strstream.str();
			}
		}
		else
		{
			output.stat_ = EJobStat::eSuccess;
		}

		//Get compiled binary result code
		CComPtr<IDxcBlob> shader_bin{ nullptr };
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader_bin), nullptr);
		assert(shader_bin != nullptr);

		//output shader code 
		auto& shader_code = output.shader_code_;
		const auto* shader_data = reinterpret_cast<uint8_t*>(shader_bin->GetBufferPointer());
		std::copy(shader_data, shader_data + shader_bin->GetBufferSize(), shader_code.code_binary_);
		shader_code.magic_num_ = 1024;//fixme

		//output hash
		CComPtr<IDxcBlob> shader_hash{ nullptr };
		result->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&shader_bin), nullptr);
		assert(shader_hash != nullptr);
	}

#endif
}