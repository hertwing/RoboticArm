#include "OdinStepsModel.h"

#include "InetCommData.h"

#include <QAbstractTableModel>
#include <vector>

OdinStepsModel::OdinStepsModel(QObject * parent) :
    QAbstractTableModel(parent)
{
}

int OdinStepsModel::rowCount(const QModelIndex & parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_steps.size());
}

int OdinStepsModel::columnCount(const QModelIndex & parent) const
{
    Q_UNUSED(parent);
    return Column::ColCount;
}

QVariant OdinStepsModel::headerData(int section, Qt::Orientation o, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (o == Qt::Horizontal) {
        switch (section) {
            case ColServo: return "Servo";
            case ColPos:   return "Pos";
            case ColSpeed: return "Speed";
            case ColDelay: return "Delay";
        }
    } else {
        return section + 1;
    }
    return {};
}

template<typename T>
QVariant OdinStepsModel::toVar(T v) {
    if constexpr (std::is_unsigned_v<T>)
        return QVariant::fromValue<qulonglong>(static_cast<qulonglong>(v));
    else
        return QVariant::fromValue<qlonglong>(static_cast<qlonglong>(v));
}

template<typename T>
bool OdinStepsModel::fromVar(const QVariant& qv, T& out) {
    bool ok = false;
    if constexpr (std::is_unsigned_v<T>) {
        qulonglong u = qv.toULongLong(&ok);
        if (!ok) return false;
        out = static_cast<T>(u);
    } else {
        qlonglong s = qv.toLongLong(&ok);
        if (!ok) return false;
        out = static_cast<T>(s);
    }
    return true;
}

QVariant OdinStepsModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid()) return {};
    const auto& s = m_steps[static_cast<size_t>(idx.row())];

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (idx.column()) {
            case ColServo: return toVar(s.servo_num);
            case ColPos:   return toVar(s.position);
            case ColSpeed: return toVar(s.speed);
            case ColDelay: return toVar(s.delay);
        }
    }
    if (role == Qt::TextAlignmentRole)
        return QVariant::fromValue<int>(Qt::AlignRight | Qt::AlignVCenter);
    return {};
}

bool OdinStepsModel::setData(const QModelIndex& idx, const QVariant& val, int role) {
    if (!idx.isValid() || role != Qt::EditRole) return false;
    auto& s = m_steps[static_cast<size_t>(idx.row())];

    switch (idx.column()) {
        case ColServo: return fromVar(val, s.servo_num) && (emit dataChanged(idx, idx), true);
        case ColPos:   return fromVar(val, s.position)  && (emit dataChanged(idx, idx), true);
        case ColSpeed: return fromVar(val, s.speed)     && (emit dataChanged(idx, idx), true);
        case ColDelay: return fromVar(val, s.delay)     && (emit dataChanged(idx, idx), true);
        default: return false;
    }
}

Qt::ItemFlags OdinStepsModel::flags(const QModelIndex& idx) const
{
    if (!idx.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

void OdinStepsModel::clear()
{
    beginResetModel();
    m_steps.clear();
    endResetModel();
}

void OdinStepsModel::addStep(const OdinServoStep& s)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_steps.push_back(s);
    endInsertRows();
}

void OdinStepsModel::removeRowAt(int r)
{
    if (r < 0 || r >= rowCount()) return;
    beginRemoveRows(QModelIndex(), r, r);
    m_steps.erase(m_steps.begin() + r);
    endRemoveRows();
}

const std::vector<OdinServoStep>& OdinStepsModel::steps() const { return m_steps; }
std::vector<OdinServoStep>& OdinStepsModel::steps() { return m_steps; }