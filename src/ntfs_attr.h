#pragma once
#include "ntfs_access.hpp"

namespace abkntfs {
    class NtfsAttr : public NtfsStructureBase {
    public:
        enum ATTR_FLAGS : uint16_t {
            ATTR_FLAG_COMPRESSED = 0x0001,
            ATTR_FLAG_ENCRYPTED = 0x4000,
            // 稀疏存储, 稀疏的簇以 0 填充.
            ATTR_FLAG_SPARSE = 0x8000,
        };
#pragma pack(push, 1)
        struct FixedFields {
            // 属性类型
            NTFS_ATTRIBUTES_TYPE attrType;
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
            // 分配给此 data runs 的大小
            uint64_t allocSize;
            // data runs 数据的真实大小
            uint64_t realSize;
            // 流(stream) 的初始状态数据大小
            uint64_t initializedDataSizeOfTheStream;
        };
#pragma pack(pop)

        struct TypeData : public AttrData_STANDARD_INFOMATION,
                          public AttrData_FILE_NAME,
                          public AttrData_DATA,
                          public AttrData_INDEX_ROOT,
                          public AttrData_INDEX_ALLOCATION,
                          public AttrData_ATTRIBUTE_LIST {
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
                    (dynamic_cast<AttrData_STANDARD_INFOMATION &>(*this)) =
                        attrData;
                    break;
                case NTFS_ATTRIBUTE_LIST:
                    (dynamic_cast<AttrData_ATTRIBUTE_LIST &>(*this)) = attrData;
                    break;
                case NTFS_FILE_NAME:
                    (dynamic_cast<AttrData_FILE_NAME &>(*this)) = attrData;
                    break;
                case NTFS_DATA:
                    (dynamic_cast<AttrData_DATA &>(*this)) = {
                        *pAttr, attrData, !attrHeader.get()->nonResident};
                    break;
                case NTFS_INDEX_ROOT:
                    (dynamic_cast<AttrData_INDEX_ROOT &>(*this)) = attrData;
                    break;
                case NTFS_INDEX_ALLOCATION:
                    TypeData *pAttrData =
                        pAttr->FindSpecAttrData(NTFS_INDEX_ROOT);
                    if (!pAttrData) break;
                    AttrData_INDEX_ROOT &indexRoot = *pAttrData;
                    if (!indexRoot.valid) break;
                    (dynamic_cast<AttrData_INDEX_ALLOCATION &>(*this)) = {
                        attrData, indexRoot};
                    break;
                }
            }

            // 获取驻留部分属性数据大小
            uint64_t len() const { return rawData.len(); }
            operator typename NtfsDataBlock const &() const { return rawData; }
        };

    public:
        std::shared_ptr<FixedFields> fields;
        std::wstring attrName;
        // 如果是 "非驻留" 属性, 则储存 data runs
        TypeData attrData;
        // 指向前一个属性
        std::shared_ptr<NtfsAttr> prevAttr = nullptr;
        // 来自哪个 文件记录号, (uint64_t)-1 表示未知.
        // 不一定可靠.
        uint64_t fileRecordFrom;
        // 原始数据拷贝, 包含整个属性的数据, 与 attrData 数据独立.
        NtfsDataBlock rawData;

        NtfsAttr() = default;
        NtfsAttr(NtfsAttr const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        };

        NtfsAttr(NtfsDataBlock const &data,
                 std::shared_ptr<NtfsAttr> previousAttr = nullptr,
                 uint64_t FRN = (uint64_t)-1);

        // 失败返回 nullptr.
        TypeData *FindSpecAttrData(NTFS_ATTRIBUTES_TYPE const type,
                                   std::wstring attrName = L"*",
                                   std::wstring fileName = L"*") {
            NtfsAttr *cur = this;
            while (nullptr != cur && cur->valid) {
                if (attrName != L"*" && cur->attrName != attrName) {
                    cur = cur->prevAttr.get();
                    continue;
                }
                if (type != cur->GetAttributeType()) {
                    cur = cur->prevAttr.get();
                    continue;
                }
                if (fileName != L"*") {
                    if (static_cast<AttrData_FILE_NAME &>(cur->attrData)
                            .valid) {
                        if (static_cast<AttrData_FILE_NAME &>(cur->attrData)
                                .filename != fileName) {
                            cur = cur->prevAttr.get();
                            continue;
                        }
                    }
                }
                return &cur->attrData;
            }
            return nullptr;
        }

        // 失败返回 nullptr
        TypeData *GetSpecAttrData(uint32_t attrId) {
            NtfsAttr *cur = this;
            while (nullptr != cur && cur->valid) {
                if (cur->fields.get()->attrId != attrId) {
                    cur = cur->prevAttr.get();
                    continue;
                }
                return &cur->attrData;
            }
            return nullptr;
        }

        NTFS_ATTRIBUTES_TYPE GetAttributeType() const {
            if (!valid) return NTFS_ATTRIBUTE_NONE;
            return fields.get()->attrType;
        }

        uint64_t GetAttributeLength() const {
            if (!valid) return false;
            return fields.get()->length;
        }

        bool IsResident() const {
            if (!valid) return false;
            return !fields.get()->nonResident;
        }

        uint64_t GetVirtualClusterCount() const {
            if (!IsResident() && valid) {
                auto &nr = static_cast<NonResidentPart &>(*fields.get());
                return nr.VCN_end - nr.VCN_beg + 1;
            }
            return 0;
        }

        // 获取属性数据的真实大小, data runs
        // 对应数据大小或驻留部分除通用属性头(ResidentPart,
        // NonResidentPart)外的大小
        uint64_t GetDataSize() {
            if (!valid) {
                return 0;
            }
            if (fields.get()->nonResident) {
                return static_cast<NonResidentPart *>(fields.get())->realSize;
            }
            return attrData.len();
        }

        // 如果是驻留属性则获取驻留数据, 如果是非驻留属性则读取 data runs
        // 对应的数据.
        NtfsDataBlock ReadAttrRawRealData(uint64_t offset, uint64_t size,
                                          Ntfs *pNtfs);

    protected:
        virtual NtfsAttr &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->fields = rr.fields;
            this->attrName = rr.attrName;
            this->attrData = rr.attrData;
            this->prevAttr = rr.prevAttr;
            this->fileRecordFrom = rr.fileRecordFrom;
            this->rawData = rr.rawData;
            return *this;
        }
        virtual NtfsAttr &Move(NtfsStructureBase &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T &rr = (T &)r;
            this->fields = std::move(rr.fields);
            this->attrName = std::move(rr.attrName);
            this->attrData = std::move(rr.attrData);
            this->prevAttr = std::move(rr.prevAttr);
            this->fileRecordFrom = rr.fileRecordFrom;
            this->rawData = rr.rawData;
            return *this;
        }
    };
}
