#pragma once

#include "stats_collector.hpp"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QStackedWidget>
#include <QTimer>

namespace stats {
class Chart;

class StatsDock final : public QWidget {
public:
    explicit StatsDock(QWidget* parent=nullptr);
    ~StatsDock() override = default;
    void shutdown();
#ifdef STREAM_STATS_PREVIEW
    void previewSamples(int count);
    void previewOffline();
#endif
private:
    void buildPages();
    QWidget* buildHome();
    QWidget* buildSettings();
    void updateSnapshot(const Snapshot& value,bool collect=true);
    void updatePeers();
    void showSummary();
    void exportSummary();
    QString summaryText()const;
    void updateHealth(const Snapshot& value,double seconds);
    void updateRecordDetails();
    void setRange(int minutes);
    void setLanguage(const QString& code);
    void arrangeCards();
    bool russian() const;
    QString phrase(const char* ru,const char* en) const;
    void resizeEvent(QResizeEvent* event) override;
    Collector collector_;
    QTimer tick_;
    QStackedWidget* pages_=nullptr;
    QWidget* home_=nullptr;
    QWidget* settings_=nullptr;
    QWidget* summaryPage_=nullptr;
    QLabel* warningLabel_=nullptr;
    QPushButton* recordDetails_=nullptr;
    QGridLayout* cards_=nullptr;
    QHBoxLayout* headerLayout_=nullptr;
    QGridLayout* metricGrids_[3]{};
    QComboBox* systemSelector_=nullptr;
    QWidget *systemCard_=nullptr,*streamCard_=nullptr,*recordCard_=nullptr;
    Chart *systemChart_=nullptr,*streamChart_=nullptr,*recordChart_=nullptr;
    QLabel *cpuObs_=nullptr,*cpuPc_=nullptr,*gpu_=nullptr,*ramObs_=nullptr,*ramFree_=nullptr;
    QLabel *fps_=nullptr,*render_=nullptr,*renderDrop_=nullptr,*encodeDrop_=nullptr;
    QLabel *streamStatus_=nullptr,*streamBitrate_=nullptr,*streamCount_=nullptr,*streamBytes_=nullptr,*streamDrop_=nullptr;
    QLabel *recordStatus_=nullptr,*recordBitrate_=nullptr,*recordFile_=nullptr,*recordSpeed_=nullptr;
    QLabel *recordTime_=nullptr,*recordDisk_=nullptr,*recordEta_=nullptr;
    QLabel* footer_=nullptr;
    QPushButton* peersToggle_=nullptr;
    QWidget* peersPanel_=nullptr;
    QLabel *peerStatus_[3]{},*peerTraffic_[3]{},*peerResources_[3]{},*peersTotal_=nullptr;
    QElapsedTimer peerClock_;
    quint64 peerCpuNs_[3]{};
    bool peerCpuReady_[3]{};
    QList<QPushButton*> rangeButtons_;
    History cpuHistory_,obsCpuHistory_,gpuHistory_,fpsHistory_,streamHistory_,recordHistory_;
    Snapshot last_;
    QString language_="auto";
    int rangeMinutes_=1;
    int systemMetricIndex_=0;
    bool horizontal_=false;
    int layoutMode_=-1;
    int compactHeader_=-1;
    bool closing_=false;
    bool recordExpanded_=false,warningsEnabled_=true;
    QElapsedTimer sessionClock_,sampleClock_;
    double cpuSum_=0,observedSeconds_=0,gpuPeak_=-1,recordSeconds_=0;
    quint64 samples_=0,recordedBytes_=0,previousRecordedBytes_=0;
    quint64 sessionRenderMissed_=0,sessionEncodeSkipped_=0,sessionNetworkDropped_=0;
    Snapshot previousHealth_;
    bool healthReady_=false;
    LossWindow networkLoss_,renderLoss_,encodeLoss_;
    SustainedProblem diskProblem_;
};
} // namespace stats
