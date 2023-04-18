#pragma once
#include "disk_reader.hpp"
#include <functional>
#include <iterator>
#include <memory>
#include <stack>
#include <stdint.h>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace abkntfs {
    struct NtfsSectors : SuccessiveSectors {
        bool sparse;

        NtfsSectors(uint64_t start, uint64_t num, bool sparse)
            : SuccessiveSectors{start, num}, sparse(sparse) {}
    };
    using NtfsSectorsInfo = std::vector<NtfsSectors>;

    enum NTFS_ATTRIBUTES_TYPE : uint32_t {
        NTFS_ATTRIBUTE_NONE = 0x00,
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

    struct NtfsFileReference {
        uint64_t fileRecordNum : 48;
        uint64_t seqNum : 16;
    };

    struct NtfsIndexEntry;
    struct NtfsIndexRecord;

    struct NtfsStructureBase;
    struct AttrData_STANDARD_INFOMATION;
    struct AttrData_FILE_NAME;
    struct AttrData_DATA;
    struct AttrData_INDEX_ROOT;
    struct AttrData_INDEX_ALLOCATION;

    class NtfsBoot;
    class NtfsAttr;
    class NtfsFileRecord;
    class Ntfs_MFT_Record;
    class Ntfs;
    class NtfsDataRuns;
}

namespace abkntfs {
    struct NtfsDataBlock {
    private:
        std::shared_ptr<std::vector<char>> pVector;
        uint64_t const offset;
        uint64_t length;

    public:
        char *const pData;
        Ntfs *const pNtfs;
        NtfsDataBlock()
            : pVector(), pData(nullptr), length(0), offset(0), pNtfs(nullptr){};
        NtfsDataBlock(NtfsDataBlock const &r)
            : offset(r.offset), pData(r.pData), pNtfs(r.pNtfs) {
            this->pVector = r.pVector;
            this->length = r.length;
        }

        NtfsDataBlock &operator=(NtfsDataBlock const &r) {
            NtfsDataBlock temp = r;
            this->~NtfsDataBlock();
            new (this) NtfsDataBlock(temp);
            return *this;
        }

        // 拷贝 r 的数据
        NtfsDataBlock(std::vector<char> const &r, Ntfs *pNtfs)
            : pData{(pVector = std::make_shared<std::vector<char>>(r))
                        .get()
                        ->data()},
              pNtfs{pNtfs}, offset(0) {
            // pVector = std::make_shared<std::vector<char>>(r);
            // pData = pVector.get()->data();
            length = pVector.get()->size();
            // this->pNtfs = pNtfs;
        }

        // 构造 r 的数据视图, 不需要注意 r 的生命周期
        NtfsDataBlock(NtfsDataBlock const &r, uint64_t offset,
                      uint64_t len = (uint64_t)-1)
            : pVector{r.pVector}, offset(offset + r.offset),
              pData{&r.pVector.get()->data()[this->offset]}, pNtfs{r.pNtfs},
              length(len) {
            if (offset > r.pVector.get()->size()) {
                this->~NtfsDataBlock();
                new (this) NtfsDataBlock();
            }
            if (len == (uint64_t)-1) {
                if (r.length < offset) {
                    this->length = 0;
                }
                else {
                    this->length = r.length - offset;
                }
            }
        }

        char &operator[](std::ptrdiff_t idx) const {
            if (idx >= length || pData == nullptr) {
                throw std::runtime_error("array index outbound error!");
            }
            return pData[idx];
        }

        operator char *() const { return pData; }

        operator std::vector<char>() {
            std::vector<char> ret;
            if (pData != nullptr) {
                ret.assign(pData, pData + length);
            }
            return ret;
        }

        uint64_t len() const { return length; }

        // 生成自身数据的副本 (只截取 offset 和 len 的片段拷贝)
        NtfsDataBlock Copy() const {
            return NtfsDataBlock(std::vector<char>(pData, pData + length),
                                 pNtfs);
        }
    };

    // 子类要求:
    //   一: 子类必须重写 Copy(), Move(), 以下 T 为子类类型:
    //     1. Copy() 模板:
    //       virtual T &Copy(T const &r) override {
    //         using TT = std::remove_reference<decltype(*this)>::type;
    //         TT const &rr = (TT const &)r;
    //         this->xxx = rr.xxx;
    //         ...
    //         return *this;
    //       }
    //     2. Move() 模板:
    //       virtual T &Move(T &r) override {
    //         using TT = std::remove_reference<decltype(*this)>::type;
    //         TT &rr = (TT &)r;
    //         this->xxx = std::move(rr.xxx);
    //         ...
    //         return *this;
    //       }
    //   二: 以下 T 为 子类类型
    //     1. 当子类需要拷贝构造时按如下模板:
    //       T(T const &r) {
    //           static_cast<NtfsStructureBase &>(*this) = r;
    //       };
    //     2. 当子类需要移动构造时按如下模板:
    //       T(T &&r) {
    //           static_cast<NtfsStructureBase &>(*this) = std::move(r);
    //       };
    //     3. 当子类需要移动赋值时按如下模板:
    //       T & operator=(T &&r) {
    //           static_cast<NtfsStructureBase &>(*this) = std::move(r);
    //           return *this;
    //       };
    //     4. 子类默认会使用此基类的拷贝赋值.
    // 原因:
    //   * 当基类实现了自定义拷贝赋值函数,
    //   子类的默认拷贝赋值函数只是简单的调用基类的自定义拷贝赋值函数.
    //   * 当基类实现了自定义拷贝构造函数,
    //   子类的默认拷贝构造函数只是简单的调用基类的自定义拷贝构造函数.
    struct NtfsStructureBase {
        // 此对象数据是否有效
        bool const valid;
        NtfsStructureBase() : valid{false} {}
        NtfsStructureBase(bool const valid) : valid{valid} {}
        NtfsStructureBase(NtfsStructureBase const &r) = delete;
        /*virtual*/ NtfsStructureBase &operator=(NtfsStructureBase const &r) {
            reinterpret_cast<bool &>(*(bool *)&valid) = r.valid;
            return this->Copy(r);
        }
        NtfsStructureBase(NtfsStructureBase &&r) = delete;
        /*virtual*/ NtfsStructureBase &
        operator=(NtfsStructureBase &&r) noexcept {
            reinterpret_cast<bool &>(*(bool *)&valid) = r.valid;
            reinterpret_cast<bool &>(*(bool *)&r.valid) = false;
            return this->Move(r);
        }
        virtual ~NtfsStructureBase() {
            reinterpret_cast<bool &>(*(bool *)&valid) = false;
        }

    protected:
        virtual NtfsStructureBase &Copy(NtfsStructureBase const &r) = 0;
        virtual NtfsStructureBase &Move(NtfsStructureBase &r) = 0;

        virtual void Reset() {
            reinterpret_cast<bool &>(*(bool *)&valid) = false;
            errno = 0;
        }
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
        int32_t clustersPerFileRecord;
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
}

#include "ntfs_index_entry.h"
#include "ntfs_index_node.h"
#include "ntfs_index_record.h"
#include "ntfs_attr_data.h"
#include "ntfs_attr.h"
#include "ntfs_file_record.h"
#include "ntfs_data_runs.h"

namespace abkntfs {
    class Ntfs : public DiskReader, public NtfsStructureBase {
    public:
        NtfsBoot bootInfo;
        NtfsFileRecord MFT_FileRecord;
        // 单位: 字节
        uint32_t FileRecordSize;
        // MFT 表中文件记录的数量
        uint32_t FileRecordsCount;

        NtfsDataBlock ReadSectors(NtfsSectorsInfo &secs) {
            std::vector<char> data;
            uint64_t p = 0, secsSize;
            for (auto &i : secs) {
                p = data.size();
                secsSize = bootInfo.bytesPerSector * i.secNum;
                data.resize(p + secsSize);
                if (i.sparse) {
                    memset(data.data() + p, 0, secsSize);
                }
                else {
                    std::vector<char> secsData =
                        DiskReader::ReadSectors(i.startSecId, i.secNum);
                    memcpy_s(data.data() + p, secsSize, secsData.data(),
                             secsData.size());
                }
            }
            return {data, this};
        }

        uint64_t WriteData(uint64_t off, NtfsDataBlock &data) {
            uint64_t writtenSize = 0;
            uint64_t ss = data.len();
            while (data.len()) {
                uint64_t startingSector = off / GetSectorSize();
                uint64_t offInSector = off - startingSector * GetSectorSize();
                uint64_t copySize = GetSectorSize() - offInSector;
                NtfsDataBlock t = {ReadSector(startingSector), this};
                if (t.len() < copySize) {
                    return writtenSize;
                }
                memcpy(t + offInSector, data, copySize);
                if (!WriteSector(startingSector, t)) {
                    return writtenSize;
                }
                data = NtfsDataBlock{data, copySize};
                writtenSize += copySize;
                if (writtenSize > ss) break;
            }
            return writtenSize;
        }

        Ntfs() = default;
        Ntfs(Ntfs const &r) = delete;
        Ntfs &operator=(Ntfs const &r) = delete;
        Ntfs(Ntfs &&r) {
            static_cast<NtfsStructureBase &>(*this) = std::move(r);
        }
        Ntfs &operator=(Ntfs &&r) {
            static_cast<NtfsStructureBase &>(*this) = std::move(r);
            return *this;
        }

        Ntfs(std::string file) : DiskReader(file), NtfsStructureBase(true) {
            try {
                bootInfo = ReadSector(0);
                MFT_FileRecord = NtfsFileRecord{
                    NtfsDataBlock{
                        DiskReader::ReadSectors(bootInfo.LCNoVCN0oMFT *
                                                    bootInfo.sectorsPerCluster,
                                                bootInfo.sectorsPerCluster),
                        this},
                    0};
                FileRecordSize = MFT_FileRecord.fixedFields.allocatedSize;
                NtfsAttr &dataAttr = *MFT_FileRecord.FindSpecAttr(NTFS_DATA);
                if (&dataAttr != nullptr) {
                    if (dataAttr.fields.get()->nonResident) {
                        NtfsAttr::NonResidentPart &nonResidentPart =
                            static_cast<NtfsAttr::NonResidentPart &>(
                                *dataAttr.fields.get());
                        FileRecordsCount =
                            nonResidentPart.allocSize / FileRecordSize;
                    }
                }
            }
            catch (std::exception &e) {
                memset(&bootInfo, 0, sizeof(NtfsBoot));
                Reset();
                return;
            }
        }

        NtfsSectorsInfo DataRunsToSectorsInfo(NtfsDataBlock const &dataRuns,
                                              uint64_t VCcount = (uint64_t)-1) {
            NtfsSectorsInfo ret;
            bool checkVCcount = VCcount != (uint64_t)-1;
            auto callback = [&](NtfsDataRuns::Partition partInfo, uint64_t lcn,
                                uint64_t num) -> bool {
                if (VCcount >= num) {
                    VCcount -= num;
                    ret.emplace_back(
                        NtfsSectors{lcn * bootInfo.sectorsPerCluster,
                                    num * bootInfo.sectorsPerCluster,
                                    !partInfo.sizeOfOffField});

                    return true;
                }
                return false;
            };
            uint64_t dataRunsLen =
                NtfsDataRuns::ParseDataRuns(dataRuns, callback);
            if (checkVCcount && VCcount) {
                throw std::runtime_error("parse data runs failure!");
            }
            return ret;
        }

        NtfsSectorsInfo DataRunsToSectorsInfo(NtfsDataBlock const &dataRuns,
                                              NtfsAttr const &attr) {
            // 判断是否是 "非驻留" 属性, 如果不是则抛出异常, 只有 "非驻留"
            // 属性才有 data runs.
            if (!attr.fields.get()->nonResident) {
                throw std::runtime_error{"Non-Resident Attribute required!"};
            }
            NtfsAttr::NonResidentPart &nonResidentPart =
                (NtfsAttr::NonResidentPart &)*attr.fields.get();
            return DataRunsToSectorsInfo(dataRuns, nonResidentPart.VCN_end -
                                                       nonResidentPart.VCN_beg +
                                                       1);
        }

        // 虚拟扇区号转换到逻辑扇区号
        NtfsSectorsInfo VSN_To_LSN(NtfsSectorsInfo map, uint64_t index,
                                   uint64_t secNum) {
            uint64_t remainSecNum = secNum;
            uint64_t remainIndex = index;
            NtfsSectorsInfo ret;
            for (auto &i : map) {
                NtfsSectors cur = i;
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

        NtfsSectorsInfo GetFileRecordAreaByFRN(uint32_t FRN) {
            uint32_t secNum = FileRecordSize / bootInfo.bytesPerSector;
            uint32_t vsn = FRN * secNum;
            try {
                NtfsSectorsInfo &availableArea =
                    MFT_FileRecord.FindSpecAttr(NTFS_DATA, L"")
                        ->attrData.dataRunsMap;
                NtfsSectorsInfo requiredArea =
                    VSN_To_LSN(availableArea, vsn, secNum);
                return requiredArea;
            }
            catch (std::exception &e) {
                return NtfsSectorsInfo();
            }
        }

        NtfsFileRecord GetFileRecordByFRN(uint64_t FRN) {
            return NtfsFileRecord{ReadSectors(GetFileRecordAreaByFRN(FRN)),
                                  FRN};
        }

        uint64_t GetDataRunsClusterNum(NtfsDataBlock &dataRuns) {
            uint64_t size = 0;
            NtfsSectorsInfo area = DataRunsToSectorsInfo(dataRuns);
            for (auto &i : area) {
                size += i.secNum / bootInfo.sectorsPerCluster;
            }
            return size;
        }

        uint64_t GetDataRunsDataSize(NtfsAttr const &attr,
                                     NtfsDataBlock &dataRuns) {
            uint64_t sectorsTotalSize = 0;
            NtfsSectorsInfo area = DataRunsToSectorsInfo(dataRuns, attr);
            for (auto &i : area) {
                sectorsTotalSize += i.secNum * GetSectorSize();
            }
            return sectorsTotalSize;
        }

        // 获得文件路径
        std::wstring GetFilePath(NtfsFileRecord &fr, std::wstring sep = L"\\") {
            std::wstring path;
            if (!fr.valid) {
                return path;
            }
            AttrData_FILE_NAME *fnAttrData =
                fr.FindSpecAttrData(NTFS_FILE_NAME);
            if (nullptr == fnAttrData) {
                return path;
            }
            uint64_t frn = fnAttrData->fileInfo.fileRef.fileRecordNum;
            // 文件记录号 5 为根目录(.)
            while (frn && frn != 5) {
                NtfsFileRecord t = GetFileRecordByFRN(frn);
                if (!t.valid) {
                    return path;
                }
                fnAttrData = t.FindSpecAttrData(NTFS_FILE_NAME);
                if (nullptr == fnAttrData) {
                    return path;
                }
                frn = fnAttrData->fileInfo.fileRef.fileRecordNum;
                path = t.GetFileName() + sep + path;
            }
            return sep + path;
        }

    protected:
        virtual Ntfs &Copy(NtfsStructureBase const &r) override {
            return *this;
        }
        virtual Ntfs &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            static_cast<DiskReader &>(*this) = std::move(rr);
            this->bootInfo = rr.bootInfo;
            this->MFT_FileRecord = std::move(rr.MFT_FileRecord);
            this->FileRecordsCount = rr.FileRecordsCount;
            this->FileRecordSize = rr.FileRecordSize;
            return *this;
        }
    };
}
