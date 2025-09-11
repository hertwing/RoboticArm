#include "ChartsPanel.h"

static QPen pen2(const QColor& c){ QPen p(c); p.setWidth(2); return p; }

void ChartsPanel::addAxesAndAttach(QChart* chart,
                                   QAbstractSeries* s1,
                                   QAbstractSeries* s2,
                                   QValueAxis*& axX,
                                   QValueAxis*& axY)
{
    for (QAbstractAxis* ax : chart->axes()) { chart->removeAxis(ax); delete ax; }
    axX = new QValueAxis(); axY = new QValueAxis();
    axX->setRange(0, CHART_BINS-1);
    axX->setLabelFormat("%.0f");
    axY->setRange(0, 100);
    axY->setTickCount(6);
    axY->setLabelFormat("%.0f");

    chart->addAxis(axX, Qt::AlignBottom);
    chart->addAxis(axY, Qt::AlignLeft);

    s1->attachAxis(axX); s1->attachAxis(axY);
    if (s2) { s2->attachAxis(axX); s2->attachAxis(axY); }
    chart->legend()->setVisible(false);
}

ChartsPanel::ChartsPanel(QChartView* cpuView,
                         QChartView* ramView,
                         QChartView* tempView,
                         QChartView* latView,
                         QObject* parent) :
    QObject(parent)
{
    initCpu(cpuView);
    initRam(ramView);
    initTemp(tempView);
    initLatency(latView);
}

void ChartsPanel::initCpu(QChartView* view)
{
    m_chart_cpu = new QChart(); m_chart_cpu->setTheme(QChart::ChartThemeLight);
    m_cpu = new QLineSeries(); m_cpu_zero = new QLineSeries(); m_cpu_area = new QAreaSeries(m_cpu, m_cpu_zero);
    m_cpu->setPen(pen2(QColor(239, 41, 41)));
    m_cpu_zero->setPen(Qt::NoPen); m_cpu_zero->setVisible(false);
    m_cpu_area->setPen(pen2(QColor(239,41,41)));
    m_cpu_area->setBrush(QColor(239, 41, 41, 80));

    auto zero = makeZeroPts();
    m_cpu->replace(zero); m_cpu_zero->replace(zero);

    m_chart_cpu->addSeries(m_cpu);
    m_chart_cpu->addSeries(m_cpu_area);
    m_chart_cpu->addSeries(m_cpu_zero);

    QValueAxis *axX=nullptr,*axY=nullptr;
    addAxesAndAttach(m_chart_cpu, m_cpu, m_cpu_area, axX, axY);
    view->setChart(m_chart_cpu);
}

void ChartsPanel::initRam(QChartView* view)
{
    m_chart_ram = new QChart(); m_chart_ram->setTheme(QChart::ChartThemeLight);
    m_ram = new QLineSeries(); m_ram_zero = new QLineSeries(); m_ram_area = new QAreaSeries(m_ram, m_ram_zero);
    m_ram->setPen(pen2(QColor(114,159,207)));
    m_ram_zero->setPen(Qt::NoPen); m_ram_zero->setVisible(false);
    m_ram_area->setPen(pen2(QColor(114,159,207)));
    m_ram_area->setBrush(QColor(114,159,207,80));

    auto zero = makeZeroPts();
    m_ram->replace(zero); m_ram_zero->replace(zero);

    m_chart_ram->addSeries(m_ram);
    m_chart_ram->addSeries(m_ram_area);
    m_chart_ram->addSeries(m_ram_zero);

    QValueAxis *axX=nullptr,*axY=nullptr;
    addAxesAndAttach(m_chart_ram, m_ram, m_ram_area, axX, axY);
    view->setChart(m_chart_ram);
}

void ChartsPanel::initTemp(QChartView* view)
{
    m_chart_temp = new QChart(); m_chart_temp->setTheme(QChart::ChartThemeLight);
    m_temp = new QLineSeries(); m_temp_zero = new QLineSeries(); m_temp_area = new QAreaSeries(m_temp, m_temp_zero);
    m_temp->setPen(pen2(QColor(252,175,62)));
    m_temp_zero->setPen(Qt::NoPen); m_temp_zero->setVisible(false);
    m_temp_area->setPen(pen2(QColor(252,175,62)));
    m_temp_area->setBrush(QColor(252,175,62,80));

    auto zero = makeZeroPts();
    m_temp->replace(zero); m_temp_zero->replace(zero);

    m_chart_temp->addSeries(m_temp);
    m_chart_temp->addSeries(m_temp_area);
    m_chart_temp->addSeries(m_temp_zero);

    QValueAxis *axX=nullptr,*axY=nullptr;
    addAxesAndAttach(m_chart_temp, m_temp, m_temp_area, axX, axY);
    view->setChart(m_chart_temp);
}

void ChartsPanel::initLatency(QChartView* view)
{
    m_chart_lat = new QChart(); m_chart_lat->setTheme(QChart::ChartThemeLight);
    m_lat_pie = new QPieSeries(); m_lat_pie->setHoleSize(0.5);
    m_lat_ok  = new QPieSlice(); m_lat_bad = new QPieSlice();
    m_lat_pie->append(m_lat_ok); m_lat_pie->append(m_lat_bad);
    m_chart_lat->addSeries(m_lat_pie);
    m_chart_lat->legend()->setVisible(false);
    view->setChart(m_chart_lat);

    m_lat_ok->setValue(0.0);
    m_lat_bad->setValue(100.0);
    m_lat_ok->setBrush(QColor(138,226,52));
}

void ChartsPanel::forceY()
{
    auto force = [&](QChart* c){
        for (QAbstractAxis* a : c->axes(Qt::Vertical)) {
            if (auto v = qobject_cast<QValueAxis*>(a)) { v->setRange(0,100); v->setTickCount(6); v->setLabelFormat("%.0f"); }
        }
        for (QAbstractAxis* a : c->axes(Qt::Horizontal)) {
            if (auto v = qobject_cast<QValueAxis*>(a)) { v->setRange(0,CHART_BINS-1); }
        }
    };
    force(m_chart_cpu);
    force(m_chart_ram);
    force(m_chart_temp);
}

void ChartsPanel::setSnapshot(const DiagnosticData& d)
{
    m_cpu_hist.fill(d.cpu_usage);
    m_ram_hist.fill(d.ram_usage);
    m_temp_hist.fill(d.cpu_temp);

    m_cpu->replace(makePts(m_cpu_hist));
    m_cpu_zero->replace(makeZeroPts());

    m_ram->replace(makePts(m_ram_hist));
    m_ram_zero->replace(makeZeroPts());

    m_temp->replace(makePts(m_temp_hist));
    m_temp_zero->replace(makeZeroPts());

    forceY();

    const double lat = d.latency;
    double pct = lat * 100.0 / LATENCY_MAX; if (pct > 100.0) pct = 100.0;
    m_lat_ok->setValue(pct);
    m_lat_bad->setValue(100.0 - pct);
    m_lat_ok->setBrush( (pct < 50.0) ? QColor(138,226,52) : QColor(239,41,41) );
    m_chart_lat->setTitle(QString("Latency: %1 ms").arg(lat, 0, 'f', 2));
}

void ChartsPanel::push(const DiagnosticData& d)
{
    shiftPush(m_cpu_hist,  d.cpu_usage);
    shiftPush(m_ram_hist,  d.ram_usage);
    shiftPush(m_temp_hist, d.cpu_temp);

    m_cpu->replace(makePts(m_cpu_hist));
    m_ram->replace(makePts(m_ram_hist));
    m_temp->replace(makePts(m_temp_hist));
    forceY();

    const double lat = d.latency;
    double pct = lat * 100.0 / LATENCY_MAX; if (pct > 100.0) pct = 100.0;
    m_lat_ok->setValue(pct);
    m_lat_bad->setValue(100.0 - pct);
    m_lat_ok->setBrush( (pct < 50.0) ? QColor(138,226,52) : QColor(239,41,41) );
    m_chart_lat->setTitle(QString("Latency: %1 ms").arg(lat, 0, 'f', 2));
}