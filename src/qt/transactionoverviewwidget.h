// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITVAULT_QT_TRANSACTIONOVERVIEWWIDGET_H
#define BITVAULT_QT_TRANSACTIONOVERVIEWWIDGET_H

#include <QListView>
#include <QSize>

QT_BEGIN_NAMESPACE
class QShowEvent;
class QWidget;
QT_END_NAMESPACE

class TransactionOverviewWidget : public QListView
{
    Q_OBJECT

public:
    explicit TransactionOverviewWidget(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void showEvent(QShowEvent* event) override;
};

#endif // BITVAULT_QT_TRANSACTIONOVERVIEWWIDGET_H
