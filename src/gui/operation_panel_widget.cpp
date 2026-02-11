#include "operation_panel_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>

namespace opm::gui {

OperationPanelWidget::OperationPanelWidget(QWidget* parent)
    : QWidget(parent) {
    setupUI();
}

OperationPanelWidget::~OperationPanelWidget() = default;

void OperationPanelWidget::setupUI() {
    auto* main_layout = new QVBoxLayout(this);
    
    // Title
    auto* title = new QLabel("Operations", this);
    title->setStyleSheet("font-weight: bold; font-size: 14px; padding: 5px;");
    main_layout->addWidget(title);
    
    // Pending operations group
    auto* pending_group = new QGroupBox("Pending Operations", this);
    auto* pending_layout = new QVBoxLayout(pending_group);
    
    pending_list_ = new QLabel("No pending operations", this);
    pending_list_->setWordWrap(true);
    pending_layout->addWidget(pending_list_);
    
    // Action buttons
    auto* button_layout = new QHBoxLayout();
    apply_button_ = new QPushButton("Apply", this);
    apply_button_->setEnabled(false);
    apply_button_->setStyleSheet("QPushButton { background-color: #0066CC; color: white; }");
    button_layout->addWidget(apply_button_);
    
    cancel_button_ = new QPushButton("Cancel", this);
    cancel_button_->setEnabled(false);
    button_layout->addWidget(cancel_button_);
    
    pending_layout->addLayout(button_layout);
    main_layout->addWidget(pending_group);
    
    // Recent operations group
    auto* recent_group = new QGroupBox("Recent Operations", this);
    auto* recent_layout = new QVBoxLayout(recent_group);
    
    recent_list_ = new QLabel("No recent operations", this);
    recent_list_->setWordWrap(true);
    recent_layout->addWidget(recent_list_);
    
    main_layout->addWidget(recent_group);
    main_layout->addStretch();
    
    // Connect buttons
    connect(apply_button_, &QPushButton::clicked, this, &OperationPanelWidget::applyClicked);
    connect(cancel_button_, &QPushButton::clicked, this, &OperationPanelWidget::cancelClicked);
}

void OperationPanelWidget::setPendingOperations(const std::vector<QString>& operations) {
    if (operations.empty()) {
        pending_list_->setText("No pending operations");
        apply_button_->setEnabled(false);
        cancel_button_->setEnabled(false);
    } else {
        QString text = "Pending:\n";
        for (const auto& op : operations) {
            text += "• " + op + "\n";
        }
        pending_list_->setText(text);
        apply_button_->setEnabled(true);
        cancel_button_->setEnabled(true);
    }
}

void OperationPanelWidget::setRecentOperations(const std::vector<QString>& operations) {
    if (operations.empty()) {
        recent_list_->setText("No recent operations");
    } else {
        QString text = "Completed:\n";
        for (const auto& op : operations) {
            text += "• " + op + "\n";
        }
        recent_list_->setText(text);
    }
}

void OperationPanelWidget::addOperation(const QString& description) {
    current_operations_.push_back(description);
    setPendingOperations(current_operations_);
}

void OperationPanelWidget::clearOperations() {
    current_operations_.clear();
    setPendingOperations(current_operations_);
}

void OperationPanelWidget::setOperationProgress(int percent) {
    // Update progress display
    if (percent >= 0 && percent <= 100) {
        pending_list_->setText(QString("%1\nProgress: %2%").arg(pending_list_->text()).arg(percent));
    }
}

} // namespace opm::gui
