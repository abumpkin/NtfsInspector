#pragma once
#include "ntfs_access.hpp"

namespace abkntfs {
    struct NtfsIndexRecord : NtfsStructureBase {

#pragma pack(push, 1)
        struct IndexRecordInfo {
            // "INDX"
            char magicNum[4] = {'I', 'N', 'D', 'X'};
            // 到 更新序列(Update Sequence, US) 的偏移
            uint16_t offToUS = 0;
            // 更新序列号(Update Sequence Number, USN) + 更新序列数组(Update
            // Sequence Array, USA) 的按字大小.
            uint16_t sizeInWordsOfUSNandUSA = 0;
            // $LogFile 序列号(sequence number)
            uint64_t seqNumOfLogFile = 0;
            // 这个 索引记录(Index Record, INDX buffer) 在 Index Allocation 中的
            // 虚拟簇号(VCN).
            uint64_t VCNofIRinIA = 0;
        } standardIndexHeader;
#pragma pack(pop)

        // 更新序列
        uint16_t US = 0;
        // 更新序列数组
        std::vector<char> USA;
        // 索引节点
        NtfsIndexNode node;

    public:
        NtfsIndexRecord() = default;
        NtfsIndexRecord(NtfsIndexRecord const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        NtfsIndexRecord &operator=(NtfsIndexRecord const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }

        NtfsIndexRecord(NtfsDataBlock &data, NTFS_ATTRIBUTES_TYPE streamType);

        // 创建新 索引记录
        // NtfsIndexRecord(std::vector<NtfsIndexEntry> const &indexEntries,
        //                 bool isLeaf)
        //     : NtfsStructureBase(true) {
        //     this->IEs = indexEntries;
        //     this->indexRecordHeader.sizeOfIEsAndHeader =
        //         sizeof(IndexRecordHeader);
        //     for (auto &i : indexEntries) {
        //         this->indexRecordHeader.sizeOfIEsAndHeader +=
        //             i.entryHeader.lengthOfIE;
        //     }
        //     this->indexRecordHeader.allocatedSizeOfIEsAndHeader =
        //         ((this->indexRecordHeader.sizeOfIEsAndHeader + 0x07) / 0x08)
        //         * 0x08;
        //     this->indexRecordHeader.notLeafNode = !isLeaf;
        // }

    protected:
        virtual NtfsIndexRecord &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->standardIndexHeader = rr.standardIndexHeader;
            this->US = rr.US;
            this->USA = rr.USA;
            this->node = rr.node;
            return *this;
        }
        virtual NtfsIndexRecord &Move(NtfsStructureBase &r) override {
            return Copy(r);
        }
    };
}