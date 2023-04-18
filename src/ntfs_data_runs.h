#pragma once
#include "ntfs_access.hpp"

namespace abkntfs {
    class NtfsDataRuns {
    public:
        struct Partition {
            uint8_t sizeOfLengthField : 4;
            uint8_t sizeOfOffField : 4;
        };
        static uint64_t ParseDataRuns(
            NtfsDataBlock const &data,
            std::function<bool(Partition partInfo, uint64_t lcn, uint64_t num)>
                callback) {
            uint64_t pos = 0;
            uint64_t LCN = 0;
            struct Partition fieldsSize;
            uint64_t clusterNum = 0;
            int64_t offsetOfLCN = 0;
            while (pos < data.len()) {
                if (data[pos] == 0) {
                    pos++;
                    break;
                }
                ((uint8_t &)fieldsSize) = data[pos];
                pos++;
                // 不支持大于 8 字节的字段
                if (fieldsSize.sizeOfLengthField > 8 ||
                    fieldsSize.sizeOfOffField > 8) {
                    // throw std::runtime_error{
                    //     "size of field greater than 8 bytes."};
                    break;
                }
                clusterNum = 0;
                memcpy(&clusterNum, &data[pos], fieldsSize.sizeOfLengthField);
                pos += fieldsSize.sizeOfLengthField;
                offsetOfLCN = 0;
                if (data[pos + fieldsSize.sizeOfOffField - 1] < 0) {
                    offsetOfLCN = -1ll;
                }
                memcpy(&offsetOfLCN, &data[pos], fieldsSize.sizeOfOffField);
                pos += fieldsSize.sizeOfOffField;
                LCN += offsetOfLCN;
                if (!callback(fieldsSize, LCN, clusterNum)) {
                    break;
                }
            }
            return pos;
        }

        static std::vector<char> GetDataRuns(NtfsDataBlock const &data,
                                             uint64_t VC_len) {
            auto callback = [&](Partition partInfo, uint64_t lcn,
                                uint64_t num) -> bool {
                if (VC_len >= num) {
                    VC_len -= num;
                    return true;
                }
                return false;
            };
            uint64_t dataRunsLen = ParseDataRuns(data, callback);
            std::vector<char> ret;
            ret.assign(&data[0], &data[dataRunsLen]);
            return ret;
        }
    };
}
