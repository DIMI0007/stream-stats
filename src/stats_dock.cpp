#include "stats_dock.hpp"
#ifndef STREAM_STATS_PREVIEW
#include <obs.h>
#include <obs-module.h>
#include <util/bmem.h>
#endif
#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSettings>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>
#include <QLocale>
#include <QLinearGradient>
#include <algorithm>

namespace stats {
namespace {

QString configFile() {
#ifdef STREAM_STATS_PREVIEW
    return QDir::tempPath()+"/stream-stats-preview-ui.ini";
#else
    char* raw=obs_module_config_path("ui.ini");
    if(!raw)return {};
    const QString path=QString::fromUtf8(raw);bfree(raw);
    QDir().mkpath(QFileInfo(path).absolutePath());
    return path;
#endif
}

QString bytesText(quint64 bytes,const QLocale& locale) {
    const double value=double(bytes);
    if(value>=1024.0*1024*1024)return locale.toString(value/(1024.0*1024*1024),'f',1)+" GB";
    if(value>=1024.0*1024)return locale.toString(value/(1024.0*1024),'f',0)+" MB";
    return locale.toString(value/1024.0,'f',0)+" KB";
}
QString duration(qint64 seconds) {
    if(seconds<0)return "—";
    const qint64 hours=seconds/3600,minutes=(seconds/60)%60,secs=seconds%60;
    return QString("%1:%2:%3").arg(hours,2,10,QChar('0')).arg(minutes,2,10,QChar('0')).arg(secs,2,10,QChar('0'));
}

class StatsMark final : public QWidget {
public:
    StatsMark(){setObjectName("StatsMark");setFixedSize(28,28);}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.scale(width()/32.0,height()/32.0);
        p.setPen(QPen(QColor("#00df8c"),2.4));p.setBrush(QColor("#06382c"));
        p.drawEllipse(QRectF(2,2,28,28));
        p.setPen(Qt::NoPen);p.setBrush(QColor("#00df8c"));
        p.drawRoundedRect(QRectF(8,16,3.5,9),1.4,1.4);
        p.drawRoundedRect(QRectF(14.3,9,3.5,16),1.4,1.4);
        p.drawRoundedRect(QRectF(20.5,13,3.5,12),1.4,1.4);
    }
};

QIcon settingsIcon(){
    QPixmap image(48,48);image.fill(Qt::transparent);
    QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);p.scale(2,2);
    p.setPen(QPen(QColor("#aebde3"),1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);p.translate(12,12);
    for(int i=0;i<8;++i){p.drawLine(QPointF(0,-8),QPointF(0,-10));p.rotate(45);}
    p.drawEllipse(QPointF(0,0),6.8,6.8);p.drawEllipse(QPointF(0,0),2.5,2.5);
    p.end();image.setDevicePixelRatio(2);return QIcon(image);
}

class SectionIcon final : public QWidget {
public:
    SectionIcon(int kind,const QColor& color):kind_(kind),color_(color){setFixedSize(18,18);}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const qreal scale=std::min(width(),height())/18.0;
        p.translate(width()/2.0,height()/2.0);p.scale(scale,scale);p.translate(-9,-9);
        p.setPen(QPen(color_,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        if(kind_==0){
            p.drawRoundedRect(QRectF(4,4,10,10),1.5,1.5);
            p.drawRoundedRect(QRectF(7,7,4,4),0.5,0.5);
            for(int n:{6,10}){
                p.drawLine(QPointF(n,1),QPointF(n,4));p.drawLine(QPointF(n,14),QPointF(n,17));
                p.drawLine(QPointF(1,n),QPointF(4,n));p.drawLine(QPointF(14,n),QPointF(17,n));
            }
        }else if(kind_==1){
            p.setBrush(color_);p.setPen(Qt::NoPen);p.drawEllipse(QPointF(9,9),2,2);
            p.setPen(QPen(color_,1.8,Qt::SolidLine,Qt::RoundCap));
            p.drawArc(QRectF(3,3,12,12),-58*16,116*16);
            p.drawArc(QRectF(0,0,18,18),-58*16,116*16);
            p.drawArc(QRectF(3,3,12,12),122*16,116*16);
            p.drawArc(QRectF(0,0,18,18),122*16,116*16);
        }else{
            p.drawEllipse(QRectF(1.5,1.5,15,15));p.setBrush(color_);p.setPen(Qt::NoPen);
            p.drawEllipse(QRectF(5.5,5.5,7,7));
        }
    }
private:
    int kind_;
    QColor color_;
};

QFrame* card(const QString& title,const QColor& accent,int icon,QVBoxLayout*& body,QHBoxLayout** titleRow=nullptr) {
    auto* frame=new QFrame;frame->setObjectName("StatsCard");frame->setMinimumWidth(0);
    auto* layout=new QVBoxLayout(frame);layout->setContentsMargins(7,5,7,5);layout->setSpacing(2);
    auto* header=new QHBoxLayout;header->setContentsMargins(0,0,0,0);header->setSpacing(5);
    auto* mark=new SectionIcon(icon,accent);mark->setObjectName("SectionIcon");header->addWidget(mark);
    auto* label=new QLabel(title);label->setObjectName("CardTitle");
    label->setProperty("accent",accent.name());label->setMinimumWidth(0);
    label->setStyleSheet(QString("color:%1;").arg(accent.name()));header->addWidget(label,1);
    layout->addLayout(header);
    if(titleRow)*titleRow=header;
    body=layout;return frame;
}

QGridLayout* metricGrid(QVBoxLayout* body) {
    auto* host=new QWidget;host->setObjectName("MetricGridHost");
    auto* grid=new QGridLayout(host);grid->setContentsMargins(0,0,0,0);grid->setSpacing(0);
    body->addWidget(host);return grid;
}

QLabel* addMetric(QGridLayout* grid,const QString& name) {
    auto* wrapper=new QWidget;wrapper->setObjectName("MetricRow");auto* row=new QHBoxLayout(wrapper);
    row->setContentsMargins(0,0,0,0);row->setSpacing(3);
    auto* left=new QLabel(name);left->setObjectName("MetricName");left->setMinimumWidth(0);
    auto* right=new QLabel("—");right->setObjectName("MetricValue");right->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    right->setMinimumWidth(0);
    row->addWidget(left,1);row->addWidget(right,0);
    wrapper->setFixedHeight(14);grid->addWidget(wrapper,grid->count(),0);
    return right;
}

} // namespace

class Chart final : public QWidget {
public:
    explicit Chart(QVector<QPair<const History*,QColor>> series,QWidget* parent=nullptr)
        :QWidget(parent),series_(std::move(series)) {setMinimumWidth(0);setFixedHeight(38);}
    void setRange(int seconds){seconds_=seconds;update();}
    void setDisplayHeight(int height){setFixedHeight(height);update();}
    void setSeries(QVector<QPair<const History*,QColor>> series,const QString& unit,const QString& label){
        series_=std::move(series);unit_=unit;label_=label;update();
    }
    void setInactiveMessage(const QString& message){inactiveMessage_=message;update();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const QRectF plot=rect().adjusted(1,2,-1,-2);
        p.setBrush(QColor("#08131f"));p.setPen(QPen(QColor("#263b50"),1));
        p.drawRoundedRect(plot,4,4);
        const bool labeled=height()>=55;
        const QRectF inner=plot.adjusted(6,labeled?17:5,-6,-6);
        p.setPen(QPen(QColor("#234056"),1,Qt::DashLine));
        for(int i=0;i<3;++i){const qreal y=inner.top()+inner.height()*i/2.0;
            p.drawLine(QPointF(inner.left(),y),QPointF(inner.right(),y));}
        if(!inactiveMessage_.isEmpty()){
            p.setPen(QColor("#8199b6"));QFont font=p.font();font.setPixelSize(labeled?11:9);p.setFont(font);
            p.drawText(inner,Qt::AlignCenter,inactiveMessage_);return;
        }
        QVector<QVector<double>> values;values.reserve(series_.size());
        double maximum=1;
        for(const auto& series:series_){
            values.push_back(series.first->recent(seconds_));
            for(double value:values.back())maximum=std::max(maximum,value);
        }
        maximum=std::ceil(maximum*1.2);
        if(labeled){
            p.setPen(QPen(series_.isEmpty()?QColor("#8ea8c7"):series_.front().second,2));
            p.drawLine(QPointF(plot.left()+7,plot.top()+9),QPointF(plot.left()+17,plot.top()+9));
            p.setPen(QColor("#b5c9e2"));QFont font=p.font();font.setPixelSize(9);p.setFont(font);
            p.drawText(QRectF(plot.left()+22,plot.top()+3,plot.width()-29,12),Qt::AlignLeft,
                label_+" · "+phraseMax(maximum,unit_));
        }
        p.save();p.setClipRect(inner.adjusted(-2,-2,2,2));
        for(int s=0;s<series_.size();++s){
            const auto& points=values[s];if(points.size()<2)continue;
            QVector<QPointF> nodes;nodes.reserve(points.size());
            for(int i=0;i<points.size();++i){
                const qreal age=qreal(points.size()-1-i);
                const qreal x=inner.right()-age*inner.width()/qreal(std::max(1,seconds_-1));
                const qreal y=inner.bottom()-qreal(points[i]/maximum)*inner.height();
                nodes.push_back(QPointF(x,y));
            }
            QPainterPath line(nodes.front());
            for(int i=1;i<nodes.size();++i){
                const QPointF mid=(nodes[i-1]+nodes[i])/2.0;
                line.quadTo(nodes[i-1],mid);
            }
            line.lineTo(nodes.back());
            if(s==0){
                QPainterPath area=line;area.lineTo(nodes.back().x(),inner.bottom());
                area.lineTo(nodes.front().x(),inner.bottom());area.closeSubpath();
                QLinearGradient gradient(0,inner.top(),0,inner.bottom());
                QColor top=series_[s].second;top.setAlpha(78);
                QColor bottom=top;bottom.setAlpha(5);
                gradient.setColorAt(0,top);gradient.setColorAt(1,bottom);p.fillPath(area,gradient);
            }
            p.setPen(QPen(series_[s].second,2.0,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p.drawPath(line);
        }
        p.restore();
    }
private:
    static QString phraseMax(double max,const QString& unit){
        return QString::number(max,'f',0)+(unit.isEmpty()?QString():" "+unit);
    }
    QVector<QPair<const History*,QColor>> series_;
    int seconds_=60;
    QString unit_,label_;
    QString inactiveMessage_;
};

StatsDock::StatsDock(QWidget* parent):QWidget(parent) {
    setObjectName("StreamStatsDock");setMinimumSize(310,200);
    setStyleSheet(R"(
      QWidget#StreamStatsDock{background:#080f18;color:#dbe7fa;font-family:'Segoe UI';font-size:10px;}
      QFrame#StatsCard{background:#0c1725;border:1px solid #29415a;border-radius:7px;}
      QLabel#CardTitle{font-size:12px;font-weight:700;}
      QLabel#MetricName{color:#9fb5d5;font-size:10px;}
      QLabel#MetricValue{color:#edf5ff;font-size:10px;font-weight:700;}
      QLabel#DockTitle{font-size:17px;font-weight:700;color:#e7fff4;}
      QLabel#OutputCount{color:#ffe184;background:#24301f;border:1px solid #70571b;border-radius:7px;font-size:10px;font-weight:700;padding:1px 6px;}
      QWidget#Footer{background:#080f18;}
      QLabel#Credit{color:#9299a5;background:#080f18;font-size:11px;}
      QLabel#SessionTrafficFooter{color:#72e9f4;background:#080f18;font-size:10px;}
      QComboBox#SystemMetricSelector{color:#c9ddf5;background:#142238;border:1px solid #34506e;border-radius:5px;padding:1px 5px;font-size:10px;}
      QComboBox#SystemMetricSelector QAbstractItemView{background:#142238;color:#dbe7fa;selection-background-color:#23506a;}
      QPushButton{color:#dbe7fa;background:#142238;border:1px solid #344b6a;border-radius:6px;padding:2px 6px;}
      QPushButton:hover{background:#1b3050;border-color:#5983b5;}
      QPushButton#SettingsButton{color:#e1ebff;background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #202a3b,stop:1 #141d2b);border:1px solid #39465d;border-radius:8px;padding:0;}
      QPushButton#SettingsButton:hover{background:#26344d;border-color:#6476a0;}
      QPushButton#ActiveRange{background:#193d4b;border-color:#00bfa2;color:#e8ffff;}
      QScrollArea{border:0;background:transparent;}
      QScrollArea QWidget#CardHost{background:transparent;}
    )");
    QSettings saved(configFile(),QSettings::IniFormat);
    language_=saved.value("language","auto").toString();
    rangeMinutes_=saved.value("rangeMinutes",1).toInt();
    if(rangeMinutes_!=1&&rangeMinutes_!=5&&rangeMinutes_!=15)rangeMinutes_=1;
    systemMetricIndex_=std::clamp(saved.value("systemMetric",0).toInt(),0,3);
    auto* root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);
    pages_=new QStackedWidget;root->addWidget(pages_);
    buildPages();
    tick_.setTimerType(Qt::CoarseTimer);
    QObject::connect(&tick_,&QTimer::timeout,this,[this]{if(!closing_)updateSnapshot(collector_.sample());});
    tick_.start(1000);
    updateSnapshot(collector_.sample());
}

void StatsDock::shutdown(){closing_=true;tick_.stop();}
#ifdef STREAM_STATS_PREVIEW
void StatsDock::previewSamples(int count){for(int i=0;i<count;++i)updateSnapshot(collector_.sample());}
void StatsDock::previewOffline(){
    Snapshot offline=last_;
    offline.stream=StreamReading{};offline.record=RecordReading{};
    updateSnapshot(offline);
}
#endif
bool StatsDock::russian() const {
    if(language_=="ru")return true;if(language_=="en")return false;
#ifdef STREAM_STATS_PREVIEW
    return true;
#else
    const char* locale=obs_get_locale();return locale&&QString::fromUtf8(locale).startsWith("ru",Qt::CaseInsensitive);
#endif
}
QString StatsDock::phrase(const char* ru,const char* en) const {return QString::fromUtf8(russian()?ru:en);}

void StatsDock::buildPages() {
    if(home_){pages_->removeWidget(home_);home_->deleteLater();home_=nullptr;}
    if(settings_){pages_->removeWidget(settings_);settings_->deleteLater();settings_=nullptr;}
    layoutMode_=-1;home_=buildHome();settings_=buildSettings();
    pages_->addWidget(home_);pages_->addWidget(settings_);pages_->setCurrentWidget(home_);
    arrangeCards();updateSnapshot(last_);
}

QWidget* StatsDock::buildHome() {
    auto* page=new QWidget;auto* outer=new QVBoxLayout(page);
    outer->setContentsMargins(8,8,8,3);outer->setSpacing(4);
    auto* top=new QHBoxLayout;top->setContentsMargins(2,2,2,2);top->setSpacing(7);
    top->addWidget(new StatsMark);
    auto* heading=new QLabel("Stream Stats");heading->setObjectName("DockTitle");top->addWidget(heading,1);
    for(int minutes:{1,5,15}){
        auto* button=new QPushButton(QString::number(minutes)+phrase(" м"," m"));
        button->setFixedSize(34,23);button->setToolTip(phrase("История графиков","Chart history"));
        QObject::connect(button,&QPushButton::clicked,this,[this,minutes]{setRange(minutes);});
        top->addWidget(button);rangeButtons_.append(button);
    }
    auto* gear=new QPushButton;gear->setObjectName("SettingsButton");gear->setFixedSize(30,28);
    gear->setIcon(settingsIcon());gear->setIconSize(QSize(19,19));
    gear->setToolTip(phrase("Настройки","Settings"));
    QObject::connect(gear,&QPushButton::clicked,this,[this]{pages_->setCurrentWidget(settings_);});
    top->addWidget(gear);outer->addLayout(top);
    auto* scroller=new QScrollArea;scroller->setWidgetResizable(true);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* host=new QWidget;host->setObjectName("CardHost");cards_=new QGridLayout(host);
    cards_->setContentsMargins(0,0,0,0);cards_->setSpacing(5);
    QVBoxLayout* body=nullptr;
    QHBoxLayout* systemHeader=nullptr;
    systemCard_=card(phrase("Система","System"),QColor("#4addd3"),0,body,&systemHeader);
    systemSelector_=new QComboBox;systemSelector_->setObjectName("SystemMetricSelector");
    systemSelector_->addItems({phrase("CPU ПК","PC CPU"),phrase("CPU OBS","OBS CPU"),"GPU","FPS"});
    systemSelector_->setToolTip(phrase("Показатель системного графика","System graph metric"));
    systemHeader->addWidget(systemSelector_);
    metricGrids_[0]=metricGrid(body);
    cpuObs_=addMetric(metricGrids_[0],phrase("CPU OBS","OBS CPU"));
    cpuPc_=addMetric(metricGrids_[0],phrase("CPU ПК","PC CPU"));
    gpu_=addMetric(metricGrids_[0],"GPU");
    ramObs_=addMetric(metricGrids_[0],phrase("RAM OBS","OBS RAM"));
    ramFree_=addMetric(metricGrids_[0],phrase("Своб. RAM","Free RAM"));
    fps_=addMetric(metricGrids_[0],"FPS");
    render_=addMetric(metricGrids_[0],phrase("Рендер","Render"));
    renderDrop_=addMetric(metricGrids_[0],phrase("Пропуск ренд.","Render missed"));
    encodeDrop_=addMetric(metricGrids_[0],phrase("Пропуск код.","Encode skipped"));
    systemChart_=new Chart({{&cpuHistory_,QColor("#00dfa4")}});
    systemChart_->setSeries({{&cpuHistory_,QColor("#00dfa4")}},"%",phrase("CPU ПК","PC CPU"));
    QObject::connect(systemSelector_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){
        if(!systemChart_)return;
        systemMetricIndex_=index;
        QSettings saved(configFile(),QSettings::IniFormat);saved.setValue("systemMetric",index);
        switch(index){
        case 1:systemChart_->setSeries({{&obsCpuHistory_,QColor("#43b7ff")}},"%",phrase("CPU OBS","OBS CPU"));break;
        case 2:systemChart_->setSeries({{&gpuHistory_,QColor("#aa79ff")}},"%","GPU");break;
        case 3:systemChart_->setSeries({{&fpsHistory_,QColor("#ffcc49")}},"FPS","FPS");break;
        default:systemChart_->setSeries({{&cpuHistory_,QColor("#00dfa4")}},"%",phrase("CPU ПК","PC CPU"));break;
        }
    });
    systemSelector_->setCurrentIndex(systemMetricIndex_);
    body->addStretch(1);
    body->addWidget(systemChart_);

    QHBoxLayout* streamHeader=nullptr;
    streamCard_=card(phrase("Трансляция","Stream"),QColor("#ffce43"),1,body,&streamHeader);
    streamCount_=new QLabel("0");streamCount_->setObjectName("OutputCount");
    streamCount_->setAlignment(Qt::AlignCenter);streamCount_->setToolTip(phrase("Активных потоковых выходов OBS","Active OBS stream outputs"));
    streamHeader->addWidget(streamCount_);
    metricGrids_[1]=metricGrid(body);
    streamStatus_=addMetric(metricGrids_[1],phrase("Статус","Status"));
    streamBitrate_=addMetric(metricGrids_[1],phrase("Общий битрейт","Total bitrate"));
    streamBytes_=addMetric(metricGrids_[1],phrase("Отправлено","Sent"));
    streamDrop_=addMetric(metricGrids_[1],phrase("Потери сети","Network loss"));
    body->addStretch(1);
    streamChart_=new Chart({{&streamHistory_,QColor("#ffbf37")}});
    streamChart_->setSeries({{&streamHistory_,QColor("#ffbf37")}},"kb/s",phrase("Общий битрейт","Total bitrate"));
    streamChart_->setToolTip(phrase("Суммарный фактический битрейт активных выходов","Actual aggregate bitrate of active outputs"));
    body->addWidget(streamChart_);

    recordCard_=card(phrase("Запись","Recording"),QColor("#ff556e"),2,body);
    metricGrids_[2]=metricGrid(body);
    recordStatus_=addMetric(metricGrids_[2],phrase("Статус","Status"));
    recordBitrate_=addMetric(metricGrids_[2],phrase("Битрейт","Bitrate"));
    recordFile_=addMetric(metricGrids_[2],phrase("Размер файла","File size"));
    recordSpeed_=addMetric(metricGrids_[2],phrase("Скорость","Write speed"));
    recordTime_=addMetric(metricGrids_[2],phrase("Время","Duration"));
    recordDisk_=addMetric(metricGrids_[2],phrase("Диск свободен","Disk free"));
    recordEta_=addMetric(metricGrids_[2],phrase("До заполн.","Until full"));
    body->addStretch(1);
    recordChart_=new Chart({{&recordHistory_,QColor("#23cdec")}});
    recordChart_->setSeries({{&recordHistory_,QColor("#23cdec")}},"kb/s",phrase("Битрейт записи","Record bitrate"));
    recordChart_->setToolTip(phrase("Фактический битрейт записи","Actual recording bitrate"));
    body->addWidget(recordChart_);
    scroller->setWidget(host);outer->addWidget(scroller,1);
    auto* footerWidget=new QWidget;footerWidget->setObjectName("Footer");
    auto* footerLayout=new QVBoxLayout(footerWidget);footerLayout->setContentsMargins(0,0,0,0);footerLayout->setSpacing(0);
    footer_=new QLabel("developed by <a href=\"https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/\" style=\"color:#59dfff;text-decoration:none;\">OTMOPOZOK</a> · v1.0.0");
    footer_->setObjectName("Credit");footer_->setTextFormat(Qt::RichText);
    footer_->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::LinksAccessibleByKeyboard);
    footer_->setOpenExternalLinks(true);footer_->setAlignment(Qt::AlignCenter);
    footer_->setFixedHeight(20);footerLayout->addWidget(footer_);
    trafficFooter_=new QLabel(phrase("Трафик за сеанс · ↑ 0 Б  |  ↓ 0 Б","Session traffic · ↑ 0 B  |  ↓ 0 B"));
    trafficFooter_->setObjectName("SessionTrafficFooter");trafficFooter_->setAlignment(Qt::AlignCenter);
    trafficFooter_->setFixedHeight(16);
    trafficFooter_->setToolTip(phrase("Этот плагин не отправляет сетевые запросы; трафик видеопотоков показан в блоке трансляции.",
        "This plugin makes no network requests; video traffic is shown in the stream block."));
    footerLayout->addWidget(trafficFooter_);outer->addWidget(footerWidget);
    setRange(rangeMinutes_);
    return page;
}

QWidget* StatsDock::buildSettings() {
    auto* page=new QWidget;auto* outer=new QVBoxLayout(page);outer->setContentsMargins(9,8,9,8);outer->setSpacing(8);
    auto* top=new QHBoxLayout;
    auto* back=new QPushButton(phrase("← Назад","← Back"));back->setFixedSize(70,29);
    QObject::connect(back,&QPushButton::clicked,this,[this]{pages_->setCurrentWidget(home_);});
    top->addWidget(back);auto* title=new QLabel(phrase("Настройки","Settings"));title->setObjectName("DockTitle");
    top->addWidget(title,1);outer->addLayout(top);
    auto* label=new QLabel(phrase("Язык интерфейса","Interface language"));outer->addWidget(label);
    auto* choices=new QHBoxLayout;choices->setSpacing(5);
    const struct {const char* code;const char* ru;const char* en;} items[]={
        {"auto","Авто","Auto"},{"ru","Русский","Russian"},{"en","English","English"}};
    for(const auto& item:items){
        auto* button=new QPushButton(phrase(item.ru,item.en));button->setCheckable(true);
        button->setChecked(language_==item.code);
        QObject::connect(button,&QPushButton::clicked,this,[this,code=QString::fromLatin1(item.code)]{setLanguage(code);});
        choices->addWidget(button,1);
    }
    outer->addLayout(choices);
    auto* hint=new QLabel(phrase("Графики обновляются раз в секунду. История хранится только в памяти OBS и очищается при закрытии программы.",
        "Charts update once per second. History stays in OBS memory and clears when OBS exits."));
    hint->setWordWrap(true);hint->setObjectName("MetricName");outer->addWidget(hint);
    outer->addStretch(1);return page;
}

void StatsDock::setRange(int minutes){
    rangeMinutes_=minutes;
    if(!configFile().isEmpty()){QSettings saved(configFile(),QSettings::IniFormat);saved.setValue("rangeMinutes",minutes);}
    for(auto* button:rangeButtons_){
        if(!button)continue;
        const bool active=button->text().startsWith(QString::number(minutes)+" ");
        button->setObjectName(active?"ActiveRange":"");
        button->style()->unpolish(button);button->style()->polish(button);
    }
    for(auto* chart:{systemChart_,streamChart_,recordChart_})if(chart)chart->setRange(minutes*60);
}

void StatsDock::setLanguage(const QString& code){
    language_=code;
    QSettings saved(configFile(),QSettings::IniFormat);saved.setValue("language",code);saved.sync();
    rangeButtons_.clear();buildPages();pages_->setCurrentWidget(settings_);
}

void StatsDock::arrangeCards(){
    if(!cards_||!home_)return;
    const bool wide=width()>=680 && width()>height()*1.3;
    const bool roomy=wide&&width()>=1100&&height()>=600;
    const int mode=roomy?2:wide?1:0;
    if(layoutMode_==mode)return;
    layoutMode_=mode;
    horizontal_=wide;
    for(auto* widget:{systemCard_,streamCard_,recordCard_})cards_->removeWidget(widget);
    if(wide){
        cards_->addWidget(systemCard_,0,0);cards_->addWidget(streamCard_,0,1);cards_->addWidget(recordCard_,0,2);
        for(int i=0;i<3;++i)cards_->setColumnStretch(i,1);
        cards_->setAlignment(Qt::Alignment());
        for(auto* frame:{systemCard_,streamCard_,recordCard_})
            frame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    }else{
        cards_->addWidget(systemCard_,0,0);cards_->addWidget(streamCard_,1,0);cards_->addWidget(recordCard_,2,0);
        cards_->setColumnStretch(0,1);cards_->setColumnStretch(1,0);cards_->setColumnStretch(2,0);
        cards_->setAlignment(Qt::AlignTop);
        for(auto* frame:{systemCard_,streamCard_,recordCard_})
            frame->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Maximum);
    }
    const int chartHeight=roomy?155:wide?23:60;
    for(auto* chart:{systemChart_,streamChart_,recordChart_})chart->setDisplayHeight(chartHeight);
    const int columns=wide&&!roomy?1:2;
    const int metricHeight=roomy?47:wide?13:34;
    const int metricFont=roomy?14:10;
    for(auto* grid:metricGrids_){
        QVector<QWidget*> rows;
        while(grid->count()){
            auto* item=grid->takeAt(0);rows.push_back(item->widget());delete item;
        }
        grid->setHorizontalSpacing(columns==2?(roomy?9:4):0);
        grid->setVerticalSpacing(roomy?8:wide?0:3);
        grid->setColumnStretch(0,1);grid->setColumnStretch(1,columns==2?1:0);
        for(int i=0;i<rows.size();++i){
            auto* row=rows[i];
            row->setFixedHeight(metricHeight);
            row->setStyleSheet(columns==2?
                "QWidget#MetricRow{background:#0d2030;border:1px solid #243d54;border-radius:5px;}":
                "QWidget#MetricRow{background:transparent;border:0;}");
            auto* rowLayout=static_cast<QHBoxLayout*>(row->layout());
            rowLayout->setContentsMargins(columns==2?5:0,0,columns==2?5:0,0);
            if(columns==2&&i==rows.size()-1&&rows.size()%2)
                grid->addWidget(row,i/columns,0,1,2);
            else grid->addWidget(row,i/columns,i%columns);
        }
    }
    for(auto* label:home_->findChildren<QLabel*>("MetricName"))
        label->setStyleSheet(QString("color:#9fb5d5;font-size:%1px;").arg(metricFont));
    for(auto* label:home_->findChildren<QLabel*>("MetricValue"))
        label->setStyleSheet(QString("color:#edf5ff;font-size:%1px;font-weight:700;").arg(metricFont));
    const auto accentValue=[&](QLabel* label,const char* color){
        label->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;").arg(color).arg(metricFont));
    };
    for(auto* label:{cpuObs_,cpuPc_,gpu_,fps_})accentValue(label,"#29e5a1");
    accentValue(streamBitrate_,"#ffce43");
    accentValue(recordBitrate_,"#35d9e8");
    for(auto* label:home_->findChildren<QLabel*>("CardTitle"))
        label->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;")
            .arg(label->property("accent").toString()).arg(roomy?22:wide?12:15));
    if(auto* title=home_->findChild<QLabel*>("DockTitle"))
        title->setStyleSheet("font-size:17px;font-weight:700;color:#e7fff4;");
    if(auto* mark=home_->findChild<QWidget*>("StatsMark"))mark->setFixedSize(28,28);
    for(auto* mark:home_->findChildren<QWidget*>("SectionIcon"))
        mark->setFixedSize(roomy?28:18,roomy?28:18);
    if(auto* gear=home_->findChild<QPushButton*>("SettingsButton"))gear->setFixedSize(30,28);
    if(systemSelector_)systemSelector_->setFixedSize(roomy?120:wide?74:87,roomy?29:20);
    if(streamCount_)streamCount_->setStyleSheet(QString("color:#ffe184;background:#24301f;border:1px solid #70571b;border-radius:7px;font-size:%1px;font-weight:700;padding:1px 6px;").arg(roomy?14:10));
    for(auto* button:rangeButtons_)button->setFixedSize(roomy?48:34,roomy?32:23);
    for(auto* frame:{systemCard_,streamCard_,recordCard_}){
        auto* layout=static_cast<QVBoxLayout*>(frame->layout());
        layout->setContentsMargins(roomy?13:7,roomy?10:wide?4:5,roomy?13:7,roomy?10:wide?4:5);
        layout->setSpacing(wide&&!roomy?1:2);
    }
}

void StatsDock::resizeEvent(QResizeEvent* event){QWidget::resizeEvent(event);if(cards_)arrangeCards();}

void StatsDock::updateSnapshot(const Snapshot& value){
    last_=value;
    cpuHistory_.add(std::max(0.0,value.system.cpuPc));
    obsCpuHistory_.add(std::max(0.0,value.system.cpuObs));
    if(value.system.gpu>=0)gpuHistory_.add(value.system.gpu);
    fpsHistory_.add(value.system.fps);
    streamHistory_.add(value.stream.kbps);
    recordHistory_.add(value.record.kbps);
    if(!cpuObs_)return;
    const QLocale locale=russian()?QLocale(QLocale::Russian):QLocale(QLocale::English);
    const auto percentage=[&](double n){return n<0?QString("—"):locale.toString(n,'f',1)+" %";};
    const auto rate=[&](double n){return locale.toString(n,'f',0)+" kb/s";};
    cpuObs_->setText(percentage(value.system.cpuObs));cpuPc_->setText(percentage(value.system.cpuPc));
    gpu_->setText(percentage(value.system.gpu));
    gpu_->setToolTip(phrase("Загрузка самого занятого GPU-движка; «—» означает, что счётчик Windows недоступен.",
        "Busiest GPU engine; — means the Windows counter is unavailable."));
    ramObs_->setText(bytesText(value.system.ramObs,locale));
    ramFree_->setText(bytesText(value.system.ramFree,locale));
    fps_->setText(locale.toString(value.system.fps,'f',1));
    render_->setText(locale.toString(value.system.renderMs,'f',1)+" ms");
    const auto frameRatio=[&](quint64 missed,quint64 total){
        const double percent=total?100.0*double(missed)/double(total):0;
        const QString count=locale.toString(missed)+" / "+locale.toString(total);
        return QPair<QString,QString>{count,count+" ("+locale.toString(percent,'f',1)+" %)"};
    };
    const auto renderFrames=frameRatio(value.system.renderMissed,value.system.renderFrames);
    const auto encodeFrames=frameRatio(value.system.encodeSkipped,value.system.encodeFrames);
    renderDrop_->setText(layoutMode_==2?renderFrames.second:renderFrames.first);
    encodeDrop_->setText(layoutMode_==2?encodeFrames.second:encodeFrames.first);
    renderDrop_->setToolTip(renderFrames.second);encodeDrop_->setToolTip(encodeFrames.second);
    streamStatus_->setText(value.stream.reconnecting?phrase("● Переподкл.","● Reconnecting"):
        value.stream.active?phrase("● В эфире","● Live"):phrase("● Неактивна","● Offline"));
    streamStatus_->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;")
        .arg(value.stream.reconnecting?"#ffca56":value.stream.active?"#00df8c":"#8fa5c2")
        .arg(layoutMode_==2?14:10));
    streamBitrate_->setText((value.stream.incomplete?QString::fromUtf8("≥ "):QString())+rate(value.stream.kbps));
    streamBitrate_->setToolTip(value.stream.incomplete?phrase("Сумма неполная: не все активные выходы отдают счётчики.",
        "Incomplete total: some active outputs do not expose measurable counters."):QString());
    streamCount_->setText(phrase("Потоков ","Outputs ")+
        (value.stream.incomplete?QString::fromUtf8("≥ "):QString())+locale.toString(value.stream.activeOutputs));
    streamCount_->setToolTip(value.stream.outputNames.isEmpty()?
        phrase("Учтённых потоковых выходов OBS","Counted OBS stream outputs"):
        phrase("Учтённые выходы OBS: ","Counted OBS outputs: ")+value.stream.outputNames.join(", "));
    streamBytes_->setText(bytesText(value.stream.totalBytes,locale));
    const double loss=value.stream.frames?100.0*double(value.stream.dropped)/double(value.stream.frames):0;
    streamDrop_->setText(locale.toString(value.stream.dropped)+" ("+locale.toString(loss,'f',1)+" %)");
    recordStatus_->setText(value.record.paused?phrase("● Пауза","● Paused"):
        value.record.active?"● REC":phrase("● Нет записи","● Offline"));
    recordStatus_->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;")
        .arg(value.record.paused?"#ffca56":value.record.active?"#ff5570":"#8fa5c2")
        .arg(layoutMode_==2?14:10));
    recordBitrate_->setText(value.record.active?rate(value.record.kbps):"—");
    recordFile_->setText(value.record.fileSizeAvailable?
        (value.record.fileSizeEstimated?QString::fromUtf8("≈ "):QString())+bytesText(value.record.fileBytes,locale):"—");
    recordFile_->setToolTip(value.record.fileSizeEstimated?
        phrase("Оценка по обработанным байтам OBS; при разделении записи может включать предыдущие файлы.",
            "Estimated from OBS processed bytes; split recordings may include previous files."):
        phrase("Размер текущего файла на диске","Current file size on disk"));
    recordSpeed_->setText(value.record.active?locale.toString(value.record.megabytesPerSecond,'f',1)+" MB/s":"—");
    recordTime_->setText(value.record.active?duration(value.record.elapsedSeconds):"—");
    recordTime_->setToolTip(phrase("Время записи с момента, когда Stream Stats увидел её запуск; паузы исключены.",
        "Recording time since Stream Stats observed its start; pauses excluded."));
    recordDisk_->setText(value.record.diskAvailable?bytesText(value.record.freeBytes,locale):"—");
    recordEta_->setText(value.record.secondsUntilFull>=0?duration(value.record.secondsUntilFull):"—");
    systemChart_->setInactiveMessage(systemMetricIndex_==2&&gpuHistory_.count()==0?
        phrase("GPU недоступен","GPU unavailable"):QString());
    streamChart_->setInactiveMessage(!value.stream.active&&!value.stream.reconnecting?
        phrase("Нет активной трансляции","No active stream"):QString());
    recordChart_->setInactiveMessage(!value.record.active?
        phrase("Нет записи","No recording"):value.record.paused?
        phrase("Запись на паузе","Recording paused"):QString());
    for(auto* chart:{systemChart_,streamChart_,recordChart_})chart->update();
}

} // namespace stats

