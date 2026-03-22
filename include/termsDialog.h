/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
 */

#ifndef TERMSDIALOG_H
#define TERMSDIALOG_H

#include <QDialog>

/**
 * @brief Terms of use dialog.
 *
 * Displays the MIT license disclaimer and requires the user to accept
 * before the application proceeds. Shown on first run or when
 * TERMS_VERSION is bumped. Declining exits the application.
 */
class TermsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TermsDialog(QWidget* parent = nullptr);
};

#endif // TERMSDIALOG_H
