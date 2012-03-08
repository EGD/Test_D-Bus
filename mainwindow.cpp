#include <QDebug>

#include "mainwindow.h"
#include "ui_mainwindow.h"

enum PlayStatus
{
    PSPlaying = 0,
    PSPaused,
    PSStopped
};

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

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    plVer(Ui::v1)
{
    setWindowTitle(QString("Test D-Bus"));

    qDBusRegisterMetaType<PlayerStatus>();

    ui->setupUi(this);
    ui->comboBox->clear();
    ui->comboBox->addItems(getPlayersList());

    playerName = ui->comboBox->count() ? ui->comboBox->itemText(0) : "clementine";

    m_player = new QDBusInterface("org.mpris." + playerName, "/Player",
                            "org.freedesktop.MediaPlayer", QDBusConnection::sessionBus());
//    m_player = new QDBusInterface("org.mpris.MediaPlayer2." + playerName, "/org/mpris/MediaPlayer2",
//                                    "org.freedesktop.MediaPlayer", QDBusConnection::sessionBus());

    if (m_player->lastError().type() != QDBusError::NoError) {
        qDebug() << QDBusError::errorString(m_player->lastError().type());
        return;
    }

    QDBusReply<QVariantMap> m_metadata = m_player->call("GetMetadata");
    QVariantMap trackInfo = m_metadata.value();
    QList<QString> keys = trackInfo.keys();
    foreach (QString key, keys) {
        ui->Memo->appendPlainText('%'+key);
    }

    connectToBus();
}

MainWindow::~MainWindow()
{
    disconnectToBus();

    delete m_player;
    delete ui;
}

void MainWindow::connectToBus()
{
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
}

void MainWindow::disconnectToBus()
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

        ui->plainTextEdit->appendPlainText(formatMetadata(trackInfo, ui->lineEdit->text()));
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

    disconnectToBus();
    connectToBus();
}

void MainWindow::setPlayerVersion(bool bVer_2 = false)
{
    bVer_2 ? plVer = Ui::v2 : plVer = Ui::v1;

    ui->comboBox->clear();
    ui->comboBox->addItems(getPlayersList());
}

QString MainWindow::secToTime(int secs) {
    int min = 0;
    int sec = secs;
    while (sec > 60) {
        ++min;
        sec -= 60;
    }

    return QString("%1:%2").arg(min).arg(sec,2,10,QChar('0'));
}

QString MainWindow::formatMetadata(QVariantMap &trackInfo, const QString &format) {
    QRegExp rx("%(\\w+-?\\w+)");

    QString outText;
    int srPos = 0;
    int cpPos = 0;

    if (format.contains("time")) {
        trackInfo["time"] = secToTime(trackInfo["time"].toInt());
    }

    while ( (srPos = rx.indexIn(format,srPos)) != -1 ) {
        outText.append(format.mid(cpPos,srPos - cpPos)).append(trackInfo.contains(rx.cap(1)) ? trackInfo[rx.cap(1)].toString() : " " );
        cpPos = srPos + rx.matchedLength();
        srPos += rx.matchedLength();
    }

    outText.append(format.mid(cpPos));

    return outText;
}

void MainWindow::onTrackChange(QVariantMap trackInfo)
{
    ui->plainTextEdit->appendPlainText(formatMetadata(trackInfo, ui->lineEdit->text()));
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
