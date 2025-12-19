#pragma once
//render runtime work IR present like UE meshbatchcommand
#include "Utils/CommonUtils.h"

namespace Shard::Render
{
    namespace Runtime
    {

        REGIST_PARAM_TYPE(UINT, RENDER_SYSTEM_MESH_BATCH_SIZE, 12u);

        //draw command inter represent
        class MINIT_API DrawWorkCommandIR
        {
        public:
            /*
                A bit flag encoding some hinting about what
                kind of command head is actually pointing
            */
            enum 
            {
                eOpaque,
                eTranslucent,
                eGeneric,
            };
            //key to sort command
            struct CommandIRSortKey
            {
                union
                {
                    uint64_t    packed_bits_{ 0 };
                    struct {
                        uint64_t    vertex_shader_hash_ : 16;
                        uint64_t    pixel_shader_hash_ : 32;
                        uint64_t    masked_ : 16;
                    }base_;
                    struct {
                        uint64_t    meshid_in_primitive_ : 16;
                        uint64_t    distance_ : 32;
                        uint64_t    priorty_ : 16;
                    }translucent_;
                    struct {
                        uint64_t    vertex_shader_hash_ : 32;
                        uint64_t    pixel_shader_hash_ : 32;
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
            /**  
            *\brief test whether two command can merge for dynamic instancing
            */
            bool IsMergeAbleForDynamicInstancing(const DrawWorkCommandIR& other) const;
            //merge two command IR
            DrawWorkCommandIR& operator+=(DrawWorkCommandIR&& other); 
        private:
            friend class DrawWorkCommandIRBatch;
            CommandIRSortKey    sort_key_;
            uint32_t    flags_{ 0u };
            uint32_t    first_index_{ 0u };
            uint32_t    num_primitives_{ 0u };
            uint32_t    num_instances_;

            //todo pso
            RenderShaderParameterBindings   shader_bindings_;

            //input stream vertex/index and others
            RenderShaderInputStreams    shader_input_streams_;
        };

        //draw command bucket
        class MINIT_API DrawWorkCommandIRBin
        {
        public:
            void AddCommand(const DrawWorkCommandIR& command);
            /**  
            *\brief distill commmands to a single command
            */ 
            DrawWorkCommandIR DistillToInstanced()const;
            //todo
        private:
        private:
            RenderArray<DrawWorkCommandIR>   commands_;
            DrawWorkCommandIR::CommandIRSortKey   key_;
        };

        using DrawWorkCommandIRBins = SmallVector<DrawWorkCommandIRBin>;

    }
}
