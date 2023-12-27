#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/FileArchive.h"
#include "Utils/Reflection.h"
#include "eastl/shared_ptr.h"

namespace Shard::Utils
{
     struct CacheEntity
     {
          uint32_t     age_{ 0 };
          String     key_;
          virtual void Serialize(FileArchive& archive) = 0;
     };

     static FORCE_INLINE bool IsCacheEntryOutOfDate(const CacheEntity& entry, uint32_t age) {
          return entry.age_ < age;
     }

     template<typename, typename=void>
     struct HasSerializeFunc :public std::false_type {};

     template<typename T>
     struct HasSerializeFunc<T, std::void_t<decltype(declval<T>().Serialize(FileArchive()))>> : public std::true_type {};

     template<typename T>
     requires HasSerializeFunc<T>::value
     class CacheEntryInstance : public CacheEntity
     {
     public:
          using InstanceType = T;
          CacheEntryInstance(InstanceType&& instance) :instance_(instance) {

          }
          void Serialize(FileArchive& archive) override {
               instance_.Serialize(archive);
          }
          InstanceType& Get() { return instance_; }
          void Set(InstanceType&& instance) { instance_ = instance; }
     private:
          InstanceType     instance_;
     };

     class MINIT_API DataFileCacheManager
     {
          static constexpr uint32_t MAGIC_NUM = 0xBA1024DF;
          using CachePtr = eastl::shared_ptr<CacheEntity>;
     public:
          void AddCacheEntry(CachePtr entry);
          void RemoveCacheEntry(const String& key);
          void LoadCache(const String& cache_file);
          void SaveCache(const String& cache_file) const;
     private:
          SmallVector<CachePtr>     cache_entities_;
     };
}