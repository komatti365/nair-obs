#include "NicoliveControlDock.h"
#include "../auth/NicoliveAuthManager.h"
#include "../obs/ObsStreamHelper.h"
#include "../common/ConfigManager.h"
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QGroupBox>

namespace Nicolive {

NicoliveControlDock::NicoliveControlDock(NicoliveApiClient *apiClient,
                                         NicoliveCommentReceiver *commentReceiver,
                                         QWidget *parent)
    : QDockWidget(tr("ニコニコ生放送 - 配信管理"), parent)
    , m_apiClient(apiClient)
    , m_commentReceiver(commentReceiver)
    , m_imageNet(new QNetworkAccessManager(this))
    , m_durationTimer(new QTimer(this))
    , m_autoRefreshTimer(new QTimer(this))
{
    setObjectName("NicoliveControlDock");
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    setupUi();

    connect(&NicoliveAuthManager::instance(), &NicoliveAuthManager::authStatusChanged,
            this, &NicoliveControlDock::onAuthStatusChanged);
    connect(&NicoliveAuthManager::instance(), &NicoliveAuthManager::userInfoUpdated,
            this, &NicoliveControlDock::onUserInfoUpdated);

    connect(m_durationTimer, &QTimer::timeout, this, &NicoliveControlDock::updateDurationTimer);
    m_durationTimer->setInterval(1000);

    // Auto refresh program info every 60s
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &NicoliveControlDock::refreshProgramInfo);
    m_autoRefreshTimer->setInterval(60000);
    m_autoRefreshTimer->start();

    // Hook comment receiver statistics
    if (m_commentReceiver) {
        connect(m_commentReceiver, &NicoliveCommentReceiver::statisticsUpdated,
                [this](qint64 viewers, qint64 comments) {
            m_currentProgram.viewCount = viewers;
            m_currentProgram.commentCount = comments;
            m_statsLabel->setText(tr("来場者: %1 / コメント: %2").arg(viewers).arg(comments));
        });
    }

    onAuthStatusChanged(NicoliveAuthManager::instance().isLoggedIn());
    if (NicoliveAuthManager::instance().isLoggedIn()) {
        onUserInfoUpdated(NicoliveAuthManager::instance().userInfo());
        refreshProgramInfo();
    }
}

void NicoliveControlDock::setupUi()
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 1. User Info Header
    m_userPanel = new QWidget(container);
    QHBoxLayout *userLayout = new QHBoxLayout(m_userPanel);
    userLayout->setContentsMargins(4, 4, 4, 4);

    m_userIconLabel = new QLabel(m_userPanel);
    m_userIconLabel->setFixedSize(36, 36);
    m_userIconLabel->setStyleSheet("border-radius: 18px; background-color: #555;");

    QVBoxLayout *userTextLayout = new QVBoxLayout();
    userTextLayout->setSpacing(1);

    QHBoxLayout *userNameLayout = new QHBoxLayout();
    m_userNameLabel = new QLabel(tr("未ログイン"), m_userPanel);
    QFont nameFont = m_userNameLabel->font();
    nameFont.setBold(true);
    m_userNameLabel->setFont(nameFont);

    m_premiumBadge = new QLabel(tr("PREMIUM"), m_userPanel);
    m_premiumBadge->setStyleSheet("background-color: #e6a100; color: #fff; font-size: 9px; font-weight: bold; border-radius: 3px; padding: 1px 4px;");
    m_premiumBadge->hide();

    userNameLayout->addWidget(m_userNameLabel);
    userNameLayout->addWidget(m_premiumBadge);
    userNameLayout->addStretch();

    m_userIdLabel = new QLabel("", m_userPanel);
    m_userIdLabel->setStyleSheet("color: #888; font-size: 10px;");

    userTextLayout->addLayout(userNameLayout);
    userTextLayout->addWidget(m_userIdLabel);

    m_loginButton = new QPushButton(tr("ログイン"), m_userPanel);
    connect(m_loginButton, &QPushButton::clicked, [this]() {
        if (NicoliveAuthManager::instance().isLoggedIn()) {
            NicoliveAuthManager::instance().logout();
        } else {
            emit openSettingsRequested();
        }
    });

    userLayout->addWidget(m_userIconLabel);
    userLayout->addLayout(userTextLayout);
    userLayout->addWidget(m_loginButton);

    mainLayout->addWidget(m_userPanel);

    // 2. Program Info Box
    QGroupBox *programBox = new QGroupBox(tr("番組情報"), container);
    QVBoxLayout *progLayout = new QVBoxLayout(programBox);
    progLayout->setSpacing(4);

    QHBoxLayout *progHeaderLayout = new QHBoxLayout();
    m_programStatusBadge = new QLabel(tr("放送前"), programBox);
    m_programStatusBadge->setStyleSheet("background-color: #555; color: white; border-radius: 3px; padding: 2px 6px; font-size: 11px;");

    m_programIdLabel = new QLabel("-", programBox);
    m_programIdLabel->setStyleSheet("color: #888;");

    progHeaderLayout->addWidget(m_programStatusBadge);
    progHeaderLayout->addWidget(m_programIdLabel);
    progHeaderLayout->addStretch();

    m_programTitleLabel = new QLabel(tr("番組が検出されていません"), programBox);
    m_programTitleLabel->setWordWrap(true);
    QFont titleFont = m_programTitleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleFont.setBold(true);
    m_programTitleLabel->setFont(titleFont);

    QHBoxLayout *statsRowLayout = new QHBoxLayout();
    m_durationLabel = new QLabel(tr("時間: --:--:--"), programBox);
    m_statsLabel = new QLabel(tr("来場者: - / コメント: -"), programBox);
    m_statsLabel->setStyleSheet("color: #888;");

    statsRowLayout->addWidget(m_durationLabel);
    statsRowLayout->addStretch();
    statsRowLayout->addWidget(m_statsLabel);

    progLayout->addLayout(progHeaderLayout);
    progLayout->addWidget(m_programTitleLabel);
    progLayout->addLayout(statsRowLayout);

    mainLayout->addWidget(programBox);

    // 3. Action Buttons
    m_applyStreamButton = new QPushButton(tr("OBS配信設定に反映（URL・キー）"), container);
    m_applyStreamButton->setStyleSheet("font-weight: bold; background-color: #0078d4; color: white; padding: 6px;");
    connect(m_applyStreamButton, &QPushButton::clicked, this, &NicoliveControlDock::applyStreamToObs);
    mainLayout->addWidget(m_applyStreamButton);

    QHBoxLayout *btnRow1 = new QHBoxLayout();
    m_startButton = new QPushButton(tr("番組開始"), container);
    m_startButton->setStyleSheet("background-color: #107c41; color: white; font-weight: bold; padding: 5px;");
    connect(m_startButton, &QPushButton::clicked, this, &NicoliveControlDock::onStartProgramClicked);

    m_endButton = new QPushButton(tr("番組終了"), container);
    m_endButton->setStyleSheet("background-color: #d83b01; color: white; font-weight: bold; padding: 5px;");
    connect(m_endButton, &QPushButton::clicked, this, &NicoliveControlDock::onEndProgramClicked);

    m_extendButton = new QPushButton(tr("+30分延長"), container);
    connect(m_extendButton, &QPushButton::clicked, this, &NicoliveControlDock::onExtendProgramClicked);

    btnRow1->addWidget(m_startButton);
    btnRow1->addWidget(m_endButton);
    btnRow1->addWidget(m_extendButton);
    mainLayout->addLayout(btnRow1);

    QHBoxLayout *btnRow2 = new QHBoxLayout();
    m_browserButton = new QPushButton(tr("ブラウザで開く"), container);
    connect(m_browserButton, &QPushButton::clicked, this, &NicoliveControlDock::onOpenInBrowserClicked);

    m_refreshButton = new QPushButton(tr("情報更新"), container);
    connect(m_refreshButton, &QPushButton::clicked, this, &NicoliveControlDock::refreshProgramInfo);

    m_settingsButton = new QPushButton(tr("設定..."), container);
    connect(m_settingsButton, &QPushButton::clicked, this, &NicoliveControlDock::openSettingsRequested);

    btnRow2->addWidget(m_browserButton);
    btnRow2->addWidget(m_refreshButton);
    btnRow2->addWidget(m_settingsButton);
    mainLayout->addLayout(btnRow2);

    mainLayout->addStretch();
    setWidget(container);

    updateUiState();
}

void NicoliveControlDock::onAuthStatusChanged(bool isLoggedIn)
{
    m_loginButton->setText(isLoggedIn ? tr("ログアウト") : tr("ログイン"));
    updateUiState();
    if (isLoggedIn) {
        refreshProgramInfo();
    } else {
        m_userNameLabel->setText(tr("未ログイン"));
        m_userIdLabel->setText("");
        m_userIconLabel->clear();
        m_premiumBadge->hide();
    }
}

void NicoliveControlDock::onUserInfoUpdated(const UserInfo &info)
{
    m_userNameLabel->setText(info.nickname.isEmpty() ? info.userId : info.nickname);
    m_userIdLabel->setText(QString("ID: %1").arg(info.userId));
    m_premiumBadge->setVisible(info.isPremium);

    if (!info.iconUrl.isEmpty()) {
        loadUserIcon(info.iconUrl);
    }
}

void NicoliveControlDock::loadUserIcon(const QString &url)
{
    QNetworkRequest req{QUrl(url)};
    QNetworkReply *reply = m_imageNet->get(req);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QPixmap pixmap;
            if (pixmap.loadFromData(reply->readAll())) {
                QPixmap rounded(36, 36);
                rounded.fill(Qt::transparent);
                QPainter painter(&rounded);
                painter.setRenderHint(QPainter::Antialiasing);
                QPainterPath path;
                path.addEllipse(0, 0, 36, 36);
                painter.setClipPath(path);
                painter.drawPixmap(0, 0, pixmap.scaled(36, 36, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                m_userIconLabel->setPixmap(rounded);
            }
        }
    });
}

void NicoliveControlDock::refreshProgramInfo()
{
    QString userId = NicoliveAuthManager::instance().userInfo().userId;
    if (userId.isEmpty()) return;

    m_apiClient->fetchLatestProgram(userId, [this, userId](bool success, const ProgramInfo &info, const QString &err) {
        if (!success) {
            qWarning() << "[N-Air OBS] Failed to fetch latest program:" << err;
            return;
        }

        m_currentProgram = info;
        m_programTitleLabel->setText(info.title.isEmpty() ? tr("（タイトル未設定）") : info.title);
        m_programIdLabel->setText(info.nicoliveProgramId);

        if (info.status == "on_air") {
            m_programStatusBadge->setText(tr("放送中"));
            m_programStatusBadge->setStyleSheet("background-color: #e81123; color: white; border-radius: 3px; padding: 2px 6px; font-weight: bold;");
            if (!m_durationTimer->isActive()) m_durationTimer->start();
        } else if (info.status == "ended") {
            m_programStatusBadge->setText(tr("終了"));
            m_programStatusBadge->setStyleSheet("background-color: #555; color: white; border-radius: 3px; padding: 2px 6px;");
            m_durationTimer->stop();
        } else {
            m_programStatusBadge->setText(tr("放送前"));
            m_programStatusBadge->setStyleSheet("background-color: #0078d4; color: white; border-radius: 3px; padding: 2px 6px;");
            m_durationTimer->stop();
        }

        // Fetch stream url & stream key
        m_apiClient->fetchBroadcastStream(userId, info.nicoliveProgramId, [this](bool ok, const QString &url, const QString &key, const QString &) {
            if (ok) {
                m_currentProgram.streamUrl = url;
                m_currentProgram.streamKey = key;

                if (ConfigManager::instance().autoApplyStreamKey()) {
                    ObsStreamHelper::applyStreamSettings(url, key);
                }
            }
        });

        // Fetch WebSocket / Rooms for comments
        m_apiClient->fetchRooms(userId, info.nicoliveProgramId, [this, userId, info](bool ok, const QString &viewUrl, const QString &) {
            if (ok) {
                m_currentProgram.viewUrl = viewUrl;
            }
            m_apiClient->fetchWsEndpoint(userId, info.nicoliveProgramId, [this, userId, info, viewUrl](bool wsOk, const QString &wsUrl, const QString &) {
                if (wsOk) {
                    m_currentProgram.wsUrl = wsUrl;
                }
                if (m_commentReceiver && (wsOk || !viewUrl.isEmpty())) {
                    m_commentReceiver->start(userId, info.nicoliveProgramId, wsUrl, viewUrl);
                }
            });
        });

        updateUiState();
        emit programChanged(m_currentProgram);
    });
}

void NicoliveControlDock::applyStreamToObs()
{
    if (m_currentProgram.streamUrl.isEmpty() || m_currentProgram.streamKey.isEmpty()) {
        QMessageBox::warning(this, tr("配信設定"), tr("ストリーム情報が取得できていません。情報更新を試してください。"));
        return;
    }

    if (ObsStreamHelper::applyStreamSettings(m_currentProgram.streamUrl, m_currentProgram.streamKey)) {
        QMessageBox::information(this, tr("配信設定"), tr("OBSの配信設定（カスタムRTMP）にニコ生のURLとストリームキーを反映しました！"));
    } else {
        QMessageBox::critical(this, tr("エラー"), tr("OBS配信設定の更新に失敗しました。"));
    }
}

void NicoliveControlDock::onStartProgramClicked()
{
    if (m_currentProgram.nicoliveProgramId.isEmpty()) return;

    if (QMessageBox::question(this, tr("番組開始"), tr("番組「%1」の放送を開始しますか？").arg(m_currentProgram.title)) != QMessageBox::Yes) {
        return;
    }

    QString userId = NicoliveAuthManager::instance().userInfo().userId;
    m_apiClient->startProgram(userId, m_currentProgram.nicoliveProgramId, [this](bool ok, const QString &err) {
        if (ok) {
            QMessageBox::information(this, tr("成功"), tr("番組の放送を開始しました！"));
            refreshProgramInfo();
        } else {
            QMessageBox::warning(this, tr("失敗"), tr("番組開始に失敗しました: %1").arg(err));
        }
    });
}

void NicoliveControlDock::onEndProgramClicked()
{
    if (m_currentProgram.nicoliveProgramId.isEmpty()) return;

    if (QMessageBox::question(this, tr("番組終了"), tr("番組「%1」を終了しますか？\n終了後は再開できません。").arg(m_currentProgram.title)) != QMessageBox::Yes) {
        return;
    }

    QString userId = NicoliveAuthManager::instance().userInfo().userId;
    m_apiClient->endProgram(userId, m_currentProgram.nicoliveProgramId, [this](bool ok, const QString &err) {
        if (ok) {
            QMessageBox::information(this, tr("成功"), tr("番組を終了しました。"));
            refreshProgramInfo();
        } else {
            QMessageBox::warning(this, tr("失敗"), tr("番組終了に失敗しました: %1").arg(err));
        }
    });
}

void NicoliveControlDock::onExtendProgramClicked()
{
    if (m_currentProgram.nicoliveProgramId.isEmpty()) return;

    QString userId = NicoliveAuthManager::instance().userInfo().userId;
    m_apiClient->extendProgram(userId, m_currentProgram.nicoliveProgramId, 30, [this](bool ok, const QString &err) {
        if (ok) {
            QMessageBox::information(this, tr("成功"), tr("番組を30分延長しました！"));
            refreshProgramInfo();
        } else {
            QMessageBox::warning(this, tr("失敗"), tr("番組延長に失敗しました: %1").arg(err));
        }
    });
}

void NicoliveControlDock::onOpenInBrowserClicked()
{
    if (m_currentProgram.nicoliveProgramId.isEmpty()) return;
    QUrl url(QString("https://live.nicovideo.jp/watch/%1").arg(m_currentProgram.nicoliveProgramId));
    QDesktopServices::openUrl(url);
}

void NicoliveControlDock::updateDurationTimer()
{
    if (m_currentProgram.beginTime <= 0) {
        m_durationLabel->setText(tr("時間: --:--:--"));
        return;
    }

    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 diff = now - m_currentProgram.beginTime;
    if (diff < 0) diff = 0;

    int hours = diff / 3600;
    int minutes = (diff % 3600) / 60;
    int seconds = diff % 60;

    m_durationLabel->setText(tr("時間: %1:%2:%3")
                                 .arg(hours, 2, 10, QChar('0'))
                                 .arg(minutes, 2, 10, QChar('0'))
                                 .arg(seconds, 2, 10, QChar('0')));
}

void NicoliveControlDock::updateUiState()
{
    bool loggedIn = NicoliveAuthManager::instance().isLoggedIn();
    bool hasProgram = !m_currentProgram.nicoliveProgramId.isEmpty();

    m_applyStreamButton->setEnabled(loggedIn && hasProgram);
    m_startButton->setEnabled(loggedIn && hasProgram && !m_currentProgram.isOnAir() && !m_currentProgram.isEnded());
    m_endButton->setEnabled(loggedIn && hasProgram && m_currentProgram.isOnAir());
    m_extendButton->setEnabled(loggedIn && hasProgram && m_currentProgram.isOnAir());
    m_browserButton->setEnabled(hasProgram);
    m_refreshButton->setEnabled(loggedIn);
}

} // namespace Nicolive

