// Copyright (c) 2014-2025 The BitVault Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITVAULT_QT_BITVAULTADDRESSVALIDATOR_H
#define BITVAULT_QT_BITVAULTADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class BitVaultAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit BitVaultAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

/** BitVault address widget validator, checks for a valid bitvault address.
 */
class BitVaultAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit BitVaultAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

#endif // BITVAULT_QT_BITVAULTADDRESSVALIDATOR_H
