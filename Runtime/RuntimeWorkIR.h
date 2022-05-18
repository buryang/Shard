#pragma once
//render runtime work IR present like UE meshbatchcommand
#include "Utils/CommonUtils.h"
#include "Scene/PrimitiveProxy.h"

namespace MetaInit
{
	namespace Runtime
	{

		//draw command inter represent
		class MINIT_API DrawWorkCommandIR
		{
		public:
			//key to sort command
			struct CommandIRSortKey
			{
				union
				{
					uint64_t	packed_bits_{ 0 };
					struct {
						uint64_t	vertex_shader_hash_ : 16;
						uint64_t	pixel_shader_hash_ : 32;
						uint64_t	masked_ : 16;
					}base_;
					struct {
						uint64_t	meshid_in_primitive_ : 16;
						uint64_t	distance_ : 32;
						uint64_t	priorty_ : 16;
					}translucent_;
					struct {
						uint64_t	vertex_shader_hash_ : 32;
						uint64_t	pixel_shader_hash_ : 32;
					}generic_;
				};
				bool operator==(const CommandIRSortKey& rhs) const
				{
					return packed_bits_ == rhs.packed_bits_;
				}
				bool operator!=(const CommandIRSortKey& rhs) const
				{
					return !(*this == rhs);
				}
				bool operator<(const CommandIRSortKey& rhs) const
				{
					return packed_bits_ < rhs.packed_bits_;
				}
			};
		private:
			friend class DrawWorkCommandIRBatch;

		};

		class MINIT_API DrawWorkCommandIRBatch
		{
		public:
		private:
			Vector<DrawWorkCommandIR>	cmd_batch_;
			VertexRepo*					vertex_repo_;
		};
	}
}
