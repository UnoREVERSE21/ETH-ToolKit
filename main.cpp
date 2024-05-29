#include <QtGui/QApplication>
#include "mw.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MW w(argc > 1 ? argv[1] : "");
    w.show();

    return a.exec();
}
