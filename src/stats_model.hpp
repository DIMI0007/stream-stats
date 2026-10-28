#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace stats {

constexpr int historySeconds = 15 * 60;

class SustainedProblem {
public:
    bool update(bool problem,double seconds,double threshold){
        if(!problem)seconds_=0;
        else if(std::isfinite(seconds)&&seconds>0&&seconds<=10)seconds_+=seconds;
        return problem&&seconds_>=threshold;
    }
private:
    double seconds_=0;
};

class LossWindow {
public:
    bool update(bool active,quint64 lost,quint64 total,double seconds){
        if(!active||!std::isfinite(seconds)||seconds<=0||seconds>5){values_.clear();return false;}
        values_.push_back({lost,total,seconds});
        double covered=0;for(const auto& value:values_)covered+=value.seconds;
        while(values_.size()>1&&covered-values_.front().seconds>=10){covered-=values_.front().seconds;values_.removeFirst();}
        quint64 dropped=0,frames=0;for(const auto& value:values_){dropped+=value.lost;frames+=value.total;}
        return covered>=10&&frames>0&&double(dropped)/double(frames)>0.01;
    }
private:
    struct Sample{quint64 lost,total;double seconds;};
    QVector<Sample> values_;
};

struct OutputReading {
    quintptr identity = 0;
    bool active = false;
    bool reconnecting = false;
    bool stream = false;
    bool possibleUnmeasuredStream = false;
    QString name;
    quint64 bytes = 0;
    quint64 frames = 0;
    quint64 dropped = 0;
};

struct StreamReading {
    bool active = false;
    bool reconnecting = false;
    bool incomplete = false;
    int activeOutputs = 0;
    double kbps = 0;
    quint64 totalBytes = 0;
    quint64 dropped = 0;
    quint64 frames = 0;
    QStringList outputNames;
};

// One identity is counted once per sample, even if frontend and plugin APIs
// both expose the same OBS output. Counter resets start a fresh contribution.
class StreamAccumulator {
public:
    StreamReading update(const QVector<OutputReading>& inputs, double elapsedSeconds) {
        StreamReading value;
        QSet<quintptr> seen;
        for (const auto& input : inputs) {
            if (!input.active && !input.reconnecting) continue;
            if (input.possibleUnmeasuredStream) value.incomplete = true;
            if (!input.stream || !input.identity || seen.contains(input.identity)) continue;
            seen.insert(input.identity);
            ++value.activeOutputs;
            if(!input.name.isEmpty())value.outputNames.push_back(input.name);
            value.active |= input.active;
            value.reconnecting |= input.reconnecting;
            auto& previous = counters_[input.identity];
            const quint64 bytesDelta = previous.seen && input.bytes >= previous.bytes
                ? input.bytes - previous.bytes : input.bytes;
            const quint64 droppedDelta = previous.seen && input.dropped >= previous.dropped
                ? input.dropped - previous.dropped : input.dropped;
            const quint64 framesDelta = previous.seen && input.frames >= previous.frames
                ? input.frames - previous.frames : input.frames;
            // The first observation has no measured interval. Existing counters
            // still contribute to session totals, but not to instant bitrate.
            if (previous.seen && elapsedSeconds > 0 && elapsedSeconds < 10)
                value.kbps += double(bytesDelta) * 8.0 / 1000.0 / elapsedSeconds;
            totalBytes_ += bytesDelta;
            totalDropped_ += droppedDelta;
            totalFrames_ += framesDelta;
            previous = {input.bytes,input.dropped,input.frames,true};
            if (input.bytes == 0 && ++zeroSamples_[input.identity] > 5)
                value.incomplete = true;
            else if (input.bytes > 0) zeroSamples_[input.identity] = 0;
        }
        for (auto it = counters_.begin(); it != counters_.end();) {
            if (!seen.contains(it.key())) {zeroSamples_.remove(it.key());it = counters_.erase(it);}
            else ++it;
        }
        value.totalBytes = totalBytes_;
        value.dropped = totalDropped_;
        value.frames = totalFrames_;
        return value;
    }
private:
    struct Counter {quint64 bytes=0,dropped=0,frames=0;bool seen=false;};
    QHash<quintptr,Counter> counters_;
    QHash<quintptr,int> zeroSamples_;
    quint64 totalBytes_ = 0,totalDropped_ = 0,totalFrames_ = 0;
};

class History {
public:
    void add(double value) {
        data_[next_] = std::isfinite(value) ? std::max(0.0,value) : 0.0;
        next_ = (next_ + 1) % historySeconds;
        count_ = std::min(count_ + 1,historySeconds);
    }
    QVector<double> recent(int seconds) const {
        const int size = std::min(count_,std::clamp(seconds,1,historySeconds));
        QVector<double> result;result.reserve(size);
        for(int i=0;i<size;++i)
            result.push_back(data_[(next_ + historySeconds - size + i) % historySeconds]);
        return result;
    }
    int count() const {return count_;}
private:
    std::array<double,historySeconds> data_{};
    int next_=0,count_=0;
};

} // namespace stats
