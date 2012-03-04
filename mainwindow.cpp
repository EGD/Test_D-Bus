#include <QDebug>
#include "mainwindow.h"
#include "ui_mainwindow.h"

enum PlayStatus
{
    PSPlaying = 0,
    PSPaused,
    PSStopped
};

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    qDBusRegisterMetaType<PlayerStatus>();

    ui->setupUi(this);
    ui->comboBox->clear();
    ui->comboBox->addItems(MainWindow::getPlayersList());

    playerName = ui->comboBox->count() ? ui->comboBox->itemText(0) : "clementine";

    m_player = new QDBusInterface("org.mpris." + playerName, "/Player",
                            "org.freedesktop.MediaPlayer", QDBusConnection::sessionBus());
//    m_player = new QDBusInterface("org.mpris.MediaPlayer2." + playerName, "/org/mpris/MediaPlayer2",
//                                    "org.freedesktop.MediaPlayer", QDBusConnection::sessionBus());

    if (m_player->lastError().type() != QDBusError::NoError) {
        qDebug() << QDBusError::errorString(m_player->lastError().type());
        return;
    }

    QDBusConnection::sessionBus().connect(
                "org.mpris." + playerName,
                "/Player",
                "org.freedesktop.MediaPlayer",
                "TrackChange",
                "a{sv}",
                this,
                SLOT(onTrackChange(QVariantMap)));

    QDBusConnection::sessionBus().connect(
                "org.mpris." + playerName,
                "/Player",
                "org.freedesktop.MediaPlayer",
                "StatusChange",
                "(iiii)",
                this,
                SLOT(onPlayerStatusChange (PlayerStatus)));

//    QDBusConnection::sessionBus().connect("org.mpris.MediaPlayer2." + playerName,
//                                          "/org/mpris/MediaPlayer2",
//                                          "org.freedesktop.DBus.Properties",
//                                          "PropertiesChanged",
//                                          this,
//                                          SLOT(onPropertyChange(QDBusMessage)));


    QDBusReply<QVariantMap> m_metadata = m_player->call("GetMetadata");
    QVariantMap trackInfo = m_metadata.value();
    QList<QString> keys = trackInfo.keys();
    foreach (QString key, keys) {
        ui->Memo->appendPlainText('%'+key);
    }
}

MainWindow::~MainWindow()
{
    QDBusConnection::sessionBus().disconnect("org.mpris." + playerName,
                                        "/Player",
                                        "org.freedesktop.MediaPlayer",
                                        "StatusChange",
                                        "(iiii)",
                                        this,
                                        SLOT(onPlayerStatusChange(PlayerStatus)));
    QDBusConnection::sessionBus().disconnect("org.mpris." + playerName,
                                        "/Player",
                                        "org.freedesktop.MediaPlayer",
                                        "TrackChange",
                                        "a{sv}",
                                        this,
                                        SLOT(onTrackChange(QVariantMap)));

//    QDBusConnection::sessionBus().disconnect ("org.mpris.MediaPlayer2." + playerName,
//                                        "/org/mpris/MediaPlayer2",
//                                        "org.freedesktop.DBus.Properties",
//                                        "PropertiesChanged",
//                                        this,
//                                        SLOT (onPropertyChange (QDBusMessage)));

    delete m_player;
    delete ui;
}

QDBusArgument &operator<< (QDBusArgument &arg, const PlayerStatus &ps)
{
    arg.beginStructure ();
    arg << ps.Play
        << ps.Random
        << ps.Repeat
        << ps.RepeatPlaylist;
    arg.endStructure ();
    return arg;
}

const QDBusArgument &operator>> (const QDBusArgument &arg, PlayerStatus &ps)
{
    arg.beginStructure();
    arg >> ps.Play
        >> ps.Random
        >> ps.Repeat
        >> ps.RepeatPlaylist;
    arg.endStructure();
    return arg;
}

QStringList MainWindow::getPlayersList()    // переделать. сделать через абстрактный класс, с переопределением метода
{
    switch (plVer) {
    case Ui::v1:
        return MainWindow::getPlayersList_MPRISv1();
        break;
    case Ui::v2:
        return MainWindow::getPlayersList_MPRISv2();
        break;
    default:
        return QStringList();
        break;
    }
}

QStringList MainWindow::getPlayersList_MPRISv1()
{
    QStringList services = QDBusConnection::sessionBus().interface()->registeredServiceNames().value().filter("org.mpris.");
    services.replaceInStrings(QRegExp("org.mpris.(MediaPlayer2.)?"),"");
    services.removeDuplicates();

    return services;
}

QStringList MainWindow::getPlayersList_MPRISv2()
{
    QStringList services = QDBusConnection::sessionBus().interface()->registeredServiceNames().value().filter("org.mpris.MediaPlayer2.");
    services.removeDuplicates();
    services.replaceInStrings("org.mpris.MediaPlayer2.","");

    return services;
}

void MainWindow::getMetadata()
{
    if (!m_player->isValid())
        {
            return;
        }

    QDBusReply<QVariantMap> m_metadata = m_player->call("GetMetadata");

    if ( m_metadata.isValid() )
    {
        QVariantMap trackInfo = m_metadata.value();
        QRegExp rx("%(\\w+-?\\w+)");

        QString format = ui->lineEdit->text();
        QString outText;
        int srPos = 0;
        int cpPos = 0;

        while ( (srPos = rx.indexIn(format,srPos)) != -1 ) {
            if ( trackInfo.contains(rx.cap(1)) ) {
                outText.append(format.mid(cpPos,srPos - cpPos) + trackInfo[rx.cap(1)].toString());
                cpPos = srPos + rx.matchedLength();
            } else {
                qDebug() << rx.cap(1);
            }
            srPos += rx.matchedLength();
        }

        outText.append(format.mid(cpPos));

        ui->plainTextEdit->appendPlainText(outText);
    }
}

void MainWindow::playerPlay()
{
    if (m_status.Play == PSPaused) {
        m_player->call(QString("Play"));
    } else {
        m_player->call(QString("Pause"));
    }
}

void MainWindow::playerStop()
{
    m_player->call(QString("Stop"));
}

void MainWindow::playerPrev()
{
    m_player->call(QString("Prev"));
}

void MainWindow::playerNext()
{
    m_player->call(QString("Next"));
}

void MainWindow::playerChange(const QString &Name)
{
    playerName = Name;
    delete m_player;
    m_player = new QDBusInterface("org.mpris." + playerName, "/Player",
                                  "org.freedesktop.MediaPlayer", QDBusConnection::sessionBus());
}

void MainWindow::setPlayerVersion(bool bVer_2 = false)
{
    bVer_2 ? plVer = Ui::v2 : plVer = Ui::v1;

    ui->comboBox->clear();
    ui->comboBox->addItems(MainWindow::getPlayersList());
}

void MainWindow::onTrackChange(QVariantMap trackInfo)
{
    QRegExp rx("%(\\w+-?\\w+)");

    QString format = ui->lineEdit->text();
    QString outText;
    int srPos = 0;
    int cpPos = 0;

    while ( (srPos = rx.indexIn(format,srPos)) != -1 ) {
        outText.append(format.mid(cpPos,srPos - cpPos)).append(trackInfo.contains(rx.cap(1)) ? trackInfo[rx.cap(1)].toString() : " " );
        cpPos = srPos + rx.matchedLength();
        srPos += rx.matchedLength();
    }

    outText.append(format.mid(cpPos));

    ui->plainTextEdit->appendPlainText(outText);
}

void MainWindow::onPlayerStatusChange(PlayerStatus status) {
    m_status.Play = status.Play;
    m_status.Random = status.Random;
    m_status.Repeat = status.Repeat;
    m_status.RepeatPlaylist = status.RepeatPlaylist;

    if (m_status.Play == PSPlaying) {
        ui->btnPlay->setText("||");
    } else {
        ui->btnPlay->setText(">");
    }
}

void MainWindow::onPropertyChange(QDBusMessage msg)
{
    QDBusArgument arg = msg.arguments().at(1).value<QDBusArgument>();
    const QVariantMap& map = qdbus_cast<QVariantMap>(arg);

    QVariant v = map.value("PlaybackStatus");
    if (v.isValid() && v.toString() == "Stopped")
    {
        return;
    }

    v = map.value("Metadata");
    if (v.isValid())
    {
        arg = v.value<QDBusArgument>();
        onTrackChange(qdbus_cast<QVariantMap>(arg));
    }
}
