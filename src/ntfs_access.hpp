#pragma once
#include "disk_reader.hpp"
#include <functional>
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

    struct NtfsIndexEntry;
    struct NtfsIndexRecord;

    struct NtfsStructureBase;
    struct TypeData_STANDARD_INFOMATION;
    struct TypeData_FILE_NAME;
    struct TypeData_DATA;
    struct TypeData_INDEX_ROOT;

    class NtfsBoot;
    class NtfsAttr;
    class Ntfs_FILE_Record;
    class Ntfs_MFT_Record;
    class Ntfs;
    class NtfsDataRuns;
}

namespace tamper {
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
        // 构造 r 的数据视图, 需注意 r 的生命周期
        // NtfsDataBlock(char *const(&r), uint64_t len, Ntfs *pNtfs)
        //     : pVector{}, pData{r}, pNtfs{pNtfs}, offset(0) {
        //     // pData = r;
        //     this->length = length;
        //     // this->pNtfs = pNtfs;
        // }

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
                this->length = r.length - offset;
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

    struct NtfsIndexEntry : NtfsStructureBase {
        enum INDEX_ENTRY_FLAGS {
            // 标志这个 索引项 指向下一个子 索引节点.
            FLAG_IE_POINT_TO_SUBNODE = 0x01,
            // 标志这个 索引项 是 索引记录 里一系列 索引项 的最后一项.
            FLAG_LAST_ENTRY_IN_THE_NODE = 0x01,
        };

#pragma pack(push, 1)
        struct {
            // 文件引用 (前 6 字节是 文件记录号, 后 2 字节是序列号 seqNumber,
            // 用于校验)
            uint64_t fileReference;
            // 整个 索引项 的长度 (单位: 字节)
            uint16_t lengthOfIE;
            // 流(stream) 的长度 (单位: 字节)
            uint16_t lengthOfStream;
            // 标志
            uint8_t flags;
            // 填充, 用于 8 字节对齐
            uint8_t padding[3];
        } entryHeader;
#pragma pack(pop)

        // 索引项 的流, 包含此 索引项 所指向的 文件记录里被索引属性的
        // 属性数据(不包含标准属性头).
        NtfsDataBlock stream;

    protected:
        virtual NtfsIndexEntry &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->entryHeader = rr.entryHeader;
            this->stream = rr.stream;
            return *this;
        }
        virtual NtfsIndexEntry &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->entryHeader = rr.entryHeader;
            this->stream = rr.stream;
            return *this;
        }
    };

    struct TypeData_STANDARD_INFOMATION : NtfsStructureBase {
        enum DOS_FILE_PERMISSIONS {
            DOS_FILE_PERMISSION_READ_ONLY = 0x0001,
            DOS_FILE_PERMISSION_HIDDEN = 0x0002,
            DOS_FILE_PERMISSION_SYSTEM = 0x0004,
            DOS_FILE_PERMISSION_ARCHIVE = 0x0020,
            DOS_FILE_PERMISSION_DEVICE = 0x0040,
            DOS_FILE_PERMISSION_NORMAL = 0x0080,
            DOS_FILE_PERMISSION_TEMPORARY = 0x0100,
            DOS_FILE_PERMISSION_SPARSE_FILE = 0x0200,
            DOS_FILE_PERMISSION_REPARSE_POINT = 0x0400,
            DOS_FILE_PERMISSION_COMPRESSED = 0x0800,
            DOS_FILE_PERMISSION_OFFLINE = 0x1000,
            DOS_FILE_PERMISSION_NOT_CONTENT_INDEXED = 0x2000,
            DOS_FILE_PERMISSION_ENCRYPTED = 0x4000
        };

#pragma pack(push, 1)
        struct {
            // 文件创建时间
            uint64_t cTime;
            // 文件修改时间
            uint64_t aTime;
            // 文件记录 修改时间
            uint64_t mTime;
            // 文件读取时间
            uint64_t rTime;
            // Dos 系统 文件权限
            uint32_t dosPermission;
            // 最大版本号
            uint32_t maximumNumberOfVersions;
            // 版本号
            uint32_t versionNumber;
            // 双向 Class ID 索引中的 Class ID
            uint32_t classId;
            // 拥有者 ID (win2000)
            uint32_t ownerId;
            // 安全 ID (win2000)
            uint32_t securityId;
            // 文件的配额大小, 占用用户的空间配额.
            // 这里应该是所有流的总数据大小(单位: 字节). (win2000)
            uint64_t quotaCharged;
            // 更新序列号(USN), 用于日志记录
            uint64_t USN;
        } info;
#pragma pack(pop)

        TypeData_STANDARD_INFOMATION() = default;
        // TypeData_STANDARD_INFOMATION(TypeData_STANDARD_INFOMATION const &r)
        //     : NtfsStructureBase(r) {
        //     this->info = r.info;
        // };
        // TypeData_STANDARD_INFOMATION &
        // operator=(TypeData_STANDARD_INFOMATION const &r) {
        //     NtfsStructureBase::operator=(r);
        //     this->info = r.info;
        // }
        TypeData_STANDARD_INFOMATION(NtfsDataBlock const &data)
            : NtfsStructureBase(true) {
            if (data.len() < sizeof(info)) {
                Reset();
                return;
            }
            errno = memcpy_s(&info, sizeof(info), &data[0], sizeof(info));
            if (errno) {
                Reset();
                return;
            }
        }

    protected:
        virtual TypeData_STANDARD_INFOMATION &
        Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->info = rr.info;
            return *this;
        }
        virtual TypeData_STANDARD_INFOMATION &
        Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->info = rr.info;
            return *this;
        }
    };

    struct TypeData_FILE_NAME : NtfsStructureBase {
        enum FILE_FLAGS {
            FILE_FLAG_READ_ONLY = 0x0001,
            FILE_FLAG_HIDDEN = 0x0002,
            FILE_FLAG_SYSTEM = 0x0004,
            FILE_FLAG_ARCHIVE = 0x0020,
            FILE_FLAG_DEVICE = 0x0040,
            FILE_FLAG_NORMAL = 0x0080,
            FILE_FLAG_TEMPORARY = 0x0100,
            FILE_FLAG_SPARSE_FILE = 0x0200,
            FILE_FLAG_REPARSE_POINT = 0x0400,
            FILE_FLAG_COMPRESSED = 0x0800,
            FILE_FLAG_OFFLINE = 0x1000,
            FILE_FLAG_NOT_CONTENT_INDEXED = 0x2000,
            FILE_FLAG_ENCRYPTED = 0x4000,
            FILE_FLAG_DIRECTORY = 0x10000000,
            FILE_FLAG_INDEX_VIEW = 0x20000000
        };

#pragma pack(push, 1)
        struct {
            // 到父级目录的文件引用
            uint64_t fileRef;
            // 文件创建时间
            uint64_t cTime;
            // 文件更改时间
            uint64_t aTime;
            // MFT 文件记录 更改时间
            uint64_t mTime;
            // 文件读取时间
            uint64_t rTime;
            // 文件分配大小
            uint64_t allocSizeOfFile;
            // 文件实际大小
            uint64_t realSizeOfFile;
            // 文件标志 (文件夹, 压缩, 隐藏)
            uint32_t flags;
            // 被 EAs(Extended Attributes) 和 Reparse 使用
            uint32_t EAs_Reparse;
            // 文件名字符数 (UTF16-LE)
            uint8_t filenameLen;
            // 填充
            uint8_t padding;
        } fileInfo;
#pragma pack(pop)
        // 名字空间
        std::wstring filename;

        TypeData_FILE_NAME() = default;
        TypeData_FILE_NAME(TypeData_FILE_NAME const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        };
        TypeData_FILE_NAME(NtfsDataBlock const &data)
            : NtfsStructureBase(true) {
            if (data.len() < sizeof(fileInfo)) {
                Reset();
                return;
            }
            errno = memcpy_s(&fileInfo, sizeof(fileInfo), &data[0],
                             sizeof(fileInfo));
            if (errno || sizeof(fileInfo) + fileInfo.filenameLen > data.len()) {
                Reset();
                return;
            }
            filename.assign((wchar_t *)&data[sizeof(fileInfo)],
                            fileInfo.filenameLen);
        }

    protected:
        virtual TypeData_FILE_NAME &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->fileInfo = rr.fileInfo;
            this->filename = rr.filename;
            return *this;
        }
        virtual TypeData_FILE_NAME &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->fileInfo = rr.fileInfo;
            this->filename = rr.filename;
            return *this;
        }
    };

    struct TypeData_DATA : NtfsStructureBase {
        std::vector<SuccessiveSectors> sectors;
        NtfsDataBlock residentData;
        uint64_t VCN_beg, VCN_end, dataSize;
        bool isResident;

        TypeData_DATA() = default;
        TypeData_DATA(TypeData_DATA const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        TypeData_DATA &operator=(TypeData_DATA const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }
        TypeData_DATA(NtfsAttr const &attr, NtfsDataBlock const &data,
                      bool isResident);

        uint64_t fGetDataSize() const { return dataSize; }

    protected:
        virtual TypeData_DATA &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->sectors = rr.sectors;
            this->residentData = rr.residentData;
            this->isResident = rr.isResident;
            this->VCN_beg = rr.VCN_beg;
            this->VCN_end = rr.VCN_end;
            this->dataSize = rr.dataSize;
            return *this;
        }
        virtual TypeData_DATA &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->sectors = std::move(rr.sectors);
            this->residentData = rr.residentData;
            this->isResident = rr.isResident;
            this->VCN_beg = rr.VCN_beg;
            this->VCN_end = rr.VCN_end;
            this->dataSize = rr.dataSize;
            return *this;
        }
    };

    struct TypeData_INDEX_ROOT : NtfsStructureBase {
#pragma pack(push, 1)
        struct {
            // 索引的属性类型, 即对什么类型的属性进行索引. 名为 "$I30" 的索引对
            // $FILE_NAME 属性进行索引.
            uint32_t attrType;
            // 排序规则, 表示是以什么顺序进行索引排序的.
            uint32_t collationRule;
            // 索引分配项(Index Allocation Entry) 的大小 (bytes), 应该是用于
            // $INDEX_ALLOCATION.
            uint32_t sizeofIAE;
            // 每个 索引记录(Index Record) 的簇大小.
            uint8_t clusterPerIR;
            // 用于 8 字节对齐
            uint8_t padding[3];
        } indexRoot;
#pragma pack(pop)

#pragma pack(push, 1)
        struct {
            // 到第一个 索引项(Index Entry) 的偏移.
            uint32_t offToFirstIE;
            // 之后一系列 索引项 的总大小 + 此头部的大小
            uint32_t totalSizeOfIEs;
            // 之后一系列 索引项 的分配大小, 包含此头部的大小
            uint32_t allocatedSizeOfIEs;
            // 标志. 0x00 表示小索引(索引内容只在 $INDEX_ROOT); 0x01
            // 表示大索引(额外索引内容存放在 $INDEX_ALLOCATION).
            uint8_t flags;
            // 填充以 8 字节对齐.
            uint8_t padding[3];
        } indexHeader;
#pragma pack(pop)

        // 索引项 数据
        NtfsDataBlock indexEntries;

        TypeData_INDEX_ROOT() = default;
        TypeData_INDEX_ROOT(NtfsDataBlock const &data)
            : NtfsStructureBase(true) {
            if (data.len() < sizeof(indexRoot) + sizeof(indexHeader)) {
                Reset();
                return;
            }
            errno = memcpy_s(&indexRoot, sizeof(indexRoot), &data[0],
                             sizeof(indexRoot));
            if (errno) {
                Reset();
                return;
            }
            NtfsDataBlock recordData{data, sizeof(indexRoot)};
            errno = memcpy_s(&indexHeader, sizeof(indexHeader), &recordData[0],
                             sizeof(indexHeader));
            if (errno || indexHeader.allocatedSizeOfIEs > recordData.len()) {
                Reset();
                return;
            }
            indexEntries = NtfsDataBlock{recordData, indexHeader.offToFirstIE};
        }

    protected:
        virtual TypeData_INDEX_ROOT &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->indexRoot = rr.indexRoot;
            this->indexHeader = rr.indexHeader;
            this->indexEntries = rr.indexEntries;
            return *this;
        }
        virtual TypeData_INDEX_ROOT &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->indexRoot = rr.indexRoot;
            this->indexHeader = rr.indexHeader;
            this->indexEntries = rr.indexEntries;
            return *this;
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

    class NtfsAttr : public NtfsStructureBase {
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
            // 标志, 为 0x01 表示此属性被索引
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

        struct TypeData : public TypeData_STANDARD_INFOMATION,
                          public TypeData_FILE_NAME,
                          public TypeData_DATA,
                          public TypeData_INDEX_ROOT {
            std::shared_ptr<NtfsAttr::FixedFields> attrHeader;
            std::wstring attrName;
            NtfsDataBlock rawData;

            TypeData() = default;
            // TypeData(TypeData const &r) = default;
            // TypeData &operator=(TypeData const &r) = default;
            // TypeData(TypeData &&r) = default;
            // TypeData &operator=(TypeData &&r) = default;
            TypeData(NtfsAttr *pAttr, NtfsDataBlock &attrData) {
                attrHeader = pAttr->fields;
                attrName = pAttr->attrName;
                this->rawData = attrData;
                switch (attrHeader.get()->attrType) {
                case NTFS_STANDARD_INFOMATION:
                    (dynamic_cast<TypeData_STANDARD_INFOMATION &>(*this)) =
                        attrData;
                    break;
                case NTFS_FILE_NAME:
                    (dynamic_cast<TypeData_FILE_NAME &>(*this)) = attrData;
                    break;
                case NTFS_DATA:
                    (dynamic_cast<TypeData_DATA &>(*this)) = {
                        *pAttr, attrData, !attrHeader.get()->nonResident};
                case NTFS_INDEX_ROOT:
                    (dynamic_cast<TypeData_INDEX_ROOT &>(*this)) = attrData;

                default:
                    break;
                }
            }

            uint64_t len() const { return rawData.len(); }
            operator typename NtfsDataBlock const &() const { return rawData; }
        };

    public:
        std::shared_ptr<FixedFields> fields;
        std::wstring attrName;
        // 如果是 "非驻留" 属性, 则储存 data runs
        TypeData attrData;

        NtfsAttr() = default;
        NtfsAttr(NtfsAttr const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        };

        NtfsAttr(NtfsDataBlock const &data);

    protected:
        virtual NtfsAttr &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->fields = rr.fields;
            this->attrName = rr.attrName;
            this->attrData = rr.attrData;
            return *this;
        }
        virtual NtfsAttr &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->fields = std::move(rr.fields);
            this->attrName = std::move(rr.attrName);
            this->attrData = std::move(rr.attrData);
            return *this;
        }
    };

    class Ntfs_FILE_Record : public NtfsStructureBase {
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
        Ntfs_FILE_Record(Ntfs_FILE_Record const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        };

        Ntfs_FILE_Record(Ntfs_FILE_Record &&r) {
            static_cast<NtfsStructureBase &>(*this) = std::move(r);
        };

        Ntfs_FILE_Record &operator=(Ntfs_FILE_Record &&r) noexcept {
            static_cast<NtfsStructureBase &>(*this) = std::move(r);
            return *this;
        }

        Ntfs_FILE_Record(NtfsDataBlock const &data);
        ~Ntfs_FILE_Record() {}

        std::wstring GetFileName() {
            std::wstring filename;
            for (auto const &p : attrs) {
                if (p.fields.get()->attrType == tamper::NTFS_FILE_NAME) {
                    tamper::TypeData_FILE_NAME fn = (NtfsDataBlock)p.attrData;
                    filename = fn.filename;
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

    protected:
        virtual Ntfs_FILE_Record &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->fixedFields = rr.fixedFields;
            this->USN = rr.USN;
            this->USA = rr.USA;
            this->attrs = rr.attrs;
            return *this;
        }
        virtual Ntfs_FILE_Record &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->fixedFields = rr.fixedFields;
            this->USN = rr.USN;
            this->USA = std::move(rr.USA);
            this->attrs = std::move(rr.attrs);
            return *this;
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
            while (pos + sizeof(Ntfs_FILE_Record::fixedFields) < data.len()) {
                t = NtfsDataBlock{data, pos, data.len() - pos};
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
        static uint64_t ParseDataRuns(
            NtfsDataBlock const &data,
            std::function<bool(uint64_t lcn, uint64_t num)> callback) {
            uint64_t pos = 0;
            uint64_t LCN = 0;
            struct {
                uint8_t sizeOfLengthField : 4;
                uint8_t sizeOfOffsetField : 4;
            } fieldsSize;
            uint64_t clusterNum = 0;
            int64_t offsetOfLCN = 0;
            while (pos < data.len()) {
                if (data[pos] == 0) {
                    pos++;
                    break;
                }
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
                if (!callback(LCN, clusterNum)) {
                    break;
                }
            }
            return pos;
        }

        static std::vector<char> GetDataRuns(NtfsDataBlock const &data,
                                             uint64_t VC_len) {
            auto callback = [&](uint64_t lcn, uint64_t num) -> bool {
                if (VC_len >= num) {
                    VC_len -= num;
                    return true;
                }
                return false;
            };
            uint64_t dataRunsLen = ParseDataRuns(data, callback);
            std::vector<char> ret;
            ret.assign(&data[0], &data[dataRunsLen]);
            return ret;
        }
    };

    class Ntfs : public DiskReader, public NtfsStructureBase {
    public:
        NtfsBoot bootInfo;
        Ntfs_FILE_Record MFT_FileRecord;
        // 单位: 字节
        uint32_t FileRecordSize;
        // MFT 表中文件记录的数量
        uint32_t FileRecordsCount;

        NtfsDataBlock ReadSectors(std::vector<SuccessiveSectors> &secs) {
            return {DiskReader::ReadSectors(secs), this};
        }

        Ntfs() = default;
        Ntfs(Ntfs const &r) = delete;
        Ntfs &operator=(Ntfs const &r) = delete;
        Ntfs(Ntfs &&r) {
            static_cast<NtfsStructureBase &>(*this) = std::move(r);
        }

        // Ntfs(Ntfs &&r)
        //     : DiskReader((DiskReader &&) r), MFT_FileRecord{
        //                                          std::move(r.MFT_FileRecord)}
        //                                          {
        //     bootInfo = r.bootInfo;
        //     FileRecordSize = r.FileRecordSize;
        //     FileRecordsCount = r.FileRecordsCount;
        // }
        // Ntfs &operator=(Ntfs &&r) = delete;
        Ntfs(std::string file) : DiskReader(file), NtfsStructureBase(true) {
            try {
                bootInfo = ReadSector(0);
                MFT_FileRecord = std::move(NtfsDataBlock{
                    DiskReader::ReadSectors(bootInfo.LCNoVCN0oMFT *
                                                bootInfo.sectorsPerCluster,
                                            bootInfo.sectorsPerCluster),
                    this});
                FileRecordSize = MFT_FileRecord.fixedFields.allocatedSize;
                NtfsAttr &dataAttr = MFT_FileRecord.GetDataAttr();
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

        std::vector<SuccessiveSectors>
        DataRunsToSectorsInfo(NtfsAttr const &attr,
                              NtfsDataBlock const &dataRuns) {
            // 判断是否是 "非驻留" 属性, 如果不是则抛出异常, 只有 "非驻留"
            // 属性才有 data runs.
            if (!attr.fields.get()->nonResident) {
                throw std::runtime_error{"Non-Resident Attribute required!"};
            }
            NtfsAttr::NonResidentPart &nonResidentPart =
                (NtfsAttr::NonResidentPart &)*attr.fields.get();
            std::vector<SuccessiveSectors> ret;
            uint64_t VC_len =
                nonResidentPart.VCN_end - nonResidentPart.VCN_beg + 1;
            auto callback = [&](uint64_t lcn, uint64_t num) -> bool {
                if (VC_len >= num) {
                    VC_len -= num;
                    ret.emplace_back(
                        SuccessiveSectors{lcn * bootInfo.sectorsPerCluster,
                                          num * bootInfo.sectorsPerCluster});
                    return true;
                }
                return false;
            };
            uint64_t dataRunsLen =
                NtfsDataRuns::ParseDataRuns(dataRuns, callback);
            if (VC_len) {
                throw std::runtime_error("parse data runs failure!");
            }
            return ret;
        }

        // 虚拟扇区号转换到逻辑扇区号
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
            uint32_t secNum = FileRecordSize / bootInfo.bytesPerSector;
            uint32_t vsn = index * secNum;
            std::vector<SuccessiveSectors> availableArea =
                MFT_FileRecord.GetDataAttr().attrData.sectors;
            std::vector<SuccessiveSectors> requiredArea =
                VSN_To_LSN(availableArea, vsn, secNum);
            return requiredArea;
        }

        NtfsDataBlock GetFileRecordByIndex(uint32_t index) {
            return ReadSectors(GetFileRecordAreaByIndex(index));
        }

        uint64_t GetDataRunsDataSize(NtfsAttr const &attr,
                                     NtfsDataBlock &dataRuns) {
            uint64_t sectorsTotalSize = 0;
            std::vector<SuccessiveSectors> area =
                DataRunsToSectorsInfo(attr, dataRuns);
            for (auto &i : area) {
                sectorsTotalSize += i.secNum * GetSectorSize();
            }
            return sectorsTotalSize;
        }

    protected:
        virtual Ntfs &Copy(NtfsStructureBase const &r) override {
            return *this;
        }
        virtual Ntfs &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            new (this) DiskReader(std::move((DiskReader &&) rr));
            this->bootInfo = rr.bootInfo;
            this->MFT_FileRecord = std::move(rr.MFT_FileRecord);
            this->FileRecordsCount = rr.FileRecordsCount;
            this->FileRecordSize = rr.FileRecordSize;
            return *this;
        }
    };
}

// NtfsAttr 定义
namespace tamper {
    NtfsAttr::NtfsAttr(NtfsDataBlock const &data)
        : attrData{}, NtfsStructureBase{true} {
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
            attrData = TypeData{
                this,
                NtfsDataBlock{typename std::vector<char>(
                                  &data[residentPart.offToAttr],
                                  &data[(uint64_t)residentPart.offToAttr] +
                                      residentPart.attrLen),
                              data.pNtfs}};
            // 计算驻留部分属性长度, 因为字段中的长度可能是错的.
            residentPartLength = residentPart.offToAttr + residentPart.attrLen;
        }
        // 长度字段为 8 的倍数.
        residentPartLength = 0x8u * ((residentPartLength + 0x07u) / 0x8u);
        // 判断属性中的原始长度字段是否错误, 错误则进行更正.
        if (fields.get()->length > data.len()) {
            fields.get()->length = residentPartLength;
        }
    }

}

// Ntfs_FILE_Record 定义
namespace tamper {
    Ntfs_FILE_Record::Ntfs_FILE_Record(NtfsDataBlock const &data)
        : NtfsStructureBase(true) {
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
        NtfsAttr t;
        uint64_t pos = 0;
        while (pos + sizeof(NtfsAttr::FixedFields) < attrsData.len()) {
            // std::cout << "attr load beg" << std::endl;
            t = NtfsDataBlock{attrsData, pos};
            pos += t.fields.get()->length;
            if (t.fields.get()->length == 0 || !t.valid) {
                break;
            }
            attrs.push_back(t);
        }
        // std::cout << "attrs load end" << std::endl;
    }
}

// TypeData_DATA 定义
namespace tamper {
    TypeData_DATA::TypeData_DATA(NtfsAttr const &attr,
                                 NtfsDataBlock const &data, bool isResident)
        : NtfsStructureBase(true), VCN_beg(0), VCN_end(0) {
        residentData = data;
        this->isResident = isResident;
        dataSize = residentData.len();
        if (!isResident) {
            NtfsAttr::NonResidentPart &nonRD =
                (NtfsAttr::NonResidentPart &)*attr.fields.get();
            sectors = data.pNtfs->DataRunsToSectorsInfo(attr, data);
            VCN_beg = nonRD.VCN_beg;
            VCN_end = nonRD.VCN_end;
            dataSize = nonRD.realSize;
        }
    }
}
