#include "mw.h"
#include "ui_mw.h"

#include <QFileDialog>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QFileDialog>
#include <QTextEdit>

#include "Qsci/qsciscintilla.h"
#include "Qsci/qscistyle.h"
#include "Qsci/qsciapis.h"
#include <QMessageBox>
#include <QProcess>
#include <QCryptographicHash>
#include <QDateTime>
#include <QMenu>

#include "buildsettings.h"
#include <math.h>

QTcpServer *s = NULL;
QFile f("firmware.bin");
QString inFile;

MW::MW(QString file, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MW), project(NULL),
    t(this)
{
    profile = NULL;
    ui->setupUi(this);
    projectModified = false;

    if (file != "")
    {
        /*ui->in->setText(file);
        ui->en->setChecked(true);*/
    }

    sets = new QSettings("Ethernet Flasher", "IDE");

    updateHome();

    ui->mainToolBar->addAction(QIcon("images/compile.png"), "Compile", this, SLOT(compile()));
    ui->mainToolBar->addAction(QIcon("images/brush.png"), "Clean", this, SLOT(clean()));
    ui->mainToolBar->addAction(QIcon("images/rebuild.png"), "Recompile", this, SLOT(recompile()));
    ui->mainToolBar->addAction(QIcon("images/chip.png"), "Upload (AVR)", this, SLOT(upload()));
    ui->mainToolBar->addAction(QIcon("images/configure.png"), "Build Settings", this, SLOT(on_actionBuild_Settings_triggered()));

    QsciAPIs* a = new QsciAPIs(&lexer);
    a->add("analogRead?(u8 pin)");
    a->prepare();
    lexer.setAPIs(a);

    lexer.setDefaultFont(QFont("Monospace"));

    s = new QTcpServer();
    s->listen(QHostAddress::Any, 19293);
    connect(s, SIGNAL(newConnection()), this, SLOT(nc()));
    ui->statusBar->showMessage("Server is running");

    tabifyDockWidget(ui->issuesdock, ui->dockWidget_3);
    tabifyDockWidget(ui->issuesdock, ui->dockWidget_4);

    toutFlags dummy;
    qRegisterMetaType<toutFlags>("toutFlags", &dummy);

    connect(&t, SIGNAL(textOut(QString, toutFlags)), this, SLOT(appendCOUT(QString, toutFlags)));
    connect(&t, SIGNAL(issueOut(QString,int,QString)), this, SLOT(addIssue(QString,int,QString)));
    connect(ui->menuSerial_Port, SIGNAL(aboutToShow()), this, SLOT(reiterateSerialPorts()));

    setWindowState(Qt::WindowMaximized);

    ui->issues->setColumnWidth(0,  250);
    ui->issues->setColumnWidth(1,   50);
    ui->issues->setColumnWidth(2, 1000);

    agMCU = new QActionGroup(this);
    agMCU->addAction(ui->actionAtmega_168p_Mini);
    ui->actionAtmega_168p_Mini->setData("atmega168p");
    agMCU->addAction(ui->actionAtmega_328p_Uno);
    ui->actionAtmega_328p_Uno->setData("atmega328p");

    agUPL = new QActionGroup(this);
    agUPL->addAction(ui->actionVia_avrdude_USB);
    agUPL->addAction(ui->actionVia_EthernetFlasher);

    agPRT = new QActionGroup(this);
    agPRT->addAction(ui->actionAuto);
    ui->actionAuto->setData("Auto");

    agCLK = new QActionGroup(this);
    agCLK->addAction(ui->action8_MHz);
    ui->action8_MHz->setData("8000000UL");
    agCLK->addAction(ui->action16_MHz);
    ui->action16_MHz->setData("16000000UL");
}

void MW::appendCOUT(QString msg, toutFlags sb)
{
    if (sb & TO_NONL)
        ui->cout->insertPlainText(msg);

    else
        ui->cout->append(msg);

    ui->cout->moveCursor(QTextCursor::End);

    if (sb & TO_STATUS)
        ui->statusBar->showMessage(msg);
}

void MW::reiterateSerialPorts()
{
    QDir d("/dev");

    QString current = agPRT->checkedAction() ? agPRT->checkedAction()->text() : "";

    foreach (QAction* a, ui->menuSerial_Port->actions())
    if (a != ui->actionAuto)
    {
            ui->menuSerial_Port->removeAction(a);
            agPRT->removeAction(a);
            delete a;
    }

    QString first;
    bool found = false;
    foreach (QString port, d.entryList(QStringList() << "ttyACM*" << "ttyUSB*", QDir::System, QDir::Name))
    {
        QAction *a = ui->menuSerial_Port->addAction("/dev/" + port);
        a->setData("/dev/" + port);
        a->setCheckable(true);
        agPRT->addAction(a);

        if (first == "")
            first = "/dev/" + port;

        if (current == "/dev/" + port)
        {
            found = true;
            a->setChecked(true);
        }
    }

    cport = found ? current : first;
}

void MW::updateHome()
{
    QString tmp = "none";
    foreach (QString rp, sets->value("recentProjects").toStringList())
    {
        if (rp.trimmed() == "")
            continue;

        if (tmp == "none")
            tmp = "";

        tmp = "<a href=\"" + rp + "\">" + rp + "</a><br>" + tmp;
    }

    ui->rprojs->setText(tmp);
}

MW::~MW()
{
    closeProject();
    delete ui;
}

void MW::cout(const QString &s)
{
    ui->cout->append(s);
}

QString MW::buildObject(const QString &sourceObject)
{
    return m_BuildDir + QCryptographicHash::hash(sourceObject.toUtf8(), QCryptographicHash::Sha1).toHex() + "_" + QFileInfo(sourceObject).fileName() + ".o";
}

void MW::addIssue(QString file, int line, QString message)
{
    if (line == 0)
        return;

    int row = ui->issues->rowCount();
    ui->issues->setRowCount(row + 1);
    QTableWidgetItem *ti;

    ti = new QTableWidgetItem(file.section('/', -1));
    ti->setData(Qt::UserRole, file);
    ui->issues->setItem(row, 0, ti);

    ti = new QTableWidgetItem(QString::number(line));
    ti->setData(Qt::UserRole, line);
    ui->issues->setItem(row, 1, ti);

    ti = new QTableWidgetItem(message);
    ti->setData(Qt::UserRole, message);
    ui->issues->setItem(row, 2, ti);

}

QDateTime MW::newestDependency(const QString& file, const QStringList& incDirs, QStringList &checkedFiles, int level)
{
    if (checkedFiles.contains(file))
        return QDateTime();

    if (level == 10)
        return QDateTime();

    checkedFiles.append(file);

    QDateTime mod = QFileInfo(file).lastModified();

    QFile f(file);
    f.open(QFile::ReadOnly);

    QString data = f.readAll(), data2 = data;
    f.close();

    // remove comments
    data.append('\n');
    data.replace(QRegExp("//[^\\n]*\\n"), "\n");

    int pos = -1;

    QRegExp r1("(\"[^\n]*(\\*\\/|\\/\\*)[^\n]*\"|/\\*.*\\*/)");
    r1.setMinimal(true);
    data.replace(r1, "");

    QRegExp r2("#include [<\"]([^>\"]*.h)[>\"].*\\n");
    QStringList incDirsCopy = incDirs;
    incDirsCopy << QFileInfo(file).canonicalPath();
    QStringList includes;
    while ((pos = r2.indexIn(data, pos + 1)) != -1)
        includes << r2.cap(1);

    includes.removeDuplicates();

    // search for the files to include
    foreach (QString inc, includes)
    {
        bool found = false;
        foreach (QString dir, incDirsCopy)
        {
            if (!QFile::exists(dir + "/" + inc))
                continue;

            QDateTime mod2 = newestDependency(inc, incDirs, checkedFiles, level + 1);
            if (mod2 > mod)
                mod = mod2;

            found = true;
        }

        if (found)
            continue;

        t.iout(file, data2.left(data2.indexOf(QRegExp("#include [<\"]" + inc + "[\">]"))).count('\n'), "Cannot find [" + inc + "]");
    }

    return mod;
}

QString MW::cleanPath(const QString& path)
{
    QFileInfo f(path);
    return f.canonicalFilePath();
}

void MW::clean()
{
    ui->cout->clear();

    if (QMessageBox::question(NULL, "Clean directory?", "Really delete all files in [" + m_BuildDir + "]?", QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
        return;

    QDir d(m_BuildDir);
    cout("Removing files from [" + m_BuildDir + "]");
    foreach (QString file, d.entryList(QDir::Files | QDir::NoDotAndDotDot))
    {
        cout("Removing " + file);
        d.remove(file);
    }
}

void MW::upload(bool fromThread)
{
    if (!fromThread)
    {
        if (t.isRunning())
            return;

        ui->cout->clear();
        ui->cout->append("Starting upload...");
        ui->issues->clearContents();
        ui->issues->setRowCount(0);
        reiterateSerialPorts();
        t.port = cport;
        t.job = "upload";
        t.mcu = agMCU->checkedAction()->data().toString();
        t.start();
        return;
    }

    QString target = m_BuildDir + "application.elf";
    if (!QFileInfo(target + ".hex").exists() ||
        QFileInfo(target + ".hex").isDir())
    {
        t.cout("Hex file doesn't exist! Did you compile your program?");
        return;
    }

    QProcess p;
    QString port = "-P " + t.port;

    if (port == "-P Auto")
        port = "";

    QString cmd = profile->value("uploader").toString() + " -U flash:w:" + target + ".hex:i " + port + " -p " + t.mcu;
    t.cout(cmd);
    p.start(cmd, QProcess::ReadOnly);

    while (p.state() != QProcess::NotRunning && !p.waitForFinished(500))
    {
        t.cout(QString::fromUtf8(p.readAllStandardError()), TO_NONL);
       // t.cout(QString::fromUtf8(p.readAllStandardOutput()), TO_NONL);
    }

    t.cout(QString::fromUtf8(p.readAllStandardError()));
    t.cout(QString::fromUtf8(p.readAllStandardOutput()));
    t.cout("Exit code: " + QString::number(p.exitCode()));

    if (!p.exitCode())
        t.cout("Upload successful", TO_STATUS);
}

void MW::recompile()
{
    clean();
    compile();
}

bool MW::hasUnsavedFiles()
{
    for (int i = 0; i < ui->files->count(); i++)
        if (ui->files->tabText(i).endsWith(" *"))
            return true;

    return false;
}

void MW::compile(bool fromThread)
{
    if (hasUnsavedFiles())
        if (QMessageBox::question(NULL, "Unsaved files", "There are unsaved files in your project. Do you want to save them before compilation?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
            saveAllFiles();

    if (!fromThread)
    {
        if (t.isRunning())
            return;

        ui->cout->clear();

        if (!profile)
        {
            ui->cout->append("Please select a build profile!");
            return;
        }

        ui->issues->clearContents();
        ui->issues->setRowCount(0);
        clearAnnotations();
        t.job = "compile";
        t.mcu = agMCU->checkedAction()->data().toString();
        t.clock = agCLK->checkedAction()->data().toString();

        t.start();
        return;
    }

    if (!QFileInfo(m_BuildDir).exists())
        QDir(projectDir.path()).mkpath(m_BuildDir);

    QString arduPath = "/home/emmeran/Downloads/arduino-1.0.1/";
    QDir adir(arduPath + "/hardware/arduino/cores/arduino");
    QStringList incDirs;
    incDirs
            << "/usr/lib/avr/include"
            << arduPath + "/hardware/tools/avr/lib/gcc/avr/4.3.2/include"
            << arduPath + "/hardware/arduino/variants/standard"
            << arduPath + "/hardware/tools/avr/lib/gcc/avr/4.3.2/include-fixed"
            << arduPath + "/hardware/arduino/cores/arduino"
            << projectDir.path();

    QMap< QTreeWidgetItem*, QString >::iterator it;
    QStringList libFiles;
    for (it = m_Libs.begin(); it != m_Libs.end(); it++)
        addDirRecursive(it.value(), libFiles, incDirs);

    QStringList buildTargets, coreTargets;

    QStringList sl = adir.entryList(QString("*.c,*.cpp,*.ino,*.pde").split(','));
    foreach (QString s, sl)
    {
        coreTargets.append(cleanPath(adir.path() + "/" + s));
        buildTargets.append(cleanPath(adir.path() + "/" + s));
    }

    foreach (QString f, m_Files)
    {
        if (f.endsWith(".cpp") ||
            f.endsWith(".c") ||
            f.endsWith(".ino") ||
            f.endsWith(".pde"))
            buildTargets.append(f);
    }

    foreach (QString f, libFiles)
    {
        if (f.endsWith(".cpp") ||
            f.endsWith(".c") ||
            f.endsWith(".ino") ||
            f.endsWith(".pde"))
            buildTargets.append(cleanPath(f));
    }

    buildTargets.removeDuplicates();
    QStringList dirtyTargets;

    // find dirty targets
    foreach (QString f, buildTargets)
    {
        QDateTime obj = QFileInfo(buildObject(f)).lastModified();

        if (!obj.isValid())
        {
            dirtyTargets.append(f);
            continue;
        }

        QStringList cf;
        if (obj > newestDependency(f, incDirs, cf))
        {
            t.cout(f + " is up to date.");
            continue;
        }

        dirtyTargets.append(f);
    }

    bool error = false;
    foreach (QString dt, dirtyTargets)
    {
        t.cout("Building " + dt);
        QProcess p;
        QString cmd;
        if (dt.endsWith(".c"))
            cmd = profile->value("compilerC").toString() + " -DF_CPU="+t.clock+" -mmcu=" + t.mcu + " " + dt + " -o " + buildObject(dt) + " -I" + incDirs.join(" -I");
        else
            cmd = profile->value("compilerCPP").toString() + " -DF_CPU="+t.clock+" -mmcu=" + t.mcu + " " + dt + " -o " + buildObject(dt) + " -I" + incDirs.join(" -I");
        t.cout(cmd);

        p.start(cmd, QProcess::ReadOnly);
        if (!p.waitForFinished(-1))
        {
            t.cout("fail");
            error = true;
        }

        else
        {
            t.cout("done");

            QString stderr = QString::fromUtf8(p.readAllStandardError());

            foreach (QString err, stderr.split('\n'))
            {
                QStringList tmp = err.split(':');

                if (tmp.size() < 3)
                    continue;

                QString file = tmp.takeFirst();

                if (file.startsWith("In file included from "))
                    continue;


                if (file.startsWith("   "))
                    continue;

                int line = tmp.takeFirst().toInt();
                t.iout(file, line, tmp.join(":"));
            }


            t.cout(stderr);
            t.cout(QString::fromUtf8(p.readAllStandardOutput()));
            t.cout("Exit code: " + QString::number(p.exitCode()));

            if (p.exitCode())
                error = true;
        }
    }

    t.cout(!error ? "Build successful" : "Build failed");

    if (error)
        return;

    error = false;
    // extract data
    foreach (QString obj, coreTargets)
    {
        QProcess p;
        QString cmd = profile->value("coreA").toString()+" " + m_BuildDir + "core.a " + buildObject(obj);
        p.start(cmd, QProcess::ReadOnly);
        p.waitForFinished(-1);

        t.cout(cmd);
        if (p.exitCode())
            error = true;
    }

    t.cout(!error ? "Core archive extraction successful" : "Core archive extraction failed");

    if (error)
        return;

    error = false;
    // link
    QString objects = "";
    QDateTime newest;
    foreach (QString obj, buildTargets)
    {
        QDateTime tmp = QFileInfo(buildObject(obj)).lastModified();

        if (!newest.isValid() || tmp > newest)
            newest = tmp;

        if (coreTargets.contains(obj))
            continue;

        objects += buildObject(obj) + " ";
    }

    QProcess p;
    QString target = m_BuildDir + "application.elf";

    if (QFileInfo(target).lastModified() > newest)
        t.cout("No link necessary");

    else
    {
        QString cmd = profile->value("linker").toString() + " -mmcu=" + t.mcu + " -o " + target + " " + objects + " " +m_BuildDir+"/core.a ";
        p.start(cmd, QProcess::ReadOnly);
        p.waitForFinished(-1);

        t.cout(cmd);
        t.cout(QString::fromUtf8(p.readAllStandardError()));
        t.cout(QString::fromUtf8(p.readAllStandardOutput()));
        t.cout("Exit code: " + QString::number(p.exitCode()));

        if (p.exitCode())
            error = true;

        t.cout(!error ? "Linking successful" : "Linking failed");

        if (error)
            return;
    }

    QString cmd = profile->value("objCopy").toString() + " -Oihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 " + target + " " + target + ".eep";
    p.start(cmd, QProcess::ReadOnly);
    p.waitForFinished(-1);

    t.cout(cmd);
    t.cout(QString::fromUtf8(p.readAllStandardError()));
    t.cout(QString::fromUtf8(p.readAllStandardOutput()));
    t.cout("Exit code: " + QString::number(p.exitCode()));

    cmd = profile->value("objCopy").toString() + " -Oihex -R .eeprom " + target + " " + target + ".hex";
    p.start(cmd, QProcess::ReadOnly);
    p.waitForFinished(-1);

    t.cout(cmd);
    t.cout(QString::fromUtf8(p.readAllStandardError()));
    t.cout(QString::fromUtf8(p.readAllStandardOutput()));
    t.cout("Exit code: " + QString::number(p.exitCode()));

    cmd = profile->value("objCopy").toString() + " -Obinary -R .eeprom " + target + " " + target + ".bin";
    p.start(cmd, QProcess::ReadOnly);
    p.waitForFinished(-1);

    t.cout(cmd);
    t.cout(QString::fromUtf8(p.readAllStandardError()));
    t.cout(QString::fromUtf8(p.readAllStandardOutput()));
    t.cout("Exit code: " + QString::number(p.exitCode()));

    if (p.exitCode() == 0)
    {
        t.cout("Build completed. " + QString::number(QFileInfo(target + ".bin").size()) + " bytes", TO_STATUS);
        t.cout("");

        p.start("avr-size " + target, QProcess::ReadOnly);
        p.waitForFinished(-1);

        t.cout(p.readAllStandardOutput());
    }
}

void MW::menuTrigger(QAction *a)
{
    if (a == ui->actionOpen_Project)
    {
        QString efpFile = QFileDialog::getOpenFileName(NULL, "Select project file", QApplication::applicationDirPath(), "*.efp");

        if (efpFile == "")
            return;

        openProject(efpFile);
    }

    else if (a == ui->actionQuit)
        close();

    else if (a == ui->actionNew_Project)
    {
        QString efp = QFileDialog::getSaveFileName(NULL, "Select project location", "Ethernet Flasher Project (*.efp)");

        if (efp == "")
            return;

        QFile f(efp);
        f.open(QFile::WriteOnly);
        f.close();

        openProject(efp);
    }

    else if (a == ui->actionSave_Project)
    {
        saveProject();
    }

    else if (a == ui->actionClose_Project)
    {
        closeProject();
    }

    else if (a == ui->actionSaveEverything)
    {
        saveEverything();
    }

    else if (a == ui->actionSave_File)
    {
        if (ui->files->currentIndex() > 0)
            saveFile(ui->files->currentIndex());
    }

    else if (a == ui->actionClose_File)
    {
        if (ui->files->currentIndex() > 0)
            closeFile(ui->files->currentIndex());
    }

    else if (a == ui->actionBuild)
        compile();

    else if (a == ui->actionRe_Build)
        recompile();

    else if (a == ui->actionUpload_2)
        upload();

    else if (a == ui->actionClean)
        clean();

}

void MW::saveEverything()
{
    saveProject();
    saveAllFiles();
}

void MW::saveAllFiles()
{
    for (int i = 0; i < ui->files->count(); i++)
    {
        if (ui->files->tabText(i).endsWith(" *"))
            saveFile(i);
    }
}

void MW::saveFile(int index)
{
    QsciScintilla *w = (QsciScintilla*)ui->files->widget(index);

    QFile f(w->property("file").toString());
    if (!f.open(QFile::WriteOnly | QFile::Truncate))
    {
        cout("Cannot open file for writing!");
        return;
    }

    f.write(w->text().toUtf8());
    f.close();

    w->setModified(false);

    ui->files->setTabText(index, w->property("file").toString().section('/', -1));
}

void MW::closeFile(int tabIndex)
{
    if (ui->files->tabText(tabIndex).endsWith(" *"))
        if (QMessageBox::question(NULL, "Closing unsaved file", "There are unsaved changes in this file, do you want to save before closing this file?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
            saveFile(tabIndex);

    ui->files->removeTab(tabIndex);
}

void MW::addDirRecursive(QString dir, QStringList &m_LibFiles, QStringList& m_LibIncDirs)
{
    if (dir.endsWith("examples"))
        return;

    QDir d(dir);
    m_LibIncDirs.append(dir);

    QStringList files = d.entryList(QString("*.cpp,*.c,*.ino,*.pde").split(','), QDir::Files | QDir::Dirs | QDir::AllDirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    foreach (QString file, files)
    {
        if (QFileInfo(dir + "/" + file).isDir())
            addDirRecursive(dir + "/" + file, m_LibFiles, m_LibIncDirs);

        else
            m_LibFiles.append(dir + "/" + file);
    }
}

void MW::addFile(QString file)
{
    file = QFileInfo(file).canonicalFilePath();

    foreach (QString f, m_Files)
        if (f == file)
            return;

    m_Files.append(file);
    m_Files.sort();
    QTreeWidgetItem* i = new QTreeWidgetItem(QStringList(file.section('/', -1)));

    i->setData(0, Qt::UserRole, "file");
    i->setData(0, Qt::UserRole + 1, file);

    tiFiles->addChild(i);
    tiFiles->sortChildren(0, Qt::AscendingOrder);
}

void MW::saveProject()
{
    if (project == NULL)
        return;

    QStringList files;

    foreach (QString file, m_Files)
    {
        if (file.startsWith(projectDir.canonicalPath()))
            files.append(file.right(file.size() - projectDir.canonicalPath().size() - 1));

        else
            files.append(file);
    }

    project->setValue("CPPFILES", files);


    QStringList libs;
    QMap< QTreeWidgetItem*, QString >::iterator it;
    for (it = m_Libs.begin(); it != m_Libs.end(); it++)
        libs << it.value();

    project->setValue("LIBS", libs);
    project->setValue("BUILDDIR", m_BuildDir);
}

void MW::openProject(QString efpFile)
{
    if (!closeProject())
        return;

    QFile qf(efpFile);
    if (!qf.exists())
        return;

    projectModified = false;
    QStringList tmp = sets->value("recentProjects").toStringList();
    tmp.removeOne(efpFile);
    tmp.removeDuplicates();

    while (tmp.size() > 4)
        tmp.removeFirst();

    tmp.append(efpFile);
    sets->setValue("recentProjects", tmp);

    updateHome();

    m_Files.clear();
    project = new QSettings(efpFile, QSettings::IniFormat);
    projectDir = QFileInfo(efpFile).canonicalPath();

    QTreeWidgetItem *root = new QTreeWidgetItem(QStringList(efpFile.section('/', -1)));
    ui->filetree->insertTopLevelItem(0, root);

    m_BuildDir = project->value("BUILDDIR", QFileInfo(efpFile).canonicalPath() + "/build/").toString();

    if (!m_BuildDir.startsWith('/'))
    {
        QString bd = QFileInfo(efpFile).canonicalPath() + "/" + m_BuildDir;

        QString bd2 = QFileInfo(bd).canonicalPath();

        if (!bd2.startsWith(QFileInfo(efpFile).canonicalPath() + "/"))
        {
            bd = QFileInfo(efpFile).canonicalPath() + "/build/";
            m_BuildDir = bd;
        }

        else
            m_BuildDir = bd2;
    }

    else if (!m_BuildDir.startsWith(QFileInfo(efpFile).canonicalPath() + "/"))
        m_BuildDir = QFileInfo(efpFile).canonicalPath() + "/build/";

    QStringList files = project->value("CPPFILES").toStringList() + project->value("CFILES").toStringList();

    QDir::setCurrent(projectDir.canonicalPath());
    tiFiles = new QTreeWidgetItem(QStringList("Files"));
    tiFiles->setData(0, Qt::UserRole, "files");
    root->addChild(tiFiles);
    tiFiles->setExpanded(true);
    files.sort();
    foreach (QString file, files)
        addFile(file);

    QStringList libs = project->value("LIBS").toStringList();
    reloadProfile();

    twLibs = new QTreeWidgetItem(QStringList("Libs"));
    twLibs->setData(0, Qt::UserRole, "libs");
    root->addChild(twLibs);
    twLibs->setExpanded(true);

    foreach (QString lib, libs)
        addLib(lib);

    root->setExpanded(true);
}

bool MW::showProjectSaveQuestion()
{
    int r = QMessageBox::question(NULL, "Project modified", "Do you want to save your project?", QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);

    if (r == QMessageBox::Yes)
        saveProject();

    else if (r == QMessageBox::Cancel)
        return false;

    return true;
}

bool MW::showFilesSaveQuestion()
{
    int r = QMessageBox::question(NULL, "Files modified", "Some files in your project haven't been saved. Do you want to save them now?", QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);

    if (r == QMessageBox::Yes)
        saveAllFiles();

    else if (r == QMessageBox::Cancel)
        return false;

    return true;
}

bool MW::closeProject()
{
    if (!project)
        return true;

    if (hasUnsavedFiles())
        if (!showFilesSaveQuestion())
            return false;

    if (projectModified)
        if (!showProjectSaveQuestion())
            return false;

    clearAnnotations();
    m_Files.clear();
    ui->filetree->clear();

    while (ui->files->count() > 1)
        ui->files->removeTab(1);


    if (project)
    {
        delete project;
        project = NULL;
    }

    m_BuildDir = "./";
    projectDir = "./";


    tiFiles = NULL;
    twLibs = NULL;
    m_Libs.clear();
    return true;
}

void MW::openFile(QTreeWidgetItem *it, int)
{
    if (it->data(0, Qt::UserRole).toString() != "file" && it->data(0, Qt::UserRole).toString() != "libfile")
        return;
    openFile(it->data(0, Qt::UserRole + 1).toString());
}

void MW::openFile(const QString &file)
{
    int i;
    for (i = 0; i < ui->files->count(); i++)
    if (ui->files->widget(i)->property("file") == file)
    {
        ui->files->setCurrentIndex(i);
        return;
    }

    QFile f(file);

    if (!f.open(QFile::ReadWrite))
    {
        QMessageBox::warning(NULL, "File open failed", "Could not open file [" + file + "]");
        return;
    }

    QsciScintilla *widget = new QsciScintilla(ui->files);
    widget->setProperty("file", file);
    widget->setText(QString::fromUtf8(f.readAll()));
    widget->setFont(QFont("Monospace", 9));
    widget->setAutoIndent(true);
    widget->setBackspaceUnindents(true);
    widget->setBraceMatching(QsciScintilla::SloppyBraceMatch);
    widget->setEdgeMode(QsciScintilla::EdgeLine);
    widget->setFolding(QsciScintilla::CircledTreeFoldStyle);
    int w = ceil(log10(widget->lines()));
    widget->setIndentationGuides(false);
    widget->setLexer(&lexer);
    widget->setIndentationWidth(4);
    widget->setMarginWidth(1, QString(w + 1, '0'));
    widget->setIndentationsUseTabs(true);
    widget->setMarginLineNumbers(1, true);
    widget->setAutoCompletionSource(QsciScintilla::AcsAll);
    widget->setAutoCompletionThreshold(2);

    connect(widget, SIGNAL(modificationChanged(bool)), this, SLOT(fileModified(bool)));

    if (!f.isWritable())
        widget->setReadOnly(true);

    ui->files->addTab(widget, file.section('/', -1));
    ui->files->setCurrentWidget(widget);
}

void MW::fileModified(bool mState)
{
    QsciScintilla* w = (QsciScintilla*)sender();

    int index = ui->files->indexOf(w);
    ui->files->setTabText(index, w->property("file").toString().section('/', -1) + (mState ? " *" : ""));
}

void MW::nc()
{
    QTcpSocket* so = s->nextPendingConnection();

    connect(so, SIGNAL(readyRead()), this, SLOT(data()));
    ui->out->append("New Arduino found!");
}

#include <QProcess>
void MW::data()
{
    QTcpSocket* so = (QTcpSocket*)QObject::sender();
    QByteArray cs = so->readAll();
    ui->out->append("Client said: " + cs.trimmed());

    if (cs.left(16) == "GET firmware.bin")
    {
        /*if (ui->in->text() != "")
        {
            QProcess::execute("avr-objcopy -O binary " + ui->in->text() + " firmware.bin");
            if (ui->en->isChecked())
            {
                f.open(QFile::ReadOnly);
                QByteArray b = f.readAll();
                f.close();

                so->putChar((unsigned char)(b.size() >> 8));
                so->putChar((unsigned char)(b.size() & 0xFF));
                so->write(b);
                ui->out->append("Sending firmware: " + QString::number(b.size() + 2) + " bytes.");
            }

            else
            {
                so->write(QByteArray::fromHex("0000"));
                ui->out->append("Firmware upgrades not enabled, cancelling firmware update.");
            }
        }

        else
        {
            so->write(QByteArray::fromHex("0000"));
            ui->out->append("No file selected, cancelling firmware update.");
        }*/

            QProcess::execute("avr-objcopy -O binary " + inFile + " firmware.bin");
            if (ui->en->isChecked())
            {
                f.open(QFile::ReadOnly);
                QByteArray b = f.readAll();
                f.close();

                so->putChar((unsigned char)(b.size() >> 8));
                so->putChar((unsigned char)(b.size() & 0xFF));
                so->write(b);
                ui->out->append("Sending firmware: " + QString::number(b.size() + 2) + " bytes.");
            }

            else
            {
                so->write(QByteArray::fromHex("0000"));
                ui->out->append("Firmware upgrades not enabled, cancelling firmware update.");
            }
    }
}

void MW::on_sel_clicked()
{
    QString file = QFileDialog::getOpenFileName(NULL, "Select firmware file (*.elf)");

    //ui->in->setText(file);

    if (file == "")
        return;

    ui->out->append("Selected firmware file: " + file);
}

void MW::on_en_stateChanged(int state)
{
    if (state)
        ui->out->append("Firmware upgrades enabled.");

    else
        ui->out->append("Firmware upgrades disabled.");
}

void MW::addLib(QString libDir)
{
    QTreeWidgetItem *wi = new QTreeWidgetItem(QStringList(libDir.section('/', -1)));
    wi->setData(0, Qt::UserRole, "lib");
    m_Libs[wi] = libDir;
    twLibs->addChild(wi);
    refreshLib(wi);
}

void MW::remLib(QTreeWidgetItem* i)
{
    twLibs->removeChild(i);
    m_Libs.remove(i);
}

void MW::addDirToTree(QTreeWidgetItem* i, QString path)
{
    QDir d(path);

    foreach (QString f, d.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files, QDir::Name))
    {
        QTreeWidgetItem* c = new QTreeWidgetItem(QStringList(f));
        i->addChild(c);

        if (QFileInfo(path + "/" + f).isDir())
        {
            c->setData(0, Qt::UserRole, "dir");
            addDirToTree(c, path + "/" + f);
        }

        else
        {
            c->setData(0, Qt::UserRole, "libfile");
            c->setData(0, Qt::UserRole + 1, path + "/" + f);
        }

        c->setExpanded(false);
    }
}

void MW::refreshLib(QTreeWidgetItem* i)
{
    foreach (QTreeWidgetItem* ch, i->takeChildren())
        delete ch;

    addDirToTree(i, m_Libs[i]);
}

void MW::on_filetree_customContextMenuRequested(QPoint pos)
{
    QTreeWidgetItem *wi = ui->filetree->itemAt(pos);
    QString type = wi->data(0, Qt::UserRole).toString();

    if (type == "libs")
    {
        QAction aAddLib("Add library...", NULL);
        QAction* ret = QMenu::exec(QList<QAction* >() << &aAddLib, ui->filetree->mapToGlobal(pos));

        if (ret == &aAddLib)
        {
            QString lib = QFileDialog::getExistingDirectory();

            if (lib == "")
                return;

            addLib(lib);
            projectModified = true;
        }
        return;
    }

    if (type == "files")
    {
        QAction aAddFiles("Add files...", NULL);
        QAction* ret = QMenu::exec(QList<QAction* >() << &aAddFiles, ui->filetree->mapToGlobal(pos));

        if (ret == &aAddFiles)
        {
            QStringList files = QFileDialog::getOpenFileNames();

            if (!files.size())
                return;

            projectModified = true;
            foreach (QString file, files)
                addFile(file);
        }
        return;
    }

    if (type == "file")
    {
        QAction aRemFile("Remove file...", NULL);
        QAction* ret = QMenu::exec(QList<QAction* >() << &aRemFile, ui->filetree->mapToGlobal(pos));

        if (ret == &aRemFile)
        {
            if (!m_Files.removeOne(wi->data(0, Qt::UserRole + 1).toString()))
            {
                QMessageBox::warning(NULL, "Failed to remove item", "Wasn't able to find item in file list");
                return;
            }
            projectModified = true;
            delete wi;
        }
        return;
    }

    if (type == "lib")
    {
        QAction aRefLib("Refresh", NULL);
        QAction aRemLib("Remove", NULL);
        QAction* ret = QMenu::exec(QList<QAction* >() << &aRefLib << &aRemLib, ui->filetree->mapToGlobal(pos));

        if (ret == &aRefLib)
        {
            projectModified = true;
            refreshLib(wi);
        }
        else if (ret == &aRemLib)
        {
            projectModified = true;
            remLib(wi);
        }
    }
}

QString MW::getEBPName(QString pname)
{
    return QApplication::instance()->applicationDirPath() + "/profiles/" + pname + ".ebp";
}

void MW::reloadProfile()
{
    if (profile)
    {
        delete profile;
        profile = NULL;
    }

    if (QFile::exists(getEBPName(project->value("PROFILE").toString())))
        profile = new QSettings(getEBPName(project->value("PROFILE").toString()), QSettings::IniFormat);
}

void MW::selectedProfile()
{
    if (!project)
        return;

    project->setValue("PROFILE", ((BuildSettings*)sender())->selectedProfile());
    reloadProfile();
}

void MW::on_actionBuild_Settings_triggered()
{
    BuildSettings* s = new BuildSettings(project ? project->value("PROFILE").toString() : "", this);
    s->show();

    connect(s, SIGNAL(accepted()), this, SLOT(selectedProfile()));
}

void MW::on_files_tabCloseRequested(int index)
{
    if (index == 0)
        return;

    closeFile(index);
}

void MW::doJob(QString job)
{
    if (job == "compile")
        compile(true);

    else if (job == "upload")
        upload(true);
}

void MW::on_rprojs_linkActivated(QString link)
{
    openProject(link);
}

void MW::clearAnnotations()
{
    for (int i = 1; i < ui->files->count(); i++)
    {
        QsciScintilla *q = ((QsciScintilla*)ui->files->widget(i));
        q->clearAnnotations();
        q->recolor();
    }
}

void MW::on_issues_cellActivated(int row, int)
{
    QString file = ui->issues->item(row, 0)->data(Qt::UserRole).toString();
    int line = ui->issues->item(row, 1)->data(Qt::UserRole).toInt();

    openFile(file);

    QsciScintilla* q = (QsciScintilla*)ui->files->currentWidget();
    clearAnnotations();
    q->ensureLineVisible(line - 1);
    q->setCursorPosition(line - 1, 0);
    q->setAnnotationDisplay(QsciScintilla::AnnotationBoxed);
    q->annotate(line - 1, ui->issues->item(row, 2)->data(Qt::UserRole).toString(), QsciStyle::OriginalCase);
    q->setFocus();
}












































