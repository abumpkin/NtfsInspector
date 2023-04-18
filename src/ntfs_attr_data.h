#pragma once
#include "ntfs_access.hpp"

namespace abkntfs {
    struct AttrData_STANDARD_INFOMATION : NtfsStructureBase {
        enum DOS_FILE_PERMISSIONS : uint32_t {
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
        struct Info {
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
        } info;
#pragma pack(pop)

        struct ExtraInfo {
            // 拥有者 ID (win2000)
            uint32_t ownerId = 0;
            // 安全 ID (win2000)
            uint32_t securityId = 0;
            // 文件的配额大小, 占用用户的空间配额.
            // 这里应该是所有流的总数据大小(单位: 字节). (win2000)
            uint64_t quotaCharged = 0;
            // 更新序列号(USN), 用于日志记录 (如果为 0, 则说明 USN Journal
            // 功能未开启).
            uint64_t USN = 0;
        } extraInfo;

        AttrData_STANDARD_INFOMATION() = default;
        AttrData_STANDARD_INFOMATION(AttrData_STANDARD_INFOMATION const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        AttrData_STANDARD_INFOMATION &
        operator=(AttrData_STANDARD_INFOMATION const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }
        AttrData_STANDARD_INFOMATION(NtfsDataBlock const &data)
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
            NtfsDataBlock remainingData = NtfsDataBlock{data, sizeof(info)};
            if (remainingData.len() >= sizeof(extraInfo)) {
                memcpy_s(&extraInfo, sizeof(extraInfo), &remainingData[0],
                         sizeof(extraInfo));
            }
        }

    protected:
        virtual AttrData_STANDARD_INFOMATION &
        Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->info = rr.info;
            this->extraInfo = rr.extraInfo;
            return *this;
        }
        virtual AttrData_STANDARD_INFOMATION &
        Move(NtfsStructureBase &r) override {
            return Copy(r);
        }
    };

    struct AttrData_ATTRIBUTE_LIST : NtfsStructureBase {
        struct NameLength {
            uint8_t lenInWords : 4;
            uint8_t unknown : 4;
        };

#pragma pack(push, 1)
        struct Info {
            NTFS_ATTRIBUTES_TYPE type;
            uint16_t length;
            NameLength nameLength;
            // 永远是 0x1A
            uint8_t offToName;
            // 属性起始 VCN, 如果为驻留属性则为 0.
            uint64_t startingVCN;
            // 属性所在 文件记录引用
            NtfsFileReference fileReference;
            uint16_t attrId;
        };
#pragma pack(pop)

        struct ListItem {
            Info info;
            std::wstring attrName;
        };

        std::vector<ListItem> list;

    public:
        AttrData_ATTRIBUTE_LIST() = default;
        AttrData_ATTRIBUTE_LIST(AttrData_ATTRIBUTE_LIST const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        AttrData_ATTRIBUTE_LIST &operator=(AttrData_ATTRIBUTE_LIST const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }

        AttrData_ATTRIBUTE_LIST(NtfsDataBlock &data) : NtfsStructureBase(true) {
            NtfsDataBlock remainingData = data;
            ListItem t;
            if (sizeof(Info) > remainingData.len()) {
                Reset();
                return;
            }
            do {
                memcpy(&t.info, remainingData, sizeof(Info));
                if (t.info.length > remainingData.len()) {
                    Reset();
                    return;
                }
                if (t.info.nameLength.lenInWords) {
                    t.attrName.resize(t.info.nameLength.lenInWords);
                    memcpy(&t.attrName[0], remainingData + t.info.offToName,
                           (uint64_t)t.info.nameLength.lenInWords << 1);
                }
                list.push_back(t);
                remainingData = NtfsDataBlock{remainingData, t.info.length};
            } while (remainingData.len() >= sizeof(Info));
        }

    protected:
        virtual AttrData_ATTRIBUTE_LIST &
        Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->list = rr.list;
            return *this;
        }
        virtual AttrData_ATTRIBUTE_LIST &Move(NtfsStructureBase &r) override {
            return Copy(r);
        }
    };

    struct AttrData_FILE_NAME : NtfsStructureBase {
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
        struct FileInfo {
            // 到父级目录的文件引用
            NtfsFileReference fileRef;
            // 文件创建时间
            uint64_t cTime;
            // 文件更改时间
            uint64_t aTime;
            // MFT 文件记录 更改时间
            uint64_t mTime;
            // 文件读取时间
            uint64_t rTime;
            // 文件分配大小 (只在 $I30 索引中此值有效,
            // 要通过文件记录获取真实大小需使用 NtfsAttr 的 GetDataSize())
            uint64_t allocSizeOfFile;
            // 文件实际大小 (只在 $I30 索引中此值有效,
            // 要通过文件记录获取真实大小需使用 NtfsAttr 的 GetDataSize())
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

    public:
        AttrData_FILE_NAME() = default;
        AttrData_FILE_NAME(AttrData_FILE_NAME const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        AttrData_FILE_NAME &operator=(AttrData_FILE_NAME const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }
        AttrData_FILE_NAME(NtfsDataBlock const &data)
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
        virtual AttrData_FILE_NAME &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->fileInfo = rr.fileInfo;
            this->filename = rr.filename;
            return *this;
        }
        virtual AttrData_FILE_NAME &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->fileInfo = rr.fileInfo;
            this->filename = rr.filename;
            return *this;
        }
    };

    struct AttrData_DATA : NtfsStructureBase {
    private:
        Ntfs *pNtfs;
        NtfsDataBlock residentData;
        // 数据的真实大小
        uint64_t dataSize;
        // 数据占用扇区数
        uint64_t dataSectorsCount;
        bool isResident;

    public:
        NtfsSectorsInfo dataRunsMap;
        uint64_t VCN_beg, VCN_end;

    public:
        AttrData_DATA() = default;
        AttrData_DATA(AttrData_DATA const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        AttrData_DATA &operator=(AttrData_DATA const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }
        AttrData_DATA(NtfsAttr const &attr, NtfsDataBlock const &data,
                      bool isResident);

        uint64_t GetDataSize() const { return dataSize; }

        NtfsDataBlock ReadData(uint64_t offset, uint64_t size);

    protected:
        virtual AttrData_DATA &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->pNtfs = rr.pNtfs;
            this->dataRunsMap = rr.dataRunsMap;
            this->residentData = rr.residentData;
            this->isResident = rr.isResident;
            this->VCN_beg = rr.VCN_beg;
            this->VCN_end = rr.VCN_end;
            this->dataSize = rr.dataSize;
            this->dataSectorsCount = rr.dataSectorsCount;
            return *this;
        }
        virtual AttrData_DATA &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->pNtfs = rr.pNtfs;
            this->dataRunsMap = std::move(rr.dataRunsMap);
            this->residentData = rr.residentData;
            this->isResident = rr.isResident;
            this->VCN_beg = rr.VCN_beg;
            this->VCN_end = rr.VCN_end;
            this->dataSize = rr.dataSize;
            this->dataSectorsCount = rr.dataSectorsCount;
            return *this;
        }
    };

    struct AttrData_INDEX_ROOT : NtfsStructureBase {
        // 排序规则
        enum COLLATION_RULES : uint32_t {
            // 根据每个字节排序
            COLLATION_RULE_BINARY = 0x00,
            // Unicode
            COLLATION_RULE_FILENAME = 0x01,
            // Unicode, 大写字母先出现
            COLLATION_RULE_UNICODE = 0x02,
            // 被 $Secure 的 $SII 属性使用
            COLLATION_RULE_ULONG = 0x10,
            // 被 $Extend/$Quota 中的 $O 属性使用
            COLLATION_RULE_SID = 0x11,
            // 被 $Secure 中的 $SDH 属性使用
            COLLATION_RULE_SECURITY_HASH = 0x12,
            // 比较一组 ULONG, 被 $Extend/$ObjId 中的 $O 属性使用
            COLLATION_RULE_ULONGS = 0x013,
        };

#pragma pack(push, 1)
        struct IndexRootInfo {
            // 索引的属性类型, 即对什么类型的属性进行索引. 名为 "$I30" 的索引对
            // $FILE_NAME 属性进行索引.
            NTFS_ATTRIBUTES_TYPE attrType;
            // 排序规则, 表示是以什么顺序进行索引排序的.
            COLLATION_RULES collationRule;
            // 索引块(Index Block) 的大小 (bytes), 用于 $INDEX_ALLOCATION.
            // 索引块 又叫做 索引分配项(Index Allocation Entry).
            uint32_t sizeofIB;
            // 每个 索引块 的簇大小, 必须为 2 的次幂.
            uint8_t clusterPerIB;
            // 用于 8 字节对齐
            uint8_t padding[3];
        } rootInfo;
#pragma pack(pop)

#pragma pack(push, 1)
        struct IndexRootHeader {
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
        };
#pragma pack(pop)

        // IndexRootHeader indexHeader;
        // 索引项 数据
        // std::vector<NtfsIndexEntry> indexEntries;

        NtfsIndexNode rootNode;

    public:
        AttrData_INDEX_ROOT() = default;
        AttrData_INDEX_ROOT(AttrData_INDEX_ROOT const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        AttrData_INDEX_ROOT &operator=(AttrData_INDEX_ROOT const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }

        AttrData_INDEX_ROOT(NtfsDataBlock const &data)
            : NtfsStructureBase(true) {
            if (data.len() < sizeof(rootInfo)) {
                Reset();
                return;
            }
            errno = memcpy_s(&rootInfo, sizeof(rootInfo), &data[0],
                             sizeof(rootInfo));
            if (errno) {
                Reset();
                return;
            }
            NtfsDataBlock nodeData{data, sizeof(rootInfo)};
            rootNode = NtfsIndexNode(nodeData, rootInfo.attrType);
            // errno = memcpy_s(&indexHeader, sizeof(indexHeader),
            // &recordData[0],
            //                  sizeof(indexHeader));
            // if (errno || indexHeader.allocatedSizeOfIEs > recordData.len()) {
            //     Reset();
            //     return;
            // }
            // recordData = NtfsDataBlock{recordData, sizeof(indexHeader)};
            // uint64_t pos = 0;
            // while (pos + sizeof(NtfsIndexEntry::entryHeader) <
            //        recordData.len()) {
            //     NtfsIndexEntry t = {NtfsDataBlock{recordData, pos},
            //                         rootInfo.attrType};
            //     if (!t.valid) {
            //         return;
            //     }
            //     pos += t.entryHeader.lengthOfIE;
            //     indexEntries.push_back(t);
            // }
        }

    protected:
        virtual AttrData_INDEX_ROOT &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->rootInfo = rr.rootInfo;
            this->rootNode = rr.rootNode;
            // this->indexHeader = rr.indexHeader;
            // this->indexEntries = rr.indexEntries;
            return *this;
        }
        virtual AttrData_INDEX_ROOT &Move(NtfsStructureBase &r) override {
            return Copy(r);
        }
    };

    struct AttrData_INDEX_ALLOCATION : NtfsStructureBase {
    private:
        // NtfsDataBlock rawIRsData;
        std::vector<NtfsIndexRecord> IRs;
        bool loaded = false;
        NtfsSectorsInfo secs;
        Ntfs *pNtfs = nullptr;
        AttrData_INDEX_ROOT *indexRoot = nullptr;

    public:
        AttrData_INDEX_ALLOCATION() = default;
        AttrData_INDEX_ALLOCATION(AttrData_INDEX_ALLOCATION &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        AttrData_INDEX_ALLOCATION &
        operator=(AttrData_INDEX_ALLOCATION const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }

        AttrData_INDEX_ALLOCATION(NtfsDataBlock &dataRuns,
                                  AttrData_INDEX_ROOT &indexRoot);

        std::vector<NtfsIndexRecord> &GetIRs();

    protected:
        virtual AttrData_INDEX_ALLOCATION &
        Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            // this->rawIRsData = rr.rawIRsData;
            this->IRs = rr.IRs;
            this->loaded = rr.loaded;
            this->secs = rr.secs;
            this->pNtfs = rr.pNtfs;
            this->indexRoot = rr.indexRoot;
            return *this;
        }
        virtual AttrData_INDEX_ALLOCATION &Move(NtfsStructureBase &r) override {
            return Copy(r);
        }
    };

    struct TypeData_BITMAP : NtfsStructureBase {
        enum BITMAP_UNIT {
            UNIT_CLUSTER,
            UNIT_RECORD,
        };
        // 每一位代表什么
        BITMAP_UNIT unit;
        NtfsDataBlock bitmap;

    public:
        TypeData_BITMAP() = default;
        TypeData_BITMAP(TypeData_BITMAP const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        TypeData_BITMAP &operator=(TypeData_BITMAP const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }

        TypeData_BITMAP(NtfsDataBlock &data, BITMAP_UNIT unit) {
            if (!data.len()) {
                Reset();
                return;
            }
            bitmap = data;
            this->unit = unit;
        }

        bool CheckPos(uint64_t pos) {
            uint64_t offInBytes = pos / 8;
            uint64_t offInBits = pos - offInBytes * 8;
            if (offInBytes > bitmap.len()) {
                throw std::runtime_error("offset outbound.");
            }
            uint8_t byte = bitmap[offInBytes];
            return byte & (1 << offInBits);
        }

        // TODO: 待优化; 没有返回 (uint64_t) -1
        uint64_t FindFreeUnitPos() {
            uint64_t offInBytes = 0;
            uint64_t offInBits = 0;
            uint8_t byte = 0xFF;
            const uint64_t NOT_FOUND = (uint64_t)-1;
            while (offInBytes < bitmap.len()) {
                byte = bitmap[offInBytes];
                if (byte == 0xFF) {
                    offInBytes++;
                    continue;
                }
                break;
            }
            for (uint8_t mask = 0x01; mask; mask <<= 1) {
                if (byte & mask) {
                    offInBits++;
                    continue;
                }
                break;
            }
            if (offInBits >= 8) {
                return NOT_FOUND;
            }
            return offInBytes * 8 + offInBits;
        }

    protected:
        virtual TypeData_BITMAP &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->bitmap = rr.bitmap;
            this->unit = rr.unit;
            return *this;
        }
        virtual TypeData_BITMAP &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            return Copy(rr);
        }
    };
}