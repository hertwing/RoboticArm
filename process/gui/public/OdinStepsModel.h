#ifndef ODINSTEPSMODEL_H
#define ODINSTEPSMODEL_H

#ifndef Q_MOC_RUN
#include <vector>
#include "InetCommData.h" // OdinServoStep
#endif

#include <QAbstractTableModel>

class OdinStepsModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit OdinStepsModel(QObject* parent=nullptr);

    int rowCount(const QModelIndex & parant = QModelIndex()) const override;
    int columnCount(const QModelIndex & parant = QModelIndex()) const override;

    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    template<typename T>
    static QVariant toVar(T v);

    template<typename T>
    static bool fromVar(const QVariant& qv, T& out);

    QVariant data(const QModelIndex& idx, int role) const override;

    bool setData(const QModelIndex& idx, const QVariant& val, int role) override;

    Qt::ItemFlags flags(const QModelIndex& idx) const override;

    void clear();
    void addStep(const OdinServoStep& s);
    void removeRowAt(int r);
    const std::vector<OdinServoStep>& steps() const;
    std::vector<OdinServoStep>& steps();

    enum Column {
        ColServo = 0,
        ColPos,
        ColSpeed,
        ColDelay,
        ColCount
    };
private:
    std::vector<OdinServoStep> m_steps;
};

#endif // ODINSTEPSMODEL_H