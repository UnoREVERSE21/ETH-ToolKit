#ifndef BUILDSETTINGS_H
#define BUILDSETTINGS_H

#include <QDialog>
#include <QSettings>

namespace Ui {
    class BuildSettings;
}

class BuildSettings : public QDialog
{
    Q_OBJECT

public:
    explicit BuildSettings(QString preset, QWidget *parent = 0);
    ~BuildSettings();

    QString selectedProfile();

private slots:
    // I know this is a mess ;)
    void on_toolButton_clicked();
    void on_toolButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_listWidget_itemSelectionChanged();
    void onInputChanged();
    bool on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void reloadProfiles(QString crt);
    void on_pushButton_4_clicked();
    void on_buttonBox_accepted();

private:
    bool modified, autochange;
    Ui::BuildSettings *ui;
    QSettings *profile;
};

#endif // BUILDSETTINGS_H
