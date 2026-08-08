#include "coresmoke.h"

#include <QApplication>
#include <QtTest/QtTest>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    CoreSmokeTest tc;
    return QTest::qExec(&tc, argc, argv);
}
