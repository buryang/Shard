#include "VulkanRayTraceKHR.h"

namespace MetaInit
{

	/*return bind table information*/
	const VulkanRayTraceBindTable::Table& VulkanRayTraceBindTable::GetBindTable() const
	{
		return shader_tbl_;
	}
}