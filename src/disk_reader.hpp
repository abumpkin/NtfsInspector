#pragma once
#include <Windows.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace tamper {
    struct SuccessiveSectors {
        uint64_t startSecId;
        uint64_t secNum;
    };

    class DiskReader {
        HANDLE fh;
        DWORD error;
        DISK_SPACE_INFORMATION diskInfo;
        char *sectorCache;

    private:
        struct SizeCompounds {
            uint32_t l;
            uint32_t h;
        };

    public:
        DiskReader(std::string file) : diskInfo{0} {
            sectorCache = nullptr;
            fh = CreateFileA(file.c_str(), GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             OPEN_EXISTING, 0, NULL);
            if (fh == INVALID_HANDLE_VALUE) {
                error = GetLastError();
                return;
            }
            GetDiskSpaceInformationA((file + "\\").c_str(), &diskInfo);
            sectorCache = new char[diskInfo.BytesPerSector];
        }
        ~DiskReader() {
            if (fh != INVALID_HANDLE_VALUE) {
                CloseHandle(fh);
            }
            if (sectorCache) {
                delete[] sectorCache;
            }
        }
        bool IsOpen() const { return fh != INVALID_HANDLE_VALUE; }
        uint32_t GetSectorSize() const { return diskInfo.BytesPerSector; }
        uint64_t GetTotalSize() const {
            return diskInfo.ActualTotalAllocationUnits *
                   diskInfo.SectorsPerAllocationUnit * diskInfo.BytesPerSector;
        }
        std::vector<char> ReadSector(uint64_t secId) {
            char *(&data) = sectorCache;
            DWORD rd;
            std::vector<char> ret;
            SizeCompounds pos;
            ((uint64_t &)pos) = (uint64_t)GetSectorSize() * secId;
            if (fh == INVALID_HANDLE_VALUE || diskInfo.BytesPerSector == 0) {
                throw std::runtime_error("fail");
            }
            SetFilePointer(fh, pos.l, (PLONG)&pos.h, FILE_BEGIN);
            if (ReadFile(fh, data, diskInfo.BytesPerSector, &rd, NULL)) {
                ret.resize(rd);
                memcpy_s(ret.data(), rd, data, diskInfo.BytesPerSector);
                if (rd < diskInfo.BytesPerSector) {
                    throw std::runtime_error("fail");
                }
                return ret;
            }
            return ret;
        }
        int32_t WriteSector(uint64_t secId, std::vector<char> data) {
            DWORD wd;
            DWORD stat;
            SizeCompounds pos;
            ((uint64_t &)pos) = (uint64_t)GetSectorSize() * secId;
            if (data.size() < GetSectorSize()) {
                data.resize(GetSectorSize());
            }
            if (fh == INVALID_HANDLE_VALUE || diskInfo.BytesPerSector == 0) {
                return 0;
            }
            SetFilePointer(fh, pos.l, (PLONG)&pos.h, FILE_BEGIN);
            // 锁定卷
            DeviceIoControl(fh, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &stat,
                            NULL);
            WriteFile(fh, data.data(), GetSectorSize(), &wd, NULL);
            // 解锁
            DeviceIoControl(fh, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &stat,
                            NULL);
            return wd;
        }
        std::vector<char> ReadSectors(uint64_t secId, uint64_t secNum) {
            // std::vector<char> ret, t;
            // uint64_t pos;
            // try {
            //     for (int i = secId; i < secId + secNum; i++) {
            //         t = ReadSector(i);
            //         if (t.size()) {
            //             pos = ret.size();
            //             ret.resize(ret.size() + t.size());
            //             memcpy_s(&ret[pos], t.size(), t.data(), t.size());
            //         }
            //     }
            //     return ret;
            // }
            // catch (std::exception &e) {
            //     throw e;
            // }
            char *(&data) = sectorCache;
            DWORD rd;
            std::vector<char> ret;
            SizeCompounds pos;
            ((uint64_t &)pos) = (uint64_t)GetSectorSize() * secId;
            if (fh == INVALID_HANDLE_VALUE || diskInfo.BytesPerSector == 0) {
                throw std::runtime_error("fail");
            }
            SetFilePointer(fh, pos.l, (PLONG)&pos.h, FILE_BEGIN);
            ret.resize(diskInfo.BytesPerSector * secNum);
            if (ReadFile(fh, ret.data(), ret.size(), &rd, NULL)) {
                if (rd < ret.size()) {
                    throw std::runtime_error("fail");
                }
                return ret;
            }
            return ret;
        }
        std::vector<char> ReadSectors(std::vector<SuccessiveSectors> &secs) {
            std::vector<char> ret, t;
            uint64_t pos;
            try {
                for (auto const &i : secs) {
                    t = ReadSectors(i.startSecId, i.secNum);
                    pos = ret.size();
                    ret.resize(ret.size() + t.size());
                    memcpy_s(&ret[pos], t.size(), t.data(), t.size());
                }
            }
            catch (std::exception &e) {
                throw e;
            }
            return ret;
        }
    };
}