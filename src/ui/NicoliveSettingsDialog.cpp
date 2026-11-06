#include "NicoliveSettingsDialog.h"
#include "../auth/NicoliveAuthManager.h"
#include "../common/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>

namespace Nicolive {

NicoliveSettingsDialog::NicoliveSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("ニコニコ生放送 設定 (N-Air OBS)"));
    setMinimumWidth(480);

    setupUi();
    loadSettings();
    updateAuthStatus();

    connect(&NicoliveAuthManager::instance(), &NicoliveAuthManager::authStatusChanged,
            this, &NicoliveSettingsDialog::updateAuthStatus);
    connect(&NicoliveAuthManager::instance(), &NicoliveAuthManager::userInfoUpdated,
            this, &NicoliveSettingsDialog::updateAuthStatus);
}

void NicoliveSettingsDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    m_tabWidget = new QTabWidget(this);

    // ================= Tab 1: Account =================
    QWidget *accountTab = new QWidget(m_tabWidget);
    QVBoxLayout *accLayout = new QVBoxLayout(accountTab);

    m_statusLabel = new QLabel(accountTab);
    QFont statusFont = m_statusLabel->font();
    statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);
    accLayout->addWidget(m_statusLabel);

    // OAuth Group
    QGroupBox *oauthGroup = new QGroupBox(tr("OAuth 2.0 認証（推奨）"), accountTab);
    QFormLayout *oauthForm = new QFormLayout(oauthGroup);

    m_clientIdInput = new QLineEdit(oauthGroup);
    m_clientIdInput->setPlaceholderText(tr("OAuth クライアント ID"));
    m_clientSecretInput = new QLineEdit(oauthGroup);
    m_clientSecretInput->setEchoMode(QLineEdit::Password);
    m_clientSecretInput->setPlaceholderText(tr("OAuth クライアント シークレット"));

    m_oauthButton = new QPushButton(tr("ブラウザを開いてログイン"), oauthGroup);
    m_oauthButton->setStyleSheet("font-weight: bold; background-color: #0078d4; color: white; padding: 6px;");
    connect(m_oauthButton, &QPushButton::clicked, this, &NicoliveSettingsDialog::onStartOAuthClicked);

    oauthForm->addRow(tr("Client ID:"), m_clientIdInput);
    oauthForm->addRow(tr("Client Secret:"), m_clientSecretInput);
    oauthForm->addRow("", m_oauthButton);
    accLayout->addWidget(oauthGroup);

    // Manual Token Group
    QGroupBox *manualGroup = new QGroupBox(tr("手動認証（フォールバック）"), accountTab);
    QFormLayout *manualForm = new QFormLayout(manualGroup);

    m_tokenInput = new QLineEdit(manualGroup);
    m_tokenInput->setEchoMode(QLineEdit::Password);
    m_tokenInput->setPlaceholderText(tr("OAuth アクセストークン (Bearer ...)"));

    m_userSessionInput = new QLineEdit(manualGroup);
    m_userSessionInput->setEchoMode(QLineEdit::Password);
    m_userSessionInput->setPlaceholderText(tr("user_session Cookie 値"));

    m_manualLoginButton = new QPushButton(tr("トークン/Cookieでログイン"), manualGroup);
    connect(m_manualLoginButton, &QPushButton::clicked, this, &NicoliveSettingsDialog::onManualLoginClicked);

    manualForm->addRow(tr("Access Token:"), m_tokenInput);
    manualForm->addRow(tr("user_session:"), m_userSessionInput);
    manualForm->addRow("", m_manualLoginButton);
    accLayout->addWidget(manualGroup);

    m_logoutButton = new QPushButton(tr("ログアウト"), accountTab);
    m_logoutButton->setStyleSheet("color: #d83b01;");
    connect(m_logoutButton, &QPushButton::clicked, this, &NicoliveSettingsDialog::onLogoutClicked);
    accLayout->addWidget(m_logoutButton);

    accLayout->addStretch();
    m_tabWidget->addTab(accountTab, tr("アカウント"));

    // ================= Tab 2: Automation =================
    QWidget *autoTab = new QWidget(m_tabWidget);
    QVBoxLayout *autoLayout = new QVBoxLayout(autoTab);

    QGroupBox *streamGroup = new QGroupBox(tr("配信連携設定"), autoTab);
    QVBoxLayout *streamVBox = new QVBoxLayout(streamGroup);

    m_autoApplyStreamKeyCheck = new QCheckBox(tr("番組検出時にOBS配信設定（RTMP URL・ストリームキー）を自動反映する"), streamGroup);
    m_autoStartProgramCheck = new QCheckBox(tr("OBSで配信開始した際に、自動で番組の放送を開始する"), streamGroup);
    m_autoEndProgramCheck = new QCheckBox(tr("OBSで配信終了した際に、自動で番組の放送を終了する"), streamGroup);

    streamVBox->addWidget(m_autoApplyStreamKeyCheck);
    streamVBox->addWidget(m_autoStartProgramCheck);
    streamVBox->addWidget(m_autoEndProgramCheck);
    autoLayout->addWidget(streamGroup);

    autoLayout->addStretch();
    m_tabWidget->addTab(autoTab, tr("自動化"));

    // ================= Tab 3: Comments =================
    QWidget *commentTab = new QWidget(m_tabWidget);
    QVBoxLayout *commentLayout = new QVBoxLayout(commentTab);

    QGroupBox *commentGroup = new QGroupBox(tr("コメント一覧の表示設定"), commentTab);
    QFormLayout *commentForm = new QFormLayout(commentGroup);

    m_maxCommentsSpin = new QSpinBox(commentGroup);
    m_maxCommentsSpin->setRange(50, 2000);
    m_maxCommentsSpin->setSingleStep(50);

    m_fontSizeSpin = new QSpinBox(commentGroup);
    m_fontSizeSpin->setRange(8, 28);

    m_showAnonymousCheck = new QCheckBox(tr("184（匿名）コメントをデフォルトで表示する"), commentGroup);

    commentForm->addRow(tr("最大保持行数:"), m_maxCommentsSpin);
    commentForm->addRow(tr("フォントサイズ:"), m_fontSizeSpin);
    commentForm->addRow("", m_showAnonymousCheck);
    commentLayout->addWidget(commentGroup);

    commentLayout->addStretch();
    m_tabWidget->addTab(commentTab, tr("コメント"));

    mainLayout->addWidget(m_tabWidget);

    // Dialog bottom buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_saveButton = new QPushButton(tr("保存"), this);
    m_saveButton->setDefault(true);
    connect(m_saveButton, &QPushButton::clicked, this, &NicoliveSettingsDialog::onSaveClicked);

    m_closeButton = new QPushButton(tr("閉じる"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(m_saveButton);
    btnLayout->addWidget(m_closeButton);
    mainLayout->addLayout(btnLayout);
}

void NicoliveSettingsDialog::loadSettings()
{
    ConfigManager &cfg = ConfigManager::instance();
    m_clientIdInput->setText(cfg.clientId());
    m_clientSecretInput->setText(cfg.clientSecret());
    m_tokenInput->setText(cfg.accessToken());
    m_userSessionInput->setText(cfg.userSession());

    m_autoApplyStreamKeyCheck->setChecked(cfg.autoApplyStreamKey());
    m_autoStartProgramCheck->setChecked(cfg.autoStartProgram());
    m_autoEndProgramCheck->setChecked(cfg.autoEndProgram());

    m_maxCommentsSpin->setValue(cfg.maxComments());
    m_fontSizeSpin->setValue(cfg.fontSize());
    m_showAnonymousCheck->setChecked(cfg.showAnonymous());
}

void NicoliveSettingsDialog::saveSettings()
{
    ConfigManager &cfg = ConfigManager::instance();
    cfg.setClientId(m_clientIdInput->text().trimmed());
    cfg.setClientSecret(m_clientSecretInput->text().trimmed());

    cfg.setAutoApplyStreamKey(m_autoApplyStreamKeyCheck->isChecked());
    cfg.setAutoStartProgram(m_autoStartProgramCheck->isChecked());
    cfg.setAutoEndProgram(m_autoEndProgramCheck->isChecked());

    cfg.setMaxComments(m_maxCommentsSpin->value());
    cfg.setFontSize(m_fontSizeSpin->value());
    cfg.setShowAnonymous(m_showAnonymousCheck->isChecked());

    cfg.save();
}

void NicoliveSettingsDialog::onStartOAuthClicked()
{
    QString cid = m_clientIdInput->text().trimmed();
    QString sec = m_clientSecretInput->text().trimmed();
    if (cid.isEmpty()) {
        QMessageBox::warning(this, tr("入力エラー"), tr("Client ID を入力してください。"));
        return;
    }
    NicoliveAuthManager::instance().startOAuthLogin(cid, sec);
}

void NicoliveSettingsDialog::onManualLoginClicked()
{
    QString token = m_tokenInput->text().trimmed();
    QString session = m_userSessionInput->text().trimmed();

    if (!token.isEmpty()) {
        NicoliveAuthManager::instance().loginWithToken(token);
    } else if (!session.isEmpty()) {
        NicoliveAuthManager::instance().loginWithUserSession(session);
    } else {
        QMessageBox::warning(this, tr("入力エラー"), tr("Access Token または user_session を入力してください。"));
    }
}

void NicoliveSettingsDialog::onLogoutClicked()
{
    NicoliveAuthManager::instance().logout();
    m_tokenInput->clear();
    m_userSessionInput->clear();
}

void NicoliveSettingsDialog::onSaveClicked()
{
    saveSettings();
    QMessageBox::information(this, tr("設定"), tr("設定を保存しました。"));
}

void NicoliveSettingsDialog::updateAuthStatus()
{
    if (NicoliveAuthManager::instance().isLoggedIn()) {
        const UserInfo &info = NicoliveAuthManager::instance().userInfo();
        m_statusLabel->setText(tr("ステータス: ログイン中 - %1 (ID: %2%3)")
                                   .arg(info.nickname, info.userId, info.isPremium ? " / プレミアム" : ""));
        m_statusLabel->setStyleSheet("color: #107c41;");
        m_logoutButton->setEnabled(true);
    } else {
        m_statusLabel->setText(tr("ステータス: 未ログイン"));
        m_statusLabel->setStyleSheet("color: #d83b01;");
        m_logoutButton->setEnabled(false);
    }
}

} // namespace Nicolive

