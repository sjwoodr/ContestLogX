#include "stationClassDialog.h"

StationClassDialog::StationClassDialog(const QString& prompt, const QStringList& options, 
                                       QWidget *parent, const QString& defaultClass)
    : QDialog(parent), m_buttonGroup(new QButtonGroup(this))
{
    setWindowTitle("Select Station Class");
    setModal(true);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    // Add prompt label
    QLabel *promptLabel = new QLabel(prompt, this);
    promptLabel->setWordWrap(true);
    layout->addWidget(promptLabel);
    
    layout->addSpacing(10);
    
    // Add radio buttons for each option
    bool first = true;
    bool defaultFound = false;
    for (const QString& option : options) {
        QStringList parts = option.split('|');
        if (parts.size() >= 3) {
            QString id = parts[0];
            QString name = parts[1];
            QString desc = parts[2];
            
            // Only append description if it's not empty
            QString displayText = desc.isEmpty() ? name : QString("%1 - %2").arg(name, desc);
            QRadioButton *radio = new QRadioButton(displayText, this);
            radio->setProperty("classId", id);
            m_buttonGroup->addButton(radio);
            layout->addWidget(radio);
            
            // Check if this is the default class or the first one
            bool shouldCheck = (!defaultClass.isEmpty() && id == defaultClass) || (first && defaultClass.isEmpty());
            if (shouldCheck) {
                radio->setChecked(true);
                m_selectedClass = id;
                defaultFound = true;
            }
            first = false;
        }
    }
    
    layout->addSpacing(10);
    
    // Add buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);
    
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    connect(m_buttonGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked),
            [this](QAbstractButton *button) {
                m_selectedClass = button->property("classId").toString();
            });
    
    resize(400, 200);
}

QString StationClassDialog::getSelectedClass() const
{
    return m_selectedClass;
}
