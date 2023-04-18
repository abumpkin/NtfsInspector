#include "ntfs_access.hpp"

// AttrData_DATA 定义
namespace abkntfs {
    AttrData_DATA::AttrData_DATA(NtfsAttr const &attr,
                                 NtfsDataBlock const &data, bool isResident)
        : NtfsStructureBase(true), VCN_beg(0), VCN_end(0) {
        // 拷贝数据
        residentData = data.Copy();
        this->isResident = isResident;
        dataSize = residentData.len();
        pNtfs = data.pNtfs;
        if (!isResident) {
            NtfsAttr::NonResidentPart &nonRD =
                (NtfsAttr::NonResidentPart &)*attr.fields.get();
            dataRunsMap = data.pNtfs->DataRunsToSectorsInfo(data, attr);
            VCN_beg = nonRD.VCN_beg;
            VCN_end = nonRD.VCN_end;
            dataSize = nonRD.realSize;
            dataSectorsCount = (VCN_end - VCN_beg + 1) *
                               data.pNtfs->bootInfo.sectorsPerCluster;
        }
    }

    NtfsDataBlock AttrData_DATA::ReadData(uint64_t offset, uint64_t size) {
        NtfsDataBlock ret;
        if (nullptr == pNtfs) {
            return ret;
        }
        if (offset > dataSize) {
            return ret;
        }
        if (offset + size > dataSize) {
            size = dataSize - offset;
        }
        if (isResident) {
            ret = NtfsDataBlock{residentData, offset, size}.Copy();
        }
        else {
            uint64_t sectorSize = pNtfs->GetSectorSize();
            uint64_t startingSector = offset / sectorSize;
            uint64_t offInSec = offset - startingSector * sectorSize;
            uint64_t sectorsNum = 1 + (size + offInSec + 1) / sectorSize;
            NtfsSectorsInfo needToRead =
                pNtfs->VSN_To_LSN(dataRunsMap, startingSector, sectorsNum);
            ret = NtfsDataBlock{pNtfs->ReadSectors(needToRead),
                                offset - startingSector * sectorSize, size};
        }
        return ret;
    }
}

// AttrData_INDEX_ALLOCATION 定义
namespace abkntfs {
    AttrData_INDEX_ALLOCATION::AttrData_INDEX_ALLOCATION(
        NtfsDataBlock &dataRuns, AttrData_INDEX_ROOT &indexRoot)
        : NtfsStructureBase(true) {
        secs = dataRuns.pNtfs->DataRunsToSectorsInfo(dataRuns);
        this->indexRoot = &indexRoot;
        this->pNtfs = dataRuns.pNtfs;
        if (this->indexRoot == nullptr || this->pNtfs == nullptr) {
            Reset();
            return;
        }
    }

    std::vector<NtfsIndexRecord> &AttrData_INDEX_ALLOCATION::GetIRs() {
        if (!this->loaded) {
            NtfsDataBlock rawIRsData = pNtfs->ReadSectors(secs);
            if (!rawIRsData.len()) {
                Reset();
                return IRs;
            }
            uint64_t pos = 0;
            while (pos < rawIRsData.len()) {
                NtfsIndexRecord t = {NtfsDataBlock{rawIRsData, pos},
                                     indexRoot->rootInfo.attrType};
                // 就算 t 是无效的也要保存记录 (方便通过 索引记录号 查找
                // 索引记录). if (!t.valid) {
                //    Reset();
                //    return;
                //}
                this->IRs.push_back(t);
                pos += indexRoot->rootInfo.sizeofIB;
            }
        }
        return IRs;
    }
}
