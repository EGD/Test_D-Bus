#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCore>
#include <QtDBus/QtDBus>

namespace Ui {
    class MainWindow;

    enum MPRISVer {v1, v2};
}

namespace MPRIS {
    enum MPRISVer {v1, v2};

    class MPRISPlayer {
    public:
      MPRISPlayer(const MPRIS::MPRISVer &eVer = v1) {
          switch (eVer) {
          case v1:
              break;
          case v2:
              break;
          }
      }

      ~MPRISPlayer() {

      }
    };
}

struct PlayerStatus
{
    int Play;
    int Random;
    int Repeat;
    int RepeatPlaylist;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected slots:
    void getMetadata();
    void playerPlay();
    void playerStop();
    void playerPrev();
    void playerNext();
    void playerChange(const QString &);
    void setPlayerVersion(bool);
    void onTrackChange(QVariantMap);
    void onPlayerStatusChange(PlayerStatus);
    void onPropertyChange(QDBusMessage);
    void onPlayersExistenceChanged(QString, QString, QString);

private:
    Ui::MainWindow *ui;
    QString playerName;
    Ui::MPRISVer plVer;
    QDBusInterface *m_player;
    PlayerStatus m_status;

    QStringList getPlayersList();
    QStringList getPlayersList_MPRISv1();
    QStringList getPlayersList_MPRISv2();
    static QString formatMetadata(QVariantMap &trackInfo, const QString &format);
    static QString secToTime(int secs);
    void refreshPlayersList();
    void connectToBus();
    void disconnectToBus();
};

Q_DECLARE_METATYPE (PlayerStatus)

#endif // MAINWINDOW_H
