#pragma once

#include <QWidget>

namespace opm::gui {

class PartitionViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit PartitionViewWidget(QWidget* parent = nullptr) : QWidget(parent) {}
};

} // namespace opm::gui
