#include "status_bar_widget.hpp"
#include <QHBoxLayout>
#include <QLabel>

namespace opm::gui {

StatusBarWidget::StatusBarWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

StatusBarWidget::~StatusBarWidget() = default;

void StatusBarWidget::setupUI() {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 2, 5, 2);
    
    status_label_ = new QLabel("Ready", this);
    status_label_->setMinimumWidth(200);
    layout->addWidget(status_label_);
    
    layout->addStretch();
    
    device_label_ = new QLabel("No device", this);
    layout->addWidget(device_label_);
    
    layout->addSpacing(20);
    
    space_label_ = new QLabel("", this);
    layout->addWidget(space_label_);
}

void StatusBarWidget::setStatus(const QString& message) {
    status_label_->setText(message);
}

void StatusBarWidget::setDeviceInfo(const QString& device_path) {
    device_label_->setText(device_path.isEmpty() ? "No device" : device_path);
}

void StatusBarWidget::setSpaceInfo(uint64_t used, uint64_t total) {
    double used_gb = used / (1024.0 * 1024.0 * 1024.0);
    double total_gb = total / (1024.0 * 1024.0 * 1024.0);
    
    QString text = QString("%1 / %2 GB used")
        .arg(used_gb, 0, 'f', 1)
        .arg(total_gb, 0, 'f', 1);
    
    space_label_->setText(text);
}

void StatusBarWidget::clear() {
    status_label_->setText("Ready");
    device_label_->setText("No device");
    space_label_->setText("");
}

} // namespace opm::gui
