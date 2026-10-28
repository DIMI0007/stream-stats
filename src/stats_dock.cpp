#include "stats_dock.hpp"
#include "plugin_telemetry.hpp"
#include "version.hpp"
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
#include <thread>
#include <cmath>
#include <QMouseEvent>
#include <QToolTip>
#include <QFileDialog>
#include <QSaveFile>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QDateTime>
#include <QFile>
#include <atomic>
#include <windows.h>

namespace stats {
namespace {
std::atomic<quint64> selfCpuNs{0};
quint64 currentThreadCpuNs(){
    FILETIME created,ended,kernel,user;
    if(!GetThreadTimes(GetCurrentThread(),&created,&ended,&kernel,&user))return 0;
    ULARGE_INTEGER k,u;k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
    u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
    return (k.QuadPart+u.QuadPart)*100;
}
struct SelfCpuScope{
    quint64 start=currentThreadCpuNs();
    ~SelfCpuScope(){const auto end=currentThreadCpuNs();if(start&&end>=start)selfCpuNs.fetch_add(end-start,std::memory_order_relaxed);}
};

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
QString peerBytes(quint64 bytes,const QLocale& locale) {
    if(bytes<1024)return locale.toString(bytes)+(locale.language()==QLocale::Russian?" Б":" B");
    if(bytes<1024*1024)return locale.toString(double(bytes)/1024.0,'f',1)+(locale.language()==QLocale::Russian?" КБ":" KB");
    if(bytes>=1024ull*1024*1024)return locale.toString(double(bytes)/(1024.0*1024.0*1024.0),'f',2)+(locale.language()==QLocale::Russian?" ГБ":" GB");
    return locale.toString(double(bytes)/(1024.0*1024.0),'f',1)+(locale.language()==QLocale::Russian?" МБ":" MB");
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
        p.setPen(Qt::NoPen);p.setBrush(QColor("#06382c"));p.drawRoundedRect(QRectF(0,0,32,32),9,9);
        const QColor green("#00df8c");p.setBrush(green);p.drawEllipse(QPointF(16,16),3,3);
        p.setBrush(Qt::NoBrush);p.setPen(QPen(green,2.5,Qt::SolidLine,Qt::RoundCap));
        p.drawArc(QRectF(7,7,18,18),-50*16,100*16);p.drawArc(QRectF(7,7,18,18),130*16,100*16);
        p.drawArc(QRectF(3,3,26,26),-45*16,90*16);p.drawArc(QRectF(3,3,26,26),135*16,90*16);
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
    auto* label=new QLabel(title);label->setObjectName("CardTitle");label->setWordWrap(true);
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

enum class MetricGlyph {Chip,Monitor,Gpu,Memory,Gauge,Timer,Frame,Encode,
    Broadcast,Upload,Data,Network,Record,File,Speed,Clock,Disk,Hourglass};

class MetricIcon final : public QWidget {
public:
    MetricIcon(MetricGlyph glyph,const QColor& color,const QString& name)
        :glyph_(glyph),color_(color){setObjectName("MetricIcon");setFixedSize(16,16);setToolTip(name);setAccessibleName(name);}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const qreal scale=std::min(width(),height())/20.0;
        p.translate(width()/2.0,height()/2.0);p.scale(scale,scale);p.translate(-10,-10);
        p.setPen(QPen(color_,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.setBrush(Qt::NoBrush);
        switch(glyph_){
        case MetricGlyph::Chip:
            p.drawRoundedRect(QRectF(5,5,10,10),1.5,1.5);
            p.drawRoundedRect(QRectF(8,8,4,4),0.7,0.7);
            for(int n:{7,13}){p.drawLine(QPointF(n,2),QPointF(n,5));p.drawLine(QPointF(n,15),QPointF(n,18));
                p.drawLine(QPointF(2,n),QPointF(5,n));p.drawLine(QPointF(15,n),QPointF(18,n));}break;
        case MetricGlyph::Monitor:
            p.drawRoundedRect(QRectF(2.5,3.5,15,11),1.5,1.5);
            p.drawLine(QPointF(10,14.5),QPointF(10,17));p.drawLine(QPointF(6,17),QPointF(14,17));break;
        case MetricGlyph::Gpu:
            p.drawRoundedRect(QRectF(2,5,16,10),1.5,1.5);p.drawEllipse(QPointF(10,10),3,3);
            p.drawLine(QPointF(4,16),QPointF(16,16));p.drawLine(QPointF(5,7),QPointF(6,7));break;
        case MetricGlyph::Memory:
            p.drawRoundedRect(QRectF(2,6,16,9),1,1);
            for(int x:{5,9,13})p.drawLine(QPointF(x,8),QPointF(x,11));
            p.drawLine(QPointF(5,15),QPointF(5,17));p.drawLine(QPointF(10,15),QPointF(10,17));
            p.drawLine(QPointF(15,15),QPointF(15,17));break;
        case MetricGlyph::Gauge:
            p.drawArc(QRectF(2.5,3,15,15),30*16,150*16);p.drawArc(QRectF(2.5,3,15,15),210*16,150*16);
            p.drawLine(QPointF(10,11),QPointF(14,7));p.setBrush(color_);p.drawEllipse(QPointF(10,11),1.2,1.2);break;
        case MetricGlyph::Timer:
            p.drawEllipse(QRectF(4,5,12,12));p.drawLine(QPointF(10,5),QPointF(10,2));
            p.drawLine(QPointF(7,2),QPointF(13,2));p.drawLine(QPointF(10,11),QPointF(13,8));break;
        case MetricGlyph::Frame:
            p.drawRoundedRect(QRectF(4,3,12,14),1,1);p.drawLine(QPointF(7,7),QPointF(13,7));
            p.drawLine(QPointF(7,10),QPointF(13,10));p.drawLine(QPointF(7,13),QPointF(11,13));break;
        case MetricGlyph::Encode:
            p.drawRoundedRect(QRectF(3.5,4,13,12),1,1);p.drawLine(QPointF(6,8),QPointF(14,8));
            p.drawLine(QPointF(6,12),QPointF(11,12));p.drawLine(QPointF(13,11),QPointF(16,14));break;
        case MetricGlyph::Broadcast:
            p.setBrush(color_);p.drawEllipse(QPointF(10,10),1.8,1.8);p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(5,5,10,10),-60*16,120*16);p.drawArc(QRectF(5,5,10,10),120*16,120*16);
            p.drawArc(QRectF(2,2,16,16),-55*16,110*16);p.drawArc(QRectF(2,2,16,16),125*16,110*16);break;
        case MetricGlyph::Upload:
            p.drawLine(QPointF(10,15),QPointF(10,3));p.drawLine(QPointF(6,7),QPointF(10,3));
            p.drawLine(QPointF(10,3),QPointF(14,7));p.drawLine(QPointF(4,14),QPointF(4,17));
            p.drawLine(QPointF(4,17),QPointF(16,17));p.drawLine(QPointF(16,17),QPointF(16,14));break;
        case MetricGlyph::Data:
            p.drawEllipse(QRectF(3,3,14,5));p.drawArc(QRectF(3,8,14,5),180*16,180*16);
            p.drawArc(QRectF(3,12,14,5),180*16,180*16);p.drawLine(QPointF(3,5.5),QPointF(3,14.5));
            p.drawLine(QPointF(17,5.5),QPointF(17,14.5));break;
        case MetricGlyph::Network:
            p.drawArc(QRectF(2,4,16,13),40*16,100*16);p.drawArc(QRectF(5,8,10,8),40*16,100*16);
            p.setBrush(color_);p.drawEllipse(QPointF(10,16),1.2,1.2);break;
        case MetricGlyph::Record:
            p.drawEllipse(QRectF(2.5,2.5,15,15));p.setBrush(color_);p.drawEllipse(QPointF(10,10),4,4);break;
        case MetricGlyph::File:
            p.drawLine(QPointF(5,2.5),QPointF(12,2.5));p.drawLine(QPointF(12,2.5),QPointF(16,6.5));
            p.drawLine(QPointF(16,6.5),QPointF(16,17));p.drawLine(QPointF(16,17),QPointF(5,17));
            p.drawLine(QPointF(5,17),QPointF(5,2.5));p.drawLine(QPointF(12,2.5),QPointF(12,6.5));
            p.drawLine(QPointF(12,6.5),QPointF(16,6.5));break;
        case MetricGlyph::Speed:
            p.drawArc(QRectF(2.5,3,15,15),30*16,300*16);
            p.drawLine(QPointF(10,11),QPointF(15,6));p.setBrush(color_);p.drawEllipse(QPointF(10,11),1.2,1.2);break;
        case MetricGlyph::Clock:
            p.drawEllipse(QRectF(2.5,2.5,15,15));p.drawLine(QPointF(10,10),QPointF(10,5));
            p.drawLine(QPointF(10,10),QPointF(13.5,12));break;
        case MetricGlyph::Disk:
            p.drawRoundedRect(QRectF(2.5,5,15,11),1.5,1.5);p.drawLine(QPointF(3,12),QPointF(17,12));
            p.setBrush(color_);p.drawEllipse(QPointF(14.5,14),0.8,0.8);break;
        case MetricGlyph::Hourglass:
            p.drawLine(QPointF(5,2.5),QPointF(15,2.5));p.drawLine(QPointF(5,17.5),QPointF(15,17.5));
            p.drawLine(QPointF(6,3),QPointF(6,6));p.drawLine(QPointF(14,3),QPointF(14,6));
            p.drawLine(QPointF(6,6),QPointF(10,10));p.drawLine(QPointF(14,6),QPointF(10,10));
            p.drawLine(QPointF(10,10),QPointF(6,14));p.drawLine(QPointF(10,10),QPointF(14,14));
            p.drawLine(QPointF(6,14),QPointF(6,17));p.drawLine(QPointF(14,14),QPointF(14,17));break;
        }
    }
private:
    MetricGlyph glyph_;
    QColor color_;
};

QLabel* addMetric(QGridLayout* grid,const QString& name,MetricGlyph glyph,const QColor& color) {
    auto* wrapper=new QWidget;wrapper->setObjectName("MetricRow");auto* row=new QHBoxLayout(wrapper);
    row->setContentsMargins(0,0,0,0);row->setSpacing(3);
    row->addWidget(new MetricIcon(glyph,color,name));
    auto* left=new QLabel(name);left->setObjectName("MetricName");left->setMinimumWidth(0);
    left->setWordWrap(true);left->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);left->setToolTip(name);
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
        :QWidget(parent),series_(std::move(series)) {setMinimumWidth(0);setFixedHeight(38);setMouseTracking(true);}
    void setRussian(bool value){ru_=value;}
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
        const QRectF inner=plot.adjusted(labeled?29:6,labeled?17:5,-6,labeled?-14:-6);
        p.setPen(QPen(QColor("#234056"),1,Qt::DashLine));
        for(int i=0;i<3;++i){const qreal y=inner.top()+inner.height()*i/2.0;
            p.drawLine(QPointF(inner.left(),y),QPointF(inner.right(),y));}
        if(!inactiveMessage_.isEmpty()){
            p.setPen(QColor("#8199b6"));QFont font=p.font();font.setPixelSize(labeled?11:9);p.setFont(font);
            p.drawText(inner,Qt::AlignCenter,inactiveMessage_);return;
        }
        QVector<QVector<double>> values;values.reserve(series_.size());
        double maximum=0;
        for(const auto& series:series_){
            values.push_back(series.first->recent(seconds_));
            for(double value:values.back())maximum=std::max(maximum,value);
        }
        const double observedMax=maximum;
        maximum=unit_=="%"?100:std::max(1.0,std::ceil(maximum*1.2));
        if(labeled){
            p.setPen(QPen(series_.isEmpty()?QColor("#8ea8c7"):series_.front().second,2));
            p.drawLine(QPointF(plot.left()+7,plot.top()+9),QPointF(plot.left()+17,plot.top()+9));
            p.setPen(QColor("#b5c9e2"));QFont font=p.font();font.setPixelSize(9);p.setFont(font);
            p.drawText(QRectF(plot.left()+22,plot.top()+3,plot.width()-29,12),Qt::AlignLeft,
                label_+QString::fromUtf8(ru_?" · макс. ":" · max ")+phraseMax(observedMax,unit_));
            p.setPen(QColor("#8ea1c0"));font.setPixelSize(8);p.setFont(font);
            p.drawText(QRectF(plot.left()+2,inner.top()-3,25,10),Qt::AlignLeft,QString::number(maximum,'f',0));
            p.drawText(QRectF(plot.left()+2,inner.bottom()-7,25,10),Qt::AlignLeft,"0");
            p.drawText(QRectF(inner.left(),inner.bottom()+2,inner.width(),10),Qt::AlignLeft,QString("−%1 %2").arg(seconds_/60).arg(ru_?"мин":"min"));
            p.drawText(QRectF(inner.left(),inner.bottom()+2,inner.width(),10),Qt::AlignRight,ru_?"сейчас":"now");
        }
        p.save();p.setClipRect(inner.adjusted(-2,-2,2,2));
        for(int s=0;s<series_.size();++s){
            const auto& points=values[s];if(points.size()<2)continue;
            QVector<QPointF> nodes;
            nodes.reserve(std::min(int(points.size()),std::max(0,int(inner.width()))*4+4));
            const auto position=[&](int i){
                const qreal age=qreal(points.size()-1-i);
                return QPointF(inner.right()-age*inner.width()/qreal(std::max(1,seconds_-1)),
                    inner.bottom()-qreal(points[i]/maximum)*inner.height());
            };
            // Keep both extremes in each screen pixel. This removes redundant
            // path segments without hiding brief CPU or bitrate spikes.
            for(int first=0;first<points.size();){
                const int column=int(std::floor(position(first).x()));
                int last=first,low=first,high=first;
                while(last+1<points.size()&&int(std::floor(position(last+1).x()))==column){
                    ++last;
                    if(points[last]<points[low])low=last;
                    if(points[last]>points[high])high=last;
                }
                nodes.push_back(position(first));
                if(low!=first&&low!=last&&low<high)nodes.push_back(position(low));
                if(high!=first&&high!=last)nodes.push_back(position(high));
                if(low!=first&&low!=last&&low>high)nodes.push_back(position(low));
                if(last!=first)nodes.push_back(position(last));
                first=last+1;
            }
            QPainterPath line(nodes.front());
            for(int i=1;i<nodes.size();++i)line.lineTo(nodes[i]);
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
    void mouseMoveEvent(QMouseEvent* event)override{
        if(series_.isEmpty()||!inactiveMessage_.isEmpty())return;
        const auto points=series_.front().first->recent(seconds_);if(points.isEmpty())return;
        const double left=height()>=55?30:7,right=width()-7;
        if(right<=left)return;
        const int age=qRound(std::clamp((right-event->position().x())/(right-left),0.0,1.0)*(seconds_-1));
        const int index=points.size()-1-age;if(index<0){QToolTip::hideText();return;}
        const auto extremes=std::minmax_element(points.cbegin(),points.cend());double sum=0;for(double value:points)sum+=value;
        const QLocale locale(ru_?QLocale::Russian:QLocale::English);
        const auto number=[&](double value){return locale.toString(value,'f',1)+" "+unit_;};
        const QString text=label_+" · "+QString::number(age)+(ru_?" сек. назад":" sec ago")+"\n"+number(points[index])+"\n"+
            (ru_?"Мин.: ":"Min: ")+number(*extremes.first)+(ru_?" · Сред.: ":" · Avg: ")+number(sum/points.size())+
            (ru_?" · Макс.: ":" · Max: ")+number(*extremes.second);
        QToolTip::showText(event->globalPosition().toPoint(),text,this);
    }
    void leaveEvent(QEvent*)override{QToolTip::hideText();}
private:
    static QString phraseMax(double max,const QString& unit){
        return QString::number(max,'f',0)+(unit.isEmpty()?QString():" "+unit);
    }
    QVector<QPair<const History*,QColor>> series_;
    int seconds_=60;
    QString unit_,label_;
    QString inactiveMessage_;
    bool ru_=true;
};

StatsDock::StatsDock(QWidget* parent):QWidget(parent) {
    setObjectName("StreamStatsDock");setMinimumSize(275,200);
    setStyleSheet(R"(
      QWidget#StreamStatsDock{background:#080f18;color:#dfe8ff;font-family:'Segoe UI';font-size:10px;}
      QLabel,QCheckBox{color:#dfe8ff;background:transparent;}
      QWidget#SummaryContent{background:#080f18;}
      QFrame#StatsCard{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #101925,stop:1 #0b131e);border:1px solid #263244;border-radius:8px;}
      QLabel#CardTitle{font-size:12px;font-weight:700;}
      QLabel#MetricName{color:#8ea1c0;font-size:10px;}
      QLabel#MetricValue{color:#dfe8ff;font-size:10px;font-weight:700;}
      QLabel#DockTitle{font-size:17px;font-weight:700;color:#e7fff4;}
      QLabel#OutputCount{color:#ffe184;background:#24301f;border:1px solid #70571b;border-radius:7px;font-size:10px;font-weight:700;padding:1px 6px;}
      QWidget#Footer{background:#080f18;}
      QLabel#Credit{color:#9299a5;background:#080f18;font-size:11px;}
      QLabel#SessionTrafficFooter{color:#72e9f4;background:#080f18;font-size:10px;}
      QPushButton#PeersToggle{color:#72e9f4;background:#0b1724;border:1px solid #263244;border-radius:5px;text-align:left;padding:0 7px;font-size:10px;}
      QPushButton#PeersToggle:hover{background:#142438;border-color:#38516d;}
      QWidget#PeersPanel{background:#0b1724;border:1px solid #263244;border-radius:6px;}
      QWidget#PeerRow{background:#101e2b;border:1px solid #263244;border-radius:5px;}
      QLabel#PeerName{color:#dfe8ff;font-size:10px;font-weight:700;}
      QLabel#PeerTraffic,QLabel#PeersTotal{color:#72e9f4;font-size:10px;}
      QLabel#PeerStatus{font-size:10px;}
      QComboBox#SystemMetricSelector{color:#e7eeff;background:#0c1420;border:1px solid #344865;border-radius:6px;padding:1px 5px;font-size:10px;}
      QComboBox#SystemMetricSelector QAbstractItemView{background:#101a2b;color:#e7eeff;selection-background-color:#26344d;}
      QPushButton{color:#e1ebff;background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #202a3b,stop:1 #141d2b);border:1px solid #39465d;border-radius:6px;padding:2px 6px;}
      QPushButton:hover{background:#26344d;border-color:#6476a0;}
      QPushButton#SettingsButton{color:#e1ebff;background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #202a3b,stop:1 #141d2b);border:1px solid #39465d;border-radius:8px;padding:0;}
      QPushButton#SettingsButton:hover{background:#26344d;border-color:#6476a0;}
      QPushButton#ActiveRange{background:#102c29;border-color:#00d58a;color:#b8ffe1;}
      QScrollArea{border:0;background:transparent;}
      QScrollArea QWidget#CardHost{background:transparent;}
    )");
    QSettings saved(configFile(),QSettings::IniFormat);
    language_=saved.value("language","auto").toString();
    rangeMinutes_=saved.value("rangeMinutes",1).toInt();
    if(rangeMinutes_!=1&&rangeMinutes_!=5&&rangeMinutes_!=15)rangeMinutes_=1;
    systemMetricIndex_=std::clamp(saved.value("systemMetric",0).toInt(),0,3);
    warningsEnabled_=saved.value("warningsEnabled",true).toBool();sessionClock_.start();sampleClock_.start();
    autoCollapseRecording_=saved.value("autoCollapseRecording",true).toBool();
    auto* root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);
    pages_=new QStackedWidget;root->addWidget(pages_);
    buildPages();
    tick_.setTimerType(Qt::CoarseTimer);
    QObject::connect(&tick_,&QTimer::timeout,this,[this]{if(!closing_){SelfCpuScope cpu;updateSnapshot(collector_.sample());}});
    tick_.start(1000);
    updateSnapshot(collector_.sample());
}

void StatsDock::shutdown(){
    if(closing_)return;closing_=true;tick_.stop();
#ifndef STREAM_STATS_PREVIEW
    const auto path=QFileInfo(configFile()).absolutePath()+"/last-session-summary.txt";
    QSaveFile saved(path);if(saved.open(QIODevice::WriteOnly)){saved.write(summaryText().toUtf8());saved.commit();}
#endif
}
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
    if(summaryPage_){pages_->removeWidget(summaryPage_);summaryPage_->deleteLater();summaryPage_=nullptr;}
    if(home_){pages_->removeWidget(home_);home_->deleteLater();home_=nullptr;}
    if(settings_){pages_->removeWidget(settings_);settings_->deleteLater();settings_=nullptr;}
    layoutMode_=-1;home_=buildHome();settings_=buildSettings();
    pages_->addWidget(home_);pages_->addWidget(settings_);pages_->setCurrentWidget(home_);
    arrangeCards();updateSnapshot(last_,false);
}

QWidget* StatsDock::buildHome() {
    auto* page=new QWidget;auto* outer=new QVBoxLayout(page);
    outer->setContentsMargins(8,8,8,3);outer->setSpacing(5);
    auto* top=new QHBoxLayout;top->setContentsMargins(2,2,2,2);top->setSpacing(7);headerLayout_=top;
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
    warningLabel_=new QLabel;warningLabel_->setObjectName("HealthWarning");warningLabel_->setWordWrap(true);
    warningLabel_->setStyleSheet("color:#ffdc45;background:#272116;border:1px solid #65532a;border-radius:5px;padding:3px;font-size:10px;");warningLabel_->hide();outer->addWidget(warningLabel_);
    auto* scroller=new QScrollArea;scroller->setWidgetResizable(true);
    scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* host=new QWidget;host->setObjectName("CardHost");cards_=new QGridLayout(host);
    cards_->setContentsMargins(0,0,0,0);cards_->setSpacing(5);
    QVBoxLayout* body=nullptr;
    QHBoxLayout* systemHeader=nullptr;
    systemCard_=card(phrase("Система","System"),QColor("#72e9f4"),0,body,&systemHeader);
    systemSelector_=new QComboBox;systemSelector_->setObjectName("SystemMetricSelector");
    systemSelector_->addItems({phrase("CPU ПК","PC CPU"),phrase("CPU OBS","OBS CPU"),phrase("GPU ПК","PC GPU"),"FPS"});
    systemSelector_->setToolTip(phrase("Показатель системного графика","System graph metric"));
    systemHeader->addWidget(systemSelector_);
    metricGrids_[0]=metricGrid(body);
    cpuObs_=addMetric(metricGrids_[0],phrase("CPU OBS","OBS CPU"),MetricGlyph::Chip,QColor("#72e9f4"));
    cpuPc_=addMetric(metricGrids_[0],phrase("CPU ПК","PC CPU"),MetricGlyph::Monitor,QColor("#07df91"));
    gpu_=addMetric(metricGrids_[0],phrase("GPU компьютера","Computer GPU"),MetricGlyph::Gpu,QColor("#b995ff"));
    ramObs_=addMetric(metricGrids_[0],phrase("RAM OBS","OBS RAM"),MetricGlyph::Memory,QColor("#72e9f4"));
    ramFree_=addMetric(metricGrids_[0],phrase("Своб. RAM","Free RAM"),MetricGlyph::Memory,QColor("#8ab7ff"));
    fps_=addMetric(metricGrids_[0],"FPS",MetricGlyph::Gauge,QColor("#07df91"));
    render_=addMetric(metricGrids_[0],phrase("Рендер","Render"),MetricGlyph::Timer,QColor("#ffdc45"));
    renderDrop_=addMetric(metricGrids_[0],phrase("Пропуск ренд.","Render missed"),MetricGlyph::Frame,QColor("#ff8484"));
    encodeDrop_=addMetric(metricGrids_[0],phrase("Пропуск код.","Encode skipped"),MetricGlyph::Encode,QColor("#f6a86c"));
    systemChart_=new Chart({{&cpuHistory_,QColor("#07df91")}});
    systemChart_->setSeries({{&cpuHistory_,QColor("#07df91")}},"%",phrase("CPU ПК","PC CPU"));
    QObject::connect(systemSelector_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){
        if(!systemChart_)return;
        systemMetricIndex_=index;
        QSettings saved(configFile(),QSettings::IniFormat);saved.setValue("systemMetric",index);
        switch(index){
        case 1:systemChart_->setSeries({{&obsCpuHistory_,QColor("#59dfff")}},"%",phrase("CPU OBS","OBS CPU"));break;
        case 2:systemChart_->setSeries({{&gpuHistory_,QColor("#b995ff")}},"%",phrase("GPU ПК","PC GPU"));break;
        case 3:systemChart_->setSeries({{&fpsHistory_,QColor("#ffdc45")}},"FPS","FPS");break;
        default:systemChart_->setSeries({{&cpuHistory_,QColor("#07df91")}},"%",phrase("CPU ПК","PC CPU"));break;
        }
    });
    systemSelector_->setCurrentIndex(systemMetricIndex_);
    body->addStretch(1);
    body->addWidget(systemChart_);

    QHBoxLayout* streamHeader=nullptr;
    streamCard_=card(phrase("Трансляция","Stream"),QColor("#ffdc45"),1,body,&streamHeader);
    streamCount_=new QLabel("0");streamCount_->setObjectName("OutputCount");
    streamCount_->setAlignment(Qt::AlignCenter);streamCount_->setToolTip(phrase("Активных потоковых выходов OBS","Active OBS stream outputs"));
    streamHeader->addWidget(streamCount_);
    metricGrids_[1]=metricGrid(body);
    streamStatus_=addMetric(metricGrids_[1],phrase("Статус","Status"),MetricGlyph::Broadcast,QColor("#ffdc45"));
    streamBitrate_=addMetric(metricGrids_[1],phrase("Общий битрейт","Total bitrate"),MetricGlyph::Upload,QColor("#ffdc45"));
    streamBytes_=addMetric(metricGrids_[1],phrase("Отправлено","Sent"),MetricGlyph::Data,QColor("#9eb9ff"));
    streamDrop_=addMetric(metricGrids_[1],phrase("Потери сети","Network loss"),MetricGlyph::Network,QColor("#ff8484"));
    body->addStretch(1);
    streamChart_=new Chart({{&streamHistory_,QColor("#ffdc45")}});
    streamChart_->setSeries({{&streamHistory_,QColor("#ffdc45")}},"kb/s",phrase("Общий битрейт","Total bitrate"));
    streamChart_->setToolTip(phrase("Суммарный фактический битрейт активных выходов","Actual aggregate bitrate of active outputs"));
    body->addWidget(streamChart_);

    QHBoxLayout* recordHeader=nullptr;
    recordCard_=card(phrase("Запись","Recording"),QColor("#ff8484"),2,body,&recordHeader);
    recordDetails_=new QPushButton(phrase("Подробнее","Details"));recordDetails_->setObjectName("RecordDetails");recordDetails_->setStyleSheet("font-size:9px;padding:0 3px;");
    recordDetails_->setFixedHeight(18);recordDetails_->setCheckable(true);recordDetails_->setChecked(recordExpanded_);recordHeader->addWidget(recordDetails_);
    QObject::connect(recordDetails_,&QPushButton::toggled,this,[this](bool open){recordExpanded_=open;updateRecordDetails();});
    metricGrids_[2]=metricGrid(body);
    recordStatus_=addMetric(metricGrids_[2],phrase("Статус","Status"),MetricGlyph::Record,QColor("#ff647e"));
    recordBitrate_=addMetric(metricGrids_[2],phrase("Битрейт","Bitrate"),MetricGlyph::Data,QColor("#72e9f4"));
    recordFile_=addMetric(metricGrids_[2],phrase("Размер файла","File size"),MetricGlyph::File,QColor("#b995ff"));
    recordSpeed_=addMetric(metricGrids_[2],phrase("Скорость","Write speed"),MetricGlyph::Speed,QColor("#07df91"));
    recordTime_=addMetric(metricGrids_[2],phrase("Время","Duration"),MetricGlyph::Clock,QColor("#8ab7ff"));
    recordDisk_=addMetric(metricGrids_[2],phrase("Диск свободен","Disk free"),MetricGlyph::Disk,QColor("#72e9f4"));
    recordEta_=addMetric(metricGrids_[2],phrase("До заполн.","Until full"),MetricGlyph::Hourglass,QColor("#ffdc45"));
    body->addStretch(1);
    recordChart_=new Chart({{&recordHistory_,QColor("#72e9f4")}});
    recordChart_->setSeries({{&recordHistory_,QColor("#72e9f4")}},"kb/s",phrase("Битрейт записи","Record bitrate"));
    recordChart_->setToolTip(phrase("Фактический битрейт записи","Actual recording bitrate"));
    body->addWidget(recordChart_);
    for(auto* chart:{systemChart_,streamChart_,recordChart_})chart->setRussian(russian());
    scroller->setWidget(host);outer->addWidget(scroller,1);
    peersToggle_=new QPushButton(phrase("▸ Наши плагины","▸ Our plugins"));
    peersToggle_->setObjectName("PeersToggle");peersToggle_->setCheckable(true);
    peersToggle_->setFixedHeight(16);
    peersToggle_->setToolTip(phrase("Трафик Chat и Notify за текущий запуск OBS. Связь означает доступность их локальных счётчиков.",
        "Chat and Notify traffic since OBS started. Connected means their local counters are reachable."));
    peersPanel_=new QWidget;peersPanel_->setObjectName("PeersPanel");
    auto* peersLayout=new QVBoxLayout(peersPanel_);peersLayout->setContentsMargins(7,5,7,5);peersLayout->setSpacing(3);
    const char* names[]={"Stream Chat","Stream Notify","Stream Stats"};
    for(int i=0;i<3;++i){
        auto* peer=new QWidget;peer->setObjectName("PeerRow");
        auto* peerLayout=new QVBoxLayout(peer);peerLayout->setContentsMargins(5,3,5,3);peerLayout->setSpacing(1);
        auto* topLine=new QHBoxLayout;topLine->setSpacing(5);
        auto* name=new QLabel(names[i]);name->setObjectName("PeerName");
        peerTraffic_[i]=new QLabel;peerTraffic_[i]->setObjectName("PeerTraffic");
        peerTraffic_[i]->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
        peerStatus_[i]=new QLabel;peerStatus_[i]->setObjectName("PeerStatus");
        peerStatus_[i]->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        peerResources_[i]=new QLabel;peerResources_[i]->setObjectName("PeerResources");
        peerResources_[i]->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        peerResources_[i]->setStyleSheet("color:#8ea1c0;font-size:10px;");
        peerResources_[i]->setToolTip(phrase("CPU — измеренные участки работы плагина; память — учтённые данные и буферы, не вся RAM процесса OBS.",
            "CPU is measured plugin work; memory is tracked data and buffers, not the full OBS process RAM."));
        topLine->addWidget(name,1);topLine->addWidget(peerResources_[i]);topLine->addWidget(peerStatus_[i]);
        peerLayout->addLayout(topLine);peerLayout->addWidget(peerTraffic_[i]);
        peersLayout->addWidget(peer);
    }
    peersTotal_=new QLabel;peersTotal_->setObjectName("PeersTotal");
    peersTotal_->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    peersLayout->addWidget(peersTotal_);
    peersPanel_->hide();
    QObject::connect(peersToggle_,&QPushButton::toggled,this,[this](bool open){
        peersPanel_->setVisible(open);
        peersToggle_->setText(open?phrase("▾ Наши плагины","▾ Our plugins"):
            phrase("▸ Наши плагины","▸ Our plugins"));
        if(open)updatePeers();
    });
    auto* footerWidget=new QWidget;footerWidget->setObjectName("Footer");
    auto* footerLayout=new QVBoxLayout(footerWidget);footerLayout->setContentsMargins(0,0,0,0);footerLayout->setSpacing(0);
    footerLayout->addWidget(peersToggle_);
    footerLayout->addWidget(peersPanel_);
    footer_=new QLabel("developed by <a href=\"https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/\" style=\"color:#59dfff;text-decoration:none;\">OTMOPOZOK</a> · v" STREAM_STATS_VERSION);
    footer_->setObjectName("Credit");footer_->setTextFormat(Qt::RichText);
    footer_->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::LinksAccessibleByKeyboard);
    footer_->setOpenExternalLinks(true);footer_->setAlignment(Qt::AlignCenter);
    footer_->setFixedHeight(20);footerLayout->addWidget(footer_);
    outer->addWidget(footerWidget);
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
    auto* warnings=new QCheckBox(phrase("Предупреждения о кадрах и диске","Frame loss and disk warnings"));
    warnings->setChecked(warningsEnabled_);warnings->setToolTip(phrase("Кадры: потери выше 1% за последние 10 секунд. Диск: меньше 5 ГБ или 15 минут записи в течение 30 секунд.","Frames: over 1% loss in the last 10 seconds. Disk: under 5 GB or 15 minutes remaining for 30 seconds."));
    QObject::connect(warnings,&QCheckBox::toggled,this,[this](bool on){warningsEnabled_=on;QSettings saved(configFile(),QSettings::IniFormat);saved.setValue("warningsEnabled",on);if(!on&&warningLabel_)warningLabel_->hide();});outer->addWidget(warnings);
    auto* autoCollapse=new QCheckBox;
    autoCollapse->setObjectName("AutoCollapseRecording");autoCollapse->setChecked(autoCollapseRecording_);
    auto* autoCollapseRow=new QHBoxLayout;autoCollapseRow->setSpacing(6);
    auto* autoCollapseLabel=new QLabel(phrase("Сворачивать статистику записи после остановки","Collapse recording statistics after stopping"));
    autoCollapseLabel->setWordWrap(true);autoCollapseLabel->setBuddy(autoCollapse);
    autoCollapseRow->addWidget(autoCollapse);autoCollapseRow->addWidget(autoCollapseLabel,1);
    QObject::connect(autoCollapse,&QCheckBox::toggled,this,[this](bool on){
        autoCollapseRecording_=on;QSettings saved(configFile(),QSettings::IniFormat);saved.setValue("autoCollapseRecording",on);
    });outer->addLayout(autoCollapseRow);
    auto* summary=new QPushButton(phrase("Сводка текущего сеанса","Current session summary"));summary->setObjectName("SessionSummaryButton");
    QObject::connect(summary,&QPushButton::clicked,this,[this]{showSummary();});outer->addWidget(summary);
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
    const bool narrow=width()<300;
    const bool compactHeader=width()<340;
    const bool wide=width()>=680 && width()>height()*1.3;
    const bool roomy=wide&&width()>=1100&&height()>=600;
    const int mode=roomy?2:wide?1:narrow?3:0;
    if(layoutMode_==mode&&compactHeader_==int(compactHeader))return;
    compactHeader_=int(compactHeader);
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
    const int chartHeight=roomy?155:wide?22:60;
    for(auto* chart:{systemChart_,streamChart_,recordChart_})chart->setDisplayHeight(chartHeight);
    const int columns=((wide&&!roomy)||narrow)?1:2;
    const int metricHeight=roomy?47:wide?18:narrow?26:34;
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
                "QWidget#MetricRow{background:#111f2d;border:1px solid #304a5d;border-radius:5px;}":
                "QWidget#MetricRow{background:transparent;border:0;}");
            auto* rowLayout=static_cast<QHBoxLayout*>(row->layout());
            rowLayout->setContentsMargins(columns==2?5:0,0,columns==2?5:0,0);
            if(columns==2&&i==rows.size()-1&&rows.size()%2)
                grid->addWidget(row,i/columns,0,1,2);
            else grid->addWidget(row,i/columns,i%columns);
        }
    }
    for(auto* label:home_->findChildren<QLabel*>("MetricName"))
        label->setStyleSheet(QString("color:#8ea1c0;font-size:%1px;").arg(metricFont));
    for(auto* label:home_->findChildren<QLabel*>("MetricValue"))
        label->setStyleSheet(QString("color:#dfe8ff;font-size:%1px;font-weight:700;").arg(metricFont));
    for(auto* icon:home_->findChildren<QWidget*>("MetricIcon"))
        icon->setFixedSize(roomy?24:wide?13:16,roomy?24:wide?13:16);
    const auto accentValue=[&](QLabel* label,const char* color){
        label->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;").arg(color).arg(metricFont));
    };
    for(auto* label:{cpuObs_,cpuPc_,gpu_,fps_})accentValue(label,"#07df91");
    accentValue(streamBitrate_,"#ffdc45");
    accentValue(recordBitrate_,"#72e9f4");
    for(auto* label:home_->findChildren<QLabel*>("CardTitle"))
        label->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:700;")
            .arg(label->property("accent").toString()).arg(roomy?22:wide?12:15));
    if(auto* title=home_->findChild<QLabel*>("DockTitle"))
        title->setStyleSheet(QString("font-size:%1px;font-weight:700;color:#e7fff4;").arg(compactHeader?15:17));
    if(headerLayout_)headerLayout_->setSpacing(compactHeader?4:7);
    if(auto* mark=home_->findChild<QWidget*>("StatsMark"))mark->setFixedSize(28,28);
    for(auto* mark:home_->findChildren<QWidget*>("SectionIcon"))
        mark->setFixedSize(roomy?28:18,roomy?28:18);
    if(auto* gear=home_->findChild<QPushButton*>("SettingsButton"))gear->setFixedSize(30,28);
    if(systemSelector_)systemSelector_->setFixedSize(roomy?120:wide?74:87,roomy?29:20);
    if(streamCount_)streamCount_->setStyleSheet(QString("color:#ffe184;background:#24301f;border:1px solid #70571b;border-radius:7px;font-size:%1px;font-weight:700;padding:1px 6px;").arg(roomy?14:10));
    for(auto* button:rangeButtons_){
        button->setFixedSize(roomy?48:compactHeader?28:34,roomy?32:23);
        button->setStyleSheet(compactHeader?"padding:0 1px;":QString());
    }
    for(auto* frame:{systemCard_,streamCard_,recordCard_}){
        auto* layout=static_cast<QVBoxLayout*>(frame->layout());
        layout->setContentsMargins(roomy?13:7,roomy?10:wide?4:5,roomy?13:7,roomy?10:wide?4:5);
        layout->setSpacing(wide&&!roomy?1:2);
    }
}

void StatsDock::resizeEvent(QResizeEvent* event){QWidget::resizeEvent(event);if(cards_)arrangeCards();}

void StatsDock::updateSnapshot(const Snapshot& value,bool collect){
    if(collect){
        const double seconds=sampleClock_.isValid()?sampleClock_.restart()/1000.0:0;
        updateHealth(value,seconds);
    }
    const bool recordingEnded=last_.record.active&&!value.record.active;
    last_=value;
    if(recordingEnded){
        recordExpanded_=!autoCollapseRecording_;
        if(recordDetails_){const QSignalBlocker blocked(recordDetails_);recordDetails_->setChecked(recordExpanded_);}
    }
    if(collect){
    cpuHistory_.add(std::max(0.0,value.system.cpuPc));
    obsCpuHistory_.add(std::max(0.0,value.system.cpuObs));
    if(value.system.gpu>=0)gpuHistory_.add(value.system.gpu);
    fpsHistory_.add(value.system.fps);
    streamHistory_.add(value.stream.kbps);
    recordHistory_.add(value.record.kbps);
    }
    if(!cpuObs_)return;
    const QLocale locale=russian()?QLocale(QLocale::Russian):QLocale(QLocale::English);
    const auto percentage=[&](double n){return n<0?QString("—"):locale.toString(n,'f',1)+" %";};
    const auto rate=[&](double n){return locale.toString(n,'f',0)+" kb/s";};
    cpuObs_->setText(percentage(value.system.cpuObs));cpuPc_->setText(percentage(value.system.cpuPc));
    gpu_->setText(percentage(value.system.gpu));
    gpu_->setToolTip(phrase("Загрузка наиболее занятого движка видеокарты всеми программами, включая OBS. Это не отдельная нагрузка OBS или плагинов. «—» — счётчик недоступен.",
        "Busiest GPU engine across all applications, including OBS. Not OBS-only or plugin GPU usage. — means unavailable."));
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
        .arg(value.stream.reconnecting?"#ffdc45":value.stream.active?"#07df91":"#8ea1c0")
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
        .arg(value.record.paused?"#ffdc45":value.record.active?"#ff8484":"#8ea1c0")
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
    if(peersPanel_&&peersPanel_->isVisible())updatePeers();
    updateRecordDetails();
}

void StatsDock::updateRecordDetails(){
    if(!recordDetails_||!metricGrids_[2])return;
    const bool active=last_.record.active;
    recordDetails_->setVisible(!active);
    recordDetails_->setText(recordExpanded_?phrase("Свернуть","Less"):phrase("Подробнее","Details"));
    for(int i=0;i<metricGrids_[2]->count();++i){auto* item=metricGrids_[2]->itemAt(i);if(item&&item->widget())item->widget()->setVisible(i==0||active||recordExpanded_);}
    const bool roomy=layoutMode_==2,wide=layoutMode_==1;
    recordChart_->setDisplayHeight(wide?22:!active&&!recordExpanded_?30:roomy?155:60);
}

void StatsDock::updateHealth(const Snapshot& value,double seconds){
    const bool timed=seconds>0&&seconds<=5;
    if(timed){
        if(value.system.cpuObs>=0){cpuSum_+=value.system.cpuObs*seconds;observedSeconds_+=seconds;}
        if(value.record.active&&!value.record.paused)recordSeconds_+=seconds;
    }
    ++samples_;if(value.system.gpu>=0)gpuPeak_=std::max(gpuPeak_,value.system.gpu);
    const auto delta=[](quint64 current,quint64 previous){return current>=previous?current-previous:quint64(0);};
    quint64 netFrames=0,netLost=0,renderFrames=0,renderLost=0,encodeFrames=0,encodeLost=0;
    if(healthReady_){
        netFrames=delta(value.stream.frames,previousHealth_.stream.frames);netLost=delta(value.stream.dropped,previousHealth_.stream.dropped);
        renderFrames=delta(value.system.renderFrames,previousHealth_.system.renderFrames);renderLost=delta(value.system.renderMissed,previousHealth_.system.renderMissed);
        encodeFrames=delta(value.system.encodeFrames,previousHealth_.system.encodeFrames);encodeLost=delta(value.system.encodeSkipped,previousHealth_.system.encodeSkipped);
        sessionNetworkDropped_+=netLost;sessionRenderMissed_+=renderLost;sessionEncodeSkipped_+=encodeLost;
    }
    if(value.record.active&&value.record.fileSizeAvailable){
        recordedBytes_+=previousHealth_.record.active&&value.record.fileBytes>=previousRecordedBytes_?value.record.fileBytes-previousRecordedBytes_:value.record.fileBytes;
        previousRecordedBytes_=value.record.fileBytes;
    }else previousRecordedBytes_=0;
    QStringList problems;
    const double elapsed=timed?seconds:0;
    if(networkLoss_.update(value.stream.active,netLost,netFrames,elapsed))problems<<phrase("Сетевые потери выше 1% за 10 с","Network frame loss over 1% in 10 sec");
    if(renderLoss_.update(true,renderLost,renderFrames,elapsed))problems<<phrase("OBS не успевает рисовать кадры","OBS cannot render frames in time");
    if(encodeLoss_.update(value.record.active||value.stream.active,encodeLost,encodeFrames,elapsed))problems<<phrase("Кодировщик пропускает кадры","Encoder is skipping frames");
    const bool diskLow=value.record.active&&value.record.diskAvailable&&(value.record.freeBytes<5ull*1024*1024*1024||(value.record.secondsUntilFull>=0&&value.record.secondsUntilFull<900));
    if(diskProblem_.update(diskLow,elapsed,30))problems<<phrase("Мало места для записи","Recording disk is running low");
    if(warningLabel_){warningLabel_->setText(problems.join(" · "));warningLabel_->setVisible(warningsEnabled_&&!problems.isEmpty());}
    previousHealth_=value;healthReady_=true;
}

QString StatsDock::summaryText()const{
    const QLocale locale(russian()?QLocale::Russian:QLocale::English);
    return phrase("Обновлено: ","Updated: ")+QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm:ss")+"\n"+
        phrase("Работа OBS под наблюдением: ","Observed OBS session: ")+duration(sessionClock_.isValid()?sessionClock_.elapsed()/1000:0)+"\n"+
        phrase("Средняя CPU OBS: ","Average OBS CPU: ")+(observedSeconds_>0?locale.toString(cpuSum_/observedSeconds_,'f',1)+" %":"—")+"\n"+
        phrase("Максимум GPU компьютера: ","Peak PC GPU: ")+(gpuPeak_>=0?locale.toString(gpuPeak_,'f',1)+" %":"—")+"\n"+
        phrase("Передано видеоданных: ","Stream video sent: ")+(last_.stream.incomplete?QString::fromUtf8("≥ "):QString())+bytesText(last_.stream.totalBytes,locale)+"\n"+
        phrase("Сетевые потери кадров: ","Network frames lost: ")+locale.toString(sessionNetworkDropped_)+"\n"+
        phrase("Пропуски рендера: ","Render frames missed: ")+locale.toString(sessionRenderMissed_)+"\n"+
        phrase("Пропуски кодирования: ","Encode frames skipped: ")+locale.toString(sessionEncodeSkipped_)+"\n"+
        phrase("Время записи без пауз: ","Recording time without pauses: ")+duration(qint64(recordSeconds_))+"\n"+
        phrase("Учтённый объём записи: ","Observed recording data: ")+bytesText(recordedBytes_,locale);
}

void StatsDock::showSummary(){
    if(summaryPage_){pages_->removeWidget(summaryPage_);summaryPage_->deleteLater();}
    summaryPage_=new QWidget;auto* outer=new QVBoxLayout(summaryPage_);outer->setContentsMargins(8,8,8,8);
    auto* back=new QPushButton(phrase("← Назад","← Back"));QObject::connect(back,&QPushButton::clicked,this,[this]{pages_->setCurrentWidget(settings_);});outer->addWidget(back);
    auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->viewport()->setStyleSheet("background:#080f18;");
    auto* content=new QWidget;content->setObjectName("SummaryContent");auto* rows=new QVBoxLayout(content);
    auto* title=new QLabel(phrase("Сводка текущего сеанса","Current session summary"));title->setObjectName("DockTitle");rows->addWidget(title);
    auto* text=new QLabel(summaryText());text->setObjectName("SessionSummaryText");text->setWordWrap(true);text->setTextFormat(Qt::PlainText);rows->addWidget(text);
    auto* exportButton=new QPushButton(phrase("Сохранить CSV","Save CSV"));QObject::connect(exportButton,&QPushButton::clicked,this,[this]{exportSummary();});rows->addWidget(exportButton);
    QFile previous(QFileInfo(configFile()).absolutePath()+"/last-session-summary.txt");if(previous.open(QIODevice::ReadOnly)){
        auto* old=new QLabel(phrase("Предыдущий запуск OBS\n","Previous OBS session\n")+QString::fromUtf8(previous.read(32768)));old->setTextFormat(Qt::PlainText);old->setWordWrap(true);rows->addWidget(old);
    }
    rows->addStretch();scroll->setWidget(content);outer->addWidget(scroll,1);pages_->addWidget(summaryPage_);pages_->setCurrentWidget(summaryPage_);
}

void StatsDock::exportSummary(){
    const auto path=QFileDialog::getSaveFileName(this,phrase("Сохранить сводку","Save session summary"),"StreamStats-"+QDateTime::currentDateTime().toString("yyyy-MM-dd-HH-mm")+".csv","CSV (*.csv)");if(path.isEmpty())return;
    QByteArray csv="\xEF\xBB\xBF";csv+="metric,value\r\n";
    for(const auto& line:summaryText().split('\n')){const auto separator=line.indexOf(": ");QString key=separator<0?line:line.left(separator),value=separator<0?QString():line.mid(separator+2);key.replace('"',"\"\"");value.replace('"',"\"\"");csv+=("\""+key+"\",\""+value+"\"\r\n").toUtf8();}
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(csv)!=csv.size()||!file.commit())QToolTip::showText(mapToGlobal(QPoint(20,20)),phrase("Не удалось сохранить файл","Could not save file"),this);
}

void StatsDock::updatePeers(){
    if(!peersTotal_)return;
    const QLocale locale=russian()?QLocale(QLocale::Russian):QLocale(QLocale::English);
    const PeerTraffic readings[]={
        readPeerTraffic(L"stream-chat-obs.dll","stream_chat_telemetry_v1","stream_chat_telemetry_v2"),
        readPeerTraffic(L"stream-notify.dll","stream_notify_telemetry_v1","stream_notify_telemetry_v2"),
        PeerTraffic{true,0,0,selfCpuNs.load(std::memory_order_relaxed),sizeof(*this),true,PeerState::Connected}};
    const double elapsedNs=peerClock_.isValid()?double(peerClock_.nsecsElapsed()):0.0;
    peerClock_.restart();
    quint64 sent=0,received=0;
    for(int i=0;i<3;++i){
        const auto& reading=readings[i];
        sent+=reading.sent;received+=reading.received;
        peerTraffic_[i]->setText(i==2?phrase("Фоновых сетевых запросов нет","No background network requests"):
            QString::fromUtf8("↑ ")+peerBytes(reading.sent,locale)+
            QString::fromUtf8("  ↓ ")+peerBytes(reading.received,locale));
        QString cpu="—";
        if(reading.hasResources&&peerCpuReady_[i]&&elapsedNs>0&&elapsedNs<10e9&&reading.cpuNs>=peerCpuNs_[i]){
            const double cores=std::max(1u,std::thread::hardware_concurrency());
            const double percent=100.0*double(reading.cpuNs-peerCpuNs_[i])/elapsedNs/cores;
            cpu=percent<0.01?phrase("<0,01 %","<0.01 %"):locale.toString(percent,'f',2)+" %";
        }
        peerResources_[i]->setText(reading.hasResources?
            "CPU "+cpu+QString::fromUtf8(" · ")+phrase("данные ","data ")+peerBytes(reading.dataBytes,locale):QString());
        peerCpuNs_[i]=reading.cpuNs;peerCpuReady_[i]=reading.hasResources;
        peerStatus_[i]->setText(i==2?phrase("● Здесь","● Here"):reading.connected?phrase("● Связь","● Linked"):
            reading.state==PeerState::Unsupported?phrase("● Нужна новая версия","● Update needed"):
            reading.state==PeerState::NoResponse?phrase("● Нет ответа","● No response"):phrase("● Не загружен","● Not loaded"));
        peerStatus_[i]->setToolTip(reading.state==PeerState::NotLoaded?phrase("Плагин выключен, не установлен или не смог загрузиться в OBS.","Plugin is disabled, not installed or failed to load in OBS."):
            reading.state==PeerState::Unsupported?phrase("Эта версия плагина не поддерживает обмен счётчиками со Stats.","This plugin version does not expose counters to Stats."):phrase("Доступ к локальным счётчикам внутри OBS. Это не проверка подключения к сервисам.","Local counter access inside OBS. This does not check service connections."));
        peerStatus_[i]->setStyleSheet(QString("color:%1;font-size:10px;").arg(reading.connected?"#07df91":"#8ea1c0"));
    }
    peersTotal_->setText(phrase("Всего","Total")+QString::fromUtf8("  ↑ ")+peerBytes(sent,locale)+
        QString::fromUtf8("  ↓ ")+peerBytes(received,locale));
}

} // namespace stats

