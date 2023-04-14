#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>

namespace tamper {
    class Devices {
        struct dev {
            std::string guidPath;
            std::string volumeName;
            std::string fileSystem;
            std::string volumePath;
        };

    public:
        std::vector<dev> devs;

        Devices() {
            const int bufLen = 1000;
            char strBuf[bufLen];
            HANDLE volumeHwd = FindFirstVolumeA(strBuf, bufLen);
            devs.emplace_back(dev{strBuf});
            if (volumeHwd == INVALID_HANDLE_VALUE) {
                return;
            }
            while (FindNextVolumeA(volumeHwd, strBuf, bufLen)) {
                devs.emplace_back(dev{strBuf});
            }
            if (GetLastError() == ERROR_NO_MORE_FILES) {
                FindVolumeClose(volumeHwd);
            }
            for (auto &i : devs) {
                char strBuf2[bufLen];
                DWORD retLen;
                if (GetVolumeInformationA(i.guidPath.c_str(), strBuf, bufLen, NULL, NULL, NULL, strBuf2, bufLen)) {
                    i.fileSystem = strBuf2;
                    i.volumeName = strBuf;
                }
                if (GetVolumePathNamesForVolumeNameA(i.guidPath.c_str(), strBuf, bufLen, &retLen)) {
                    // for (int i = 0; i < retLen; i++) {
                    //     if (strBuf[i] == 0) {
                    //         strBuf[i] = '\n';
                    //     }
                    // }
                    i.volumePath = strBuf;
                }
            }
        }
        ~Devices() {}
    };
}
