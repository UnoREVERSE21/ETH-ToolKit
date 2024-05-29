#include "buildsettings.h"
#include "ui_buildsettings.h"
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <mw.h>

BuildSettings::BuildSettings(QString preset, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildSettings),
    profile(NULL)
{
    ui->setupUi(this);

    reloadProfiles(preset);

    connect(ui->compilerC, SIGNAL(textEdited(QString)), this, SLOT(onInputChanged()));
    connect(ui->compilerCPP, SIGNAL(textEdited(QString)), this, SLOT(onInputChanged()));
    connect(ui->coreA, SIGNAL(textEdited(QString)), this, SLOT(onInputChanged()));
    connect(ui->linker, SIGNAL(textEdited(QString)), this, SLOT(onInputChanged()));
    connect(ui->pathCore, SIGNAL(textEdited(QString)), this, SLOT(onInputChanged()));
    connect(ui->pathIncludes, SIGNAL(textEdited(QString)), this, SLOT(onInputChanged()));
    connect(ui->uploader, SIGNAL(textEdited(QString)), this, SLOT(onInputChanged()));
    modified = false;
    autochange = false;
}

void BuildSettings::reloadProfiles(QString crt)
{
    QDir d(QApplication::applicationDirPath() + "/profiles/");
    autochange = true;

    if (crt == "" && ui->listWidget->count() != 0)
        crt = ui->listWidget->currentItem()->text();

    ui->listWidget->clear();

    foreach (QString file, d.entryList(QStringList("*.ebp")))
    {
        ui->listWidget->addItem(QFileInfo(file).baseName());

        if (QFileInfo(file).baseName() == crt)
            ui->listWidget->setCurrentRow(ui->listWidget->count() - 1);
    }

    autochange = false;
}

void BuildSettings::onInputChanged()
{
    if (profile == NULL)
    {
        QMessageBox::warning(NULL, "No profile", "You didn't select a profile yet!");
        return;
    }

    if (!autochange)
        modified = true;

    QLineEdit *e = (QLineEdit *)sender();

    if (e->objectName() == "profileName")
        e->setText(e->text().replace(QRegExp("[^A-Za-z0-9\\. ]*"), ""));
}

QString BuildSettings::selectedProfile()
{
    return ui->listWidget->currentItem()->text();
}

BuildSettings::~BuildSettings()
{
    delete ui;
}

void BuildSettings::on_toolButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory();
    if (dir != "")
        ui->pathCore->setText(dir);
}

void BuildSettings::on_toolButton_2_clicked()
{
    QString dir = QFileDialog::getExistingDirectory();
    if (dir != "")
        ui->pathIncludes->setText(dir);
}

void BuildSettings::on_pushButton_3_clicked()
{
    QString name = QInputDialog::getText(NULL, "Profile name", "Please enter a name for the new profile (a-z, A-Z, 0-9, dots and spaces only) ");

    name.replace(QRegExp("[^\\.a-zA-Z0-9 ]*"), "");

    if (name == "")
        return;

    for (int i = 0; i < ui->listWidget->count(); i++)
    {
        if (ui->listWidget->item(i)->text() != name)
            continue;

        QMessageBox::critical(NULL, "Profile exists", "A profile with this name already exists!");
        return;
    }

    ui->listWidget->addItem(new QListWidgetItem(name));
    ui->listWidget->setCurrentRow(ui->listWidget->count() - 1);
}

bool BuildSettings::on_pushButton_clicked()
{
    if (profile == NULL)
    {
        QMessageBox::warning(NULL, "No profile", "You didn't select a profile yet!");
        return false;
    }

    if (ui->listWidget->currentItem()->text() == "Default")
    {
        if (QMessageBox::question(NULL, "Changing default profile", "Are you sure, you want to overwrite the Default profile?", QMessageBox::No, QMessageBox::Yes) == QMessageBox::No)
            return false;
    }

    if (QFileInfo(profile->fileName()).baseName() != ui->profileName->text())
    {
        for (int i = 0; i < ui->listWidget->count(); i++)
        {
            if (ui->listWidget->item(i)->text() != ui->profileName->text())
                continue;

            QMessageBox::critical(NULL, "Profile exists", "Cannot rename this profile, since a profile with this name already exists!");
            return false;
        }

        if (!QFile::rename(profile->fileName(), ((MW*)parent())->getEBPName(ui->profileName->text())))
            QMessageBox::critical(NULL, "Renaming failed", "Renaming the profile file failed!");

        else
        {
            ui->listWidget->currentItem()->setText(ui->profileName->text());

            delete profile;
            profile = new QSettings(((MW*)parent())->getEBPName(ui->profileName->text()), QSettings::IniFormat);
        }
    }

    modified = false;
    profile->setValue("compilerC", ui->compilerC->text());
    profile->setValue("compilerCPP", ui->compilerCPP->text());
    profile->setValue("coreA", ui->coreA->text());
    profile->setValue("objCopy", ui->objCopy->text());
    profile->setValue("linker", ui->linker->text());
    profile->setValue("pathCore", ui->pathCore->text());
    profile->setValue("pathIncludes", ui->pathIncludes->text());
    profile->setValue("uploader", ui->uploader->text());
    return true;
}

void BuildSettings::on_listWidget_itemSelectionChanged()
{
    if (autochange)
        autochange = false;

    else if (modified)
    {
        if (QMessageBox::question(NULL, "Modified profile", "You haven't saved this profile yet, do you want to do so now?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
        {
            autochange = true;
            if (!on_pushButton_clicked())
            {
                for (int i = 0; i < ui->listWidget->count(); i++)
                {
                    if (ui->listWidget->item(i)->text() != QFileInfo(profile->fileName()).baseName())
                        continue;

                    ui->listWidget->setCurrentRow(i);
                    return;
                }

                return;
            }
        }
    }

    if (profile != NULL)
    {
        delete profile;
        profile = NULL;
    }

    profile = new QSettings(((MW*)parent())->getEBPName(ui->listWidget->currentItem()->text()), QSettings::IniFormat);

    profile->setValue("void", 0);

    autochange = true;
    ui->profileName->setText(ui->listWidget->currentItem()->text());
    ui->compilerC->setText(profile->value("compilerC").toString());
    ui->compilerCPP->setText(profile->value("compilerCPP").toString());
    ui->coreA->setText(profile->value("coreA").toString());
    ui->linker->setText(profile->value("linker").toString());
    ui->objCopy->setText(profile->value("objCopy").toString());
    ui->pathCore->setText(profile->value("pathCore").toString());
    ui->pathIncludes->setText(profile->value("pathIncludes").toString());
    ui->uploader->setText(profile->value("uploader").toString());
    autochange = false;
}

void BuildSettings::on_pushButton_2_clicked()
{
    if (profile == NULL)
    {
        QMessageBox::critical(NULL, "No profile selected", "Please select a profile that you want to delete");
        return;
    }

    if (ui->listWidget->currentItem()->text() == "Default")
        return;

    if (QMessageBox::question(NULL, "Deleting profile", "Are you sure that you want to delete the profile " + ui->listWidget->currentItem()->text() + "?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
        return;

    if (!QFile::remove(((MW*)parent())->getEBPName(ui->listWidget->currentItem()->text())))
    {
        QMessageBox::critical(NULL, "Profile deletion failed", "The profile file could not be deleted!");
        return;
    }

    delete profile;
    profile = NULL;

    delete ui->listWidget->takeItem(ui->listWidget->currentRow());
}

void BuildSettings::on_pushButton_4_clicked()
{
    int row = ui->listWidget->currentRow();

    QFile f(((MW*)parent())->getEBPName(ui->listWidget->currentItem()->text()));

    for (int i = 0; i < ui->listWidget->count(); i++)
    {
        if (i == row)
            continue;

        if (ui->listWidget->item(i)->text() == ui->listWidget->currentItem()->text() + " clone")
        {
            ui->listWidget->setCurrentRow(i);
            return;
        }
    }

    f.open(QFile::ReadOnly);

    QFile f2(((MW*)parent())->getEBPName(ui->listWidget->currentItem()->text() + " clone"));
    f2.open(QFile::WriteOnly | QFile::Truncate);
    f2.write(f.readAll());
    f.close();
    f2.close();

    reloadProfiles("");
}

void BuildSettings::on_buttonBox_accepted()
{
    if (!modified)
        return;

    if (QMessageBox::question(NULL, "Modified profile", "You haven't saved this profile yet, do you want to do so now?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        autochange = true;
        on_pushButton_clicked();
    }
}































