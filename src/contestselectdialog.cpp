#include "contestselectdialog.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QCoreApplication>
#include <QFileDialog>

ContestSelectDialog::ContestSelectDialog(QWidget *parent)
    : QDialog(parent), m_openingExisting(false)
{
    setWindowTitle("Select Contest");
    setModal(true);
    resize(400, 350);

    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QLabel *label = new QLabel("Select a contest:", this);
    layout->addWidget(label);
    
    m_contestList = new QListWidget(this);
    layout->addWidget(m_contestList);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_openExistingButton = new QPushButton("Open Existing", this);
    m_okButton = new QPushButton("OK", this);
    m_cancelButton = new QPushButton("Cancel", this);
    buttonLayout->addWidget(m_openExistingButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    layout->addLayout(buttonLayout);
    
    connect(m_okButton, &QPushButton::clicked, this, &ContestSelectDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_openExistingButton, &QPushButton::clicked, this, &ContestSelectDialog::onOpenExistingClicked);
    connect(m_contestList, &QListWidget::itemDoubleClicked, this, &ContestSelectDialog::onItemDoubleClicked);
    
    loadContestList();
}

QString ContestSelectDialog::selectedContestFile() const
{
    return m_selectedFile;
}

void ContestSelectDialog::onOkClicked()
{
    QListWidgetItem *item = m_contestList->currentItem();
    if (item) {
        m_selectedFile = item->data(Qt::UserRole).toString();
        accept();
    }
}

void ContestSelectDialog::onItemDoubleClicked(QListWidgetItem *item)
{
    if (item) {
        m_selectedFile = item->data(Qt::UserRole).toString();
        accept();
    }
}

void ContestSelectDialog::onOpenExistingClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open Existing Log"), "", tr("ContestLogX Files (*.clx)"));
    
    if (!fileName.isEmpty()) {
        m_selectedFile = fileName;
        m_openingExisting = true;
        accept();
    }
}

void ContestSelectDialog::loadContestList()
{
    QDir contestsDir(QCoreApplication::applicationDirPath() + "/contests");
    if (!contestsDir.exists()) {
        contestsDir = QDir("contests");
    }
    
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = contestsDir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo &fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString name = obj["name"].toString();
                QString description = obj["description"].toString();
                
                QListWidgetItem *item = new QListWidgetItem(m_contestList);
                item->setText(name.isEmpty() ? fileInfo.baseName() : name);
                item->setToolTip(description);
                item->setData(Qt::UserRole, fileInfo.absoluteFilePath());
            }
        }
    }
}
