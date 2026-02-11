#pragma once

#include <QStatusBar>

namespace opm::gui {

class StatusBarWidget : public QStatusBar {
    Q_OBJECT
public:
    explicit StatusBarWidget(QWidget* parent = nullptr) : QStatusBar(parent) {}
};

} // namespace opm::gui
