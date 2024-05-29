#ifndef MW_H
#define MW_H

#include <QMainWindow>
#include <QTreeWidgetItem>
#include <QSettings>
#include <QDir>
#include <Qsci/qscilexercpp.h>
#include <QDateTime>
#include <QActionGroup>
#include "workthread.h"

namespace Ui {
    class MW;
}

class MW : public QMainWindow
{
    Q_OBJECT

public:
    explicit MW(QString file = "", QWidget *parent = 0);
    void doJob(QString job);
    QString getEBPName(QString pname);
    ~MW();

private:
    Ui::MW *ui;
    QSettings *project, *profile;
    QDir projectDir;
    QsciLexerCPP lexer;
    QStringList m_Files;
    QString m_BuildDir;

    QString buildObject(const QString& sourceObject);
    QDateTime newestDependency(const QString& file, const QStringList &incDirs, QStringList &checkedFiles, int level = 0);
    QString cleanPath(const QString& path);
    void addDirRecursive(QString dir, QStringList& libFiles, QStringList& m_IncDirs);
    void addLib(QString libDir);
    void addFile(QString file);
    void remLib(QTreeWidgetItem* i);
    void refreshLib(QTreeWidgetItem* i);
    void addDirToTree(QTreeWidgetItem* i, QString path);
    void reloadProfile();

    QMap< QTreeWidgetItem*, QString > m_Libs;

    QTreeWidgetItem *twLibs, *tiFiles;
    bool projectModified;
    QSettings *sets;
    WorkThread t;
    QString cport;

    QActionGroup *agMCU, *agUPL, *agCLK, *agPRT;

private slots:
    void addIssue(QString file, int line, QString message);
    void on_en_stateChanged(int );
    void on_sel_clicked();
    void reiterateSerialPorts();
    void nc();
    void data();
    void openFile(QTreeWidgetItem*,int);
    void openFile(const QString &f);
    void saveEverything();
    bool hasUnsavedFiles();
    void saveFile(int);
    bool showProjectSaveQuestion();
    bool showFilesSaveQuestion();
    void appendCOUT(QString, toutFlags);
    void saveAllFiles();
    void closeFile(int);
    void clearAnnotations();
    void openProject(QString efpFile = "");
    void saveProject();
    void updateHome();
    bool closeProject();
    void fileModified(bool);
    void menuTrigger(QAction*);
    void compile(bool fromThread = false);
    void clean();
    void upload(bool fromThread = false);
    void selectedProfile();
    void recompile();
    void cout(const QString&);
    void on_filetree_customContextMenuRequested(QPoint pos);
    void on_actionBuild_Settings_triggered();
    void on_files_tabCloseRequested(int index);
    void on_rprojs_linkActivated(QString link);
    void on_issues_cellActivated(int row, int column);
};

#endif // MW_H
