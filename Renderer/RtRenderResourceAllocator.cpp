#include "Renderer/RtRenderResourceAllocator.h"

namespace MetaInit::Renderer {
	class RtTransiantResourceAllocatorImpl {
		friend class RtTransiantResourceAllocator;
	public:
	private:
		std::mutex	mutex_;

	};
}