#include "VulkanRHI.h"
#include "GLM/glm.hpp"

#include <string>
#include <vector>
#include <fstream>

namespace MetaInit
{
	/*for:"this feature is not used, the implementation will 
	  perform its own memory allocations. Since most memory 
	  allocations are off the critical path", so just set null*/
	VkAllocationCallbacks* g_host_alloc = VK_NULL_HANDLE;

}