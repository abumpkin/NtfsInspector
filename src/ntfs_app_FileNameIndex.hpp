#pragma once
#include "ntfs_access.hpp"

namespace abkntfs {
    // 对文件名的索引, $I30, $FILE_NAME
    struct NtfsFileNameIndex : NtfsStructureBase {
        struct FileInfoInIndex {
            const bool valid = false;
            AttrData_FILE_NAME fn;
            NtfsFileReference fileRef;
            FileInfoInIndex() = default;
            FileInfoInIndex(AttrData_FILE_NAME const &fn,
                            NtfsFileReference fileRef)
                : valid(true), fn{fn}, fileRef{fileRef} {};
        };

        AttrData_INDEX_ROOT::IndexRootInfo indexInfo;
        // 根节点
        NtfsIndexNode rootNode;
        std::vector<NtfsIndexRecord> IRs;

    public:
        NtfsFileNameIndex() = default;
        NtfsFileNameIndex(NtfsFileNameIndex const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
        }
        NtfsFileNameIndex &operator=(NtfsFileNameIndex const &r) {
            static_cast<NtfsStructureBase &>(*this) = r;
            return *this;
        }

        NtfsFileNameIndex(NtfsFileRecord &fileRecord)
            : NtfsStructureBase(true) {
            if (!fileRecord.valid) {
                Reset();
                return;
            }
            AttrData_INDEX_ROOT *indexRootAttrData =
                fileRecord.FindSpecAttrData(NTFS_INDEX_ROOT, L"$I30");
            if (indexRootAttrData == nullptr) {
                Reset();
                return;
            }
            this->indexInfo = indexRootAttrData->rootInfo;
            this->rootNode = indexRootAttrData->rootNode;
            if (rootNode.nodeHeader.notLeafNode) {
                AttrData_INDEX_ALLOCATION *indexAllocationData =
                    fileRecord.FindSpecAttrData(NTFS_INDEX_ALLOCATION, L"$I30");
                if (indexAllocationData == nullptr) {
                    Reset();
                    return;
                }
                this->IRs = indexAllocationData->GetIRs();
            }
        }

        // 遍历文件信息.
        void ForEachFileInfo(
            std::function<bool(FileInfoInIndex fileInfo)> callback) {
            std::stack<NtfsIndexNode> nodeTrace;
            std::stack<uint64_t> iterationTrace;
            NtfsIndexNode curNode = rootNode;
            uint64_t curIteration = 0;
            while (true) {
                if (curIteration < curNode.IEs.size()) {
                    NtfsIndexEntry &curEntry = curNode.IEs[curIteration];
                    if (curEntry.entryHeader.flags &
                        NtfsIndexEntry::FLAG_IE_POINT_TO_SUBNODE) {
                        nodeTrace.push(curNode);
                        iterationTrace.push(curIteration);
                        if (curEntry.pIndexRecordNumber >= IRs.size()) break;
                        curNode = IRs[curEntry.pIndexRecordNumber].node;
                        curIteration = 0;
                        continue;
                    }
                    curIteration++;
                    if (curEntry.entryHeader.flags &
                        NtfsIndexEntry::FLAG_LAST_ENTRY_IN_THE_NODE) {
                        continue;
                    }
                    if (!callback(FileInfoInIndex{
                            AttrData_FILE_NAME{curEntry.stream},
                            curEntry.entryHeader.fileReference})) {
                        break;
                    }
                    continue;
                }
                if (!nodeTrace.empty()) {
                    curNode = nodeTrace.top();
                    curIteration = iterationTrace.top();
                    nodeTrace.pop();
                    iterationTrace.pop();
                    NtfsIndexEntry &curEntry = curNode.IEs[curIteration];
                    if (curEntry.stream.len()) {
                        if (!callback(FileInfoInIndex{
                                AttrData_FILE_NAME{curEntry.stream},
                                curEntry.entryHeader.fileReference})) {
                            break;
                        }
                    }
                    curIteration++;
                    continue;
                }
                break;
            }
        }

        // 根据文件名查找文件
        FileInfoInIndex FindFile(std::wstring filename) {
            std::stack<NtfsIndexNode> nodeTrace;
            std::stack<uint64_t> iterationTrace;
            NtfsIndexNode curNode = rootNode;
            uint64_t curIteration = curNode.IEs.size() - 1;
            while (true) {
                NtfsIndexEntry &curEntry = curNode.IEs[curIteration];
                if (curEntry.entryHeader.flags &
                    NtfsIndexEntry::FLAG_IE_POINT_TO_SUBNODE) {
                    AttrData_FILE_NAME curFilename(curEntry.stream);
                    // 不正常
                    if (!curFilename.valid) {
                        break;
                    }
                    int cmp = filename.compare(curFilename.filename);
                    if (cmp == 0) {
                        return FileInfoInIndex{
                            curFilename, curEntry.entryHeader.fileReference};
                    }
                    if (cmp > 0) {
                        if (curIteration) {
                            curIteration--;
                            continue;
                        }
                        break;
                    }
                    nodeTrace.push(curNode);
                    iterationTrace.push(curIteration);
                    if (curEntry.pIndexRecordNumber >= IRs.size()) break;
                    curNode = IRs[curEntry.pIndexRecordNumber].node;
                    curIteration = curNode.IEs.size() - 1;
                    continue;
                }
                if (curEntry.entryHeader.flags &
                    NtfsIndexEntry::FLAG_LAST_ENTRY_IN_THE_NODE) {
                    if (curIteration) {
                        curIteration--;
                        continue;
                    }
                    break;
                }
                AttrData_FILE_NAME curFilename(curEntry.stream);
                // 不正常
                if (!curFilename.valid) {
                    break;
                }
                if (filename.compare(curFilename.filename) == 0) {
                    return FileInfoInIndex{curFilename,
                                           curEntry.entryHeader.fileReference};
                }
                if (curIteration) {
                    curIteration--;
                    continue;
                }
                break;
            }
            return FileInfoInIndex();
        }

    protected:
        virtual NtfsFileNameIndex &Copy(NtfsStructureBase const &r) override {
            using T = std::remove_reference<decltype(*this)>::type;
            T const &rr = (T const &)r;
            this->indexInfo = rr.indexInfo;
            this->rootNode = rr.rootNode;
            this->IRs = rr.IRs;
            return *this;
        }
        virtual NtfsFileNameIndex &Move(NtfsStructureBase &r) override {
            return Copy(r);
        }
    };
}