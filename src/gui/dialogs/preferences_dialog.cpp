#include "dialogs/preferences_dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QSettings>

namespace opm::gui {

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
    , tab_widget_(nullptr)
    , dark_mode_checkbox_(nullptr)
    , show_hidden_checkbox_(nullptr)
    , refresh_interval_combo_(nullptr)
    , reset_button_(nullptr)
    , button_box_(nullptr) {
    setWindowTitle("Preferences");
    setMinimumWidth(450);
    setupUI();
    loadSettings();
}

void PreferencesDialog::setupUI() {
    auto* main_layout = new QVBoxLayout(this);

    tab_widget_ = new QTabWidget(this);

    // General tab
    auto* general_tab = new QWidget(tab_widget_);
    auto* general_layout = new QVBoxLayout(general_tab);

    auto* appearance_group = new QGroupBox("Appearance", general_tab);
    auto* appearance_layout = new QVBoxLayout(appearance_group);

    dark_mode_checkbox_ = new QCheckBox("Enable Dark Mode", appearance_group);
    show_hidden_checkbox_ = new QCheckBox("Show Hidden Files", appearance_group);

    appearance_layout->addWidget(dark_mode_checkbox_);
    appearance_layout->addWidget(show_hidden_checkbox_);
    general_layout->addWidget(appearance_group);

    auto* refresh_group = new QGroupBox("Refresh", general_tab);
    auto* refresh_layout = new QHBoxLayout(refresh_group);
    refresh_layout->addWidget(new QLabel("Refresh Interval:", refresh_group));
    refresh_interval_combo_ = new QComboBox(refresh_group);
    refresh_interval_combo_->addItem("2 seconds", 2000);
    refresh_interval_combo_->addItem("5 seconds", 5000);
    refresh_interval_combo_->addItem("10 seconds", 10000);
    refresh_interval_combo_->addItem("30 seconds", 30000);
    refresh_layout->addWidget(refresh_interval_combo_);
    refresh_layout->addStretch();
    general_layout->addWidget(refresh_group);

    general_layout->addStretch();
    tab_widget_->addTab(general_tab, "General");

    // Buttons
    button_box_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
    reset_button_ = button_box_->button(QDialogButtonBox::Reset);

    main_layout->addWidget(tab_widget_);
    main_layout->addWidget(button_box_);

    // Connections
    connect(button_box_, &QDialogButtonBox::accepted, this, &PreferencesDialog::onAccepted);
    connect(button_box_, &QDialogButtonBox::rejected, this, &PreferencesDialog::reject);
    connect(button_box_, &QDialogButtonBox::clicked, [this](QAbstractButton* button) {
        if (button_box_->buttonRole(button) == QDialogButtonBox::ResetRole) {
            onResetToDefaults();
        }
    });
}

void PreferencesDialog::loadSettings() {
    QSettings settings;
    dark_mode_checkbox_->setChecked(settings.value("darkMode", false).toBool());
    show_hidden_checkbox_->setChecked(settings.value("showHidden", false).toBool());
    int interval = settings.value("refreshInterval", 5000).toInt();
    refresh_interval_combo_->setCurrentIndex(refresh_interval_combo_->findData(interval));
}

void PreferencesDialog::saveSettings() {
    QSettings settings;
    settings.setValue("darkMode", dark_mode_checkbox_->isChecked());
    settings.setValue("showHidden", show_hidden_checkbox_->isChecked());
    settings.setValue("refreshInterval", refresh_interval_combo_->currentData().toInt());
}

void PreferencesDialog::onAccepted() {
    saveSettings();
    emit settingsChanged();
    accept();
}

void PreferencesDialog::onResetToDefaults() {
    dark_mode_checkbox_->setChecked(false);
    show_hidden_checkbox_->setChecked(false);
    refresh_interval_combo_->setCurrentIndex(1); // 5 seconds
}

} // namespace opm::gui
