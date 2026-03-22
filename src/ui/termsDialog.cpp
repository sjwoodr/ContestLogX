/*
 * ContestLogX - Amateur Radio Contest Logging Software
 * Copyright (c) 2025-2026 Steve Woodruff, N9OH
 *
 * Released under the MIT License. See LICENSE file for details.
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
        "<b>MIT License</b>.</p>"
        "<p>This program is provided <b>AS IS</b>, "
        "<b>WITHOUT WARRANTY OF ANY KIND</b>, express or implied, including but "
        "not limited to the warranties of <b>MERCHANTABILITY</b> or "
        "<b>FITNESS FOR A PARTICULAR PURPOSE</b>.</p>"
        "<p>See the LICENSE file included with ContestLogX or the "
        "<a href=\"https://opensource.org/licenses/MIT\">full license text online</a>.</p>"
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
