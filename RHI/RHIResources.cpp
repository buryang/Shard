#include "RHI/RHIResource.h"
#include "RHIResources.h"

namespace MetaInit::RHI
{
	void* RHIBuffer::MapBackMem()
	{
		if ("FIXME not support map") {
			PLOG(ERROR) << "buffer not support map to cpu";
		}

		if (nullptr == mapped_data_) {
			//map to map_data_
		}
		return mapped_data_;
	}
}