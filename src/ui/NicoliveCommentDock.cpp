#include "NicoliveCommentDock.h"
#include "../auth/NicoliveAuthManager.h"
#include "../common/ConfigManager.h"
#include <QHeaderView>
#include <QDateTime>
#include <QMessageBox>
#include <QScrollBar>

namespace Nicolive {

NicoliveCommentDock::NicoliveCommentDock(NicoliveApiClient *apiClient, NicoliveCommentReceiver *commentReceiver,
					 QWidget *parent)
	: QDockWidget(tr("ニコニコ生放送 - コメント"), parent),
	  m_apiClient(apiClient),
	  m_commentReceiver(commentReceiver)
{
	setObjectName("NicoliveCommentDock");
	setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

	setupUi();

	if (m_commentReceiver) {
		connect(m_commentReceiver, &NicoliveCommentReceiver::commentReceived, this,
			&NicoliveCommentDock::addComment);
	}

	connect(&ConfigManager::instance(), &ConfigManager::configChanged, this, &NicoliveCommentDock::applySettings);

	applySettings();
}

void NicoliveCommentDock::setupUi()
{
	QWidget *container = new QWidget(this);
	QVBoxLayout *mainLayout = new QVBoxLayout(container);
	mainLayout->setContentsMargins(6, 6, 6, 6);
	mainLayout->setSpacing(6);

	// 1. Toolbar
	QHBoxLayout *toolbarLayout = new QHBoxLayout();
	m_autoScrollCheck = new QCheckBox(tr("自動スクロール"), container);
	m_autoScrollCheck->setChecked(true);

	m_hideAnonymousCheck = new QCheckBox(tr("184非表示"), container);
	connect(m_hideAnonymousCheck, &QCheckBox::toggled, [this](bool hide) {
		for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
			bool isAnon = m_tableWidget->item(i, 1)->data(Qt::UserRole).toBool();
			if (isAnon) {
				m_tableWidget->setRowHidden(i, hide);
			}
		}
	});

	m_clearButton = new QPushButton(tr("クリア"), container);
	connect(m_clearButton, &QPushButton::clicked, this, &NicoliveCommentDock::clearComments);

	toolbarLayout->addWidget(m_autoScrollCheck);
	toolbarLayout->addWidget(m_hideAnonymousCheck);
	toolbarLayout->addStretch();
	toolbarLayout->addWidget(m_clearButton);
	mainLayout->addLayout(toolbarLayout);

	// 2. Table Widget
	m_tableWidget = new QTableWidget(container);
	m_tableWidget->setColumnCount(4);
	m_tableWidget->setHorizontalHeaderLabels({tr("#"), tr("ユーザー"), tr("コメント"), tr("時刻")});
	m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
	m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
	m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_tableWidget->verticalHeader()->setVisible(false);
	m_tableWidget->setAlternatingRowColors(true);
	mainLayout->addWidget(m_tableWidget);

	// 3. Post Comment Section
	QVBoxLayout *postLayout = new QVBoxLayout();
	postLayout->setSpacing(4);

	QHBoxLayout *typeLayout = new QHBoxLayout();
	m_commentTypeCombo = new QComboBox(container);
	m_commentTypeCombo->addItem(tr("放送者コメント（運営コメント）"));
	m_commentTypeCombo->addItem(tr("通常コメント"));
	connect(m_commentTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&NicoliveCommentDock::onCommentTypeChanged);

	m_senderNameInput = new QLineEdit(container);
	m_senderNameInput->setPlaceholderText(tr("名前（省略可）"));
	m_senderNameInput->setMaximumWidth(120);

	m_colorCombo = new QComboBox(container);
	m_colorCombo->addItems({tr("白"), tr("赤"), tr("青"), tr("緑"), tr("黄"), tr("オレンジ"), tr("紫")});

	typeLayout->addWidget(m_commentTypeCombo);
	typeLayout->addWidget(m_senderNameInput);
	typeLayout->addWidget(m_colorCombo);
	postLayout->addLayout(typeLayout);

	QHBoxLayout *inputLayout = new QHBoxLayout();
	m_commentInput = new QLineEdit(container);
	m_commentInput->setPlaceholderText(tr("コメントを入力... (Enterで送信)"));
	connect(m_commentInput, &QLineEdit::returnPressed, this, &NicoliveCommentDock::onSendCommentClicked);

	m_sendButton = new QPushButton(tr("送信"), container);
	m_sendButton->setStyleSheet("font-weight: bold; background-color: #0078d4; color: white;");
	connect(m_sendButton, &QPushButton::clicked, this, &NicoliveCommentDock::onSendCommentClicked);

	inputLayout->addWidget(m_commentInput);
	inputLayout->addWidget(m_sendButton);
	postLayout->addLayout(inputLayout);

	mainLayout->addLayout(postLayout);

	setWidget(container);
}

void NicoliveCommentDock::applySettings()
{
	int fontSize = ConfigManager::instance().fontSize();
	QFont font = m_tableWidget->font();
	font.setPointSize(fontSize);
	m_tableWidget->setFont(font);

	m_hideAnonymousCheck->setChecked(!ConfigManager::instance().showAnonymous());
}

void NicoliveCommentDock::addComment(const CommentItem &comment)
{
	int maxRows = ConfigManager::instance().maxComments();
	if (m_tableWidget->rowCount() >= maxRows) {
		m_tableWidget->removeRow(0);
		if (!m_comments.isEmpty())
			m_comments.removeFirst();
	}

	m_comments.append(comment);

	int row = m_tableWidget->rowCount();
	m_tableWidget->insertRow(row);

	// 0: Number
	QTableWidgetItem *noItem = new QTableWidgetItem(QString::number(comment.no > 0 ? comment.no : row + 1));
	noItem->setTextAlignment(Qt::AlignCenter);

	// 1: User name / ID
	QString userDisplay = comment.userName.isEmpty() ? (comment.isAnonymous ? tr("184") : comment.userId)
							 : comment.userName;
	if (comment.isBroadcaster) {
		userDisplay = tr("【放送者】") + (comment.userName.isEmpty() ? "" : " " + comment.userName);
	}
	QTableWidgetItem *userItem = new QTableWidgetItem(userDisplay);
	userItem->setData(Qt::UserRole, comment.isAnonymous);

	// 2: Text
	QTableWidgetItem *textItem = new QTableWidgetItem(comment.text);

	// 3: Time
	QDateTime dt = QDateTime::fromSecsSinceEpoch(comment.date);
	QTableWidgetItem *timeItem = new QTableWidgetItem(dt.toString("hh:mm:ss"));
	timeItem->setTextAlignment(Qt::AlignCenter);

	// Coloring
	if (comment.isBroadcaster) {
		QColor bg(60, 60, 20); // subtle dark gold/yellow highlight
		noItem->setBackground(bg);
		userItem->setBackground(bg);
		textItem->setBackground(bg);
		timeItem->setBackground(bg);
		QFont boldFont = textItem->font();
		boldFont.setBold(true);
		textItem->setFont(boldFont);
		userItem->setFont(boldFont);
	}

	if (!comment.modifier.color.isEmpty()) {
		QColor textColor(comment.modifier.color);
		if (textColor.isValid()) {
			textItem->setForeground(textColor);
		}
	}

	m_tableWidget->setItem(row, 0, noItem);
	m_tableWidget->setItem(row, 1, userItem);
	m_tableWidget->setItem(row, 2, textItem);
	m_tableWidget->setItem(row, 3, timeItem);

	if (m_hideAnonymousCheck->isChecked() && comment.isAnonymous) {
		m_tableWidget->setRowHidden(row, true);
	}

	if (m_autoScrollCheck->isChecked()) {
		m_tableWidget->scrollToBottom();
	}
}

void NicoliveCommentDock::clearComments()
{
	m_tableWidget->setRowCount(0);
	m_comments.clear();
}

void NicoliveCommentDock::onCommentTypeChanged(int index)
{
	// 0: Broadcaster, 1: Normal
	m_senderNameInput->setVisible(index == 0);
}

void NicoliveCommentDock::onSendCommentClicked()
{
	QString text = m_commentInput->text().trimmed();
	if (text.isEmpty())
		return;

	QString programId = m_commentReceiver ? m_commentReceiver->currentProgramId() : QString();
	if (programId.isEmpty()) {
		QMessageBox::warning(this, tr("送信エラー"), tr("放送中番組が選択されていません。"));
		return;
	}

	QString userId = NicoliveAuthManager::instance().userInfo().userId;
	int type = m_commentTypeCombo->currentIndex();

	static const QString colorMap[] = {"", "red", "blue", "green", "yellow", "orange", "purple"};
	QString color = colorMap[m_colorCombo->currentIndex()];

	if (type == 0) {
		// Broadcaster comment
		QString name = m_senderNameInput->text().trimmed();
		m_apiClient->sendBroadcasterComment(
			userId, programId, text, name, color, true, [this](bool ok, const QString &err) {
				if (ok) {
					m_commentInput->clear();
				} else {
					QMessageBox::warning(this, tr("送信エラー"),
							     tr("放送者コメント送信に失敗しました: %1").arg(err));
				}
			});
	} else {
		// Normal comment
		m_apiClient->sendNormalComment(programId, text, 0, [this](bool ok, const QString &err) {
			if (ok) {
				m_commentInput->clear();
			} else {
				QMessageBox::warning(this, tr("送信エラー"),
						     tr("通常コメント送信に失敗しました: %1").arg(err));
			}
		});
	}
}

} // namespace Nicolive
