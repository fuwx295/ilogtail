// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "HistoryFileImporter.h"
#include "common/Thread.h"
#include "common/TimeUtil.h"
#include "common/RuntimeUtil.h"
#include "common/FileSystemUtil.h"
#include "config_manager/ConfigManager.h"
#include "processor/daemon/LogProcess.h"
#include "logger/Logger.h"
#include "reader/LogFileReader.h"

namespace logtail {

HistoryFileImporter::HistoryFileImporter() {
    LOG_INFO(sLogger, ("HistoryFileImporter", "init"));
    mThread = CreateThread([this]() { Run(); });
}

void HistoryFileImporter::PushEvent(const HistoryFileEvent& event) {
    LOG_INFO(sLogger, ("push event", event.String()));
    mEventQueue.PushItem(event);
}

void HistoryFileImporter::Run() {
    while (true) {
        HistoryFileEvent event;
        mEventQueue.PopItem(event);
        std::vector<std::string> objList;
        if (!GetAllFiles(event.mDirName, event.mFileName, objList)) {
            LOG_WARNING(sLogger, ("get all files", "failed"));
            continue;
        }
        ProcessEvent(event, objList);
    }
}

void HistoryFileImporter::LoadCheckPoint() {
    std::string historyDataPath = GetProcessExecutionDir() + "history_file_checkpoint";
    FILE* readPtr = fopen(historyDataPath.c_str(), "r");
    if (readPtr != NULL) {
        fclose(readPtr);
    }
}

void HistoryFileImporter::ProcessEvent(const HistoryFileEvent& event, const std::vector<std::string>& fileNames) {
    static LogProcess* logProcess = LogProcess::GetInstance();

    auto& config = event.mConfig;
    LOG_INFO(sLogger, ("begin load history files, count", fileNames.size())("file list", ToString(fileNames)));
    for (size_t i = 0; i < fileNames.size(); ++i) {
        auto startTime = GetCurrentTimeInMilliSeconds();
        const std::string& fileName = fileNames[i];
        const std::string filePath = PathJoin(event.mDirName, fileName);
        LOG_INFO(sLogger,
                 ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")(
                     "process", "begin")("file", filePath));

        // create reader
        DevInode devInode = GetFileDevInode(filePath);
        if (!devInode.IsValid()) {
            LOG_WARNING(sLogger,
                        ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")(
                            "process", "failed")("file", filePath)("reason", "invalid dev inode"));
            continue;
        }

        LogFileReaderPtr readerSharePtr(config->CreateLogFileReader(event.mDirName, fileName, devInode, true));
        if (readerSharePtr == NULL) {
            LOG_WARNING(sLogger,
                        ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")(
                            "process", "failed")("file", filePath)("reason", "create log file reader failed"));
            continue;
        }
        if (!readerSharePtr->UpdateFilePtr()) {
            LOG_WARNING(sLogger,
                        ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")(
                            "process", "failed")("file", filePath)("reason", "open file ptr failed"));
            continue;
        }
        readerSharePtr->SetLastFilePos(event.mStartPos);
        readerSharePtr->CheckFileSignatureAndOffset(false);

        bool doneFlag = false;
        lastPushBufferTime = GetCurrentTimeInMicroSeconds();
        while (true) {
            while (!logProcess->IsValidToReadLog(readerSharePtr->GetLogstoreKey())) {
                usleep(1000 * 10);
            }
            LogBuffer* logBuffer = new LogBuffer;
            readerSharePtr->ReadLog(*logBuffer, nullptr);
            if (!logBuffer->rawBuffer.empty()) {
                logBuffer->logFileReader = readerSharePtr;
                FlowControl(logBuffer->rawBuffer.size(), event.mRate);
                logProcess->PushBuffer(logBuffer, 100000000);
            } else {
                delete logBuffer;
                // when ReadLog return false, retry once
                if (doneFlag) {
                    break;
                }
                doneFlag = true;
            }
        }
        auto doneTime = GetCurrentTimeInMilliSeconds();
        LOG_INFO(sLogger,
                 ("[progress]", std::string("[") + ToString(i + 1) + "/" + ToString(fileNames.size()) + "]")("process",
                                                                                                             "done")(
                     "file", filePath)("offset", readerSharePtr->GetLastFilePos())("time(ms)", doneTime - startTime));
    }
}

void HistoryFileImporter::FlowControl(uint32_t bufferSize, double rate) {
    if (rate == 0) {
        return;
    }
    auto now = GetCurrentTimeInMicroSeconds();
    // rate is in bytes per second
    double realRateTime = now - lastPushBufferTime;
    if (bufferSize * 1000000 > realRateTime * rate) {
        double limitRateTime = bufferSize / rate * 1000000;
        usleep(limitRateTime - realRateTime);
        LOG_DEBUG(sLogger,
                  ("history file import wait because of flow control, wait time in microsecond:",
                   limitRateTime - realRateTime));
    }
    lastPushBufferTime = GetCurrentTimeInMicroSeconds();
}
} // namespace logtail
