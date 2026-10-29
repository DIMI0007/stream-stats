#include "stats_model.hpp"
#include <cmath>
#include <cstdio>

int main() {
    {
        stats::StreamAccumulator three;
        stats::OutputReading twitch;twitch.identity=101;twitch.active=true;twitch.stream=true;twitch.name="Twitch";
        auto youtube=twitch;youtube.identity=102;youtube.name="YouTube";
        auto kick=twitch;kick.identity=103;kick.name="Kick";
        auto initial=three.update({twitch,youtube,kick,kick},1.0);
        if(initial.activeOutputs!=3||initial.outputNames.size()!=3)return 14;
        twitch.bytes=1000000;youtube.bytes=2000000;kick.bytes=1500000;
        const auto total=three.update({twitch,youtube,kick,kick},1.0);
        if(total.activeOutputs!=3||std::abs(total.kbps-36000.0)>0.01||total.totalBytes!=4500000||!total.outputNames.contains("Kick"))return 15;
        kick.active=false;twitch.bytes+=1000000;youtube.bytes+=2000000;
        const auto stopped=three.update({twitch,youtube,kick},1.0);
        if(stopped.activeOutputs!=2||std::abs(stopped.kbps-24000.0)>0.01||stopped.totalBytes!=7500000)return 16;
    }
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
    stats::SustainedProblem problem;
    for(int i=0;i<9;++i)if(problem.update(true,1,10))return 6;
    if(problem.update(false,1,10))return 7;
    for(int i=0;i<9;++i)if(problem.update(true,1,10))return 8;
    if(!problem.update(true,1,10))return 9;
    if(problem.update(false,1,10)||problem.update(true,60,10))return 10;
    std::puts("Sustained warning thresholds, recovery and long sampling gaps passed");
    stats::LossWindow window;
    for(int i=0;i<9;++i)if(window.update(true,i%2?0:2,60,1))return 11;
    if(!window.update(true,0,60,1))return 12;
    for(int i=0;i<10;++i)window.update(true,0,60,1);
    if(window.update(true,0,60,1)||window.update(false,60,60,1)||window.update(true,60,60,60))return 13;
    return 0;
}
