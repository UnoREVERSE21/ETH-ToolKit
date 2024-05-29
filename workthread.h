#ifndef WORKTHREAD_H
#define WORKTHREAD_H

#include <QThread>
class MW;

#include <QMetaType>

enum toutFlags
{
    TO_NONE = 0,
    TO_STATUS = 1,
    TO_NONL = 2,
};

class WorkThread : public QThread
{
    Q_OBJECT
public:
    explicit WorkThread(MW *parent = 0);

    void run();
    QString job;
    QString port, mcu, clock;

    void cout(const QString &s, toutFlags statusbar = TO_NONE);
    void iout(const QString &file, int line, const QString &message);

private:
    MW *par;

signals:

    void textOut(QString, toutFlags);
    void issueOut(QString, int, QString);

public slots:

};

#endif // WORKTHREAD_H
