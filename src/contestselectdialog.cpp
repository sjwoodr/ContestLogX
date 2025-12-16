#include "contestselectdialog.h"
#include "debuglogger.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
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
    
    DebugLogger::instance().log("ContestSelectDialog", 
        QString("Loading contests from: %1").arg(contestsDir.absolutePath()));
    
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = contestsDir.entryInfoList(filters, QDir::Files);
    
    DebugLogger::instance().log("ContestSelectDialog", 
        QString("Found %1 JSON files").arg(files.size()));
    
    for (const QFileInfo &fileInfo : files) {
        QString filePath = fileInfo.absoluteFilePath();
        DebugLogger::instance().log("ContestSelectDialog", 
            QString("Processing: %1").arg(fileInfo.fileName()));
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            DebugLogger::instance().log("ContestSelectDialog", 
                QString("ERROR: Cannot open file: %1").arg(filePath));
            continue;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            DebugLogger::instance().log("ContestSelectDialog", 
                QString("ERROR: JSON parse error in %1: %2 at offset %3")
                    .arg(fileInfo.fileName())
                    .arg(parseError.errorString())
                    .arg(parseError.offset));
            continue;
        }
        
        if (!doc.isObject()) {
            DebugLogger::instance().log("ContestSelectDialog", 
                QString("ERROR: JSON root is not an object in %1").arg(fileInfo.fileName()));
            continue;
        }
        
        QJsonObject obj = doc.object();
        
        // Try to get name from either top-level or nested under "contest"
        QString name = obj["name"].toString();
        QString description = obj["description"].toString();
        
        if (name.isEmpty() && obj.contains("contest")) {
            QJsonObject contestObj = obj["contest"].toObject();
            name = contestObj["name"].toString();
            description = contestObj["description"].toString();
        }
        
        if (name.isEmpty()) {
            DebugLogger::instance().log("ContestSelectDialog", 
                QString("WARNING: No name found in %1, using filename").arg(fileInfo.fileName()));
            name = fileInfo.baseName();
        }
        
        DebugLogger::instance().log("ContestSelectDialog", 
            QString("Successfully loaded: %1").arg(name));
        
        QListWidgetItem *item = new QListWidgetItem(m_contestList);
        item->setText(name);
        item->setToolTip(description.isEmpty() ? name : description);
        item->setData(Qt::UserRole, filePath);
    }
    
    DebugLogger::instance().log("ContestSelectDialog", 
        QString("Total contests loaded: %1").arg(m_contestList->count()));
}
