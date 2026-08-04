#pragma once

#include <QDialog>
#include <memory>

// Qt forward declarations (global scope - NOT inside opm::gui)
class QLabel;
class QPushButton;
class QDialogButtonBox;
class QTabWidget;
class QCheckBox;
class QComboBox;

namespace opm {
class DiskIO;
}

namespace opm::gui {

// Dialog for application preferences
class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override = default;

signals:
    void settingsChanged();

private slots:
    void onAccepted();
    void onResetToDefaults();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    // UI elements
    QTabWidget* tab_widget_;
    QCheckBox* dark_mode_checkbox_;
    QCheckBox* show_hidden_checkbox_;
    QComboBox* refresh_interval_combo_;
    QPushButton* reset_button_;
    QDialogButtonBox* button_box_;
};

} // namespace opm::gui
