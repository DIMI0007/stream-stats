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
    void updateSnapshot(const Snapshot& value);
    void updatePeers();
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
    QLabel *peerStatus_[2]{},*peerTraffic_[2]{},*peersTotal_=nullptr;
    QList<QPushButton*> rangeButtons_;
    History cpuHistory_,obsCpuHistory_,gpuHistory_,fpsHistory_,streamHistory_,recordHistory_;
    Snapshot last_;
    QString language_="auto";
    int rangeMinutes_=1;
    int systemMetricIndex_=0;
    bool horizontal_=false;
    int layoutMode_=-1;
    bool closing_=false;
};
} // namespace stats
