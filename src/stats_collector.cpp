#include "stats_collector.hpp"
#ifdef STREAM_STATS_PREVIEW
namespace stats {
Collector::Collector() {clock_.start();}
Collector::~Collector() = default;
Snapshot Collector::sample() {
    static int tick=0;++tick;
    Snapshot value;
    value.system.cpuPc=12.0+double(tick%6);
    value.system.cpuObs=0.9+double(tick%4)/10.0;
    value.system.gpu=24.0;
    value.system.ramObs=381ull*1024*1024;
    value.system.ramFree=quint64(18.6*1024*1024*1024);
    value.system.fps=60;value.system.renderMs=0.9;
    value.system.renderFrames=54974;value.system.encodeFrames=54974;
    value.stream.active=true;value.stream.activeOutputs=2;
    value.stream.kbps=32200+double(tick%8)*700;
    value.stream.totalBytes=quint64(19.36*1024*1024*1024);
    value.stream.frames=54974;
    value.record.active=true;value.record.kbps=48500+double(tick%6)*400;
    value.record.megabytesPerSecond=6.1;
    value.record.fileBytes=quint64(26.43*1024*1024*1024);
    value.record.freeBytes=319ull*1024*1024*1024;
    value.record.elapsedSeconds=4894;value.record.secondsUntilFull=51120;
    value.record.fileSizeAvailable=true;value.record.diskAvailable=true;
    return value;
}
void Collector::gpuLoop() {}
} // namespace stats
#else
#include <obs.h>
#include <obs-frontend-api.h>
#include <media-io/video-io.h>
#include <util/bmem.h>
#include <QFileInfo>
#include <QStorageInfo>
#include <QUrl>
#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>

namespace stats {
namespace {

quint64 fileTime(const FILETIME& time) {
    ULARGE_INTEGER value{};value.LowPart=time.dwLowDateTime;value.HighPart=time.dwHighDateTime;
    return value.QuadPart;
}

bool networkEndpoint(const char* raw) {
    if (!raw || !*raw) return false;
    const QString scheme=QUrl(QString::fromUtf8(raw)).scheme().toLower();
    return scheme=="rtmp" || scheme=="rtmps" || scheme=="srt" ||
           scheme=="rist" || scheme=="rtsp" || scheme=="rtp" ||
           scheme=="udp" || scheme=="whip" || scheme=="http" || scheme=="https";
}

bool hasNetworkDestination(obs_output_t* output) {
    obs_data_t* settings=obs_output_get_settings(output);
    if(!settings)return false;
    bool found=false;
    for(const char* name:{"url","server","destination","target"})
        found |= networkEndpoint(obs_data_get_string(settings,name));
    obs_data_release(settings);
    return found;
}

struct EnumerateContext {
    QVector<OutputReading>* readings;
    obs_output_t* mainOutput;
    obs_output_t* recordOutput;
    obs_output_t* replayOutput;
};

bool collectOutput(void* opaque,obs_output_t* output) {
    auto& context=*static_cast<EnumerateContext*>(opaque);
    const bool active=obs_output_active(output);
    const bool reconnecting=obs_output_reconnecting(output);
    if ((!active&&!reconnecting)||output==context.recordOutput||output==context.replayOutput)
        return true;
    const auto flags=obs_output_get_flags(output);
    const bool service=(flags&OBS_OUTPUT_SERVICE)!=0;
    const bool stream=service||output==context.mainOutput||hasNetworkDestination(output);
    QString descriptor=QString::fromUtf8(obs_output_get_id(output));
    if(const char* name=obs_output_get_name(output))descriptor += " "+QString::fromUtf8(name);
    descriptor=descriptor.toLower();
    // OBS has no universal flag that distinguishes every third-party network
    // output from every custom file output. Never silently call an unknown
    // active output a complete stream total.
    const bool knownLocal=descriptor.contains("record")||descriptor.contains("replay")||
        descriptor.contains("virtualcam")||descriptor.contains("virtual_camera")||
        descriptor.contains("ffmpeg_muxer")||descriptor.contains("file_output");
    const bool possible=!stream&&!knownLocal;
    OutputReading reading;
    reading.identity=reinterpret_cast<quintptr>(output);
    if(const char* name=obs_output_get_name(output))reading.name=QString::fromUtf8(name);
    reading.active=active;reading.reconnecting=reconnecting;
    reading.stream=stream;reading.possibleUnmeasuredStream=possible;
    if(stream) {
        reading.bytes=obs_output_get_total_bytes(output);
        reading.frames=quint64(std::max(0,obs_output_get_total_frames(output)));
        reading.dropped=quint64(std::max(0,obs_output_get_frames_dropped(output)));
    }
    context.readings->push_back(reading);
    return true;
}

} // namespace

Collector::Collector() {
    clock_.start();
    gpuThread_=std::thread([this]{gpuLoop();});
}
Collector::~Collector() {
    stopGpu_=true;gpuWake_.notify_all();
    if(gpuThread_.joinable())gpuThread_.join();
}

Snapshot Collector::sample() {
    Snapshot result;
    const qint64 nowMs=clock_.elapsed();
    const double seconds=previousMs_>0?double(nowMs-previousMs_)/1000.0:0;
    previousMs_=nowMs;

    FILETIME idle{},kernel{},user{},create{},exit{},procKernel{},procUser{};
    if(GetSystemTimes(&idle,&kernel,&user) &&
       GetProcessTimes(GetCurrentProcess(),&create,&exit,&procKernel,&procUser)) {
        const quint64 i=fileTime(idle),k=fileTime(kernel),u=fileTime(user);
        const quint64 pk=fileTime(procKernel),pu=fileTime(procUser);
        if(cpuReady_ && k>=systemKernel_ && u>=systemUser_ && i>=systemIdle_ &&
           pk>=processKernel_ && pu>=processUser_) {
            const quint64 total=(k-systemKernel_)+(u-systemUser_);
            const quint64 busy=total>i-systemIdle_?total-(i-systemIdle_):0;
            if(total) {
                result.system.cpuPc=std::clamp(100.0*double(busy)/double(total),0.0,100.0);
                result.system.cpuObs=std::clamp(100.0*double((pk-processKernel_)+(pu-processUser_))/double(total),0.0,100.0);
            }
        }
        systemIdle_=i;systemKernel_=k;systemUser_=u;
        processKernel_=pk;processUser_=pu;cpuReady_=true;
    }
    PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
    if(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory)))
        result.system.ramObs=memory.WorkingSetSize;
    MEMORYSTATUSEX machine{};machine.dwLength=sizeof(machine);
    if(GlobalMemoryStatusEx(&machine))result.system.ramFree=machine.ullAvailPhys;
    result.system.gpu=gpuPercent_.load();
    result.system.fps=obs_get_active_fps();
    result.system.renderMs=double(obs_get_average_frame_time_ns())/1000000.0;
    result.system.renderMissed=obs_get_lagged_frames();
    result.system.renderFrames=obs_get_total_frames();
    if(auto* video=obs_get_video()) {
        result.system.encodeSkipped=video_output_get_skipped_frames(video);
        result.system.encodeFrames=video_output_get_total_frames(video);
    }

    obs_output_t* mainOutput=obs_frontend_get_streaming_output();
    obs_output_t* recordOutput=obs_frontend_get_recording_output();
    obs_output_t* replayOutput=obs_frontend_get_replay_buffer_output();
    QVector<OutputReading> readings;
    EnumerateContext context{&readings,mainOutput,recordOutput,replayOutput};
    obs_enum_outputs(collectOutput,&context);
    // Some frontend integrations expose their primary output without it being
    // present in the enum snapshot. The accumulator deduplicates by identity.
    if(mainOutput && obs_output_active(mainOutput))collectOutput(&context,mainOutput);
    result.stream=streams_.update(readings,seconds);
    if(mainOutput)obs_output_release(mainOutput);
    if(replayOutput)obs_output_release(replayOutput);

    result.record.active=obs_frontend_recording_active();
    result.record.paused=result.record.active&&obs_frontend_recording_paused();
    if(result.record.active) {
        if(!recordWasActive_){
            recordElapsedSeconds_=0;recordPreviousReady_=false;
            recordSmoothedBytesPerSecond_=0;
        }else if(!result.record.paused&&seconds>0&&seconds<10){
            recordElapsedSeconds_+=seconds;
        }
        recordWasActive_=true;
        result.record.elapsedSeconds=qint64(recordElapsedSeconds_);
        char* raw=obs_frontend_get_current_record_output_path();
        QString directory=raw?QString::fromUtf8(raw):QString();
        if(raw)bfree(raw);
        // The frontend API returns the configured recording directory, not
        // the current filename. Never use its size or creation time as a file.
        QString currentFile;
        if(recordOutput){
            obs_data_t* settings=obs_output_get_settings(recordOutput);
            if(settings){
                for(const char* key:{"path","url"}){
                    const QString candidate=QString::fromUtf8(obs_data_get_string(settings,key));
                    if(!candidate.isEmpty()&&QFileInfo(candidate).isFile()){
                        currentFile=candidate;break;
                    }
                }
                obs_data_release(settings);
            }
        }
        if(!currentFile.isEmpty())directory=QFileInfo(currentFile).absolutePath();
        const QString pathKey=currentFile.isEmpty()?directory:currentFile;
        if(pathKey!=recordPath_){
            recordPath_=pathKey;recordPreviousReady_=false;
            recordSmoothedBytesPerSecond_=0;
        }
        const quint64 processed=recordOutput?obs_output_get_total_bytes(recordOutput):0;
        QFileInfo file(currentFile);
        if(!currentFile.isEmpty()&&file.isFile()){
            result.record.fileSizeAvailable=true;
            result.record.fileBytes=quint64(std::max<qint64>(0,file.size()));
        }
        if(processed>0&&(!result.record.fileSizeAvailable||result.record.fileBytes==0)){
            result.record.fileSizeAvailable=true;
            result.record.fileSizeEstimated=true;
            result.record.fileBytes=processed;
        }
        if(!directory.isEmpty()&&QFileInfo(directory).isDir()){
            const QStorageInfo disk(directory);
            if(disk.isValid()&&disk.isReady()){
                result.record.diskAvailable=true;
                result.record.freeBytes=quint64(std::max<qint64>(0,disk.bytesAvailable()));
            }
        }
        const quint64 current=processed?processed:result.record.fileBytes;
        if(recordPreviousReady_&&current>=recordPreviousBytes_&&seconds>0&&seconds<10&&!result.record.paused) {
            const double perSecond=double(current-recordPreviousBytes_)/seconds;
            result.record.kbps=perSecond*8.0/1000.0;
            result.record.megabytesPerSecond=perSecond/(1024.0*1024.0);
            recordSmoothedBytesPerSecond_=recordSmoothedBytesPerSecond_==0?perSecond:
                0.15*perSecond+0.85*recordSmoothedBytesPerSecond_;
            if(recordSmoothedBytesPerSecond_>1024 && result.record.diskAvailable)
                result.record.secondsUntilFull=qint64(std::min(double(std::numeric_limits<qint64>::max()),
                    double(result.record.freeBytes)/recordSmoothedBytesPerSecond_));
        }
        recordPreviousBytes_=current;recordPreviousReady_=true;
    } else {
        recordPath_.clear();recordWasActive_=false;recordElapsedSeconds_=0;recordPreviousReady_=false;
        recordPreviousBytes_=0;recordSmoothedBytesPerSecond_=0;
    }
    if(recordOutput)obs_output_release(recordOutput);
    return result;
}

void Collector::gpuLoop() {
    PDH_HQUERY query=nullptr;PDH_HCOUNTER counter=nullptr;
    if(PdhOpenQueryW(nullptr,0,&query)!=ERROR_SUCCESS)return;
    if(PdhAddEnglishCounterW(query,L"\\GPU Engine(*)\\Utilization Percentage",0,&counter)!=ERROR_SUCCESS) {
        PdhCloseQuery(query);return;
    }
    PdhCollectQueryData(query);
    while(!stopGpu_) {
        std::unique_lock<std::mutex> lock(gpuMutex_);
        gpuWake_.wait_for(lock,std::chrono::seconds(3),[this]{return stopGpu_.load();});
        lock.unlock();
        if(stopGpu_)break;
        if(PdhCollectQueryData(query)!=ERROR_SUCCESS)continue;
        DWORD bytes=0,count=0;
        const auto sizeStatus=PdhGetFormattedCounterArrayW(counter,PDH_FMT_DOUBLE,&bytes,&count,nullptr);
        if(sizeStatus!=PDH_MORE_DATA||!bytes)continue;
        std::vector<unsigned char> buffer(bytes);
        auto* items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if(PdhGetFormattedCounterArrayW(counter,PDH_FMT_DOUBLE,&bytes,&count,items)!=ERROR_SUCCESS)continue;
        double maxEngine=-1;
        for(DWORD i=0;i<count;++i) {
            if(items[i].FmtValue.CStatus!=PDH_CSTATUS_VALID_DATA&&
               items[i].FmtValue.CStatus!=PDH_CSTATUS_NEW_DATA)continue;
            // Report the busiest engine, avoiding a false >100% sum over
            // multiple adapters and independent GPU engines.
            maxEngine=std::max(maxEngine,items[i].FmtValue.doubleValue);
        }
        if(maxEngine>=0)gpuPercent_=std::clamp(maxEngine,0.0,100.0);
    }
    PdhCloseQuery(query);
}

} // namespace stats
#endif
