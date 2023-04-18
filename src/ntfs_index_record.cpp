#include "ntfs_index_record.h"

// NtfsIndexRecord 定义
namespace abkntfs {
    NtfsIndexRecord::NtfsIndexRecord(NtfsDataBlock &data,
                                     NTFS_ATTRIBUTES_TYPE streamType)
        : NtfsStructureBase(true), US(0) {
        if (data.len() < sizeof(standardIndexHeader)) {
            Reset();
            return;
        }
        errno = memcpy_s(&standardIndexHeader, sizeof(standardIndexHeader),
                         data, sizeof(standardIndexHeader));
        // 判断 索引标志
        if (memcmp(&standardIndexHeader.magicNum, "INDX",
                   sizeof(standardIndexHeader.magicNum))) {
            Reset();
            return;
        }
        // 获取 US 和 USA
        this->US = *(uint16_t *)&data[standardIndexHeader.offToUS];
        this->USA.resize(
            (uint64_t)(standardIndexHeader.sizeInWordsOfUSNandUSA - 1) << 1);
        memcpy(USA.data(), &data[(uint64_t)standardIndexHeader.offToUS + 2],
               USA.size());
        // 进行数据修正
        if ((USA.size() >> 1) * data.pNtfs->GetSectorSize() > data.len()) {
            Reset();
            return;
        }
        for (uint64_t i = 0; i < (USA.size() >> 1); i++) {
            memcpy(&data[data.pNtfs->GetSectorSize() * (i + 1) - 2],
                   &USA[i << 1], 2);
        }
        // 减去 标准索引头 的数据
        NtfsDataBlock remainingData =
            NtfsDataBlock{data, sizeof(standardIndexHeader)};
        // 解析 索引节点 数据
        this->node = NtfsIndexNode{remainingData, streamType};
    }
}