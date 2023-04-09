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

void showData(std::vector<char> &data, int width = 32) {
    uint64_t dpos = 0;
    while (dpos < data.size()) {
        for (int p = width; p; p--) {
            if (dpos >= data.size()) {
                break;
            }
            std::cout << std::uppercase << std::hex << std::setw(2)
                      << std::setfill('0') << (uint32_t)(uint8_t)data[dpos++]
                      << " ";
        }
        std::cout << std::endl;
    }
}

int main() {
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
    // try {
    tamper::Ntfs disk{volumePath};
    if (!disk.IsOpen()) {
        // 打开失败
        throw std::exception();
    }
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
    auto secs = disk.DataRunsToSectorsInfo(disk.MFT_FileRecord.GetDataAttr());
    std::cout << "MFT Areas:" << std::endl;
    for (auto &i : secs) {
        std::cout << "  start: " << std::dec << i.startSecId
                  << " num: " << i.secNum << std::endl;
    }
    // 列出所有文件名 43 CheatEngine75.exe
    std::cout << "files:" << std::endl;
    for (int i = 0; i < disk.FileRecordsNumber; i++) {
        std::vector<tamper::SuccessiveSectors> req =
            disk.GetFileRecordAreaByIndex(i);
        tamper::Ntfs_FILE_Record rcd = disk.ReadSectors(req);
        for (auto &p : rcd.attrs) {
            if (p.fields.get()->length > 1000 /* p.fields.get()->attrType == tamper::NTFS_INDEX_ALLOCATION */) {
                // tamper::TypeData_FILE_NAME *fileInfo =
                //     (tamper::TypeData_FILE_NAME *)p.attrData.data();
                if (true) {
                    std::cout << "Area:" << std::dec << "[" << i << "]"
                              << std::endl;
                    for (auto &i : req) {
                        std::cout << "  start: " << std::dec << i.startSecId
                                  << " num: " << i.secNum << std::endl;
                    }
                    std::string filename = wstr2str(rcd.GetFileName());
                    if (!filename.empty()) {
                        std::cout << "  filename: \"" << filename << "\""
                                  << std::endl;
                    }
                    std::cout << "  file record flags: " << std::hex
                              << rcd.fixedFields.flags << std::endl;
                    for (auto &o : rcd.attrs) {
                        std::cout << "  attr type:" << std::hex
                                  << o.fields.get()->attrType << std::endl;
                        std::cout << "    is non-resident: " << std::dec
                                  << (int)o.fields.get()->nonResident
                                  << std::endl;
                        std::cout << "    length: " << std::dec
                                  << o.fields.get()->length << std::endl;
                        std::cout << "    attr id: " << std::dec
                                  << o.fields.get()->attrId << std::endl;
                        if (o.fields.get()->nameLen) {
                            std::cout << "    name: " << std::dec
                                      << wstr2str(o.attrName) << std::endl;
                        }
                        if (o.fields.get()->attrType ==
                            tamper::NTFS_FILE_NAME) {
                            tamper::TypeData_FILE_NAME *fileInfo =
                                (tamper::TypeData_FILE_NAME *)o.attrData.data();
                            std::cout << "    flags: " << std::hex
                                      << fileInfo->flags << std::endl;
                        }
                        if (o.fields.get()->attrType ==
                            tamper::NTFS_INDEX_ALLOCATION) {
                            tamper::NtfsAttr::NonResidentPart &nonResidentPart =
                                (tamper::NtfsAttr::NonResidentPart &)*o.fields
                                    .get();
                            std::cout << "    alloc size: " << std::dec
                                      << nonResidentPart.allocSize << std::endl;
                            std::cout << "    real size: " << std::dec
                                      << nonResidentPart.realSize << std::endl;
                            std::cout << "    data runs size:" << std::dec
                                      << nonResidentPart.length -
                                             nonResidentPart.offToDataRuns
                                      << std::endl;
                            std::cout << "    offset to data runs:" << std::dec
                                      << nonResidentPart.offToDataRuns
                                      << std::endl;
                        }
                    }
                }
            }
        }
    }
    while (true) {
        std::cout << "input file record index: ";
        std::cin >> vol_i; // 258172 29_daily
        std::vector<tamper::SuccessiveSectors> area = disk.GetFileRecordAreaByIndex(vol_i);
        tamper::NtfsDataBlock block = disk.ReadSectors(area);
        tamper::Ntfs_FILE_Record trcd = block;
        data = block;
        showData(data);
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
