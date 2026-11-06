#pragma once

#include <QDockWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QNetworkAccessManager>
#include "../common/NicoliveData.h"
#include "../api/NicoliveApiClient.h"
#include "../comment/NicoliveCommentReceiver.h"

namespace Nicolive {

class NicoliveControlDock : public QDockWidget {
	Q_OBJECT

public:
	explicit NicoliveControlDock(NicoliveApiClient *apiClient, NicoliveCommentReceiver *commentReceiver,
				     QWidget *parent = nullptr);
	~NicoliveControlDock() override = default;

	const ProgramInfo &currentProgram() const { return m_currentProgram; }

public slots:
	void refreshProgramInfo();
	void applyStreamToObs();

signals:
	void openSettingsRequested();
	void programChanged(const ProgramInfo &program);

private slots:
	void onAuthStatusChanged(bool isLoggedIn);
	void onUserInfoUpdated(const UserInfo &info);
	void onStartProgramClicked();
	void onEndProgramClicked();
	void onExtendProgramClicked();
	void onOpenInBrowserClicked();
	void updateDurationTimer();

private:
	void setupUi();
	void updateUiState();
	void loadUserIcon(const QString &url);

	NicoliveApiClient *m_apiClient = nullptr;
	NicoliveCommentReceiver *m_commentReceiver = nullptr;
	QNetworkAccessManager *m_imageNet = nullptr;

	ProgramInfo m_currentProgram;
	QTimer *m_durationTimer = nullptr;
	QTimer *m_autoRefreshTimer = nullptr;

	// UI elements - User info
	QWidget *m_userPanel = nullptr;
	QLabel *m_userIconLabel = nullptr;
	QLabel *m_userNameLabel = nullptr;
	QLabel *m_userIdLabel = nullptr;
	QLabel *m_premiumBadge = nullptr;
	QPushButton *m_loginButton = nullptr;

	// UI elements - Program info
	QWidget *m_programPanel = nullptr;
	QLabel *m_programTitleLabel = nullptr;
	QLabel *m_programIdLabel = nullptr;
	QLabel *m_programStatusBadge = nullptr;
	QLabel *m_durationLabel = nullptr;
	QLabel *m_statsLabel = nullptr;

	// UI elements - Actions
	QPushButton *m_applyStreamButton = nullptr;
	QPushButton *m_startButton = nullptr;
	QPushButton *m_endButton = nullptr;
	QPushButton *m_extendButton = nullptr;
	QPushButton *m_browserButton = nullptr;
	QPushButton *m_refreshButton = nullptr;
	QPushButton *m_settingsButton = nullptr;
};

} // namespace Nicolive
