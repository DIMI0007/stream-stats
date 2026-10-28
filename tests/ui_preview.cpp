#include "stats_dock.hpp"
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>
#include <QComboBox>
#include <cstdio>

int main(int argc,char** argv) {
    std::fprintf(stderr,"Preview process started\n");std::fflush(stderr);
    QApplication app(argc,argv);
    std::fprintf(stderr,"Qt application initialized\n");std::fflush(stderr);
    const QString output=argc>1?QString::fromLocal8Bit(argv[1]):"ui-previews";
    QDir().mkpath(output);
    QWidget host;
    host.resize(1,1);host.show();
    stats::StatsDock dock(&host);
    std::fprintf(stderr,"Preview panel constructed\n");std::fflush(stderr);
    dock.previewSamples(75);
    if(dock.findChildren<QLabel*>("Credit").size()!=1||
       dock.findChildren<QPushButton*>("PeersToggle").size()!=1||
       dock.findChildren<QWidget*>("PeersPanel").size()!=1||
       dock.findChildren<QLabel*>("PeerTraffic").size()!=3||
       dock.findChildren<QLabel*>("PeerResources").size()!=3||
       dock.findChildren<QLabel*>("OutputCount").size()!=1||
       dock.findChildren<QComboBox*>("SystemMetricSelector").size()!=1)return 5;
    auto* peersToggle=dock.findChild<QPushButton*>("PeersToggle");
    auto* peersPanel=dock.findChild<QWidget*>("PeersPanel");
    if(!peersToggle||!peersPanel||peersToggle->isChecked()||!peersPanel->isHidden())return 8;
    auto* logo=dock.findChild<QWidget*>("StatsMark");
    auto* gear=dock.findChild<QPushButton*>("SettingsButton");
    if(!logo||logo->size()!=QSize(28,28)||!gear||gear->size()!=QSize(30,28)||
       gear->iconSize()!=QSize(19,19)||gear->icon().isNull())return 6;
    std::fprintf(stderr,"Preview samples prepared\n");std::fflush(stderr);
    auto capture=[&](const char* file,int width,int height,bool requireNoScroll){
        std::fprintf(stderr,"Capture %s begin\n",file);std::fflush(stderr);
        dock.resize(width,height);dock.show();app.processEvents(QEventLoop::AllEvents,200);
        std::fprintf(stderr,"Capture %s laid out\n",file);std::fflush(stderr);
        if(dock.width()!=width||dock.height()!=height){
            std::fprintf(stderr,"Unexpected rendered size: %dx%d\n",dock.width(),dock.height());return false;
        }
        auto* scroll=dock.findChild<QScrollArea*>();
        if(!scroll)return false;
        const bool saved=dock.grab().save(output+"/"+QString::fromLatin1(file));
        std::fprintf(stderr,"Capture %s saved=%d\n",file,int(saved));std::fflush(stderr);
        if(requireNoScroll&&scroll->verticalScrollBar()->maximum()>0){
            std::fprintf(stderr,"Unexpected vertical scroll at %dx%d: %d px\n",
                width,height,scroll->verticalScrollBar()->maximum());return false;
        }
        const auto cards=dock.findChildren<QFrame*>("StatsCard");
        if(cards.size()!=3)return false;
        if(width>=680 && !(cards[0]->geometry().top()==cards[1]->geometry().top() &&
                           cards[1]->geometry().top()==cards[2]->geometry().top()))return false;
        return saved;
    };
    if(!capture("compact-720x260.png",720,260,true))return 1;
    if(!capture("horizontal-1600x900.png",1600,900,true))return 2;
    if(!capture("vertical-360x900.png",360,900,true))return 3;
    peersToggle->click();app.processEvents(QEventLoop::AllEvents,200);
    if(!peersPanel->isVisible()||!capture("plugins-expanded-360x900.png",360,900,false))return 9;
    peersToggle->click();app.processEvents(QEventLoop::AllEvents,200);
    if(!capture("narrow-280x900.png",280,900,false))return 7;
    dock.previewOffline();
    if(!capture("vertical-offline-310x900.png",310,900,true))return 4;
    auto* recordDetails=dock.findChild<QPushButton*>("RecordDetails");if(!recordDetails||!recordDetails->isVisible())return 10;
    recordDetails->click();app.processEvents();if(!recordDetails->isChecked())return 11;
    dock.previewSamples(1);app.processEvents();if(recordDetails->isVisible())return 15;
    dock.previewOffline();app.processEvents();if(recordDetails->isChecked()||!recordDetails->isVisible())return 16;
    gear->click();app.processEvents();
    auto* summary=dock.findChild<QPushButton*>("SessionSummaryButton");if(!summary)return 12;summary->click();app.processEvents();
    auto* summaryText=dock.findChild<QLabel*>("SessionSummaryText");if(!summaryText||summaryText->text().isEmpty())return 13;
    if(!dock.grab().save(output+"/session-summary.png"))return 14;
    std::puts("Responsive Stream Stats UI previews passed");
    return 0;
}
