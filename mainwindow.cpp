#include <QDebug>

#include "mainwindow.h"
#include "ui_mainwindow.h"

enum PlayingStatus
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
    refreshPlayersList();

    playerName = ui->comboBox->count() ? ui->comboBox->itemText(0) : "clementine";

    QDBusConnection::sessionBus().connect("org.freedesktop.DBus",
                                          "/org/freedesktop/DBus",
                                          "org.freedesktop.DBus",
                                          "NameOwnerChanged",
                                          this,
                                          SLOT(onPlayersExistenceChanged(QString, QString, QString)));

    m_player = new QDBusInterface("org.mpris." + playerName, "/Player",
                            "org.freedesktop.MediaPlayer", QDBusConnection::sessionBus());
    //m_player = new QDBusInterface("org.mpris.MediaPlayer2." + playerName, "/org/mpris/MediaPlayer2",
    //                                "org.freedesktop.MediaPlayer", QDBusConnection::sessionBus());

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

    QDBusReply<PlayerStatus> status = m_player->call("GetStatus");
    onPlayerStatusChange(status);

    connectToBus();
}

MainWindow::~MainWindow()
{
    disconnectToBus();
    QDBusConnection::sessionBus().disconnect("org.freedesktop.DBus",
                                           "/org/freedesktop/DBus",
                                           "org.freedesktop.DBus",
                                           "NameOwnerChanged",
                                           this,
                                           SLOT(onPlayersExistenceChanged(QString, QString, QString)));

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
                SLOT(onPlayerStatusChange(PlayerStatus)));

    QDBusConnection::sessionBus().connect(
                "org.mpris.MediaPlayer2." + playerName,
                "/org/mpris/MediaPlayer2",
                "org.freedesktop.DBus.Properties",
                "PropertiesChanged",
                this,
                SLOT(onPropertyChange(QDBusMessage)));
}

void MainWindow::disconnectToBus()
{
    QDBusConnection::sessionBus().disconnect(
                "org.mpris." + playerName,
                "/Player",
                "org.freedesktop.MediaPlayer",
                "StatusChange",
                "(iiii)",
                this,
                SLOT(onPlayerStatusChange(PlayerStatus)));

    QDBusConnection::sessionBus().disconnect(
                "org.mpris." + playerName,
                "/Player",
                "org.freedesktop.MediaPlayer",
                "TrackChange",
                "a{sv}",
                this,
                SLOT(onTrackChange(QVariantMap)));

    QDBusConnection::sessionBus().disconnect (
                "org.mpris.MediaPlayer2." + playerName,
                "/org/mpris/MediaPlayer2",
                "org.freedesktop.DBus.Properties",
                "PropertiesChanged",
                this,
                SLOT (onPropertyChange(QDBusMessage)));
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
    QStringList ret_list;

    foreach (QString service, services) {
        if (service.startsWith("org.mpris.") && !service.startsWith("org.mpris.MediaPlayer2.")) {
            ret_list << service.replace("org.mpris.","");
        }
    }

    return ret_list;
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
    if (!m_player || !m_player->isValid())
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
    if (!m_player || !m_player->isValid())
    {
        return;
    }

    if (m_status.Play == PSPaused) {
        m_player->call(QString("Play"));
    } else {
        m_player->call(QString("Pause"));
    }
}

void MainWindow::playerStop()
{
    if (!m_player || !m_player->isValid())
    {
        return;
    }

    m_player->call(QString("Stop"));
}

void MainWindow::playerPrev()
{
    if (!m_player || !m_player->isValid())
    {
        return;
    }

    m_player->call(QString("Prev"));
}

void MainWindow::playerNext()
{
    if (!m_player || !m_player->isValid())
    {
        return;
    }

    m_player->call(QString("Next"));
}

void MainWindow::playerChange(const QString &Name)
{
    playerName = Name;

    if (m_player && m_player->isValid()) {
        disconnectToBus();
        delete m_player;
    }

    m_player = new QDBusInterface("org.mpris." + playerName, "/Player",
                                  "org.freedesktop.MediaPlayer", QDBusConnection::sessionBus());
    connectToBus();
}

void MainWindow::setPlayerVersion(bool bVer_2 = false)
{
    bVer_2 ? plVer = Ui::v2 : plVer = Ui::v1;

    refreshPlayersList();
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

void MainWindow::refreshPlayersList()
{
    ui->comboBox->clear();
    ui->comboBox->addItems(getPlayersList());
}

void MainWindow::onPlayersExistenceChanged(QString name, QString, QString newOwner)
{
    switch (plVer) {
    case MPRIS::v2:
        if (!name.startsWith("org.mpris.MediaPlayer2.")) {
            return;
        }
        break;
    case MPRIS::v1:
        if (!name.startsWith("org.mpris.") || name.startsWith("org.mpris.MediaPlayer2.")) {
            return;
        }
        break;
    default:
        break;
    }

    QString newPlayer = name.replace(QRegExp("org.mpris.(MediaPlayer2.)?"),"");

    if (!newOwner.isEmpty()) {
        qDebug() << "Available new player " + newPlayer + ".";
        qDebug() << "newOwner " + newOwner;

        if (playerName.compare(newPlayer)) {
            playerChange(newPlayer);
        }
    } else if (newOwner.isEmpty ()) {
        qDebug() << "Player " + newPlayer + " shutdown.";
        qDebug() << "newOwner " + newOwner;

        if (playerName.compare(newPlayer))
        {
            disconnectToBus();
            delete m_player;
        }
    }

    refreshPlayersList();
}

void MainWindow::onPropertyChange(QDBusMessage msg)
{
    qDebug() << "changed";
    QDBusArgument arg = msg.arguments ().at (1).value<QDBusArgument> ();
    const QVariantMap& map = qdbus_cast<QVariantMap> (arg);

    QVariant v = map.value ("Metadata");
    if (v.isValid ())
    {
        arg = v.value<QDBusArgument> ();
        QVariantMap m = qdbus_cast<QVariantMap> (arg);
        ui->plainTextEdit->appendPlainText("\n");
        foreach (QString key, m.keys()) {
            ui->plainTextEdit->appendPlainText(QString("%1: %2").arg(key,m[key].toString()));
        }

        QString str;
        if (m.contains("xesam:title"))
            str.append("title " + m["xesam:title"].toString() + " ");

        if (m.contains("xesam:artist"))
            str.append("artist " + m["xesam:artist"].toString() + " ");

        if (m.contains("xesam:album"))
            str.append("source " + m["xesam:album"].toString() + " ");

        if (m.contains("xesam:trackNumber"))
            str.append("track " + m["xesam:trackNumber"].toString() + " ");

        if (m.contains("mpris:length"))
            str.append(QString("length %1").arg(secToTime(m["mpris:length"].toLongLong() / 1000000)));
        ui->plainTextEdit->appendPlainText(str);
    }

    v = map.value("PlaybackStatus");
    if (v.isValid())
    {
        ui->plainTextEdit->appendPlainText("Status " + v.toString());
    }
}
