#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "worker.h"

#include <QThread>
#include <QTimer>
#include <QFileDialog>
#include <QDir>
#include <QCloseEvent>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_thread(new QThread(this))
    , m_worker(new Worker)
    , m_timer(new QTimer(this))
    , m_paused(false)
    , m_running(false)
{
    ui->setupUi(this);

    m_worker->moveToThread(m_thread);

    connect(this, &MainWindow::startWork, m_worker, &Worker::process);
    connect(m_worker, &Worker::fileStarted, this, &MainWindow::onFileStarted);
    connect(m_worker, &Worker::progress, this, &MainWindow::onProgress);
    connect(m_worker, &Worker::status, this, &MainWindow::onStatus);
    connect(m_worker, &Worker::finished, this, &MainWindow::onFinished);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(ui->browseSearchButton, &QPushButton::clicked, this, &MainWindow::onBrowseSearch);
    connect(ui->browseOutputButton, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
    connect(ui->timerModeRadio, &QRadioButton::toggled, this, &MainWindow::onTimerModeToggled);
    connect(ui->startButton, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(ui->pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseResume);
    connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onTick);

    m_thread->start();

    ui->periodSpin->setEnabled(false);
    setRunningUi(false);
}

MainWindow::~MainWindow()
{
    m_worker->requestStop();
    m_thread->quit();
    m_thread->wait();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_timer->stop();
    m_worker->requestStop();
    m_thread->quit();
    m_thread->wait();
    event->accept();
}

void MainWindow::onBrowseSearch()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Папка поиска"),
                                                          ui->searchEdit->text());
    if (!dir.isEmpty())
        ui->searchEdit->setText(dir);
}

void MainWindow::onBrowseOutput()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Папка сохранения"),
                                                          ui->outputEdit->text());
    if (!dir.isEmpty())
        ui->outputEdit->setText(dir);
}

void MainWindow::onTimerModeToggled(bool checked)
{
    ui->periodSpin->setEnabled(checked);
}

void MainWindow::onStart()
{
    const QStringList masks = parseMasks(ui->maskEdit->text());
    if (masks.isEmpty()) {
        ui->statusLabel->setText(QStringLiteral("Маска не задана"));
        return;
    }

    const QString searchDir = ui->searchEdit->text();
    const QString outputDir = ui->outputEdit->text();

    if (searchDir.isEmpty() || !QDir(searchDir).exists()) {
        ui->statusLabel->setText(QStringLiteral("Папка поиска не существует"));
        return;
    }
    if (outputDir.isEmpty() || !QDir(outputDir).exists()) {
        ui->statusLabel->setText(QStringLiteral("Папка сохранения не существует"));
        return;
    }

    quint8 key[8];
    if (!parseKey(ui->keyEdit->text(), key)) {
        ui->statusLabel->setText(QStringLiteral("Ключ должен содержать ровно 16 hex-символов"));
        return;
    }

    const bool timerMode = ui->timerModeRadio->isChecked();
    const int period = ui->periodSpin->value();
    if (timerMode && period <= 0) {
        ui->statusLabel->setText(QStringLiteral("Период должен быть больше 0"));
        return;
    }

    const bool overwrite = ui->overwriteRadio->isChecked();
    const bool deleteInput = ui->deleteCheck->isChecked();

    m_worker->configure(masks, deleteInput, outputDir, searchDir, overwrite, timerMode, key);

    m_paused = false;
    m_running = true;
    setRunningUi(true);
    ui->pauseButton->setText(QStringLiteral("Пауза"));
    ui->progressBar->setValue(0);

    emit startWork();

    if (timerMode)
        m_timer->start(period * 1000);
}

void MainWindow::onPauseResume()
{
    if (!m_running)
        return;

    if (m_paused) {
        m_worker->requestResume();
        m_paused = false;
        ui->pauseButton->setText(QStringLiteral("Пауза"));
        ui->statusLabel->setText(QStringLiteral("Возобновлено"));
    } else {
        m_worker->requestPause();
        m_paused = true;
        ui->pauseButton->setText(QStringLiteral("Продолжить"));
        ui->statusLabel->setText(QStringLiteral("Пауза"));
    }
}

void MainWindow::onStop()
{
    m_timer->stop();
    m_worker->requestStop();
    m_running = false;
    m_paused = false;
    setRunningUi(false);
    ui->pauseButton->setText(QStringLiteral("Пауза"));
    ui->statusLabel->setText(QStringLiteral("Остановлено"));
}

void MainWindow::onTick()
{
    if (!m_running)
        return;
    emit startWork();
}

void MainWindow::onFileStarted(QString name)
{
    ui->statusLabel->setText(QStringLiteral("Файл: ") + name);
}

void MainWindow::onProgress(int percent)
{
    ui->progressBar->setValue(percent);
}

void MainWindow::onStatus(QString text)
{
    ui->statusLabel->setText(text);
}

void MainWindow::onFinished()
{
    if (m_timer->isActive())
        return;
    m_running = false;
    m_paused = false;
    setRunningUi(false);
    ui->pauseButton->setText(QStringLiteral("Пауза"));
    ui->statusLabel->setText(QStringLiteral("Готово"));
}

QStringList MainWindow::parseMasks(const QString &text) const
{
    QStringList result;
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[;,]")),
                                         Qt::SkipEmptyParts);
    for (const QString &raw : parts) {
        const QString part = raw.trimmed();
        if (part.isEmpty())
            continue;
        if (part.startsWith(QLatin1Char('*')))
            result << part;
        else if (part.startsWith(QLatin1Char('.')))
            result << QStringLiteral("*") + part;
        else if (!part.contains(QLatin1Char('.')))
            result << QStringLiteral("*.") + part;
        else
            result << part;
    }
    return result;
}

bool MainWindow::parseKey(const QString &text, quint8 key[8]) const
{
    QString clean = text;
    clean.remove(QLatin1Char(' '));
    if (clean.length() != 16)
        return false;

    const QRegularExpression re(QStringLiteral("^[0-9A-Fa-f]{16}$"));
    if (!re.match(clean).hasMatch())
        return false;

    const QByteArray bytes = QByteArray::fromHex(clean.toLatin1());
    if (bytes.size() != 8)
        return false;

    for (int i = 0; i < 8; ++i)
        key[i] = static_cast<quint8>(bytes[i]);
    return true;
}

void MainWindow::setRunningUi(bool running)
{
    ui->startButton->setEnabled(!running);
    ui->pauseButton->setEnabled(running);
    ui->stopButton->setEnabled(running);
}
