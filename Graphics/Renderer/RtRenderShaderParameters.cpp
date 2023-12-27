#include "Utils/CommonUtils.h"
#include "Renderer/RtRenderShaderParameters.h"
#include "spirv_reflect.h"

namespace Shard::Renderer {

     void GetSPIRVShaderParameterInfosReflection(const Span<uint8_t>& spriv_code, RtShaderParameterInfosMap& map) 
     {
          SpvReflectShaderModule module;
          auto ret = spvReflectCreateShaderModule(spriv_code.size(), spriv_code.data(), &module);
          assert(ret == SPV_REFLECT_RESULT_SUCCESS);

          uint32_t binding_count{ 0u };
          ret = spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
          assert(ret == SPV_REFLECT_RESULT_SUCCESS);
          eastl::unique_ptr<SpvReflectDescriptorBinding*[]> bindings{ new SpvReflectDescriptorBinding* [binding_count] };
          ret = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.get());
          const auto trans_spv_binding_func = [](const SpvReflectDescriptorBinding* binding, ShaderParametersReflection& allocation) ->void {
               allocation.name_ = binding->name;
               allocation.base_index_ = binding->set;
               allocation.buffer_index_ = binding->binding;
               /*
               if (binding->descriptor_type == SpvReflectDescriptorType::SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
                    if(binding->resource_type == SpvReflectResourceType::SPV_REFLECT_RESOURCE_FLAG_UAV){
                         allocation.type_ = xxx;
                    }
               }
               */
          };
          for (auto n = 0; n < binding_count; ++n) {
               const auto* binding = bindings[n];
               ShaderParametersReflection allocation;
               trans_spv_binding_func(binding, allocation);
               map.insert({allocation.name_, allocation});
          }

          spvReflectDestroyShaderModule(&module);
     }
     
     RtShaderParameterInfosMapUtils::RtShaderParameterInfosMapUtils(RtShaderParameterInfosMap& map):map_(map)
     {
     }
     
     bool RtShaderParameterInfosMapUtils::FindShaderParameters(const String& name, ShaderParametersReflection& allocation)
     {
          if (const auto iter = map_.find(name); iter != map_.end()) {
               allocation = iter->second;
               return true;
          }
          return false;
     }
     
     void RtShaderParameterInfosMapUtils::AddShaderParameters(const ShaderParametersReflection& allocation)
     {
          ShaderParametersReflection temp;
          if (!FindShaderParameters(allocation.name_, temp)) {
               map_.insert({ allocation.name_, allocation });
          }
     }

     void RtShaderParameterInfosMapUtils::RemoveShaderParameters(const String& name)
     {
          map_.erase(name);
     }
     
     const RtShaderParameterInfosMapUtils::HashType& RtShaderParameterInfosMapUtils::ComputeHash() const
     {
          blake3_hasher hasher;
          for (const auto& [key, val] : map_) {

          }
          HashType hash_val;
          blake3_hasher_finalize(&hasher, hash_val.GetBytes(), hash_val.GetHashSize());
          return hash_val;
     }
}