// Copyright (c) 2011-2020 The BitVault Core developers
// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITVAULT_QT_OPENURIDIALOG_H
#define BITVAULT_QT_OPENURIDIALOG_H

#include <QDialog>

class PlatformStyle;

namespace Ui {
    class OpenURIDialog;
}

class OpenURIDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OpenURIDialog(const PlatformStyle* platformStyle, QWidget* parent);
    ~OpenURIDialog();

    QString getURI();

protected Q_SLOTS:
    void accept() override;
    void changeEvent(QEvent* e) override;

private:
    Ui::OpenURIDialog* ui;

    const PlatformStyle* m_platform_style;
};

#endif // BITVAULT_QT_OPENURIDIALOG_H
