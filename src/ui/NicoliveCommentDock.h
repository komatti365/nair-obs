#pragma once

#include <QDockWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../common/NicoliveData.h"
#include "../api/NicoliveApiClient.h"
#include "../comment/NicoliveCommentReceiver.h"

namespace Nicolive {

class NicoliveCommentDock : public QDockWidget {
    Q_OBJECT

public:
    explicit NicoliveCommentDock(NicoliveApiClient *apiClient,
                                NicoliveCommentReceiver *commentReceiver,
                                QWidget *parent = nullptr);
    ~NicoliveCommentDock() override = default;

public slots:
    void addComment(const CommentItem &comment);
    void clearComments();

private slots:
    void onSendCommentClicked();
    void onCommentTypeChanged(int index);

private:
    void setupUi();
    void applySettings();

    NicoliveApiClient *m_apiClient = nullptr;
    NicoliveCommentReceiver *m_commentReceiver = nullptr;

    QTableWidget *m_tableWidget = nullptr;
    QCheckBox *m_autoScrollCheck = nullptr;
    QCheckBox *m_hideAnonymousCheck = nullptr;
    QPushButton *m_clearButton = nullptr;

    // Comment Post Form
    QComboBox *m_commentTypeCombo = nullptr;
    QLineEdit *m_senderNameInput = nullptr;
    QLineEdit *m_commentInput = nullptr;
    QComboBox *m_colorCombo = nullptr;
    QPushButton *m_sendButton = nullptr;

    QList<CommentItem> m_comments;
};

} // namespace Nicolive

