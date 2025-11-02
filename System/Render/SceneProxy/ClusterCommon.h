#pragma once
#include "Utils/CommonUtils.h"

namespace Shard::FXR
{
	//cluster render info,especially for gpu render
	struct ClusterRender
	{
		float4x4 view_matrix_;
		//others
	};
}
