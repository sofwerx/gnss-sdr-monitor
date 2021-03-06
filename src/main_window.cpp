#include "main_window.h"
#include "ui_main_window.h"
#include "constellation_delegate.h"
#include "cn0_delegate.h"
#include "doppler_delegate.h"
#include "led_delegate.h"
#include "preferences_dialog.h"

#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkDatagram>

#include <QGeoLocation>
#include <QGeoCoordinate>

#include <QStringList>
#include <QDebug>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <sstream>
#include <iostream>

#define DEFAULT_PORT 1337

Main_Window::Main_Window(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Main_Window)
{
    ui->setupUi(this);


    /*
    // Map: QQuickWidget.
    map_widget = new QQuickWidget(this);
    map_widget->setSource(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    map_widget->setMinimumSize(512, 512);
    map_widget->setMaximumSize(512, 512);
    map_widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    ui->gridLayout->addWidget(map_widget, 1, 1, Qt::AlignCenter);
    */


    // QTimer.
    timer = new QTimer(this);
    timer->start(1000);


    // QMenuBar.
    ui->actionQuit->setIcon(QIcon::fromTheme("application-exit"));
    ui->actionQuit->setShortcuts(QKeySequence::Quit);

    ui->actionPreferences->setIcon(QIcon::fromTheme("preferences-desktop"));
    ui->actionPreferences->setShortcuts(QKeySequence::Preferences);

    connect(ui->actionQuit, &QAction::triggered, qApp, &QApplication::quit);
    connect(ui->actionPreferences, &QAction::triggered, this, &Main_Window::show_preferences);


    // QToolbar.
    start = ui->mainToolBar->addAction("Start");
    stop = ui->mainToolBar->addAction("Stop");
    clear = ui->mainToolBar->addAction("Clear");
    ui->mainToolBar->addSeparator();
    start->setEnabled(false);
    stop->setEnabled(true);
    clear->setEnabled(false);
    connect(start, &QAction::triggered, this, &Main_Window::toggle_capture);
    connect(stop, &QAction::triggered, this, &Main_Window::toggle_capture);
    connect(clear, &QAction::triggered, this, &Main_Window::clear_entries);


    // Model.
    model = new Channel_Table_Model();


    // QTableView.
    // Tie the model to the view.
    ui->tableView->setModel(model);
    ui->tableView->setShowGrid(false);
    ui->tableView->verticalHeader()->hide();
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setItemDelegateForColumn(4, new Constellation_Delegate());
    ui->tableView->setItemDelegateForColumn(5, new Cn0_Delegate());
    ui->tableView->setItemDelegateForColumn(6, new Doppler_Delegate());
    ui->tableView->setItemDelegateForColumn(8, new LED_Delegate());
    //ui->tableView->setAlternatingRowColors(true);
    //ui->tableView->setSelectionBehavior(QTableView::SelectRows);


    // Socket.
    socket = new QUdpSocket(this);


    // QStautsBar
    //statusBar()->showMessage("Listening on UDP port " + QString::number(port));


    // StyleSheet.
    //setStyleSheet( styleSheet().append("QWidget {background-color: #333333; color: #EFF0F1;}"));
    //setStyleSheet( styleSheet().append("QStatusBar{border-top: 1px outset grey;}"));
    //setStyleSheet( styleSheet().append("QToolTip { color: #fff; background-color: #000; border: none; }"));


    // Connect Signals & Slots.
    connect(socket, &QUdpSocket::readyRead, this, &Main_Window::add_entry);
    connect(qApp, &QApplication::aboutToQuit, this, &Main_Window::quit);


    // Load settings from last session.
    load_settings();
}

Main_Window::~Main_Window()
{
    delete ui;
}

void Main_Window::toggle_capture()
{
    if (start->isEnabled())
    {
        start->setEnabled(false);
        stop->setEnabled(true);
    }
    else
    {
        start->setEnabled(true);
        stop->setEnabled(false);
    }
}

void Main_Window::add_entry()
{
    while (socket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = socket->receiveDatagram();
        stocks = read_gnss_synchro(datagram.data().data(), datagram.data().size());

        /*
        statusBar()->showMessage("Listening on UDP port " + QString::number(port) + ": "
                                 + QString::number(datagram.data().size()) + " bytes");
                                 */

        if(stop->isEnabled())
        {
            model->populate_channels(stocks);
            clear->setEnabled(true);
        }
    }
}

void Main_Window::clear_entries()
{
    model->clear_channels();
    clear->setEnabled(false);
}

/*
void Main_Window::set_map_location()
{
    double lat = rand() % 50 + 1;
    double lon = rand() % 5 + 1;

    qDebug() << "lat :\t" << lat << "\t" << "lon :\t" << lon;

    QGeoCoordinate *coord = new QGeoCoordinate();
    coord->setLatitude(lat);
    coord->setLongitude(lon);
    map_widget->rootObject()->setProperty("center", QVariant::fromValue(coord));
}
*/

void Main_Window::quit()
{
    save_settings();
}

std::vector<Gnss_Synchro> Main_Window::read_gnss_synchro(char buff[], int bytes)
{
    try
    {
        std::string archive_data(&buff[0], bytes);
        std::istringstream archive_stream(archive_data);
        boost::archive::binary_iarchive archive(archive_stream);
        archive >> stocks;
    }
    catch (std::exception& e)
    {
        qDebug() << e.what();
    }

    return stocks;
}

void Main_Window::save_settings()
{
    settings.beginGroup("Main_Window");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
    settings.endGroup();

    settings.beginGroup("tableView");
    settings.beginWriteArray("column");
    for (int i = 0; i < model->get_columns(); i++) {
        settings.setArrayIndex(i);
        settings.setValue("width", ui->tableView->columnWidth(i));
    }
    settings.endArray();
    settings.endGroup();

    /*
    settings.beginGroup("Channel_Table_Model");
    settings.setValue("buffer_size", buffer_size);
    settings.endGroup();
    */

    /*
    settings.beginGroup("socket");
    settings.setValue("port", port);
    settings.endGroup();
    */

    qDebug() << "Settings Saved";
}

void Main_Window::load_settings()
{
    settings.beginGroup("Main_Window");
    move(settings.value("pos", QPoint(0, 0)).toPoint());
    resize(settings.value("size", QSize(1400, 600)).toSize());
    settings.endGroup();

    settings.beginGroup("tableView");
    settings.beginReadArray("column");
    for (int i = 0; i < model->get_columns(); i++) {
        settings.setArrayIndex(i);
        ui->tableView->setColumnWidth(i, settings.value("width", 100).toInt());
    }
    settings.endArray();
    settings.endGroup();

    set_port();

    qDebug() << "Settings Loaded";
}

void Main_Window::show_preferences()
{
    Preferences_Dialog *preferences = new Preferences_Dialog(this);
    connect(preferences, &Preferences_Dialog::accepted, model, &Channel_Table_Model::set_buffer_size);
    connect(preferences, &Preferences_Dialog::accepted, this, &Main_Window::set_port);
    preferences->exec();
}

void Main_Window::set_port()
{
    QSettings settings;
    settings.beginGroup("Preferences_Dialog");
    port = settings.value("port", DEFAULT_PORT).toInt();
    settings.endGroup();

    socket->disconnectFromHost();
    socket->bind(QHostAddress::LocalHost, port);
}
