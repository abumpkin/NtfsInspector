#include "find_devices.hpp"
#include "my_utilities.hpp"
#include "ntfs_access.hpp"
#include "ntfs_app_UsnJrnl.hpp"
#include <chrono>
#include <codecvt>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <locale>
#include <ratio>
#include <sstream>
#include <string>

// 加载卷
abkntfs::Ntfs LoadVolume() {
    abkntfs::Devices devs;
    int vol_i = 0;
    // 展示设备
    for (auto i : devs.devs) {
        std::cout << "[" << vol_i++ << "] "
                  << "卷: ";
        std::cout << i.guidPath << std::endl;
        if (!i.volumePath.empty())
            std::cout << "  挂载点: " << i.volumePath << std::endl;
        if (!i.volumeName.empty())
            std::cout << "  卷名: " << i.volumeName << std::endl;
        if (!i.fileSystem.empty())
            std::cout << "  文件系统: " << i.fileSystem << std::endl;
    }

    // 选择
    while (true) {
        std::cout << "输入卷的索引: ";
        std::cin >> vol_i;
        if (vol_i < 0 || vol_i >= devs.devs.size()) {
            std::cout << "索引号有误!!!" << std::endl;
            continue;
        }
        if (devs.devs[vol_i].fileSystem != "NTFS") {
            std::cout << "请选择 NTFS 文件系统的卷!!!" << std::endl;
            continue;
        }
        break;
    }
    std::string volumePath = devs.devs[vol_i].guidPath.substr(
        0, devs.devs[vol_i].guidPath.size() - 1);

    std::vector<char> data;

    return abkntfs::Ntfs{volumePath};
}

void ShowSecsInfo(abkntfs::NtfsSectorsInfo secs) {
    for (auto &i : secs) {
        std::cout << "  起始扇区号: " << std::dec << i.startSecId
                  << "\t扇区数量: " << i.secNum
                  << "\t是否物理存在: " << std::boolalpha << !i.sparse
                  << std::endl;
    }
}

// 显示卷信息
void ShowVolumeInfo(abkntfs::Ntfs &disk) {
    std::cout << "卷总大小: " << std::dec
              << FriendlyFileSize(disk.GetTotalSize()) << std::endl;
    // 输出 $Boot 信息
    std::cout << "$Boot 信息:" << std::endl;
    std::cout << "  每个簇的扇区数: " << std::dec
              << (int)disk.bootInfo.sectorsPerCluster << std::endl;
    std::cout << "  分卷序列号: " << std::dec
              << disk.bootInfo.volumeSerialNumber << std::endl;
    std::cout << "  MFT 的逻辑簇号(LCN): " << std::dec
              << disk.bootInfo.LCNoVCN0oMFT << std::endl;
    std::cout << "  扇区总数: " << std::dec << disk.bootInfo.numberOfSectors
              << std::endl;
    // 获得 MFT Areas
    auto secs = disk.MFT_FileRecord.FindSpecAttr(abkntfs::NTFS_DATA)
                    ->attrData.dataRunsMap;
    std::cout << "MFT 占用的扇区信息:" << std::endl;
    ShowSecsInfo(secs);
}

void ShowStandardInfo(abkntfs::AttrData_STANDARD_INFOMATION &info,
                      uint32_t preSpace = 0) {
    if (!info.valid) {
        return;
    }
    std::cout << ssp{preSpace} << "文件创建时间: " << NtfsTime(info.info.cTime)
              << std::endl;
    std::cout << ssp{preSpace} << "文件修改时间: " << NtfsTime(info.info.aTime)
              << std::endl;
    std::cout << ssp{preSpace} << "文件读取时间: " << NtfsTime(info.info.rTime)
              << std::endl;
    std::cout << ssp{preSpace}
              << "文件记录修改时间: " << NtfsTime(info.info.mTime) << std::endl;
    std::cout << ssp{preSpace} << "更新序列号(USN): " << std::dec
              << info.extraInfo.USN << std::endl;
}

void ShowAttrList(abkntfs::AttrData_ATTRIBUTE_LIST &info,
                  uint32_t preSpace = 0) {
    if (!info.valid) {
        return;
    }
    std::cout << ssp{preSpace} << "属性列表:" << std::endl;
    int count = 0;
    for (auto &i : info.list) {
        std::cout << ssp{preSpace + 2} << "属性 " << std::dec << count++ << ":"
                  << std::endl;
        std::cout << ssp{preSpace + 4} << "属性类型: 0x" << std::hex
                  << i.info.type << std::endl;
        std::cout << ssp{preSpace + 4} << "属性名: " << wstr2str(i.attrName)
                  << std::endl;
        std::cout << ssp{preSpace + 4} << "属性 ID: " << std::dec
                  << i.info.attrId << std::endl;
        std::cout << ssp{preSpace + 4} << "属性所在文件记录号: " << std::dec
                  << i.info.fileReference.fileRecordNum << std::endl;
        std::cout << ssp{preSpace + 4}
                  << "起始 VCN (如为驻留属性则为 0): " << std::dec
                  << i.info.startingVCN << std::endl;
    }
}

void ShowAttributesFlag(uint32_t flags, uint32_t preSpace = 0) {
    std::cout << ssp{preSpace} << "标志:";
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_NORMAL) {
        std::cout << " 正常";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_READ_ONLY) {
        std::cout << " 只读";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_HIDDEN) {
        std::cout << " 隐藏";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_ARCHIVE) {
        std::cout << " 文档";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_DIRECTORY) {
        std::cout << " 目录";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_SYSTEM) {
        std::cout << " 系统文件";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_TEMPORARY) {
        std::cout << " 临时文件";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_DEVICE) {
        std::cout << " 设备文件";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_INDEX_VIEW) {
        std::cout << " 索引视图文件";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_COMPRESSED) {
        std::cout << " 压缩";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_ENCRYPTED) {
        std::cout << " 加密";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_SPARSE_FILE) {
        std::cout << " 稀疏";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_NOT_CONTENT_INDEXED) {
        std::cout << " 非内容索引";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_REPARSE_POINT) {
        std::cout << " 重解析点";
    }
    if (flags & abkntfs::AttrData_FILE_NAME::FILE_FLAG_OFFLINE) {
        std::cout << " 脱机";
    }
    if (flags == 0) {
        std::cout << " 无";
    }
    std::cout << std::endl;
}

void ShowFileName(abkntfs::AttrData_FILE_NAME &fileName,
                  uint32_t preSpace = 0) {
    if (!fileName.valid) {
        return;
    }
    ShowAttributesFlag(fileName.fileInfo.flags, preSpace);
    std::cout << ssp{preSpace} << "文件名: " << wstr2str(fileName.filename)
              << std::endl;
    // 似乎总是 0
    std::cout << ssp{preSpace} << "文件实际大小: "
              << FriendlyFileSize(fileName.fileInfo.realSizeOfFile)
              << std::endl;
    std::cout << ssp{preSpace} << "文件分配大小: "
              << FriendlyFileSize(fileName.fileInfo.allocSizeOfFile)
              << std::endl;
    std::cout << ssp{preSpace}
              << "创建时间: " << NtfsTime(fileName.fileInfo.cTime) << std::endl;
    std::cout << ssp{preSpace}
              << "修改时间: " << NtfsTime(fileName.fileInfo.aTime) << std::endl;
    std::cout << ssp{preSpace}
              << "读取时间: " << NtfsTime(fileName.fileInfo.rTime) << std::endl;
    std::cout << ssp{preSpace}
              << "文件记录修改时间: " << NtfsTime(fileName.fileInfo.mTime)
              << std::endl;
    std::cout << ssp{preSpace} << "父级目录文件记录号: " << std::dec
              << fileName.fileInfo.fileRef.fileRecordNum << std::endl;
}

void ShowIndexEntries(const std::vector<abkntfs::NtfsIndexEntry> &entries,
                      uint32_t preSpace = 0) {
    for (int i = 0; i < entries.size(); i++) {
        const abkntfs::NtfsIndexEntry &entry = entries[i];
        std::cout << ssp{preSpace} << "索引项 [" << std::dec << i << "]"
                  << std::endl;
        std::cout << ssp{preSpace + 2} << "标志:";
        if (entry.entryHeader.flags & entry.FLAG_IE_POINT_TO_SUBNODE) {
            std::cout << " 指向子节点";
        }
        if (entry.entryHeader.flags & entry.FLAG_LAST_ENTRY_IN_THE_NODE) {
            std::cout << " 节点中的最后一项";
        }
        if (entry.entryHeader.flags == 0) std::cout << " 没有";
        std::cout << std::endl;
        std::cout << ssp{preSpace + 2} << "指向的文件记录号: " << std::dec
                  << entry.entryHeader.fileReference.fileRecordNum << std::endl;
        if (entry.streamType == abkntfs::NTFS_FILE_NAME && entry.stream.len()) {
            std::cout << ssp{preSpace + 2}
                      << "所指向文件的文件名属性($FILE_NAME)信息:" << std::endl;
            abkntfs::AttrData_FILE_NAME pointed = entry.stream;
            ShowFileName(pointed, preSpace + 4);
        }
        if (entry.entryHeader.flags & entry.FLAG_IE_POINT_TO_SUBNODE) {
            std::cout << ssp{preSpace + 2}
                      << "索引项指向的子节点的索引记录号: " << std::dec
                      << entry.pIndexRecordNumber << std::endl;
        }
    }
}

void ShowIndexRoot(abkntfs::AttrData_INDEX_ROOT &info, uint32_t preSpace = 0) {
    if (!info.valid) {
        return;
    }
    std::cout << ssp{preSpace} << "索引基本信息:" << std::endl;
    std::cout << ssp{preSpace + 2} << "被索引的属性类型: 0x" << std::hex
              << std::uppercase << info.rootInfo.attrType << std::endl;
    std::cout << ssp{preSpace + 2} << "索引记录的大小: " << std::dec
              << info.rootInfo.sizeofIB << std::endl;
    std::cout << ssp{preSpace + 2} << "索引的类型:";
    if (!info.rootNode.nodeHeader.notLeafNode) {
        std::cout << " 小索引";
    }
    else {
        std::cout << " 大索引";
    }
    std::cout << std::endl;

    std::cout << ssp{preSpace} << "根节点索引项: " << std::endl;
    ShowIndexEntries(info.rootNode.IEs, preSpace + 2);
}

void ShowIndexAlloc(abkntfs::AttrData_INDEX_ALLOCATION &info,
                    uint32_t preSpace = 0) {
    if (!info.valid) {
        return;
    }
    std::cout << ssp{preSpace} << "所有索引记录: " << std::endl;
    for (int i = 0; i < info.GetIRs().size(); i++) {
        abkntfs::NtfsIndexRecord &IR = info.GetIRs()[i];
        std::cout << ssp{preSpace + 2} << "索引记录 [" << std::dec << i << "]";
        if (!IR.valid) {
            std::cout << "\t无效索引记录!!!" << std::endl;
            continue;
        }
        std::cout << ssp{preSpace + 2} << "是否为叶子节点: " << std::boolalpha
                  << !IR.node.nodeHeader.notLeafNode << std::endl;
        ShowIndexEntries(IR.node.IEs, preSpace + 4);
    }
}

void ShowAttrInfo(abkntfs::NtfsAttr &attr, uint32_t preSpace = 0) {
    bool flag = true;
    std::cout << ssp{preSpace} << "属性类型: 0x" << std::hex << std::uppercase
              << attr.GetAttributeType();
    std::cout << "\t 属性 ID: " << std::dec << attr.fields.get()->attrId;
    std::cout << "\t是否为驻留属性: " << std::boolalpha
              << !attr.fields.get()->nonResident;
    std::cout << "\t 来自: " << std::dec << attr.fileRecordFrom << std::endl;
    std::cout << ssp{preSpace + 2} << "属性大小(字节): " << std::dec
              << attr.GetAttributeLength() << std::endl;
    if (!attr.attrName.empty()) {
        std::cout << ssp{preSpace + 2} << "属性名称: " << std::dec
                  << wstr2str(attr.attrName) << std::endl;
    }
    if (attr.GetAttributeType() == abkntfs::NTFS_STANDARD_INFOMATION) {
        ShowStandardInfo(attr.attrData, preSpace + 2);
        flag = false;
    }
    if (attr.GetAttributeType() == abkntfs::NTFS_ATTRIBUTE_LIST) {
        ShowAttrList(attr.attrData, preSpace + 2);
        flag = false;
    }
    if (attr.GetAttributeType() == abkntfs::NTFS_FILE_NAME) {
        ShowFileName(attr.attrData, preSpace + 2);
        flag = false;
    }
    if (attr.GetAttributeType() == abkntfs::NTFS_INDEX_ROOT) {
        ShowIndexRoot(attr.attrData, preSpace + 2);
        flag = false;
    }
    if (!attr.IsResident()) {
        std::cout << ssp{preSpace + 2}
                  << "虚拟簇号(Virtual Cluster Number)数量: "
                  << attr.GetVirtualClusterCount() << std::endl;
        uint64_t allocSize = static_cast<abkntfs::NtfsAttr::NonResidentPart &>(
                                 *attr.fields.get())
                                 .allocSize;
        uint64_t realSize = attr.GetDataSize();
        std::cout << ssp{preSpace + 2}
                  << "数据流(data runs) 对应数据的分配大小(字节): " << std::dec
                  << allocSize << " (" << FriendlyFileSize(allocSize) << ")"
                  << std::endl;

        std::cout << ssp{preSpace + 2}
                  << "数据流(data runs) 对应数据的实际大小(字节): " << std::dec
                  << realSize << " (" << FriendlyFileSize(realSize) << ")"
                  << std::endl;

        std::cout << ssp{preSpace + 2}
                  << "数据流(data runs) 二进制数据: " << std::endl;
        ShowHex((abkntfs::NtfsDataBlock)attr.attrData, attr.attrData.len(), 16,
                preSpace + 4);
        flag = false;
    }
    else if (attr.GetAttributeType() == abkntfs::NTFS_DATA) {
        std::cout << ssp{preSpace + 2}
                  << "驻留数据大小: " << FriendlyFileSize(attr.GetDataSize())
                  << std::endl;
        std::cout << ssp{preSpace + 2} << "驻留数据:" << std::endl;
        ShowHex((abkntfs::NtfsDataBlock)attr.attrData, attr.attrData.len(), 16,
                preSpace + 4);
        flag = false;
    }
    if (attr.GetAttributeType() == abkntfs::NTFS_INDEX_ALLOCATION) {
        // ShowIndexAlloc(attr.attrData, preSpace + 2);
        flag = false;
    }
    if (flag) {
        std::cout << ssp{preSpace + 2} << "原始数据: " << std::endl;
        ShowHex(attr.attrData.rawData, attr.attrData.rawData.len(), 16,
                preSpace + 4);
    }
}

// 显示 文件记录 详细信息
void ShowFileRecordInfo(abkntfs::Ntfs &disk, uint64_t idx,
                        uint32_t preSpace = 0) {
    abkntfs::NtfsFileRecord rcd = disk.GetFileRecordByFRN(idx);
    std::string filename = wstr2str(rcd.GetFileName());
    std::cout << ssp{preSpace} << "文件记录 [" << std::dec << idx << "]"
              << std::endl;
    if (rcd.valid) {
        if (!filename.empty()) {
            std::cout << ssp{preSpace} << "  文件名: \"" << filename << "\""
                      << std::endl;
        }
        std::cout << ssp{preSpace} << "  文件记录标志: ";
        if (rcd.fixedFields.flags == 0) {
            std::cout << "未使用 ";
        }
        else {
            if (rcd.fixedFields.flags & 0x01) {
                std::cout << "存在 ";
            }
            if (rcd.fixedFields.flags & 0x02) {
                std::cout << "目录 ";
            }
        }
        std::cout << std::endl;
    }
    else {
        std::cout << ssp{preSpace} << "文件记录号无效! 最大值: " << std::dec
                  << disk.FileRecordsCount << std::endl;
        return;
    }
    std::cout << ssp{preSpace} << "  下一个属性 ID: " << std::dec
              << rcd.fixedFields.nextAttrID << std::endl;
    std::cout << ssp{preSpace} << "  硬链接数量: " << std::dec
              << rcd.fixedFields.hardLinkCount << std::endl;
    std::cout << ssp{preSpace}
              << "  到基文件记录的记录号(0 表示自身就是基文件记录): "
              << std::dec << rcd.fixedFields.fileReference.fileRecordNum
              << std::endl;

    std::cout << ssp{preSpace} << "  所有属性:" << std::endl;
    for (auto &o : rcd.attrs) {
        abkntfs::NtfsAttr &oRef = *o.get();
        ShowAttrInfo(oRef, 6 + preSpace);
    }
}

// 显示文件夹文件
void ShowDirFiles(abkntfs::NtfsFileRecord rcd) {
    abkntfs::NtfsFileNameIndex index = rcd;
    if (index.valid) {
        std::cout << std::endl;
        std::cout << "文件夹内容:" << std::endl;
        uint64_t number = 1;
        auto showFile =
            [&](abkntfs::NtfsFileNameIndex::FileInfoInIndex info) -> bool {
            std::cout << std::left << std::setfill(' ') << std::setw(8)
                      << "  [" + std::to_string(number++) + "]";
            std::cout << std::left << std::setfill(' ') << std::setw(40)
                      << "  文件名: \"" + wstr2str(info.fn.filename) + "\"";
            std::cout << "  文件记录号: " << std::dec << std::left
                      << info.fileRef.fileRecordNum << std::endl;
            return true;
        };
        index.ForEachFileInfo(showFile);
    }
    else {
        std::cout << "此非文件夹!" << std::endl;
    }
    std::cout << std::endl;
}

// 显示 文件记录 的 16 进制数据
void ShowFileRecordHex(abkntfs::Ntfs &disk, uint64_t idx,
                       uint32_t preSpace = 2) {
    uint64_t rcdIdx = idx;
    abkntfs::NtfsSectorsInfo area = disk.GetFileRecordAreaByFRN(rcdIdx);
    abkntfs::NtfsDataBlock block = disk.ReadSectors(area);
    abkntfs::NtfsFileRecord trcd = abkntfs::NtfsFileRecord{block, idx};
    if (trcd.valid) {
        std::vector<char> data = block;
        ShowHex(&data[0], data.size(), 32, preSpace);
    }
}

// 动作 对象
class CommandParser {
    static std::string PopParameter(std::string &cmd) {
        std::string ret;
        cmd = trim(cmd);
        if (cmd.empty()) return ret;
        uint64_t pos = cmd.find_first_of(" \t");
        if (pos != std::string::npos) {
            ret = cmd.substr(0, pos);
            cmd = trim(cmd.substr(pos));
        }
        else {
            ret = cmd;
            cmd.clear();
        }
        return ret;
    }

    static uint64_t ToUll(std::string &text) {
        try {
            return std::stoull(text);
        }
        catch (...) {
            return 0;
        }
    }

private:
    template <class T> class Exists {
        T val;
        bool exists;

    public:
        Exists() : exists{false}, val{} {};
        operator T &() { return val; }
        bool ex() const { return exists; }
        Exists(T const &val) : exists{true}, val{val} {}
        Exists &operator=(T const &val) {
            this->val = val;
            exists = true;
            return *this;
        }
    };

    struct PrintParams {
        // 指定打印文件记录号
        Exists<uint64_t> FRN;
        // 打印指定扇区
        Exists<uint64_t> sectorId;
        // 打印指定簇号
        Exists<uint64_t> clusterId;
        // 是否十六进制显示
        bool hex = false;
        // 显示目录文件
        bool dir = false;
        // 打印 $J 日志最后多少条记录
        Exists<uint64_t> logJn;
        // 单独出现打印分卷信息; 跟 attrId 组合打印属性 data runs 的对应扇区.
        bool info = false;
        // 打印指定属性
        Exists<uint64_t> attrId;
        // 打印一个空闲位置
        Exists<uint64_t> freeUnit;
    };

public:
    abkntfs::Ntfs disk;
    CommandParser(abkntfs::Ntfs &disk) { this->disk = std::move(disk); };
    void Parse(std::string cmd) {
        std::string param = PopParameter(cmd);
        if (compareStrNoCase(param, "p")) {
            PrintParams ps;
            while (!cmd.empty()) {
                param = PopParameter(cmd);
                if (compareStrNoCase(param, "frn")) {
                    ps.FRN = ToUll(PopParameter(cmd));
                }
                if (compareStrNoCase(param, "sec")) {
                    ps.sectorId = ToUll(PopParameter(cmd));
                }
                if (compareStrNoCase(param, "clu")) {
                    ps.clusterId = ToUll(PopParameter(cmd));
                }
                if (compareStrNoCase(param, "hex")) {
                    ps.hex = true;
                }
                if (compareStrNoCase(param, "dir")) {
                    ps.dir = true;
                }
                if (compareStrNoCase(param, "logJ")) {
                    ps.logJn = ToUll(PopParameter(cmd));
                    if (ps.logJn == 0) {
                        ps.logJn = 10;
                    }
                }
                if (compareStrNoCase(param, "info")) {
                    ps.info = true;
                }
                if (compareStrNoCase(param, "attrid")) {
                    ps.attrId = ToUll(PopParameter(cmd));
                }
                if (compareStrNoCase(param, "free")) {
                    ps.freeUnit = ToUll(PopParameter(cmd));
                }
            }
            Print(ps);
        }
    }

    void Print(PrintParams &ps) {
        bool flag = false;
        if (ps.FRN.ex()) {
            if (ps.attrId.ex()) {
                abkntfs::NtfsFileRecord t = disk.GetFileRecordByFRN(ps.FRN);
                abkntfs::NtfsAttr *pAttr = t.GetSpecAttr(ps.attrId);
                if (nullptr == pAttr) {
                    std::cout << "无此属性." << std::endl;
                    return;
                }
                if (ps.hex) {
                    ShowHex(pAttr->rawData, pAttr->rawData.len(), 16, 4);
                }
                else {
                    ShowAttrInfo(*pAttr);
                }
                if (ps.info) {
                    if (!pAttr->IsResident()) {
                        ShowSecsInfo(disk.DataRunsToSectorsInfo(pAttr->attrData,
                                                                *pAttr));
                    }
                }
            }
            else if (ps.dir) {
                ShowDirFiles(disk.GetFileRecordByFRN(ps.FRN));
            }
            else if (ps.hex) {
                ShowFileRecordHex(disk, ps.FRN);
            }
            else if (ps.freeUnit.ex()) {
                abkntfs::NtfsFileRecord t = disk.GetFileRecordByFRN(ps.FRN);
                abkntfs::NtfsAttr *td = t.FindSpecAttr(abkntfs::NTFS_BITMAP);
                if (nullptr == td) {
                    return;
                }
                abkntfs::TypeData_BITMAP bm;
                bm = {td->ReadAttrRawRealData(0, td->GetDataSize(), &disk),
                      abkntfs::TypeData_BITMAP::UNIT_RECORD};
                std::cout << "  自由空间单元: " << bm.FindFreeUnitPos()
                          << std::endl;
            }
            else {
                ShowFileRecordInfo(disk, ps.FRN);
            }
            flag = true;
        }
        else if (ps.freeUnit.ex()) {
            abkntfs::NtfsFileRecord t = disk.GetFileRecordByFRN(6);
            abkntfs::NtfsAttr *td = t.FindSpecAttr(abkntfs::NTFS_DATA, L"");
            if (nullptr == td) {
                return;
            }
            abkntfs::TypeData_BITMAP bm;
            bm = {td->ReadAttrRawRealData(0, td->GetDataSize(), &disk),
                  abkntfs::TypeData_BITMAP::UNIT_RECORD};
            uint64_t fi = bm.FindFreeUnitPos();
            if (fi != (uint64_t)-1) {
                std::cout << "  自由空间单元: " << fi << std::endl;
            }
            flag = true;
        }
        else if (ps.logJn.ex()) {
            abkntfs::NtfsUsnJrnl logJ{disk};
            auto logRecs = logJ.GetLastN(ps.logJn);
            for (auto &i : logRecs) {
                std::cout << "[USN " << std::dec << i.fixed.offInJ << "]";
                std::cout << ssp{0} << "[" << NtfsTime(i.fixed.time) << "]";
                std::cout << ssp{0} << "[FRN " << std::dec
                          << i.fixed.fileRef.fileRecordNum << "]";
                std::cout << "[" << wstr2str(i.fileName) << "]";
                std::cout << ssp{0} << "[";
                if (i.fixed.reason &
                    abkntfs::NtfsUsnJrnl::FILE_OR_DIRECTORY_WAS_CREATED) {
                    std::cout << " 文件创建";
                }
                if (i.fixed.reason &
                    abkntfs::NtfsUsnJrnl::FILE_OR_DIRECTORY_WAS_DELETED) {
                    std::cout << " 文件删除";
                }
                if (i.fixed.reason & abkntfs::NtfsUsnJrnl::FILE_CLOSED) {
                    std::cout << " 文件被关闭";
                }
                if (i.fixed.reason &
                    abkntfs::NtfsUsnJrnl::FILE_OR_DIRECTORY_RENAMED_NEW) {
                    std::cout << " 重命名(显示新名)";
                }
                if (i.fixed.reason &
                    abkntfs::NtfsUsnJrnl::FILE_OR_DIRECTORY_RENAMED_OLD) {
                    std::cout << " 重命名(显示旧名)";
                }
                if (i.fixed.reason &
                    (abkntfs::NtfsUsnJrnl::UNNAMED_DATA_STREAM_WAS_ADDED |
                     abkntfs::NtfsUsnJrnl::UNNAMED_DATA_STREAM_WAS_OVERWRITTEN |
                     abkntfs::NtfsUsnJrnl::UNNAMED_DATA_STREAM_WAS_TRUNCATED)) {
                    std::cout << " 内容被修改";
                }
                if (i.fixed.reason & abkntfs::NtfsUsnJrnl::FILE_ATTR_CHANGED) {
                    std::cout << " 属性改变";
                }
                if (i.fixed.reason &
                    (abkntfs::NtfsUsnJrnl::NAMED_DATA_STREAM_WAS_ADDED |
                     abkntfs::NtfsUsnJrnl::NAMED_DATA_STREAM_WAS_OVERWRITTEN |
                     abkntfs::NtfsUsnJrnl::NAMED_DATA_STREAM_WAS_TRUNCATED)) {
                    std::cout << " 额外内容被修改";
                }
                if (i.fixed.reason &
                    abkntfs::NtfsUsnJrnl::ACCESS_RIGHTS_CHANGED) {
                    std::cout << " 访问权限修改";
                }
                if (i.fixed.reason &
                    abkntfs::NtfsUsnJrnl::FILE_COMPRESSED_CHANGED) {
                    std::cout << " 压缩状态改变";
                }
                if (i.fixed.reason &
                    abkntfs::NtfsUsnJrnl::FILE_ENCRYPTED_OR_DECRYPTED) {
                    std::cout << " 加密状态改变";
                }
                if (i.fixed.reason &
                    abkntfs::NtfsUsnJrnl::FILE_HARD_LINK_CHANGED) {
                    std::cout << " 硬链接数量改变";
                }
                if (i.fixed.reason & abkntfs::NtfsUsnJrnl::FILE_OBJID_CHANGED) {
                    std::cout << " 对象 ID 改变";
                }
                std::cout << "]" << std::endl;
                ShowAttributesFlag(i.fixed.fileAttributes, 2);
                abkntfs::NtfsFileRecord t =
                    disk.GetFileRecordByFRN(i.fixed.fileRef.fileRecordNum);
                std::cout << ssp{2}
                          << "所在目录: " << wstr2str(disk.GetFilePath(t))
                          << std::endl;

                // std::cout << ssp{2} << "源信息: " << std::dec
                //           << i.fixed.sourceInfo << std::endl;
            }
            flag = true;
        }
        else if (ps.sectorId.ex()) {
            if (ps.sectorId > disk.bootInfo.numberOfSectors) {
                std::cout << "最大扇区号: " << std::dec
                          << disk.bootInfo.numberOfSectors - 1 << std::endl;
                return;
            }
            auto data = disk.ReadSector(ps.sectorId);
            ShowHex(data.data(), data.size(), 16, 4);
            flag = true;
        }
        else if (ps.clusterId.ex()) {
            if (ps.clusterId > disk.bootInfo.numberOfSectors /
                                   disk.bootInfo.sectorsPerCluster) {
                std::cout << "最大扇区号: " << std::dec
                          << disk.bootInfo.numberOfSectors /
                                     disk.bootInfo.sectorsPerCluster -
                                 1
                          << std::endl;
                return;
            }
            auto data = disk.DiskReader::ReadSectors(
                ps.clusterId * disk.bootInfo.sectorsPerCluster,
                disk.bootInfo.sectorsPerCluster);
            ShowHex(data.data(), data.size(), 32, 4);
            flag = true;
        }
        else if (ps.info) {
            ShowVolumeInfo(disk);
            flag = true;
        }
        // 不能被执行
        if (!flag) {
            std::cout << "无法解析此命令." << std::endl;
        }
    }
};

int main() {
    // 选择并加载卷
    abkntfs::Ntfs disk = LoadVolume();
    // tamper::Ntfs disk2 = std::move(disk);
    int choose = 0;

    if (!disk.IsOpen() || !disk.valid) {
        // 打开失败
        std::cout << "打开失败!" << std::endl;
        std::cin >> choose;
        return 0;
    }

    // 显示卷信息
    ShowVolumeInfo(disk);

    CommandParser parser(std::move(disk));

    getchar();
    while (true) {
        std::cout << "> ";
        std::string cmd;
        std::getline(std::cin, cmd);
        parser.Parse(cmd);
    }
}
