/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026, by Steve Woodruff, N9OH
 *
 * This file is part of ContestLogX.
 *
 * ContestLogX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ContestLogX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ContestLogX.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef TERMSDIALOG_H
#define TERMSDIALOG_H

#include <QDialog>

/**
 * @brief First-run terms of use dialog.
 *
 * Displays the GPL3 warranty disclaimer and requires the user to accept
 * before the application proceeds. Declining exits the application.
 */
class TermsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TermsDialog(QWidget* parent = nullptr);
};

#endif // TERMSDIALOG_H
