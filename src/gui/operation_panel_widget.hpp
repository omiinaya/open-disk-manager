#pragma once

#include <QWidget>
#include <vector>
#include <QString>

class QLabel;
class QPushButton;

namespace opm::gui {

class OperationPanelWidget : public QWidget {
    Q_OBJECT

public:
    explicit OperationPanelWidget(QWidget* parent = nullptr);
    ~OperationPanelWidget() override;

    void setPendingOperations(const std::vector<QString>& operations);
    void setRecentOperations(const std::vector<QString>& operations);
    void addOperation(const QString& description);
    void clearOperations();
    void setOperationProgress(int percent);

signals:
    void applyClicked();
    void cancelClicked();

private:
    void setupUI();

    QLabel* pending_list_;
    QLabel* recent_list_;
    QPushButton* apply_button_;
    QPushButton* cancel_button_;
    std::vector<QString> current_operations_;
};

} // namespace opm::gui
