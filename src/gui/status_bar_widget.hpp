#pragma once

#include <QWidget>
#include <cstdint>

class QLabel;

namespace opm::gui {

class StatusBarWidget : public QWidget {
    Q_OBJECT

public:
    explicit StatusBarWidget(QWidget* parent = nullptr);
    ~StatusBarWidget() override;

    void setStatus(const QString& message);
    void setDeviceInfo(const QString& device_path);
    void setSpaceInfo(uint64_t used, uint64_t total);
    void clear();

private:
    void setupUI();

    QLabel* status_label_;
    QLabel* device_label_;
    QLabel* space_label_;
};

} // namespace opm::gui
