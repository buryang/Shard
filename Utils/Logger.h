#pragma once
#include "glog/logging.h"

namespace MetaInit {
	namespace Logging {
		inline void InitLogging(int argc, char* argv[]) {
			google::InitGoogleLogging(argv[0]);
		}

		enum class Level
		{
			eDisable,
			eFatal,
			eError,
			eWarning,
			eInfo,
			eDebug,
		};
		inline void SetLoggingLevel(Level level) {
			///no use function
		}

	}
}
