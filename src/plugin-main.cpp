#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include <QPointer>
#include <QMetaObject>

#include "common/ConfigManager.h"
#include "auth/NicoliveAuthManager.h"
#include "api/NicoliveApiClient.h"
#include "comment/NicoliveCommentReceiver.h"
#include "obs/ObsStreamHelper.h"
#include "ui/NicoliveControlDock.h"
#include "ui/NicoliveCommentDock.h"
#include "ui/NicoliveSettingsDialog.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("nair-obs", "en-US")

namespace {

Nicolive::NicoliveApiClient *g_apiClient = nullptr;
Nicolive::NicoliveCommentReceiver *g_commentReceiver = nullptr;
QPointer<Nicolive::NicoliveControlDock> g_controlDock = nullptr;
QPointer<Nicolive::NicoliveCommentDock> g_commentDock = nullptr;
QPointer<Nicolive::NicoliveSettingsDialog> g_settingsDialog = nullptr;

void openSettingsDialog()
{
	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!g_settingsDialog) {
		g_settingsDialog = new Nicolive::NicoliveSettingsDialog(mainWindow);
	}
	g_settingsDialog->show();
	g_settingsDialog->raise();
	g_settingsDialog->activateWindow();
}

void onFrontendEvent(enum obs_frontend_event event, void *private_data)
{
	Q_UNUSED(private_data);

	if (event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		qInfo() << "[N-Air OBS] OBS Streaming started.";
		if (Nicolive::ConfigManager::instance().autoStartProgram() && g_controlDock) {
			const auto &prog = g_controlDock->currentProgram();
			if (prog.isValid() && !prog.isOnAir() && !prog.isEnded()) {
				QString userId = Nicolive::NicoliveAuthManager::instance().userInfo().userId;
				g_apiClient->startProgram(userId, prog.nicoliveProgramId, [](bool ok, const QString &err) {
					if (ok) {
						qInfo() << "[N-Air OBS] Auto-started Nicolive program successfully.";
					} else {
						qWarning()
							<< "[N-Air OBS] Failed to auto-start Nicolive program:" << err;
					}
				});
			}
		}
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		qInfo() << "[N-Air OBS] OBS Streaming stopped.";
		if (Nicolive::ConfigManager::instance().autoEndProgram() && g_controlDock) {
			const auto &prog = g_controlDock->currentProgram();
			if (prog.isValid() && prog.isOnAir()) {
				QString userId = Nicolive::NicoliveAuthManager::instance().userInfo().userId;
				g_apiClient->endProgram(userId, prog.nicoliveProgramId, [](bool ok, const QString &err) {
					if (ok) {
						qInfo() << "[N-Air OBS] Auto-ended Nicolive program successfully.";
					} else {
						qWarning() << "[N-Air OBS] Failed to auto-end Nicolive program:" << err;
					}
				});
			}
		}
	}
}

} // namespace

bool obs_module_load(void)
{
	qInfo() << "[N-Air OBS] Loading Nicolive Integration Plugin...";

	Nicolive::ConfigManager::instance().load();

	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	g_apiClient = new Nicolive::NicoliveApiClient();
	g_commentReceiver = new Nicolive::NicoliveCommentReceiver();

	// Create Docks
	g_controlDock = new Nicolive::NicoliveControlDock(g_apiClient, g_commentReceiver, mainWindow);
	g_commentDock = new Nicolive::NicoliveCommentDock(g_apiClient, g_commentReceiver, mainWindow);

	QObject::connect(g_controlDock, &Nicolive::NicoliveControlDock::openSettingsRequested,
			 []() { openSettingsDialog(); });

	obs_frontend_add_dock(g_controlDock);
	obs_frontend_add_dock(g_commentDock);

	// Add Menu Item under Tools
	obs_frontend_add_tools_menu_item(
		"ニコニコ生放送 設定 (N-Air OBS)...", [](void *) { openSettingsDialog(); }, nullptr);

	// Register event callback
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);

	qInfo() << "[N-Air OBS] Plugin loaded successfully.";
	return true;
}

void obs_module_unload(void)
{
	qInfo() << "[N-Air OBS] Unloading Nicolive Integration Plugin...";

	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);

	if (g_commentReceiver) {
		g_commentReceiver->stop();
		delete g_commentReceiver;
		g_commentReceiver = nullptr;
	}

	delete g_apiClient;
	g_apiClient = nullptr;

	delete g_settingsDialog;
	g_settingsDialog = nullptr;

	Nicolive::ConfigManager::instance().save();
}
