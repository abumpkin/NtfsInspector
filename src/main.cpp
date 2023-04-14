#include "disk_reader.hpp"
#include "find_devices.hpp"
#include "ntfs_access.hpp"
#include <chrono>
#include <codecvt>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <locale>
#include <ratio>
#include <sstream>
#include <string>

std::wstring str2wstr(const std::string &s) {
    char *pcurLocale = ::setlocale(LC_ALL, NULL); // curLocale = "C";
    std::string curLocale;
    if (pcurLocale != nullptr) {
        curLocale.assign(pcurLocale);
    }

    const char *_Source = s.c_str();
    size_t _Dsize = s.size() + 1;
    wchar_t *_Dest = new wchar_t[_Dsize];
    wmemset(_Dest, 0, _Dsize);
    mbstowcs(_Dest, _Source, _Dsize);
    std::wstring result = _Dest;
    delete[] _Dest;

    ::setlocale(LC_ALL, curLocale.c_str());

    return result;
}

std::string wstr2str(const std::wstring &ws) {

    char *pcurLocale = ::setlocale(LC_ALL, NULL); // curLocale = "C";
    std::string curLocale;
    std::string result;
    if (pcurLocale != nullptr) {
        curLocale.assign(pcurLocale);
    }
    ::setlocale(LC_CTYPE, "chs");
    const wchar_t *_Source = ws.c_str();
    size_t _Dsize = 2 * ws.size() + 1;
    char *_Dest = new char[_Dsize];
    memset(_Dest, 0, _Dsize);
    size_t num = wcstombs(_Dest, _Source, _Dsize);
    if (num != (size_t)-1) {
        result.assign(_Dest, num);
    }
    delete[] _Dest;

    ::setlocale(LC_ALL, curLocale.c_str());

    return result;
}

struct ssp {
    uint32_t preSpace;
    template <class _Elem, class _Traits>
    typename std::basic_ostream<_Elem, _Traits> &
    operator()(typename std::basic_ostream<_Elem, _Traits> &_Ostr) {
        uint32_t sp = preSpace;
        while (sp--) {
            _Ostr.put(_Ostr.widen(' '));
        }
        return _Ostr;
    }
};

template <class _Elem, class _Traits>
auto operator<<(std::basic_ostream<_Elem, _Traits> &_Ostr, ssp &sps)
    -> std::basic_ostream<_Elem, _Traits> & {
    return sps(_Ostr);
}

// 友好显示文件大小
std::string FriendlyFileSize(uint64_t size) {
    std::stringstream output;
    double res = size;
    const char *const units[] = {"B", "KB", "MB", "GB", "TB"};
    int scale = 0;
    while (res > 1024.0 && scale < 4) {
        res /= 1024.0;
        scale++;
    }
    output << std::setprecision(3) << std::fixed << res << " " << units[scale];
    return output.str();
}

// 友好显示时间 (NTFS 时间(微软时间) 转 UTC 时间)
std::string FriendlyTime(uint64_t time) {
    static char timeStr[50] = {};
    std::chrono::microseconds usTime{time / 10};
    std::chrono::system_clock::duration sys =
        std::chrono::duration_cast<std::chrono::system_clock::duration>(usTime);
    std::chrono::time_point<std::chrono::system_clock> tp(sys);
    std::time_t t_c = std::chrono::system_clock::to_time_t(tp);
    std::tm *ptm = std::localtime(&t_c);
    std::tm emptyTm = {0, 0, 0, 1, 0, -299};
    std::string ret;
    if (nullptr != ptm) {
        ptm->tm_year -= 369;
    }
    else
        ptm = &emptyTm;
    std::strftime(timeStr, 50, "UTC %F T%T %z", ptm);
    ret = timeStr;
    return ret;
}

// 展示 16 进制数据
void ShowData(char const *data, uint64_t len, int width = 32, int preWhite = 0,
              uint32_t divisionHeight = (uint32_t)-1, bool showAscii = true) {
    uint64_t dpos = 0;
    uint32_t dDivide = divisionHeight;
    uint32_t blockNum = 1;
    while (dpos < len) {
        for (int p = preWhite; p; p--) {
            std::cout << " ";
        }
        for (int p = 0; p < width; p++) {
            if (dpos + p >= len) {
                for (int o = (width - p) * 3; o; o--) {
                    std::cout << " ";
                }
                break;
            }
            std::cout << std::uppercase << std::hex << std::setw(2)
                      << std::setfill('0') << (uint32_t)(uint8_t)data[dpos + p]
                      << " ";
        }
        if (showAscii) {
            std::cout << "    ";
            for (int p = 0; p < width; p++) {
                if (dpos + p >= len) {
                    break;
                }
                if ((uint8_t)data[dpos + p] >= 0x20 &&
                    (uint8_t)data[dpos + p] <= 0x7E) {
                    std::cout << data[dpos + p];
                }
                else {
                    std::cout << ".";
                }
            }
        }
        std::cout << std::endl;
        if (!(--dDivide)) {
            for (int p = preWhite; p; p--) {
                std::cout << " ";
            }
            std::cout << "  偏移: 0x" << std::hex << std::uppercase
                      << std::setw(8) << std::setfill('0') << dpos + width;
            std::cout << "  块号: " << std::dec << blockNum;
            std::cout << std::endl;
            dDivide = divisionHeight;
            blockNum++;
        }
        dpos += width;
    }
}

// 加载卷
tamper::Ntfs LoadVolume() {
    tamper::Devices devs;
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
        if (vol_i < 0 || vol_i > devs.devs.size()) {
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

    return tamper::Ntfs{volumePath};
}

// 显示卷信息
void ShowVolumeInfo(tamper::Ntfs &disk) {
    std::cout << "卷总大小: " << std::dec
              << FriendlyFileSize(disk.GetTotalSize()) << std::endl;
    // 输出 $Boot 信息
    std::cout << "$Boot 信息:" << std::endl;
    std::cout << "  每个簇的扇区数: " << (int)disk.bootInfo.sectorsPerCluster
              << std::endl;
    std::cout << "  每个文件记录使用的簇数量: "
              << disk.bootInfo.clustersPerFileRecord << std::endl;
    std::cout << "  MFT 的逻辑簇号(LCN): " << disk.bootInfo.LCNoVCN0oMFT
              << std::endl;
    // 获得 MFT Areas
    auto secs =
        disk.MFT_FileRecord.GetSpecAttr(tamper::NTFS_DATA)->attrData.sectors;
    std::cout << "MFT 占用的扇区信息:" << std::endl;
    for (auto &i : secs) {
        std::cout << "  起始扇区号: " << std::dec << i.startSecId
                  << " 扇区数量: " << i.secNum << std::endl;
    }
}

void ShowStandardInfo(tamper::NtfsFileRecord &rcd, uint32_t preSpace = 0) {
    tamper::NtfsAttr::TypeData *pTypeData =
        rcd.GetSpecAttrData(tamper::NTFS_STANDARD_INFOMATION);
    if (nullptr == pTypeData) {
        return;
    }
    tamper::TypeData_STANDARD_INFOMATION &info = *pTypeData;

    std::cout << ssp{preSpace}
              << "文件创建时间: " << FriendlyTime(info.info.cTime) << std::endl;
    std::cout << ssp{preSpace}
              << "文件修改时间: " << FriendlyTime(info.info.aTime) << std::endl;
    std::cout << ssp{preSpace}
              << "文件读取时间: " << FriendlyTime(info.info.rTime) << std::endl;
    std::cout << ssp{preSpace}
              << "文件记录修改时间: " << FriendlyTime(info.info.mTime)
              << std::endl;
    std::cout << ssp{preSpace} << "更新序列号(USN): " << std::dec
              << info.extraInfo.USN << std::endl;
}

void ShowFileName(tamper::TypeData_FILE_NAME &fileName, uint32_t preSpace = 0) {
    if (!fileName.valid) {
        return;
    }
    std::cout << ssp{preSpace} << "标志:";
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_NORMAL) {
        std::cout << " 正常";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_READ_ONLY) {
        std::cout << " 只读";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_HIDDEN) {
        std::cout << " 隐藏";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_ARCHIVE) {
        std::cout << " 文档";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_DIRECTORY) {
        std::cout << " 目录";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_SYSTEM) {
        std::cout << " 系统文件";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_TEMPORARY) {
        std::cout << " 临时文件";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_DEVICE) {
        std::cout << " 设备文件";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_INDEX_VIEW) {
        std::cout << " 索引视图文件";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_COMPRESSED) {
        std::cout << " 压缩";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_ENCRYPTED) {
        std::cout << " 加密";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_SPARSE_FILE) {
        std::cout << " 稀疏";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_NOT_CONTENT_INDEXED) {
        std::cout << " 没有被索引";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_REPARSE_POINT) {
        std::cout << " 重解析点";
    }
    if (fileName.fileInfo.flags & fileName.FILE_FLAG_OFFLINE) {
        std::cout << " 脱机";
    }
    if (fileName.fileInfo.flags == 0) {
        std::cout << " 没有";
    }
    std::cout << std::endl;

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
              << "创建时间: " << FriendlyTime(fileName.fileInfo.cTime)
              << std::endl;
    std::cout << ssp{preSpace}
              << "修改时间: " << FriendlyTime(fileName.fileInfo.aTime)
              << std::endl;
    std::cout << ssp{preSpace}
              << "读取时间: " << FriendlyTime(fileName.fileInfo.rTime)
              << std::endl;
    std::cout << ssp{preSpace}
              << "文件记录修改时间: " << FriendlyTime(fileName.fileInfo.mTime)
              << std::endl;
    std::cout << ssp{preSpace} << "父级目录文件记录号: " << std::dec
              << fileName.fileInfo.fileRef.fileRecordNum << std::endl;
}

void ShowIndexEntries(const std::vector<tamper::NtfsIndexEntry> &entries,
                      uint32_t preSpace = 0) {
    for (int i = 0; i < entries.size(); i++) {
        const tamper::NtfsIndexEntry &entry = entries[i];
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
        if (entry.streamType == tamper::NTFS_FILE_NAME && entry.stream.len()) {
            std::cout << ssp{preSpace + 2}
                      << "所指向文件的文件名属性($FILE_NAME)信息:" << std::endl;
            tamper::TypeData_FILE_NAME pointed = entry.stream;
            ShowFileName(pointed, preSpace + 4);
        }
        if (entry.entryHeader.flags & entry.FLAG_IE_POINT_TO_SUBNODE) {
            std::cout << ssp{preSpace + 2}
                      << "索引项指向的子节点的索引记录号: " << std::dec
                      << entry.pIndexRecordNumber << std::endl;
        }
    }
}

void ShowIndexRoot(tamper::TypeData_INDEX_ROOT &info, uint32_t preSpace = 0) {
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

void ShowIndexAlloc(tamper::TypeData_INDEX_ALLOCATION &info,
                    uint32_t preSpace = 0) {
    if (!info.valid) {
        return;
    }
    std::cout << ssp{preSpace} << "所有索引记录: " << std::endl;
    for (int i = 0; i < info.IRs.size(); i++) {
        tamper::NtfsIndexRecord &IR = info.IRs[i];
        std::cout << ssp{preSpace + 2} << "索引记录 [" << std::dec << i << "]";
        if (!IR.valid) {
            std::cout << "\t无效索引记录!!!" << std::endl;
            continue;
        }
        std::cout << ssp{preSpace + 2} << "是否为叶子节点: " << std::boolalpha
                  << !IR.node.nodeHeader.notLeafNode << std::endl;
        std::cout << ssp{preSpace + 2} << "所有索引项:" << std::endl;
        ShowIndexEntries(IR.node.IEs, preSpace + 2);
    }
}

// 显示 文件记录 详细信息
void ShowFileRecordInfo(tamper::Ntfs &disk, uint64_t idx) {
    std::vector<tamper::SuccessiveSectors> req =
        disk.GetFileRecordAreaByIndex(idx);
    tamper::NtfsFileRecord rcd = disk.ReadSectors(req);
    std::string filename = wstr2str(rcd.GetFileName());
    std::cout << "文件记录 [" << std::dec << idx << "]" << std::endl;
    if (rcd.valid) {
        if (!filename.empty()) {
            std::cout << "  文件名: \"" << filename << "\"" << std::endl;
        }
        std::cout << "  文件记录标志: ";
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
        std::cout << "文件记录号无效! 最大值: " << std::dec
                  << disk.FileRecordsCount << std::endl;
    }

    std::cout << "  所有属性:" << std::endl;
    for (auto &o : rcd.attrs) {
        tamper::NtfsAttr &oRef = *o.get();
        std::cout << ssp{4} << "属性类型: 0x" << std::hex << std::uppercase
                  << oRef.GetAttributeType();
        std::cout << "\t是否为驻留属性: " << std::boolalpha
                  << !oRef.fields.get()->nonResident << std::endl;
        std::cout << ssp{6} << "属性大小(字节): " << std::dec
                  << oRef.GetAttributeLength() << std::endl;
        if (!oRef.attrName.empty()) {
            std::cout << ssp{6} << "属性名称: " << std::dec
                      << wstr2str(oRef.attrName) << std::endl;
        }
        if (oRef.GetAttributeType() == tamper::NTFS_STANDARD_INFOMATION) {
            ShowStandardInfo(rcd, 6);
        }
        if (oRef.GetAttributeType() == tamper::NTFS_FILE_NAME) {
            tamper::NtfsAttr::TypeData *pData =
                rcd.GetSpecAttrData(tamper::NTFS_FILE_NAME, oRef.attrName);
            if (nullptr != pData) {
                ShowFileName(*pData, 6);
            }
        }
        if (oRef.GetAttributeType() == tamper::NTFS_INDEX_ROOT) {
            tamper::NtfsAttr::TypeData *pData =
                rcd.GetSpecAttrData(tamper::NTFS_INDEX_ROOT, oRef.attrName);
            if (nullptr != pData) {
                ShowIndexRoot(*pData, 6);
            }
        }
        if (!oRef.IsResident()) {
            std::cout << ssp{6} << "虚拟簇号(Virtual Cluster Number)数量: "
                      << oRef.GetVirtualClusterCount() << std::endl;
            std::cout << ssp{6}
                      << "数据流(data runs) 对应数据的分配大小(字节): "
                      << std::dec
                      << FriendlyFileSize(disk.GetDataRunsDataSize(
                             oRef, oRef.attrData.rawData))
                      << std::endl;

            std::cout << ssp{6}
                      << "数据流(data runs) 对应数据的实际大小(字节): "
                      << std::dec << FriendlyFileSize(oRef.GetDataSize())
                      << std::endl;

            std::cout << ssp{6}
                      << "数据流(data runs) 二进制数据: " << std::endl;
            ShowData((tamper::NtfsDataBlock)oRef.attrData, oRef.attrData.len(),
                     16, 8);
        }
        else if (oRef.GetAttributeType() == tamper::NTFS_DATA) {
            std::cout << ssp{6} << "驻留数据大小: "
                      << FriendlyFileSize(oRef.GetDataSize()) << std::endl;
            std::cout << ssp{6} << "驻留数据:" << std::endl;
            ShowData((tamper::NtfsDataBlock)oRef.attrData, oRef.attrData.len(),
                     16, 8);
        }
        if (oRef.GetAttributeType() == tamper::NTFS_INDEX_ALLOCATION) {
            tamper::NtfsAttr::TypeData *pData = rcd.GetSpecAttrData(
                tamper::NTFS_INDEX_ALLOCATION, oRef.attrName);
            if (nullptr != pData) {
                ShowIndexAlloc(*pData, 6);
            }
        }
    }
    tamper::NtfsFileNameIndex index = rcd;
    if (index.valid) {
        std::cout << std::endl;
        std::cout << "文件夹内容:" << std::endl;
        uint64_t number = 1;
        auto showFile = [&](tamper::TypeData_FILE_NAME fileInfo,
                            uint64_t indexRecordNumber) -> bool {
            std::cout << std::left << std::setfill(' ') << std::setw(8)
                      << "  [" + std::to_string(number++) + "]";
            std::cout << std::left << std::setfill(' ') << std::setw(40)
                      << "  文件名: \"" + wstr2str(fileInfo.filename) + "\"";
            std::cout << "  文件记录号: " << std::dec << std::left
                      << indexRecordNumber << std::endl;
            return true;
        };
        index.ForEachFileInfo(showFile);
    }
    std::cout << std::endl;
}

// 显示 文件记录 的 16 进制数据
void ShowFileRecordHex(tamper::Ntfs &disk, uint64_t idx,
                       uint32_t preSpace = 2) {
    uint64_t rcdIdx = idx;
    std::vector<tamper::SuccessiveSectors> area =
        disk.GetFileRecordAreaByIndex(rcdIdx);
    tamper::NtfsDataBlock block = disk.ReadSectors(area);
    tamper::NtfsFileRecord trcd = block;
    if (trcd.valid) {
        std::vector<char> data = block;
        ShowData(&data[0], data.size(), 32, preSpace);
    }
}

int main() {
    // 选择并加载卷
    tamper::Ntfs disk = LoadVolume();
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

    // 列出所有文件名 51 47 big_dir
    // std::cout << "MFT Files:" << std::endl;
    // for (int i = 0; i < disk.FileRecordsCount; i++) {
    //     ShowFileRecordInfo(disk, i);
    //     getchar();
    // }

    while (true) {
        uint64_t rcdIdx;
        std::cout << "输入文件记录号: ";
        std::cin >> rcdIdx; // 258172 29_daily
        // ShowFileRecordHex(disk, rcdIdx);
        ShowFileRecordInfo(disk, rcdIdx);
    }
    // }
    // catch (...) {
    //     std::cout << "error!!!" << std::endl;
    // }

    // std::cout << std::boolalpha << std::endl
    //           << disk.WriteSector(1000,
    //                               std::vector<char>(disk.GetSectorSize(),
    //                               '0'))
    //           << std::endl;
}
