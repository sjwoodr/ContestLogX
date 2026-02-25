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

#include "termsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QPushButton>

TermsDialog::TermsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("ContestLogX — Terms of Use");
    setMinimumSize(560, 380);
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(16, 16, 16, 16);

    QLabel* heading = new QLabel("Welcome to ContestLogX");
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 2);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    QTextBrowser* text = new QTextBrowser;
    text->setReadOnly(true);
    text->setOpenExternalLinks(true);
    text->setHtml(
        "<p>ContestLogX is free software distributed under the "
        "<b>GNU General Public License, version 3</b>.</p>"
        "<p>This program is distributed in the hope that it will be useful, "
        "but <b>WITHOUT ANY WARRANTY</b>; without even the implied warranty of "
        "<b>MERCHANTABILITY</b> or <b>FITNESS FOR A PARTICULAR PURPOSE</b>. "
        "See the GNU General Public License for more details.</p>"
        "<p>You should have received a copy of the GNU General Public License "
        "along with ContestLogX. If not, see "
        "<a href=\"https://www.gnu.org/licenses/\">https://www.gnu.org/licenses/</a>.</p>"
        "<p>By clicking <b>I Accept</b>, you acknowledge that you have read and "
        "understood these terms.</p>"
    );
    layout->addWidget(text);

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    QPushButton* declineButton = new QPushButton("Decline");
    QPushButton* acceptButton  = new QPushButton("I Accept");
    acceptButton->setDefault(true);

    buttonLayout->addWidget(declineButton);
    buttonLayout->addWidget(acceptButton);
    layout->addLayout(buttonLayout);

    connect(acceptButton,  &QPushButton::clicked, this, &QDialog::accept);
    connect(declineButton, &QPushButton::clicked, this, &QDialog::reject);
}
