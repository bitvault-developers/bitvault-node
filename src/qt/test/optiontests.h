// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITVAULT_QT_TEST_OPTIONTESTS_H
#define BITVAULT_QT_TEST_OPTIONTESTS_H

#include <common/settings.h>
#include <qt/optionsmodel.h>
#include <univalue.h>

#include <QObject>

class OptionTests : public QObject
{
    Q_OBJECT
public:
    explicit OptionTests(interfaces::Node& node);

private Q_SLOTS:
    void init(); // called before each test function execution.
    void migrateSettings();
    void integerGetArgBug();
    void parametersInteraction();
    void extractFilter();

private:
    interfaces::Node& m_node;
    common::Settings m_previous_settings;
};

#endif // BITVAULT_QT_TEST_OPTIONTESTS_H
