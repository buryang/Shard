#include "Utils/DataFileCache.h"

namespace Shard::Utils
{

     void DataFileCacheManager::AddCacheEntry(CachePtr entry)
     {
          if(auto iter = eastl::find_if(cache_entities_.begin(), cache_entities_.end(),
               [entry](const auto& val) {return val->key == entry->key}); iter == cache_entities_.end()){
               cache_entities_.emplace_back(entry);
          }
          LOG(ERROR) << "conflict entry insert";
     }

     void DataFileCacheManager::RemoveCacheEntry(const String& key) {
          if (auto iter = eastl::find_if(cache_entities_.begin(), cache_entities_.end(),
               [key](const auto& val) {return val->key == key}); iter != cache_entities_.end()) {
               cache_entities_.erase(iter);
          }
     }

     void DataFileCacheManager::LoadCache(const String& cache_file) {
          FileArchive archive(cache_file, FileArchive::EArchiveMode::eRead);
          const auto file_size = archive.Tell() - sizeof(FileArchive::SizeType) - sizeof(uint64_t);//crc size 
          if (file_size <= 0) {
               LOG(ERROR) << "file size is too small";
               return;
          }
          const auto crc = CalcFileArchiveCrc(archive, 0, file_size);
          archive.Seek(file_size);
          FileArchive::SizeType file_size_rec;
          archive << file_size_rec;
          if (file_size_rec != file_size) {
               LOG(ERROR) << "file size is not correct";
               return;
          }
          uint64_t crc_rec{ 0 };
          archive << crc_rec;
          if (crc != crc_rec) {
               LOG(ERROR) << "file crc is not correct";
               return;
          }
          archive.Seek(0);
          uint32_t magic_num{ 0 };
          archive << magic_num;
          if (magic_num != MAGIC_NUM) {
               LOG(ERROR) << "input file is not a cache file";
               return;
          }
          eastl_size_t num_enties{ 0 };
          archive << num_enties;
          cache_entities_.clear();
          for (auto n = 0; n < num_enties; ++n) {
               //to be fixed
               
          }

     }

     void DataFileCacheManager::SaveCache(const String& cache_file) const{
          FileArchive archive(cache_file, FileArchive::EArchiveMode::eWrite);
          archive << MAGIC_NUM;
          archive << cache_entities_.size();
          for (const auto& cache_entity : cache_entities_) {
               cache_entity->Serialize(archive);
          }
          const auto file_size = archive.Tell();
          archive << file_size;
          //crc code
          const auto crc = CalcFileArchiveCrc(archive, 0, file_size);
          archive << crc;
     }

}