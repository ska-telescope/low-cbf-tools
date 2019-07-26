//#include <QtGui>   // QT 4
#include <QtWidgets> // QT 5
#include <QApplication>
#include <QDebug>
#include "mainwindow.h"

void dbgMsgHndlr(QtMsgType msgType, const QMessageLogContext &ctx
        ,const QString &msg);

int main(int argc, char * argv[])
{
    (void) qInstallMessageHandler(dbgMsgHndlr);
    qDebug() << "** Startup **";

    QApplication app(argc, argv);

    MainWindow mw;
    QObject::connect(&mw, SIGNAL(done()), &app, SLOT(quit()));
    mw.show();

    // run Qt event loop
    int rv =  app.exec();

    qDebug() << "*** Exit ***";
    return rv;
}

void dbgMsgHndlr(QtMsgType msgType
        , const QMessageLogContext &ctx
        , const QString& msg)
{
    QDateTime time = QDateTime::currentDateTime();
    QString timeString = time.toString("yyyy-MM-dd hh:mm:ss");

    FILE *logFile = fopen("log-file.txt", "a");
    switch(msgType)
    {
        case QtDebugMsg:
            fprintf(logFile, "%s DEBUG: %s\n", timeString.toLatin1().data()
                    , msg.toLatin1().data());
            break;
        case QtInfoMsg:
            fprintf(logFile, "%s INFO: %s\n", timeString.toLatin1().data()
                    , msg.toLatin1().data());
            break;
        case QtWarningMsg:
            fprintf(logFile, "%s WARNING: %s\n", timeString.toLatin1().data()
                    , msg.toLatin1().data());
            break;
        case QtCriticalMsg:
            fprintf(logFile, "%s CRITICAL: %s\n", timeString.toLatin1().data()
                    , msg.toLatin1().data());
            break;
        case QtFatalMsg:
            fprintf(logFile, "%s FATAL: (line %d, '%s') %s\n"
                    , timeString.toLatin1().data()
                    , ctx.line
                    , ctx.file
                    , msg.toLatin1().data());
            abort();
            break;
    }
    fclose(logFile);
}
