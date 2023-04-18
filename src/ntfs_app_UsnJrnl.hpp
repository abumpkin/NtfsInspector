#pragma once
#include "ntfs_access.hpp"
#include "ntfs_app_FileNameIndex.hpp"

namespace abkntfs {

    // $UsnJrnl 解析
    struct NtfsUsnJrnl : NtfsStructureBase {
        enum REASON_FLAGS : uint32_t {
            // 无名数据流被重写
            UNNAMED_DATA_STREAM_WAS_OVERWRITTEN = 0x01,
            // 无名数据流被添加
            UNNAMED_DATA_STREAM_WAS_ADDED = 0x02,
            // 无名数据流被从头重写
            UNNAMED_DATA_STREAM_WAS_TRUNCATED = 0x04,
            // 具名数据流被重写
            NAMED_DATA_STREAM_WAS_OVERWRITTEN = 0x10,
            // 具名数据流被添加
            NAMED_DATA_STREAM_WAS_ADDED = 0x20,
            // 具名数据流被从头重写
            NAMED_DATA_STREAM_WAS_TRUNCATED = 0x40,
            // 文件或文件夹被创建
            FILE_OR_DIRECTORY_WAS_CREATED = 0x100,
            // 文件或文件夹被删除
            FILE_OR_DIRECTORY_WAS_DELETED = 0x200,
            // 文件或文件夹的拓展属性被修改; 这些属性对 windows
            // 应用程序不可访问.
            FILE_EXTENDED_ATTR_CHANGED = 0x400,
            // 文件或文件夹的访问权限被修改
            ACCESS_RIGHTS_CHANGED = 0x800,
            // 文件或文件夹被重命名, JEntryFixed 里的文件名是其原名.
            FILE_OR_DIRECTORY_RENAMED_OLD = 0x1000,
            // 文件或文件夹被重命名, JEntryFixed 里的文件名是其新名.
            FILE_OR_DIRECTORY_RENAMED_NEW = 0x2000,
            // 用户改变了 FILE_ATTRIBUTE_NOT_CONTENT_INDEXED 属性
            NOT_CONTENT_INDEXED_ATTR_CHANGED = 0x4000,
            // 用户改变了文件属性或时间戳.
            FILE_ATTR_CHANGED = 0x8000,
            // 文件(文件夹)被添加或删除硬链接
            FILE_HARD_LINK_CHANGED = 0x10000,
            // 文件(文件夹)压缩状态被改变
            FILE_COMPRESSED_CHANGED = 0x20000,
            // 文件(文件夹)加密状态被改变
            FILE_ENCRYPTED_OR_DECRYPTED = 0x40000,
            // 文件(文件夹)的对象 ID 被改变.
            FILE_OBJID_CHANGED = 0x80000,
            // 文件(文件夹)包含的重解析点(reparse point)被修改.
            REPARSE_POINT_CHANGED = 0x100000,
            // 文件添加了或移除了一个具名流(Named Stream).
            FILE_NAMED_STREAM_CHANGED = 0x200000,
            // 文件或文件夹被关闭.
            FILE_CLOSED = 0x80000000,
        };

        enum struct SOURCE_INFO : uint32_t {
            // 此操作提供由操作系统对文件或文件夹做出更改的信息.
            MADE_BY_OPERATING_SYSTEM = 0x01,
            // 此操作对文件或文件夹新增了一个私有数据流(Data Stream).
            A_PRIVATE_DATA_STREAM_ADDED = 0x02,
            // 对文件的副本进行的创建或更新内容的操作.
            REPLICATION_FILE_OPERATION = 0x04
        };

        // $UsnJrnl:$Max
        struct Max {
            // J 记录最大物理大小
            uint64_t maximumSize;
            // 每次新分配的大小增量
            uint64_t allocationDelta;
            // USN ID 跟 USN 没关系, USN ID 是卷中不变的属性, 在 USN journal 2.0
            // 中此值是个 Ntfs 时间
            uint64_t USN_ID;
            // 最低有效 USN
            uint64_t lowestValidUSN;
        };

#pragma pack(push, 1)
        struct JEntryFixed {
            // 总大小 8 字节对齐
            uint32_t sizeOfEntry;
            // 主版本号
            uint16_t majorV;
            // 次版本号
            uint16_t minorV;
            // 到文件记录的引用
            NtfsFileReference fileRef;
            // 到父文件记录的引用
            NtfsFileReference parentFileRef;
            // 此条目在 $J 中的偏移 (当作 USN?)
            uint64_t offInJ;
            // 时间
            uint64_t time;
            // 原因
            uint32_t reason;
            // 源信息
            uint32_t sourceInfo;
            // 安全 ID
            uint32_t securityID;
            // 文件属性
            uint32_t fileAttributes;
            // 文件名长度 (单位: 字节)
            uint16_t sizeOfFileName;
            // 到文件名的偏移
            uint16_t offToFileName;
        };
#pragma pack(pop)

        // $UsnJrnl:$J 条目
        struct JEntry : NtfsStructureBase {
            JEntryFixed fixed;
            std::wstring fileName;
            JEntry() = default;
            JEntry(JEntry const &r) {
                static_cast<NtfsStructureBase &>(*this) = r;
            }
            JEntry &operator=(JEntry const &r) {
                static_cast<NtfsStructureBase &>(*this) = r;
                return *this;
            }

            JEntry(NtfsDataBlock &data, uint64_t off)
                : NtfsStructureBase(true) {
                if (data.len() < sizeof(fixed)) {
                    Reset();
                    return;
                }
                memcpy(&fixed, data, sizeof(fixed));
                if (fixed.offInJ != off) {
                    Reset();
                    return;
                }
                // 总大小 8 字节对齐
                uint64_t expectedSize =
                    ((fixed.offToFileName + fixed.sizeOfFileName + 0x07) /
                     0x08) *
                    0x08;
                if (fixed.sizeOfEntry != expectedSize) {
                    Reset();
                    return;
                }
                fileName.assign((wchar_t *)(data + fixed.offToFileName),
                                fixed.sizeOfFileName >> 1);
            }

        protected:
            virtual JEntry &Copy(NtfsStructureBase const &r) override {
                using T = std::remove_reference<decltype(*this)>::type;
                T const &rr = (T const &)r;
                this->fixed = rr.fixed;
                this->fileName = rr.fileName;
                return *this;
            }
            virtual JEntry &Move(NtfsStructureBase &r) override {
                return Copy(r);
            }
        };

        Ntfs *pNtfs;
        NtfsFileReference usnJrnlFRN;

    private:
        Max GetMax(NtfsFileRecord &usnJrnl) {
            Max ret = Max{};
            if (!valid) {
                return ret;
            }
            // $UsnJrnl:Max
            NtfsAttr *pDataMax = usnJrnl.FindSpecAttr(NTFS_DATA, L"$Max");
            if (nullptr == pDataMax) {
                return ret;
            }
            memcpy(&ret, (NtfsDataBlock)pDataMax->attrData,
                   pDataMax->attrData.len());
            return ret;
        }

    public:
        NtfsUsnJrnl() = default;
        NtfsUsnJrnl(Ntfs &disk) : NtfsStructureBase(true) {
            if (!disk.valid) {
                Reset();
                return;
            }
            pNtfs = &disk;
            // $Extend
            NtfsFileRecord extend = disk.GetFileRecordByFRN(11);
            if (!extend.valid) {
                Reset();
                return;
            }
            // $UsnJrnl 信息
            NtfsFileNameIndex::FileInfoInIndex usnJrnlInfo =
                NtfsFileNameIndex{extend}.FindFile(L"$UsnJrnl");
            if (!usnJrnlInfo.valid) {
                Reset();
                return;
            }
            usnJrnlFRN = usnJrnlInfo.fileRef;
        }

        Max GetMax() {
            Max ret = Max{};
            if (!valid) {
                return ret;
            }
            // $UsnJrnl 文件记录
            NtfsFileRecord usnJrnl =
                pNtfs->GetFileRecordByFRN(usnJrnlFRN.fileRecordNum);
            return GetMax(usnJrnl);
        }

        std::vector<JEntry> GetLogs(uint64_t vcn) {
            std::vector<JEntry> ret;
            if (!valid) {
                return ret;
            }
            // $UsnJrnl 文件记录
            NtfsFileRecord usnJrnl =
                pNtfs->GetFileRecordByFRN(usnJrnlFRN.fileRecordNum);
            // $UsnJrnl:J
            AttrData_DATA *pDataJ = usnJrnl.FindSpecAttrData(NTFS_DATA, L"$J");
            if (nullptr == pDataJ) {
                return ret;
            }
            if (vcn > pDataJ->VCN_end || vcn < pDataJ->VCN_beg) {
                return ret;
            }
            // 读取大小
            uint64_t clusterSize =
                pNtfs->bootInfo.sectorsPerCluster * pNtfs->GetSectorSize();
            uint64_t off = vcn * clusterSize;
            NtfsDataBlock entriesData = pDataJ->ReadData(off, clusterSize);
            while (entriesData.len()) {
                JEntry t = JEntry{entriesData, off};
                if (!t.valid) {
                    break;
                }
                entriesData = NtfsDataBlock{entriesData, t.fixed.sizeOfEntry};
                off += t.fixed.sizeOfEntry;
                ret.push_back(t);
            }
            return ret;
        }

        std::vector<JEntry> GetLastN(uint64_t n) {
            std::vector<JEntry> ret;
            if (!valid) {
                return ret;
            }
            // $UsnJrnl 文件记录
            NtfsFileRecord usnJrnl =
                pNtfs->GetFileRecordByFRN(usnJrnlFRN.fileRecordNum);
            // $UsnJrnl:J
            NtfsAttr *pDataJ = usnJrnl.FindSpecAttr(NTFS_DATA, L"$J");
            if (nullptr == pDataJ) {
                return ret;
            }
            uint64_t clusterSize = pNtfs->bootInfo.sectorsPerCluster *
                                   pNtfs->bootInfo.bytesPerSector;
            uint64_t clusters =
                (pDataJ->GetDataSize() + (clusterSize - 1)) / clusterSize;
            if (clusters == 0) {
                return ret;
            }
            uint64_t num = 1;
            while (ret.size() < n) {
                auto logs = GetLogs(clusters - num);
                num++;
                if (n - ret.size() < logs.size()) {
                    ret.insert(
                        ret.begin(),
                        std::next(logs.begin(), logs.size() - (n - ret.size())),
                        logs.end());
                }
                else {
                    ret.insert(ret.begin(), logs.begin(), logs.end());
                }
            }
            return ret;
        }

    protected:
        virtual NtfsUsnJrnl &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;

            return *this;
        }
        virtual NtfsUsnJrnl &Move(NtfsStructureBase &r) override {
            return Copy(r);
        }
    };
}