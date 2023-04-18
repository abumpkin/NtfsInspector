#pragma once
#include "ntfs_access.hpp"

namespace abkntfs {
    struct NtfsIndexEntry : NtfsStructureBase {
        enum INDEX_ENTRY_FLAGS {
            // 标志这个 索引项 指向下一个子 索引节点.
            FLAG_IE_POINT_TO_SUBNODE = 0x01,
            // 标志这个 索引项 是 索引记录 里一系列 索引项 的最后一项.
            FLAG_LAST_ENTRY_IN_THE_NODE = 0x02,
            // 最大标志
            FLAG_MAX = FLAG_IE_POINT_TO_SUBNODE | FLAG_LAST_ENTRY_IN_THE_NODE,
        };

#pragma pack(push, 1)
        struct EntryHeader {
            // 文件引用 (前 6 字节是 文件记录号, 后 2 字节是序列号 seqNumber,
            // 用于校验)
            NtfsFileReference fileReference;
            // 整个 索引项 的长度 (单位: 字节)
            uint16_t lengthOfIE;
            // 流(stream) 的长度 (单位: 字节)
            uint16_t lengthOfStream;
            // 标志 (INDEX_ENTRY_FLAGS)
            uint8_t flags;
            // 填充, 用于 8 字节对齐
            uint8_t padding[3];
        } entryHeader;
#pragma pack(pop)

        // 索引项 的流, 包含此 索引项 所指向的 文件记录里被索引属性的
        // 属性数据(不包含标准属性头).
        NtfsDataBlock stream;
        // 当为 大索引 时, 指向下一个节点的 索引记录号
        uint64_t pIndexRecordNumber = 0;
        // 流的数据类型
        NTFS_ATTRIBUTES_TYPE streamType{NTFS_ATTRIBUTE_NONE};

    public:
        NtfsIndexEntry() = default;

        NtfsIndexEntry(NtfsIndexEntry const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }

        NtfsIndexEntry &operator=(NtfsIndexEntry const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }

        NtfsIndexEntry::NtfsIndexEntry(NtfsDataBlock &data,
                                       NTFS_ATTRIBUTES_TYPE streamType)
            : NtfsStructureBase(true), streamType(streamType) {
            if (data.len() < sizeof(entryHeader)) {
                Reset();
                return;
            }
            errno = memcpy_s(&entryHeader, sizeof(entryHeader), &data[0],
                             sizeof(entryHeader));
            if (errno ||
                entryHeader.lengthOfStream > data.len() - sizeof(entryHeader)) {
                Reset();
                return;
            }
            if (entryHeader.lengthOfIE <
                sizeof(entryHeader) + entryHeader.lengthOfStream) {
                Reset();
                return;
            }
            // 有除 INDEX_ENTRY_FLAGS 之外的 flag 则出错.
            if ((entryHeader.flags | FLAG_MAX) ^ FLAG_MAX) {
                Reset();
                return;
            }
            // 流数据进行拷贝而不是建立视图.
            stream = NtfsDataBlock(
                std::vector<char>((char *)data + sizeof(entryHeader),
                                  data + sizeof(entryHeader) +
                                      entryHeader.lengthOfStream),
                data.pNtfs);
            // stream = NtfsDataBlock{data, sizeof(entryHeader),
            //                        entryHeader.lengthOfStream};
            // 额外数据即为指向 根节点的 索引记录号
            NtfsDataBlock remainingData = NtfsDataBlock{
                data, sizeof(entryHeader) + entryHeader.lengthOfStream,
                entryHeader.lengthOfIE -
                    (sizeof(entryHeader) + entryHeader.lengthOfStream)};
            if (remainingData.len() >= sizeof(pIndexRecordNumber)) {
                this->pIndexRecordNumber =
                    *(uint64_t *)&remainingData[remainingData.len() -
                                                sizeof(pIndexRecordNumber)];
            }
        }

    protected:
        virtual NtfsIndexEntry &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->entryHeader = rr.entryHeader;
            this->stream = rr.stream;
            this->streamType = rr.streamType;
            this->pIndexRecordNumber = rr.pIndexRecordNumber;
            return *this;
        }
        virtual NtfsIndexEntry &Move(NtfsStructureBase &r) override {
            return Copy(r);
        }
    };
}