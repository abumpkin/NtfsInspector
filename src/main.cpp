#include "disk_reader.hpp"
#include "find_devices.hpp"
#include "ntfs_access.hpp"
#include <codecvt>
#include <iomanip>
#include <iostream>
#include <locale>
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
            std::cout << "Offset: 0x" << std::hex << std::uppercase
                      << std::setw(8) << std::setfill('0') << dpos + width;
            std::cout << "  BlockNumber: " << std::dec << blockNum;
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

    return tamper::Ntfs{volumePath};
}

// 显示卷信息
void ShowVolumeInfo(tamper::Ntfs &disk) {
    std::cout << "total size: " << std::dec
              << FriendlyFileSize(disk.GetTotalSize()) << std::endl;
    // 输出 $Boot 信息
    std::cout << "$Boot info:" << std::endl;
    std::cout << "  sectors per cluster: "
              << (int)disk.bootInfo.sectorsPerCluster << std::endl;
    std::cout << "  clusters per MFT record: "
              << disk.bootInfo.clustersPer_MFT_Record << std::endl;
    std::cout << "  Logic cluster number of MFT: " << disk.bootInfo.LCNoVCN0oMFT
              << std::endl;
    // 获得 MFT Areas
    auto secs = disk.MFT_FileRecord.GetDataAttr()->attrData.sectors;
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
        tamper::NtfsAttr &oRef = *o.get();
        std::cout << "  attr type:" << std::hex << std::uppercase
                  << oRef.GetAttributeType() << std::endl;
        std::cout << "    is non-resident: " << std::dec
                  << (int)oRef.fields.get()->nonResident << std::endl;
        std::cout << "    length: " << std::dec << oRef.GetAttributeLength()
                  << std::endl;
        if (!oRef.attrName.empty()) {
            std::cout << "    name: " << std::dec << wstr2str(oRef.attrName)
                      << std::endl;
        }
        if (oRef.GetAttributeType() == tamper::NTFS_FILE_NAME) {
            tamper::TypeData_FILE_NAME fileInfo = oRef.attrData;
            std::cout << "    flags: " << std::hex << fileInfo.fileInfo.flags
                      << std::endl;
        }
        if (oRef.GetAttributeType() == tamper::NTFS_INDEX_ROOT) {
            tamper::TypeData_INDEX_ROOT indexRoot = oRef.attrData;
            std::cout << "    indexed Attribute Type: " << std::hex
                      << std::uppercase << indexRoot.rootInfo.attrType
                      << std::endl;
            std::cout << "    size of Index Block: " << std::dec
                      << indexRoot.rootInfo.sizeofIB << std::endl;
            std::cout << "    type of the Index:";
            if (indexRoot.indexHeader.flags ==
                indexRoot.HEADER_FLAG_SMALL_INDEX) {
                std::cout << " SMALL_INDEX";
            }
            else if (indexRoot.indexHeader.flags ==
                     indexRoot.HEADER_FLAG_LARGE_INDEX)
                std::cout << " LARGE_INDEX";
            else
                std::cout << " 0x" << std::hex << indexRoot.indexHeader.flags;
            std::cout << std::endl;

            std::cout << "    Entries: " << std::endl;
            for (int i = 0; i < indexRoot.indexEntries.size(); i++) {
                tamper::NtfsIndexEntry &entry = indexRoot.indexEntries[i];
                std::cout << "      Entry [" << std::dec << i << "]"
                          << std::endl;
                std::cout << "        flags:";
                if (entry.entryHeader.flags & entry.FLAG_IE_POINT_TO_SUBNODE) {
                    std::cout << " POINT_TO_SUBNODE";
                }
                if (entry.entryHeader.flags &
                    entry.FLAG_LAST_ENTRY_IN_THE_NODE) {
                    std::cout << " LAST_ENTRY_IN_THE_NODE";
                }
                if (entry.entryHeader.flags == 0) std::cout << " None";
                std::cout << std::endl;
                std::cout << "        point to file record: " << std::dec
                          << entry.entryHeader.fileReference.fileRecordNum
                          << std::endl;
                if (entry.streamType == tamper::NTFS_FILE_NAME) {
                    tamper::TypeData_FILE_NAME pointed = entry.stream;
                    if (pointed.filename.size())
                        std::cout << "        point to Filename: "
                                  << wstr2str(pointed.filename) << std::endl;
                }
                if (entry.stream.len()) {
                    std::cout << "        stream data hex:" << std::endl;
                    ShowData(entry.stream, entry.stream.len(), 16, 10);
                }
                if (indexRoot.indexHeader.flags ==
                    indexRoot.HEADER_FLAG_LARGE_INDEX) {
                    std::cout
                        << "        point to Index Records number:" << std::dec
                        << entry.pIndexRecordNumber << std::endl;
                }
            }
        }
        if (!oRef.IsResident()) {
            std::cout << "    Virtual Clusters count: "
                      << oRef.GetVirtualClusterCount() << std::endl;
            std::cout << "    data runs size: " << std::dec
                      << FriendlyFileSize(disk.GetDataRunsDataSize(
                             oRef, oRef.attrData.rawData))
                      << std::endl;
            std::cout << "    data runs: " << std::endl;
            ShowData((tamper::NtfsDataBlock)oRef.attrData, oRef.attrData.len(),
                     16, 6);
        }
        if (oRef.GetAttributeType() == tamper::NTFS_INDEX_ALLOCATION) {
            tamper::TypeData_INDEX_ALLOCATION IA = oRef.attrData;
            // std::cout << "    Raw Data:" << std::endl;
            // ShowData(IA.rawIRsData,
            // IA.rawIRsData.len(),
            //         32, 6, 16, true);
            std::cout << "    Index Records: " << std::endl;
            for (int i = 0; i < IA.IRs.size(); i++) {
                tamper::NtfsIndexRecord &IR = IA.IRs[i];
                std::cout << "      IR [" << std::dec << i << "]" << std::endl;
                if (!IR.valid) {
                    std::cout << "        NOT VALID!!!" << std::endl;
                    continue;
                }
                std::cout << "        is leaf node: " << std::boolalpha
                          << !IR.indexRecordHeader.notLeafNode << std::endl;
                std::cout << "        Index Entries:" << std::endl;
                for (int p = 0; p < IR.IEs.size(); p++) {
                    tamper::NtfsIndexEntry &entry = IR.IEs[p];
                    std::cout << "          Entry [" << std::dec << p << "]"
                              << std::endl;
                    std::cout << "            flags:";
                    if (entry.entryHeader.flags &
                        entry.FLAG_IE_POINT_TO_SUBNODE) {
                        std::cout << " POINT_TO_SUBNODE";
                    }
                    if (entry.entryHeader.flags &
                        entry.FLAG_LAST_ENTRY_IN_THE_NODE) {
                        std::cout << " LAST_ENTRY_IN_THE_NODE";
                    }
                    if (entry.entryHeader.flags == 0) std::cout << " None";
                    std::cout << std::endl;
                    std::cout
                        << "            point to file record: " << std::dec
                        << entry.entryHeader.fileReference.fileRecordNum
                        << std::endl;
                    if (!IR.indexRecordHeader.notLeafNode) {
                        tamper::TypeData_FILE_NAME pointed = entry.stream;
                        std::cout << "            point to Filename: "
                                  << wstr2str(pointed.filename) << std::endl;
                    }
                    else {
                        std::cout << "            stream data hex:"
                                  << std::endl;
                        ShowData(entry.stream, entry.stream.len(), 16, 14);
                        std::cout
                            << "            point to Index Records number: "
                            << std::dec << entry.pIndexRecordNumber
                            << std::endl;
                    }
                }
            }
        }
    }
}

// 显示 文件记录 的 16 进制数据
void ShowFileRecordHex(tamper::Ntfs &disk, uint64_t idx,
                       uint32_t preSpace = 2) {
    uint64_t rcdIdx = idx;
    std::vector<tamper::SuccessiveSectors> area =
        disk.GetFileRecordAreaByIndex(rcdIdx);
    tamper::NtfsDataBlock block = disk.ReadSectors(area);
    tamper::Ntfs_FILE_Record trcd = block;
    std::vector<char> data = block;
    ShowData(&data[0], data.size(), 32, preSpace);
}

int main() {
    // 选择并加载卷
    tamper::Ntfs disk = LoadVolume();
    // tamper::Ntfs disk2 = std::move(disk);
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
