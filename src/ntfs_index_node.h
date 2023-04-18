#pragma once
#include "ntfs_access.hpp"

namespace abkntfs {
        struct NtfsIndexNode : NtfsStructureBase {
#pragma pack(push, 1)
        struct IndexNodeHeader {
            // 到第一个 IE 的偏移.
            uint32_t offsetToTheFirstEntry = sizeof(IndexNodeHeader);
            // IE 列表的总大小 + 此结构体大小 (单位: 字节)
            uint32_t sizeOfIEsAndHeader = 0;
            // IE 列表的总分配大小 + 此结构体的分配大小(offsetToTheFirstEntry)
            // (单位: 字节, 8 字节对齐)
            uint32_t allocatedSizeOfIEsAndHeader = 0;
            // 是否为叶子节点(0x00 表示为叶子节点, 0x01 表示非叶子节点)
            uint8_t notLeafNode = 0x00;
            // 填充 (实现 8 字节对齐)
            uint8_t padding[3] = {};
        } nodeHeader;
#pragma pack(pop)

        // 索引项数据
        std::vector<NtfsIndexEntry> IEs;

    public:
        NtfsIndexNode() = default;
        NtfsIndexNode(NtfsIndexNode const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        NtfsIndexNode &operator=(NtfsIndexNode const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }
        NtfsIndexNode(NtfsDataBlock &data, NTFS_ATTRIBUTES_TYPE streamType)
            : NtfsStructureBase(true) {
            // 数据大小必须要大于 索引节点头 的大小
            if (errno || data.len() < sizeof(NtfsIndexNode::IndexNodeHeader)) {
                Reset();
                return;
            }
            errno = memcpy_s(&nodeHeader, sizeof(nodeHeader), data,
                             sizeof(nodeHeader));
            // 数据大小必须要大于 索引项 + 索引节点头 的大小
            if (errno || nodeHeader.sizeOfIEsAndHeader > data.len()) {
                Reset();
                return;
            }
            // 索引项 + 索引节点头 不可能小于 索引节点头 大小.
            if (nodeHeader.sizeOfIEsAndHeader < sizeof(nodeHeader)) {
                Reset();
                return;
            }
            // 加载 索引项
            uint64_t pos = 0;
            NtfsDataBlock remainingData =
                NtfsDataBlock{data, nodeHeader.offsetToTheFirstEntry};
            while (pos < remainingData.len()) {
                NtfsIndexEntry t = {NtfsDataBlock{remainingData, pos},
                                    streamType};
                if (!t.valid) {
                    return;
                }
                pos += t.entryHeader.lengthOfIE;
                IEs.push_back(t);
                if (t.entryHeader.flags & t.FLAG_LAST_ENTRY_IN_THE_NODE) {
                    return;
                }
            }
        }

    protected:
        virtual NtfsIndexNode &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->nodeHeader = rr.nodeHeader;
            this->IEs = rr.IEs;
            return *this;
        }
        virtual NtfsIndexNode &Move(NtfsStructureBase &r) override {
            return Copy(r);
        }
    };

}