#pragma once
#include "disk_reader.hpp"
#include <cstdalign>
#include <memory>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <vector>

namespace tamper {
    enum NTFS_ATTRIBUTES_TYPE {
        NTFS_STANDARD_INFOMATION = 0x10,
        NTFS_ATTRIBUTE_LIST = 0x20,
        NTFS_FILE_NAME = 0x30,
        NTFS_OBJECT_ID = 0x40,
        NTFS_SECURITY_DESCRIPTOR = 0x50,
        NTFS_VOLUME_NAME = 0x60,
        NTFS_VOLUME_INFORMATION = 0x70,
        NTFS_DATA = 0x80,
        NTFS_INDEX_ROOT = 0x90,       // 永远 "驻留"
        NTFS_INDEX_ALLOCATION = 0xA0, // 永远 "非驻留"
        NTFS_BITMAP = 0xB0,
        NTFS_REPARSE_POINT = 0xC0,
        NTFS_EA_INFORMATION = 0xD0,
        NTFS_EA = 0xE0,
        NTFS_LOGGED_UTILITY_STREAM = 0x100
    };

    struct NtfsDataBlock;
    struct TypeData;
    struct TypeData_FILE_NAME;

    class NtfsBoot;
    class NtfsAttr;
    class Ntfs_FILE_Record;
    class Ntfs_MFT_Record;
    class Ntfs;
    class NtfsDataRuns;
}

namespace tamper {

    struct NtfsDataBlock {
        std::shared_ptr<std::vector<char>> pVector;
        char *const pData;
        uint64_t len;
        Ntfs *const pNtfs;
        NtfsDataBlock(NtfsDataBlock const &r) = default;
        NtfsDataBlock(std::vector<char> const &r, Ntfs *pNtfs)
            : pData{(pVector = std::make_shared<std::vector<char>>(r))
                        .get()
                        ->data()},
              pNtfs{pNtfs} {
            // pVector = std::make_shared<std::vector<char>>(r);
            // pData = pVector.get()->data();
            len = pVector.get()->size();
            // this->pNtfs = pNtfs;
        }
        NtfsDataBlock(char *const(&r), uint64_t len, Ntfs *pNtfs)
            : pVector{}, pData{r}, pNtfs{pNtfs} {
            // pData = r;
            this->len = len;
            // this->pNtfs = pNtfs;
        }
        NtfsDataBlock &operator=(NtfsDataBlock const &r) {
            new (this) NtfsDataBlock(r);
            return *this;
        }

        char &operator[](std::ptrdiff_t idx) const {
            if (idx >= len || pData == nullptr) {
                throw std::runtime_error("array index outbound error!");
            }
            return pData[idx];
        }
        operator char *() const { return pData; }
        operator std::vector<char>() const { return *pVector; }
    };
    struct TypeData;
    struct TypeData_FILE_NAME {
        // 到父级目录的文件引用
        uint64_t fileRef;
        // 文件创建时间
        uint64_t cTime;
        // 文件更改时间
        uint64_t aTime;
        // MFT 更改时间
        uint64_t mTime;
        // 文件读取时间
        uint64_t rTime;
        // 文件分配大小
        uint64_t allocSizeOfFile;
        // 文件实际大小
        uint64_t realSizeOfFile;
        // 文件标志 (文件夹, 压缩, 隐藏)
        uint32_t flags;
        // 被 EAs 和 Reparse 使用
        uint32_t EAs_Reparse;
        // 文件名字符数 (UTF16-LE)
        uint8_t filenameLen;
        // 填充
        uint8_t padding;
        // 名字空间
        wchar_t filename[1];
    };

#pragma pack(push, 1)
    class NtfsBoot {
    public:
        char bootLoaderRoutine[3];
        wchar_t systemId[4];
        uint16_t bytesPerSector;
        uint8_t sectorsPerCluster;
        char unused1[7];
        uint8_t mediaDescriptor;
        char unused2[2];
        uint16_t sectorsPerTrack;
        uint16_t numberOfHeads;
        char unused3[8];
        char hex80008000[4];
        uint64_t numberOfSectors;
        uint64_t LCNoVCN0oMFT; // LCN of VCN 0 of the $MFT
        uint64_t LCNoVCN0oMFTMirr;
        int32_t clustersPer_MFT_Record;
        int32_t clustersPerIndexRecord;
        uint64_t volumeSerialNumber;
        char data[0x1b8];

        NtfsBoot() = default;
        NtfsBoot(std::vector<char> &sectorData) {
            memcpy_s(this, 512, sectorData.data(), sectorData.size());
        }
        NtfsBoot &operator=(std::vector<char> &sectorData) {
            memcpy_s(this, 512, sectorData.data(), sectorData.size());
            return *this;
        }
        operator std::vector<char>() {
            std::vector<char> ret;
            ret.resize(sizeof(NtfsBoot));
            memcpy_s(ret.data(), ret.size(), this, ret.size());
            return ret;
        }
    };
#pragma pack(pop)
    class NtfsAttr {
    public:
#pragma pack(push, 1)
        struct FixedFields {
            // 属性类型
            uint32_t attrType;
            // 整个属性驻留部分的长度, 8 的倍数,
            // 如果是文件记录中最后一个属性此值可能是随机数.
            uint32_t length;
            // 非驻留标志, 非驻留时为 0x01
            uint8_t nonResident;
            // 名称长度(名称编码 UTF16-LE)
            uint8_t nameLen;
            // 到名称的偏移量
            uint16_t offToName;
            // 标志
            uint16_t flags;
            // 属性 ID
            uint16_t attrId;
        };
#pragma pack(pop)
#pragma pack(push, 1)
        struct ResidentPart : public FixedFields {
            // 属性去除头部后的长度
            uint32_t attrLen;
            // 到属性数据的偏移量
            uint16_t offToAttr;
            // 索引标志
            uint8_t indexedFlag;
            // 无用填充
            uint8_t padding;
        };
#pragma pack(pop)
#pragma pack(push, 1)
        struct NonResidentPart : public FixedFields {
            // 起始 VCN
            uint64_t VCN_beg;
            // 末尾 VCN
            uint64_t VCN_end;
            // 到 data runs 的偏移
            uint16_t offToDataRuns;
            // 压缩单元大小 (2^n 簇)
            uint16_t compressionUnitSize;
            // 无用填充
            uint32_t padding;
            // 分配给此属性的大小
            uint64_t allocSize;
            // 属性的真实大小
            uint64_t realSize;
            // 流(stream) 的初始状态数据大小
            uint64_t initializedDataSizeOfTheStream;
        };
#pragma pack(pop)
        std::shared_ptr<FixedFields> fields;
        std::wstring attrName;
        // 如果是 "非驻留" 属性, 则储存 data runs
        std::vector<char> attrData;
        // 属性不正常
        bool badAttr;

        NtfsAttr() = default;
        NtfsAttr(NtfsDataBlock const &data);

        bool IsBad() const { return badAttr; }
    };

    class Ntfs_FILE_Record {
    public:
        enum FLAGS {
            FILE_RECORD_IN_USE = 0x01,
            FILE_RECORD_IS_DIRECTORY = 0x02,
        };
#pragma pack(push, 1)
        struct {
            // "FILE"
            char magicNumber[4];
            // 更新序列(update sequence, US) 偏移量
            uint16_t offsetToUS;
            // 按字算的 (更新序列 + 数组) 的大小
            uint16_t sizeInWordOfUSN;
            // $LogFile 序列号 (LSN)
            uint64_t LSN;
            // 序列号, 此记录被重用的次数, 循环计数
            uint16_t seqNumber;
            // 硬链接数
            uint16_t hardLinkCount;
            // 到 第一个属性 的偏移量
            uint16_t offToFirstAttr;
            // 标志
            uint16_t flags;
            // 此记录的真实大小(单位: 字节) (为 8 的倍数, 包含尾部
            // 0xFFFFFFFF)
            uint32_t realSize;
            // 此记录的分配大小(单位: 字节)
            uint32_t allocatedSize;
            // 文件到 base FILE 记录 的引用, 0 表示本身就是 base 文件记录，
            // base 文件记录如果存在 extension
            // 文件记录则会存在　ATTRIBUTE_LIST 属性中.
            uint64_t fileReference;
            // 下一个属性 ID
            uint16_t nextAttrID;
            // 保留 (NTFS 3.1+)
            uint16_t resv;
            // 文件记录编号 (NTFS 3.1+)
            uint32_t index;
        } fixedFields;
#pragma pack(pop)
        uint16_t USN;
        // US array
        std::vector<char> USA;
        // 属性数据
        std::vector<NtfsAttr> attrs;

        Ntfs_FILE_Record() = default;
        Ntfs_FILE_Record(NtfsDataBlock const &data);

        Ntfs_FILE_Record &operator=(NtfsDataBlock const &data) {
            new (this) Ntfs_FILE_Record(data);
            return *this;
        }
        ~Ntfs_FILE_Record() {}

        std::wstring GetFileName() {
            std::wstring filename;
            for (auto const &p : attrs) {
                if (p.fields.get()->attrType == tamper::NTFS_FILE_NAME) {
                    tamper::TypeData_FILE_NAME *fn =
                        (tamper::TypeData_FILE_NAME *)p.attrData.data();
                    filename.assign((wchar_t *)fn->filename, fn->filenameLen);
                    return filename;
                }
            }
            return filename;
        }

        // 可能返回空引用!!!
        NtfsAttr &GetDataAttr() {
            for (auto &p : attrs) {
                if (p.fields.get()->attrType == tamper::NTFS_DATA) {
                    return p;
                }
            }
            return *(NtfsAttr *)nullptr;
        }
    };

    class Ntfs_MFT_Record {
    public:
        std::vector<Ntfs_FILE_Record> records;

        Ntfs_MFT_Record() = default;
        Ntfs_MFT_Record(NtfsDataBlock const &data) {
            Ntfs_FILE_Record t;
            uint64_t pos = 0;
            uint32_t entrySize = 0;
            while (pos + sizeof(Ntfs_FILE_Record::fixedFields) < data.len) {
                t = NtfsDataBlock{(&data[pos]), data.len - pos, data.pNtfs};
                pos += t.fixedFields.allocatedSize;
                if (entrySize == 0) {
                    entrySize = t.fixedFields.allocatedSize;
                }
                if (t.fixedFields.allocatedSize == 0) {
                    pos += entrySize;
                }
                records.push_back(t);
            }
        }
    };

    class NtfsDataRuns {
    public:
        template <class T>
        static std::vector<char> GetDataRuns(T const &data, uint64_t VC_len) {
            std::vector<char> ret;
            uint32_t pos = 0;
            uint32_t LCN = 0;
            struct {
                uint8_t sizeOfLengthField : 4;
                uint8_t sizeOfOffsetField : 4;
            } fieldsSize;
            uint64_t clusterNum = 0;
            int64_t offsetOfLCN = 0;
            while (data[pos] != 0 && VC_len > 0) {
                ((uint8_t &)fieldsSize) = data[pos];
                pos++;
                // 不支持大于 8 字节的字段
                if (fieldsSize.sizeOfLengthField > 8 ||
                    fieldsSize.sizeOfOffsetField > 8) {
                    // throw std::runtime_error{
                    //     "size of field greater than 8 bytes."};
                    break;
                }
                clusterNum = 0;
                memcpy(&clusterNum, &data[pos], fieldsSize.sizeOfLengthField);
                pos += fieldsSize.sizeOfLengthField;
                offsetOfLCN = 0;
                if (data[pos + fieldsSize.sizeOfOffsetField - 1] < 0) {
                    offsetOfLCN = -1ll;
                }
                memcpy(&offsetOfLCN, &data[pos], fieldsSize.sizeOfOffsetField);
                pos += fieldsSize.sizeOfOffsetField;
                LCN += offsetOfLCN;
                if (VC_len >= clusterNum) {
                    VC_len -= clusterNum;
                }
                else {
                    break;
                }
            }
            ret.assign(&data[0], &data[pos + 1]);
            return ret;
        }
    };

    class Ntfs : public DiskReader {
    public:
        NtfsBoot bootInfo;
        Ntfs_FILE_Record MFT_FileRecord;
        // 单位: 字节
        uint32_t FILE_RecordSize;
        // MFT 表中文件记录的数量
        uint32_t FileRecordsNumber;

        NtfsDataBlock ReadSectors(std::vector<SuccessiveSectors> &secs) {
            return {DiskReader::ReadSectors(secs), this};
        }

        Ntfs(std::string file) : DiskReader(file) {
            try {
                bootInfo = ReadSector(0);
                MFT_FileRecord = NtfsDataBlock{
                    DiskReader::ReadSectors(bootInfo.LCNoVCN0oMFT *
                                                bootInfo.sectorsPerCluster,
                                            bootInfo.sectorsPerCluster),
                    this};
                FILE_RecordSize = MFT_FileRecord.fixedFields.allocatedSize;
                NtfsAttr &dataAttr = MFT_FileRecord.GetDataAttr();
                if (&dataAttr != nullptr) {
                    if (dataAttr.fields.get()->nonResident) {
                        NtfsAttr::NonResidentPart &nonResidentPart =
                            static_cast<NtfsAttr::NonResidentPart &>(
                                *dataAttr.fields.get());
                        FileRecordsNumber =
                            nonResidentPart.allocSize / FILE_RecordSize;
                    }
                }
            }
            catch (std::exception &e) {
                memset(&bootInfo, 0, sizeof(NtfsBoot));
                return;
            }
        }

        std::vector<SuccessiveSectors> DataRunsToSectorsInfo(NtfsAttr &attr) {
            // 判断是否是 "非驻留" 属性, 如果不是则抛出异常, 只有 "非驻留"
            // 属性才有 data runs.
            if (!attr.fields.get()->nonResident) {
                throw std::runtime_error{"Non-Resident Attribute required!"};
            }
            std::vector<char> &dataRuns = attr.attrData;
            std::vector<SuccessiveSectors> ret;
            uint32_t pos = 0;
            uint32_t LCN = 0;
            struct {
                uint8_t sizeOfLengthField : 4;
                uint8_t sizeOfOffsetField : 4;
            } fieldsSize;
            uint64_t clusterNum = 0;
            int64_t offsetOfLCN = 0;
            while (dataRuns[pos] != 0 && pos < dataRuns.size()) {
                ((uint8_t &)fieldsSize) = dataRuns[pos];
                pos++;
                // 不支持大于 8 字节的字段
                if (fieldsSize.sizeOfLengthField > 8 ||
                    fieldsSize.sizeOfOffsetField > 8) {
                    throw std::runtime_error{
                        "size of field greater than 8 bytes."};
                }
                memcpy(&clusterNum, &dataRuns[pos],
                       fieldsSize.sizeOfLengthField);
                pos += fieldsSize.sizeOfLengthField;
                offsetOfLCN = 0;
                if (dataRuns[pos + fieldsSize.sizeOfOffsetField - 1] < 0) {
                    offsetOfLCN = -1ll;
                }
                memcpy(&offsetOfLCN, &dataRuns[pos],
                       fieldsSize.sizeOfOffsetField);
                pos += fieldsSize.sizeOfOffsetField;
                LCN += offsetOfLCN;
                ret.emplace_back(
                    SuccessiveSectors{LCN * bootInfo.sectorsPerCluster,
                                      clusterNum * bootInfo.sectorsPerCluster});
            }
            return ret;
        }

        std::vector<SuccessiveSectors>
        VSN_To_LSN(std::vector<SuccessiveSectors> map, uint64_t index,
                   uint64_t secNum) {
            uint64_t remainSecNum = secNum;
            uint64_t remainIndex = index;
            std::vector<SuccessiveSectors> ret;
            for (auto &i : map) {
                SuccessiveSectors cur = i;
                if (remainIndex > cur.secNum) {
                    remainIndex -= cur.secNum;
                    continue;
                }
                cur.startSecId += remainIndex;
                cur.secNum -= secNum;
                remainIndex = 0;
                if (cur.secNum > 0) {
                    if (cur.secNum >= remainSecNum) {
                        cur.secNum = remainSecNum;
                        remainSecNum = 0;
                        ret.push_back(cur);
                        return ret;
                    }
                    else {
                        remainSecNum -= cur.secNum;
                        ret.push_back(cur);
                    }
                }
            }
            if (remainSecNum) {
                throw std::runtime_error{"Too many sectors requested!"};
            }
            return ret;
        }

        std::vector<SuccessiveSectors>
        GetFileRecordAreaByIndex(uint32_t index) {
            uint32_t secNum = FILE_RecordSize / bootInfo.bytesPerSector;
            uint32_t vsn = index * secNum;
            std::vector<SuccessiveSectors> availableArea =
                DataRunsToSectorsInfo(MFT_FileRecord.GetDataAttr());
            std::vector<SuccessiveSectors> requiredArea =
                VSN_To_LSN(availableArea, vsn, secNum);
            return requiredArea;
        }

        NtfsDataBlock GetFileRecordByIndex(uint32_t index) {
            return ReadSectors(GetFileRecordAreaByIndex(index));
        }
    };
}

// NtfsAttr 定义
namespace tamper {
    NtfsAttr::NtfsAttr(NtfsDataBlock const &data) : attrData{}, badAttr{false} {
        struct FixedFields fixedFields;
        // 属性的驻留部分长度
        uint32_t residentPartLength;
        // 赋值固定部分
        memcpy_s(&fixedFields, sizeof(fixedFields), &data[0],
                 sizeof(fixedFields));
        if (fixedFields.nonResident > 1 ||
            fixedFields.attrType > NTFS_LOGGED_UTILITY_STREAM) {
            fields = std::make_shared<FixedFields>(fixedFields);
            badAttr = true;
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
            attrData = NtfsDataRuns::GetDataRuns(
                &data[nonResidentPart.offToDataRuns],
                nonResidentPart.VCN_end - nonResidentPart.VCN_beg + 1ull);
            // 计算驻留部分属性长度, 因为字段中的长度可能是错的.
            residentPartLength =
                attrData.size() + nonResidentPart.offToDataRuns;
        }
        else {
            fields = std::make_shared<ResidentPart>();
            ResidentPart &residentPart =
                static_cast<ResidentPart &>(*fields.get());
            // 赋值 "驻留" 属性头
            memcpy_s(fields.get(), sizeof(ResidentPart), &data[0],
                     sizeof(ResidentPart));
            // 如果是 "驻留" 属性, 则获取属性的具体数据.
            attrData.resize(residentPart.attrLen);
            if (attrData.size()) {
                memcpy_s(&attrData[0], attrData.size(),
                         &data[residentPart.offToAttr], attrData.size());
            }
            // 计算驻留部分属性长度, 因为字段中的长度可能是错的.
            residentPartLength = residentPart.offToAttr + residentPart.attrLen;
        }
        // 长度字段为 8 的倍数.
        residentPartLength = 0x8u * ((residentPartLength + 0x07u) / 0x8u);
        // 判断属性中的原始长度字段是否错误, 错误则进行更正.
        if (fields.get()->length > data.len) {
            fields.get()->length = residentPartLength;
        }
    }

}

// Ntfs_FILE_Record 定义
namespace tamper {
    Ntfs_FILE_Record::Ntfs_FILE_Record(NtfsDataBlock const &data) {
        memcpy_s(&fixedFields, sizeof(fixedFields), &data[0],
                 sizeof(fixedFields));
        if (memcmp(fixedFields.magicNumber, "FILE", 4)) {
            return;
        }
        if (fixedFields.offsetToUS + fixedFields.sizeInWordOfUSN * 2 + 2 >=
            data.len) {
            return;
        }
        USN = *(uint16_t *)(&data[fixedFields.offsetToUS]);
        USA.resize((fixedFields.sizeInWordOfUSN - 1) << 1);
        memcpy_s(USA.data(), USA.size(), &data[fixedFields.offsetToUS + 2],
                 USA.size());
        // 进行数据修正
        uint16_t &sectorSize = data.pNtfs->bootInfo.bytesPerSector;
        for (int i = 0; i < (USA.size() >> 1); i++) {
            memcpy_s(&data[sectorSize * i + (sectorSize - 2)], 2, &USA[i << 1],
                     2);
        }

        // 截取属性数据
        std::vector<char> attrsData;
        attrsData.resize(fixedFields.realSize - fixedFields.offToFirstAttr);
        memcpy_s(attrsData.data(), attrsData.size(),
                 &data[fixedFields.offToFirstAttr], attrsData.size());
        // std::cout << "record alloc size: " << std::dec <<
        // fixedFields.allocatedSize
        //           << std::endl;
        // 获得属性列表.
        NtfsAttr t;
        uint64_t pos = 0;
        while (pos + sizeof(NtfsAttr::FixedFields) < attrsData.size()) {
            // std::cout << "attr load beg" << std::endl;
            t = NtfsDataBlock{&attrsData[pos],
                              fixedFields.realSize -
                                  fixedFields.offToFirstAttr - pos,
                              data.pNtfs};
            pos += t.fields.get()->length;
            if (t.fields.get()->length == 0 || t.IsBad()) {
                break;
            }
            attrs.push_back(t);
        }
        // std::cout << "attrs load end" << std::endl;
    }
}
