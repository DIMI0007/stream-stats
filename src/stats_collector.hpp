#pragma once

#include "stats_model.hpp"
#include <QElapsedTimer>
#include <QString>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace stats {

struct SystemReading {
    double cpuPc = -1,cpuObs = -1,gpu = -1;
    quint64 ramObs = 0,ramFree = 0;
    double fps = 0,renderMs = 0;
    quint64 renderMissed = 0,renderFrames = 0;
    quint64 encodeSkipped = 0,encodeFrames = 0;
};
struct RecordReading {
    bool active = false,paused = false;
    double kbps = 0,megabytesPerSecond = 0;
    quint64 fileBytes = 0,freeBytes = 0;
    qint64 elapsedSeconds = 0,secondsUntilFull = -1;
    bool fileSizeAvailable = false,fileSizeEstimated = false,diskAvailable = false;
};
struct Snapshot {
    SystemReading system;
    StreamReading stream;
    RecordReading record;
};

class Collector {
public:
    Collector();
    ~Collector();
    Collector(const Collector&)=delete;
    Collector& operator=(const Collector&)=delete;
    Snapshot sample();
private:
    void gpuLoop();
    std::atomic<bool> stopGpu_{false};
    std::atomic<double> gpuPercent_{-1};
    std::mutex gpuMutex_;
    std::condition_variable gpuWake_;
    std::thread gpuThread_;
    QElapsedTimer clock_;
    qint64 previousMs_ = 0;
    quint64 systemIdle_=0,systemKernel_=0,systemUser_=0;
    quint64 processKernel_=0,processUser_=0;
    bool cpuReady_=false;
    StreamAccumulator streams_;
    quint64 recordPreviousBytes_=0;
    bool recordPreviousReady_=false;
    double recordSmoothedBytesPerSecond_=0;
    double recordElapsedSeconds_ = 0;
    bool recordWasActive_ = false;
    QString recordPath_;
};

} // namespace stats
