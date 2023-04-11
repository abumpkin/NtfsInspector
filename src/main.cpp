#include "disk_reader.hpp"
#include "find_devices.hpp"
#include "ntfs_access.hpp"
#include <codecvt>
#include <iomanip>
#include <iostream>
#include <locale>
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

// 展示 16 进制数据
void ShowData(char const *data, uint64_t len, int width = 32,
              int preWhite = 0) {
    uint64_t dpos = 0;
    while (dpos < len) {
        for (int p = preWhite; p; p--) {
            std::cout << " ";
        }
        for (int p = width; p; p--) {
            if (dpos >= len) {
                break;
            }
            std::cout << std::uppercase << std::hex << std::setw(2)
                      << std::setfill('0') << (uint32_t)(uint8_t)data[dpos++]
                      << " ";
        }
        std::cout << std::endl;
    }
}

// 加载卷
tamper::Ntfs LoadVolume() {
    tamper::Devices devs;
    int vol_i = 0;
    // 展示设备
    for (auto i : devs.devs) {
        std::cout << "[" << vol_i++ << "] "
                  << "volume: ";
        std::cout << i.guidPath << std::endl;
        std::cout << "  file system: " << i.fileSystem << std::endl;
        if (!i.volumeName.empty())
            std::cout << "  volume name: " << i.volumeName << std::endl;
        if (!i.volumePath.empty())
            std::cout << "  mount: " << i.volumePath << std::endl;
    }

    // 选择
    std::cout << "input index of volume:";
    std::cin >> vol_i;

    std::string volumePath = devs.devs[vol_i].guidPath.substr(
        0, devs.devs[vol_i].guidPath.size() - 1);

    std::vector<char> data;

    return tamper::Ntfs {volumePath};
}

// 显示卷信息
void ShowVolumeInfo(tamper::Ntfs &disk) {
    std::cout << "total size: " << std::dec << disk.GetTotalSize() << std::endl;
    // 输出 $Boot 信息
    std::cout << "$Boot info:" << std::endl;
    std::cout << "  sectors per cluster: "
              << (int)disk.bootInfo.sectorsPerCluster << std::endl;
    std::cout << "  clusters per MFT record: "
              << disk.bootInfo.clustersPer_MFT_Record << std::endl;
    std::cout << "  Logic cluster number of MFT: " << disk.bootInfo.LCNoVCN0oMFT
              << std::endl;
    // 获得 MFT Areas
    auto secs = disk.MFT_FileRecord.GetDataAttr().attrData.sectors;
    std::cout << "MFT Areas:" << std::endl;
    for (auto &i : secs) {
        std::cout << "  start: " << std::dec << i.startSecId
                  << " num: " << i.secNum << std::endl;
    }
}

// 显示 文件记录 详细信息
void ShowFileRecordInfo(tamper::Ntfs &disk, uint64_t idx) {
    std::vector<tamper::SuccessiveSectors> req =
        disk.GetFileRecordAreaByIndex(idx);
    tamper::Ntfs_FILE_Record rcd = disk.ReadSectors(req);
    std::string filename = wstr2str(rcd.GetFileName());
    std::cout << "FILE RECORD [" << std::dec << idx << "]" << std::endl;
    if (!filename.empty()) {
        std::cout << "  file name: \"" << filename << "\"" << std::endl;
    }
    std::cout << "  file record flags: ";
    if (rcd.fixedFields.flags == 0) {
        std::cout << "UNUSED ";
    }
    else {
        if (rcd.fixedFields.flags & 0x01) {
            std::cout << "EXISTS ";
        }
        if (rcd.fixedFields.flags & 0x02) {
            std::cout << "DIRECTORY ";
        }
    }
    std::cout << std::endl;

    for (auto &o : rcd.attrs) {
        std::cout << "  attr type:" << std::hex << std::uppercase
                  << o.fields.get()->attrType << std::endl;
        std::cout << "    is non-resident: " << std::dec
                  << (int)o.fields.get()->nonResident << std::endl;
        std::cout << "    length: " << std::dec << o.fields.get()->length
                  << std::endl;
        // std::cout << "    attr id: " << std::dec << o.fields.get()->attrId
        //           << std::endl;
        if (o.fields.get()->nameLen) {
            std::cout << "    name: " << std::dec << wstr2str(o.attrName)
                      << std::endl;
        }
        if (o.fields.get()->attrType == tamper::NTFS_FILE_NAME) {
            tamper::TypeData_FILE_NAME fileInfo = o.attrData;
            std::cout << "    flags: " << std::hex << fileInfo.fileInfo.flags
                      << std::endl;
        }
        if (o.fields.get()->nonResident) {
            tamper::NtfsAttr::NonResidentPart &nonResidentPart =
                (tamper::NtfsAttr::NonResidentPart &)*o.fields.get();
            std::cout << "    Virtual Clusters count: "
                      << nonResidentPart.VCN_end - nonResidentPart.VCN_beg + 1
                      << std::endl;
            std::cout << "    data runs: " << std::endl;
            ShowData((tamper::NtfsDataBlock)o.attrData, o.attrData.len(), 16,
                     6);
        }
    }
}

// 显示 文件记录 的 16 进制数据
void ShowFileRecordHex(tamper::Ntfs &disk, uint64_t idx) {
    uint64_t rcdIdx = idx;
    std::vector<tamper::SuccessiveSectors> area =
        disk.GetFileRecordAreaByIndex(rcdIdx);
    tamper::NtfsDataBlock block = disk.ReadSectors(area);
    tamper::Ntfs_FILE_Record trcd = block;
    std::vector<char> data = block;
    ShowData(&data[0], data.size());
}

int main() {
    // 选择并加载卷
    tamper::Ntfs disk = LoadVolume();
    tamper::Ntfs disk2 = std::move(disk);
    int choose = 0;

    if (!disk.IsOpen()) {
        // 打开失败
        std::cout << "load failure!" << std::endl;
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
        std::cout << "input file record index: ";
        std::cin >> rcdIdx; // 258172 29_daily
        ShowFileRecordHex(disk, rcdIdx);
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
