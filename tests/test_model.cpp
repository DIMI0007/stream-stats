#include "stats_model.hpp"
#include <cmath>
#include <cstdio>

int main() {
    stats::StreamAccumulator accumulator;
    stats::OutputReading first;first.identity=1;first.active=true;first.stream=true;
    first.name="Twitch output";
    first.bytes=1000;first.frames=100;first.dropped=2;
    auto value=accumulator.update({first,first},1.0);
    if(value.activeOutputs!=1||value.outputNames.size()!=1||value.totalBytes!=1000||value.dropped!=2||value.kbps!=0)return 1;
    first.bytes=126000;first.frames=200;first.dropped=3;
    stats::OutputReading second=first;second.identity=2;second.bytes=2000;second.frames=10;second.dropped=0;
    value=accumulator.update({first,first,second},1.0);
    if(value.activeOutputs!=2||value.totalBytes!=128000||value.dropped!=3||
       std::abs(value.kbps-1000.0)>0.01)return 2;
    // A restarted output resets its byte counter without making the total go backwards.
    first.bytes=500;first.frames=5;first.dropped=0;
    second.active=false;
    value=accumulator.update({first,second},1.0);
    if(value.activeOutputs!=1||value.totalBytes!=128500||value.kbps<3.9||value.kbps>4.1)return 3;
    stats::OutputReading unknown;unknown.identity=3;unknown.active=true;unknown.possibleUnmeasuredStream=true;
    value=accumulator.update({first,unknown},1.0);
    if(!value.incomplete||value.activeOutputs!=1)return 4;
    stats::History history;
    for(int i=0;i<stats::historySeconds+10;++i)history.add(i);
    const auto last=history.recent(60);
    if(history.count()!=stats::historySeconds||last.size()!=60||
       last.front()!=stats::historySeconds-50||last.back()!=stats::historySeconds+9)return 5;
    std::puts("Stream aggregation, deduplication, resets, incomplete totals and history passed");
    return 0;
}
