#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include <fstream>

namespace Shard::Utils {

        class MINIT_API IOArchive
        {
        public:
                using SizeType = uint64_t;
                enum EArchiveMode {
                        eUndefined = 0,
                        eRead = 1 << 0,
                        eWrite = 1 << 1,
                        eAppend = eWrite | 0x100,
                };
                bool IsReading()const { return archive_mode_ & eRead; }
                virtual bool IsEof() const = 0;
                virtual constexpr SizeType Tell()const = 0;
                virtual void Serialize(void* data, SizeType size) = 0;
                virtual ~IOArchive() = default;
        protected:
                EArchiveMode        archive_mode_{ eUndefined };
        };

        //file archive 
        class MINIT_API FileArchive : public IOArchive {
        public:
                using PositionType = SizeType;
                using IOArchive::EArchiveMode;

                FileArchive() = default;
                FileArchive(const String& file_path, EArchiveMode mode);
                void Open(const String& file_path, EArchiveMode mode);
                void Close();
                constexpr SizeType Tell()const override;
                FileArchive& Seek(PositionType pos);
                bool IsEof() const override { return archive_stream_.eof(); }
                void Serialize(void* data, SizeType size) override;
                template <typename T>
                friend FileArchive& operator << (FileArchive& archive, T&& value) {
                        assert(archive.archive_mode_ != FileArchive::eUndefined);
                        if (archive.IsReading()) {
                                archive.archive_stream_ >> std::forward<T>(value);
                        }
                        else {
                                archive.archive_stream_ << std::forward<T>(value);
                        }
                        return archive;
                }
        private:
                std::fstream        archive_stream_;
        };

        //binary memory stream archive
        class MINIT_API BinaryArchive : public IOArchive {
        public:
                using Blob = Vector<uint8_t>;
                using PositionType = Blob::iterator;
                using IOArchive::EArchiveMode;
                BinaryArchive() = default;
                BinaryArchive(Blob* binary, EArchiveMode mode);
                constexpr SizeType Tell()const override;
                bool IsEof() const override { return curr_pos_ == archive_blob_->end(); }
                void Serialize(void* data, SizeType size) override;
                template <typename T>
                friend BinaryArchive& operator <<(BinaryArchive& archive, T&& value) {
                        assert(archive_blob_ != nullptr);
                        if (archive.IsReading()) {
                                //todo
                        }
                        else
                        {

                        }
                }
        private:
                Blob* archive_blob_{ nullptr };
                PositionType        curr_pos_;
        };

        constexpr uint64_t CalcFileArchiveCrc(const FileArchive& archive, FileArchive::PositionType beg, FileArchive::PositionType end);

        struct FileImageSectionDescriptor {
                using Ptr = FileImageSectionDescriptor*;
                uint64_t        offset_{ 0 };
                uint64_t        size_{ 0 };
                Ptr        parent_{ nullptr };
                SmallVector<Ptr>        children_;
                FORCE_INLINE bool IsParent()const { return !children_.empty(); }
                FORCE_INLINE bool IsChild()const { return parent_ != nullptr; }
        };

        class MINIT_API FileImageSection {
        public:
                friend class FileImageWriter;
                FileImageSection() = default;
                FileImageSection(const FileImageSectionDescriptor& desc);
                FileImageSection& Load(FileArchive& archive);
                FileImageSection& Save(FileArchive& archive);
                void Init(const FileImageSectionDescriptor& desc);
                /*
                bool operator==(const FileImageSection& rhs) {
                        return 
                }
                */
        private:
                FileImageSectionDescriptor        desc_;
                Vector<uint8_t>        image_;
        };

        class MINIT_API FileImage {
        public:
                FileImage& Load(FileArchive& archive);
                FileImage& Save(FileArchive& archive);
                FileImageSection& AddEmptySection();
                FileImageSection& AddSection(FileImageSection&& section);
        private:
                SmallVector<FileImageSection>        image_sections_;
        };


        class MINIT_API FileImageWriter {
        public:
                FileImageWriter(FileImageSection& section):section_(section) {}
                FileImageWriter(FileImage& image):section_(image.AddEmptySection()) {}
                FileImageWriter& Write(const void* data, FileImageSectionDescriptor& desc);
        private:
                FileImageSection& section_;
        };
}