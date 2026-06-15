#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Worker;
class QThread;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void startWork();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onBrowseSearch();
    void onBrowseOutput();
    void onTimerModeToggled(bool checked);
    void onStart();
    void onPauseResume();
    void onStop();
    void onTick();

    void onFileStarted(QString name);
    void onProgress(int percent);
    void onStatus(QString text);
    void onFinished();

private:
    QStringList parseMasks(const QString &text) const;
    bool parseKey(const QString &text, quint8 key[8]) const;
    void setRunningUi(bool running);

    Ui::MainWindow *ui;
    QThread *m_thread;
    Worker *m_worker;
    QTimer *m_timer;
    bool m_paused;
    bool m_running;
};

#endif
