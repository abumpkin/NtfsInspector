#include "ntfs_access.hpp"

// Ntfs_FILE_Record 定义
namespace abkntfs {
    NtfsFileRecord::NtfsFileRecord(NtfsDataBlock const &data, uint64_t FRN)
        : NtfsStructureBase(true), FRN(FRN) {
        if (data.len() < sizeof(fixedFields)) {
            Reset();
            return;
        }
        memcpy_s(&fixedFields, sizeof(fixedFields), &data[0],
                 sizeof(fixedFields));
        if (memcmp(fixedFields.magicNumber, "FILE", 4)) {
            Reset();
            return;
        }
        if ((uint64_t)fixedFields.offsetToUS +
                (uint64_t)fixedFields.sizeInWordOfUSN * 2 + 2 >=
            data.len()) {
            Reset();
            return;
        }
        USN = *(uint16_t *)(&data[fixedFields.offsetToUS]);
        USA.resize(((uint64_t)fixedFields.sizeInWordOfUSN - 1) << 1);
        memcpy_s(USA.data(), USA.size(),
                 &data[(uint64_t)fixedFields.offsetToUS + 2], USA.size());
        // 进行数据修正
        uint16_t &sectorSize = data.pNtfs->bootInfo.bytesPerSector;
        for (uint64_t i = 0; i < (USA.size() >> 1); i++) {
            memcpy_s(&data[(uint64_t)sectorSize * i + (sectorSize - 2)], 2,
                     &USA[i << 1], 2);
        }

        // 截取属性数据
        NtfsDataBlock attrsData{data, fixedFields.offToFirstAttr,
                                fixedFields.realSize -
                                    fixedFields.offToFirstAttr};
        // 获得属性列表.
        std::shared_ptr<NtfsAttr> t;
        uint64_t pos = 0;
        while (pos + sizeof(NtfsAttr::FixedFields) < attrsData.len()) {
            t = std::make_shared<NtfsAttr>(
                NtfsDataBlock{attrsData, pos},
                !attrs.empty() && attrs.back().get()->valid ? attrs.back()
                                                            : nullptr,
                FRN);
            pos += t.get()->fields.get()->length;
            if (t.get()->fields.get()->length == 0 || !t.get()->valid) {
                break;
            }
            attrs.push_back(t);
        }
        // 加载属性列表里的属性 (如果有)
        NtfsAttr::TypeData *pAttrData = FindSpecAttrData(NTFS_ATTRIBUTE_LIST);
        if (nullptr != pAttrData) {
            AttrData_ATTRIBUTE_LIST &attrList = *pAttrData;
            for (auto &i : attrList.list) {
                if (i.info.fileReference.fileRecordNum != FRN) {
                    NtfsFileRecord t = data.pNtfs->GetFileRecordByFRN(
                        i.info.fileReference.fileRecordNum);
                    for (auto &p : t.attrs) {
                        if (!attrs.empty()) {
                            p.get()->prevAttr = attrs.back();
                        }
                        attrs.push_back(p);
                    }
                }
            }
        }
    }
}