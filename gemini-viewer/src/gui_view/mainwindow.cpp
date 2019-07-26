#include "mainwindow.h"
#include "pub_client.h"
#include <QMenuBar>
#include <QVBoxLayout>
#include <QMessageBox>

#include <QTableView>
//#include <QTreeView>
#include <QHeaderView>
#include <QFileInfo>
#include "event_model.h"
#include "open_fpga_dlg.h"
#include "address_map.h"

MainWindow::MainWindow()
    : QMainWindow(0)
{
    QString title = QString("Gemini Publish Events");
    setWindowTitle(title);
    setMinimumSize(QSize(400,400));

    // FILE menu
    QMenu * fileMenu = menuBar()->addMenu(tr("&File"));

    QAction * openAct = new QAction(tr("&Open FPGA..."), this);
    fileMenu->addAction(openAct);
    connect(openAct, SIGNAL(triggered())
            , this, SLOT(onOpenFPGA_fromMenu()));

    // HELP menu
    QMenu * helpMenu = menuBar()->addMenu(tr("&Help"));

    QAction * aboutAct = new QAction(tr("&About..."), this);
    helpMenu->addAction(aboutAct);
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(helpAbout()));

    // Window contents go in a single frame
    QFrame *topFrame = new QFrame();
    setCentralWidget(topFrame);

    // Frame lays items out vertically
    QVBoxLayout *mainLayout = new QVBoxLayout;
    topFrame->setLayout(mainLayout);

    // Text box for display of random output
    //m_text_edit = new QTextEdit();
    //m_text_edit->setReadOnly(true);
    //m_text_edit->setUndoRedoEnabled(false);
    //m_text_edit->setMinimumHeight(100);
    //m_text_edit->setDocumentTitle("Output");
    //m_text_edit->document()->setMaximumBlockCount(100); // limit length
    //mainLayout->addWidget(m_text_edit);

    // View of event sources
    m_eventmodel = new Event_model(this);
    QTableView *tv = new QTableView(this);
    tv->setModel(m_eventmodel);
    tv->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tv->horizontalHeader()->setStretchLastSection(true);
    connect(tv, &QTableView::clicked
            , this, &MainWindow::onOpenFPGA_fromModel);
    //tv->horizontalHeader()->setSortIndicator(0,Qt::AscendingOrder);
    //tv->horizontalHeader()->setSortIndicatorShown(true);
    mainLayout->addWidget(tv);

    // Start listening for publish packets
    m_pubclient = new Pub_client(); // listening on default port
    connect(m_pubclient, &Pub_client::ready_read
                    , m_eventmodel, &Event_model::recv_data);
}

MainWindow::~MainWindow()
{
    qDebug() << "MainWindow::~MainWindow";
    disconnect(m_pubclient, &Pub_client::ready_read
            , m_eventmodel, &Event_model::recv_data);
    delete m_pubclient;

    // Call destructors on any open FPGA windows
    for(auto win: m_fpga_windows)
    {
        win->hide();
        delete win; // deleteLater doesn't work: no "later" after app closes.
    }

    // TODO
}

// Called back when our Event_model m_eventmodel gets clicked on
void MainWindow::onOpenFPGA_fromModel(const QModelIndex &index)
{
    // First check if the click was to clear counters
    bool complete = m_eventmodel->reset(index.row(), index.column());
    if(complete)
        return;

    // Otherwise the click  means to open an FPGA host
    QHostAddress host = m_eventmodel->getHost(index.row());
    // The FPGA connection port will be one less than the broadcast port
    uint16_t port = m_eventmodel->getPort(index.row()) - 1;

    qDebug() << "";
    qDebug() << "Opening host" << host.toString() << "port" << port;
    openFPGA(host, port, "");
}

void MainWindow::onOpenFPGA_fromMenu()
{
    QHostAddress host;
    host.setAddress("127.0.0.1");
    uint16_t port = 30000;
    openFPGA(host, port, "");
}

void MainWindow::openFPGA(QHostAddress host, uint16_t port, QString file)
{
    Open_fpga_dlg * open_fpga_dlg = new Open_fpga_dlg(host, port, file);
    QPoint posn = this->pos();
    open_fpga_dlg->move(posn.x()+20, posn.y()+100);
    int rv = open_fpga_dlg->exec();
    // Do nothing if user cancelled or gave invalid data
    if((rv != QDialog::Accepted) || (! open_fpga_dlg->isValid()))
    {
        delete open_fpga_dlg;
        if(!open_fpga_dlg->isValid())
        {
            qDebug() << "MainWindow::openFPGA Invalid host/port/file";
        }
        return;
    }

    // Retrieve user entries from the dialog
    host = open_fpga_dlg->get_host();
    port = open_fpga_dlg->get_port();
    file = open_fpga_dlg->get_filename();
    delete open_fpga_dlg;

    // The filename has to exist, be a file, and be readable
    QFileInfo f_inf(file);
    if(!f_inf.exists() || !f_inf.isFile() || !f_inf.isReadable())
    {
        if (!f_inf.exists())
            qDebug() << "MainWindow::openFPGA() No File:" << file;
        if (!f_inf.isFile())
            qDebug() << "MainWindow::openFPGA() Not a File:" << file;
        if (!f_inf.isReadable())
            qDebug() << "MainWindow::openFPGA() Not readable:" << file;

        QMessageBox msg;
        msg.setText("Can't open file " + file);
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("FPGA config filename error");
        msg.exec();
        return;
    }

    // If we have already a window to the FPGA, just raise it to top
    for(auto fpgawin: m_fpga_windows)
    {
        if(fpgawin->isSameFPGA(host, port))
        {
            fpgawin->raise();
            return;
        }
    }

    // Parse the address map for this FPGA
    std::unique_ptr<Address_map> am
            = std::make_unique<Address_map>(Address_map(file));
    am->load();
    if(!am->isLoadedOk())
    {
        // no need for explanatory dialog - Address Map will have done that
        // 'am' auto-deleted as pointer goes out of scope
        return;
    }

    // open a new FPGA window
    qDebug().nospace().noquote() << "open FPGA window to " << host.toString()
                                 << ":" << port << ", fpgamap='"
                                 << file << "'";
    Fpga_window * fpga_win = new Fpga_window(host, port, file, std::move(am));
    connect(fpga_win, &Fpga_window::closed, this, &MainWindow::onFpgaWinClose);
    m_fpga_windows.push_back(fpga_win);
    //fpga_win->show(); // Window will show itself if CNX to FPGA succeeds
}

void MainWindow::onFpgaWinClose(Fpga_window *win)
{
    for(auto it=m_fpga_windows.begin(); it!=m_fpga_windows.end(); ++it)
    {
        Fpga_window *pt = *it;
        if(pt == win)
        {
            m_fpga_windows.erase(it);
            disconnect(win, &Fpga_window::closed
                    , this, &MainWindow::onFpgaWinClose);
            //qDebug().noquote() << "DeleteLater called on" << pt->getId();
            pt->deleteLater();

            break;
        }
    }
}

// Override closeEvent so we can emit signal that we're closed
void MainWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    emit done(); 
}

void MainWindow::helpAbout()
{
    QMessageBox msg;
    msg.setWindowTitle("about Gemini Viewer");
    QString txt = QString("<h2>App</h2>"
            "<p>keith.bengston@csiro.au"
            "<p><i>Built: %1 %2</i>").arg(__DATE__).arg(__TIME__);

    msg.setText(txt);
    QPoint posn = this->pos();
    msg.move(posn.x()+40, posn.y()+100);
    msg.exec();
}


