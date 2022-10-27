#include "RHI/RHIGlobalEntity.h"

namespace MetaInit::RHI
{
	RHIGlobalEntity::Ptr RHIGlobalEntity::Instance()
	{
		auto back_end = static_cast<RHIBackEnd>(GET_PARAM_TYPE_VAL(UINT, RHI_ENTITY_BACKEND));
		auto iter = create_func_repo_.find(back_end);
		if (!IsBackEndSupported(back_end) || iter == create_func_repo_.end()) {
			return nullptr;
		}
		return (iter->second)();
	}

	bool RHIGlobalEntity::RegistCreateFunc(RHIBackEnd back_end, CreateFunc&& func)
	{
		if (!IsBackEndSupported(back_end) || create_func_repo_.find(back_end) != create_func_repo_.end()) {
			//deal with error
		}
		create_func_repo_.insert(eastl::make_pair(back_end, func));
		return true;
	}
}