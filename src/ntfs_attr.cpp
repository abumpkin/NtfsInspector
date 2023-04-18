#include "ntfs_access.hpp"

// NtfsAttr 定义
namespace abkntfs {
    NtfsAttr::NtfsAttr(NtfsDataBlock const &data,
                       std::shared_ptr<NtfsAttr> previousAttr, uint64_t FRN)
        : attrData{}, NtfsStructureBase{true}, prevAttr(previousAttr),
          fileRecordFrom(FRN) {
        struct FixedFields fixedFields;
        // 属性的驻留部分长度
        uint32_t residentPartLength;
        // 赋值固定部分
        memcpy_s(&fixedFields, sizeof(fixedFields), &data[0],
                 sizeof(fixedFields));
        if (fixedFields.nonResident > 1 ||
            fixedFields.attrType > NTFS_LOGGED_UTILITY_STREAM) {
            fields = std::make_shared<FixedFields>(fixedFields);
            Reset();
            return;
        }
        // 如果是 "具名属性", 则进行名称赋值.
        if (fixedFields.nameLen) {
            attrName.assign((const wchar_t *)&data[fixedFields.offToName],
                            fixedFields.nameLen);
        }
        // 如果是 "非驻留" 属性
        if (fixedFields.nonResident) {
            fields = std::make_shared<NonResidentPart>();
            NonResidentPart &nonResidentPart =
                static_cast<NonResidentPart &>(*fields.get());
            // 赋值 "非驻留" 属性头
            memcpy_s(fields.get(), sizeof(NonResidentPart), &data[0],
                     sizeof(NonResidentPart));
            // 获取 data runs, 以 0x00 结尾.
            attrData = TypeData{
                this, NtfsDataBlock{
                          NtfsDataRuns::GetDataRuns(
                              NtfsDataBlock{data, nonResidentPart.offToDataRuns,
                                            data.len() -
                                                nonResidentPart.offToDataRuns},
                              nonResidentPart.VCN_end -
                                  nonResidentPart.VCN_beg + 1ull),
                          data.pNtfs}};
            // 计算驻留部分属性长度, 因为字段中的长度可能是错的.
            residentPartLength = attrData.len() + nonResidentPart.offToDataRuns;
        }
        else {
            fields = std::make_shared<ResidentPart>();
            ResidentPart &residentPart =
                static_cast<ResidentPart &>(*fields.get());
            // 赋值 "驻留" 属性头
            memcpy_s(fields.get(), sizeof(ResidentPart), &data[0],
                     sizeof(ResidentPart));
            // 如果是 "驻留" 属性, 则获取属性的具体数据.
            attrData =
                TypeData{this, NtfsDataBlock{data, residentPart.offToAttr,
                                             residentPart.attrLen}
                                   .Copy()};
            // 计算驻留部分属性长度, 因为字段中的长度可能是错的.
            residentPartLength = residentPart.offToAttr + residentPart.attrLen;
        }
        // 长度字段为 8 的倍数.
        residentPartLength = 0x8u * ((residentPartLength + 0x07u) / 0x8u);
        // 判断属性中的原始长度字段是否错误, 错误则进行更正.
        if (fields.get()->length > data.len()) {
            fields.get()->length = residentPartLength;
        }
        // 拷贝原始数据
        this->rawData = NtfsDataBlock{data, 0, residentPartLength}.Copy();
    }

    NtfsDataBlock NtfsAttr::ReadAttrRawRealData(uint64_t offset, uint64_t size,
                                                Ntfs *pNtfs) {
        NtfsDataBlock ret;
        if (nullptr == pNtfs) {
            return ret;
        }
        if (offset > GetDataSize()) {
            return ret;
        }
        if (offset + size > GetDataSize()) {
            size = GetDataSize() - offset;
        }
        if (!fields.get()->nonResident) {
            ret = NtfsDataBlock{(NtfsDataBlock)attrData, offset, size}.Copy();
        }
        else {
            uint64_t sectorSize = pNtfs->GetSectorSize();
            uint64_t startingSector = offset / sectorSize;
            uint64_t offInSec = offset - startingSector * sectorSize;
            uint64_t sectorsNum = 1 + (size + offInSec + 1) / sectorSize;
            NtfsSectorsInfo dataRunsMap =
                pNtfs->DataRunsToSectorsInfo((NtfsDataBlock)attrData);
            NtfsSectorsInfo needToRead =
                pNtfs->VSN_To_LSN(dataRunsMap, startingSector, sectorsNum);
            ret = NtfsDataBlock{pNtfs->ReadSectors(needToRead),
                                offset - startingSector * sectorSize, size};
        }
        return ret;
    }
}