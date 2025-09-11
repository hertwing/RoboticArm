#ifndef CHARTSPANEL_H
#define CHARTSPANEL_H

#ifndef Q_MOC_RUN
#include "odin/diagnostic_handler/DataTypes.h"
#include <array>
#endif

#include <QObject>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QAreaSeries>
#include <QPieSeries>
#include <QPieSlice>
#include <QValueAxis>
#include <QVector>
#include <QPointF>
#include <QPen>
#include <QColor>

using namespace odin::diagnostic_handler;

QT_BEGIN_NAMESPACE
class QChartView;
QT_END_NAMESPACE

class ChartsPanel : public QObject {
    Q_OBJECT
public:
    explicit ChartsPanel(QChartView* cpuView,
                         QChartView* ramView,
                         QChartView* tempView,
                         QChartView* latView,
                         QObject* parent=nullptr);

    void setSnapshot(const DiagnosticData& d); 
    void push(const DiagnosticData& d);

private:
    static constexpr int CHART_BINS = 10;
    static constexpr double LATENCY_MAX = 20.0;

    // hist
    std::array<double, CHART_BINS> m_cpu_hist{};
    std::array<double, CHART_BINS> m_ram_hist{};
    std::array<double, CHART_BINS> m_temp_hist{};

    // charts & series
    QChart* m_chart_cpu   = nullptr;
    QChart* m_chart_ram   = nullptr;
    QChart* m_chart_temp  = nullptr;
    QChart* m_chart_lat   = nullptr;

    QLineSeries* m_cpu      = nullptr;
    QLineSeries* m_cpu_zero = nullptr;
    QAreaSeries* m_cpu_area = nullptr;

    QLineSeries* m_ram      = nullptr;
    QLineSeries* m_ram_zero = nullptr;
    QAreaSeries* m_ram_area = nullptr;

    QLineSeries* m_temp      = nullptr;
    QLineSeries* m_temp_zero = nullptr;
    QAreaSeries* m_temp_area = nullptr;

    QPieSeries* m_lat_pie = nullptr;
    QPieSlice*  m_lat_ok  = nullptr;
    QPieSlice*  m_lat_bad = nullptr;

    static inline void shiftPush(std::array<double, CHART_BINS>& v, double x)
    {
        for (int i=0;i<CHART_BINS-1;++i) v[(size_t)i] = v[(size_t)i+1];
        v.back() = x;
    }

    static QVector<QPointF> makeZeroPts()
    {
        QVector<QPointF> pts; pts.reserve(CHART_BINS);
        for (int i=0;i<CHART_BINS;++i) pts.append(QPointF(i, 0.0));
        return pts;
    }

    static QVector<QPointF> makePts(const std::array<double, CHART_BINS>& v)
    {
        QVector<QPointF> pts; pts.reserve(CHART_BINS);
        for (int i=0;i<CHART_BINS;++i) pts.append(QPointF(i, v[(size_t)i]));
        return pts;
    }

    static void addAxesAndAttach(QChart* chart,
                                 QAbstractSeries* s1,
                                 QAbstractSeries* s2,
                                 QValueAxis*& axX,
                                 QValueAxis*& axY);
    void initCpu(QChartView* view);
    void initRam(QChartView* view);
    void initTemp(QChartView* view);
    void initLatency(QChartView* view);
    void forceY();
};

#endif // CHARTSPANEL_H