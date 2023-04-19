#include "RHI/RHIGlobalEntity.h"

namespace MetaInit::RHI
{
	RHIGlobalEntity::Ptr RHIGlobalEntity::Instance()
	{
		auto back_end = static_cast<ERHIBackEnd>(GET_PARAM_TYPE_VAL(UINT, RHI_ENTITY_BACKEND));
		auto iter = create_func_repo_.find(back_end);
		if (!IsBackEndSupported(back_end) || iter == create_func_repo_.end()) {
			return nullptr;
		}
		return (iter->second)();
	}

	bool RHIGlobalEntity::RegistCreateFunc(ERHIBackEnd back_end, CreateFunc&& func)
	{
		if (!IsBackEndSupported(back_end) || create_func_repo_.find(back_end) != create_func_repo_.end()) {
			PLOG(ERROR) << "regist backend(" << static_cast<uint32_t>(back_end) << "create function failed" << std::endl;
		}
		create_func_repo_.insert(eastl::make_pair(back_end, func));
		return true;
	}
	
	RHIShaderLibraryInterface::Ptr RHIGlobalEntity::CreateShaderLibrary()
	{
		return nullptr;
	}
}