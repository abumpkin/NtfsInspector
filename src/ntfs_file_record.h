#pragma once
#include "ntfs_access.hpp"

namespace abkntfs {
    class NtfsFileRecord : public NtfsStructureBase {
    public:
        enum FLAGS : uint16_t {
            // 如果未设此标志说明此记录未被使用.
            FILE_RECORD_IN_USE = 0x01,
            FILE_RECORD_IS_DIRECTORY = 0x02,
        };
#pragma pack(push, 1)
        struct RecordHeader {
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
            NtfsFileReference fileReference;
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
        std::vector<std::shared_ptr<NtfsAttr>> attrs;
        // 这个文件记录的编号, 未知则为 (uint64_t)-1
        uint64_t FRN = (uint64_t)-1;

        NtfsFileRecord() = default;
        NtfsFileRecord(NtfsFileRecord const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        };

        NtfsFileRecord(NtfsFileRecord &&r) {
            static_cast<NtfsStructureBase &>(*this) = std::move(r);
        };

        NtfsFileRecord &operator=(NtfsFileRecord &&r) noexcept {
            static_cast<NtfsStructureBase &>(*this) = std::move(r);
            return *this;
        }

        NtfsFileRecord(NtfsDataBlock const &data, uint64_t FRN);
        ~NtfsFileRecord() {}

        std::wstring GetFileName() {
            std::wstring filename;
            AttrData_FILE_NAME *pFn = this->FindSpecAttrData(NTFS_FILE_NAME);
            if (nullptr == pFn) {
                return filename;
            }
            return pFn->filename;
        }

        // 失败返回 nullptr.
        NtfsAttr::TypeData *FindSpecAttrData(NTFS_ATTRIBUTES_TYPE const type,
                                             std::wstring attrName = L"*",
                                             std::wstring fileName = L"*") {
            if (attrs.empty()) {
                return nullptr;
            }
            NtfsAttr *cur = attrs.back().get();
            return cur->FindSpecAttrData(type, attrName, fileName);
        }

        // 失败返回 nullptr
        NtfsAttr::TypeData *GetSpecAttrData(uint32_t attrId) {
            if (attrs.empty()) {
                return nullptr;
            }
            NtfsAttr *cur = attrs.back().get();
            return cur->GetSpecAttrData(attrId);
        }

        // 失败返回 nullptr.
        NtfsAttr *FindSpecAttr(NTFS_ATTRIBUTES_TYPE const type,
                               std::wstring attrName = L"*",
                               std::wstring fileName = L"*") {
            for (auto &p : attrs) {
                if (p.get()->fields.get()->attrType == type) {
                    if (attrName != L"*") {
                        if (p.get()->attrName != attrName) {
                            continue;
                        }
                    }
                    if (fileName != L"*") {
                        if (static_cast<AttrData_FILE_NAME &>(p.get()->attrData)
                                .valid) {
                            if (static_cast<AttrData_FILE_NAME &>(
                                    p.get()->attrData)
                                    .filename != fileName) {
                                continue;
                            }
                        }
                    }
                    return p.get();
                }
            }
            return nullptr;
        }

        // 失败返回 nullptr.
        NtfsAttr *GetSpecAttr(uint32_t attrId) {
            for (auto &p : attrs) {
                if (p.get()->fields.get()->attrId == attrId) {
                    return p.get();
                }
            }
            return nullptr;
        }

    protected:
        virtual NtfsFileRecord &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->fixedFields = rr.fixedFields;
            this->USN = rr.USN;
            this->USA = rr.USA;
            this->attrs = rr.attrs;
            this->FRN = rr.FRN;
            return *this;
        }
        virtual NtfsFileRecord &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->fixedFields = rr.fixedFields;
            this->USN = rr.USN;
            this->USA = std::move(rr.USA);
            this->attrs = std::move(rr.attrs);
            this->FRN = rr.FRN;
            return *this;
        }
    };
}