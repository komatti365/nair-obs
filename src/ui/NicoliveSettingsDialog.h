#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>
#include <QTabWidget>

namespace Nicolive {

class NicoliveSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit NicoliveSettingsDialog(QWidget *parent = nullptr);
    ~NicoliveSettingsDialog() override = default;

private slots:
    void onStartOAuthClicked();
    void onManualLoginClicked();
    void onLogoutClicked();
    void onSaveClicked();
    void updateAuthStatus();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    QTabWidget *m_tabWidget = nullptr;

    // Tab 1: Account
    QLabel *m_statusLabel = nullptr;
    QLineEdit *m_clientIdInput = nullptr;
    QLineEdit *m_clientSecretInput = nullptr;
    QPushButton *m_oauthButton = nullptr;
    QLineEdit *m_tokenInput = nullptr;
    QLineEdit *m_userSessionInput = nullptr;
    QPushButton *m_manualLoginButton = nullptr;
    QPushButton *m_logoutButton = nullptr;

    // Tab 2: Automation
    QCheckBox *m_autoApplyStreamKeyCheck = nullptr;
    QCheckBox *m_autoStartProgramCheck = nullptr;
    QCheckBox *m_autoEndProgramCheck = nullptr;

    // Tab 3: Comments
    QSpinBox *m_maxCommentsSpin = nullptr;
    QSpinBox *m_fontSizeSpin = nullptr;
    QCheckBox *m_showAnonymousCheck = nullptr;

    QPushButton *m_saveButton = nullptr;
    QPushButton *m_closeButton = nullptr;
};

} // namespace Nicolive

