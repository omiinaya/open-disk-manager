#pragma once

#include <QWidget>

namespace opm::gui {

class OperationPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit OperationPanelWidget(QWidget* parent = nullptr) : QWidget(parent) {}
};

} // namespace opm::gui
