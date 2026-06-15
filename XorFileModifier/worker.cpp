#include "worker.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

static const qint64 CHUNK = 1 << 20;

static bool samePath(const QString &a, const QString &b)
{
    return QFileInfo(a).absoluteFilePath().compare(QFileInfo(b).absoluteFilePath(),
                                                   Qt::CaseInsensitive) == 0;
}

Worker::Worker(QObject *parent)
    : QObject(parent)
    , m_deleteInput(false)
    , m_overwrite(true)
    , m_timerMode(false)
    , m_stop(0)
    , m_pause(0)
{
    for (int i = 0; i < 8; ++i)
        m_keyBytes[i] = 0;
}

void Worker::configure(const QStringList &masks,
                       bool deleteInput,
                       const QString &outputDir,
                       const QString &searchDir,
                       bool overwrite,
                       bool timerMode,
                       const quint8 key[8])
{
    m_masks = masks;
    m_deleteInput = deleteInput;
    m_outputDir = outputDir;
    m_searchDir = searchDir;
    m_overwrite = overwrite;
    m_timerMode = timerMode;
    for (int i = 0; i < 8; ++i)
        m_keyBytes[i] = key[i];

    m_stop.storeRelaxed(0);
    m_pause.storeRelaxed(0);
    m_processed.clear();
}

void Worker::process()
{
    QDir dir(m_searchDir);
    const QStringList files = dir.entryList(m_masks, QDir::Files);

    for (const QString &fileName : files) {
        if (m_stop.loadRelaxed())
            break;

        const QString inPath = dir.absoluteFilePath(fileName);
        if (m_processed.contains(inPath))
            continue;

        emit fileStarted(fileName);
        emit status(QStringLiteral("Обработка: ") + fileName);

        const bool completed = processFile(inPath);
        if (!completed && m_stop.loadRelaxed())
            break;
    }

    emit finished();
}

bool Worker::processFile(const QString &inPath)
{
    QFile inFile(inPath);
    if (!inFile.open(QIODevice::ReadOnly)) {
        emit status(QStringLiteral("Ошибка открытия входного файла: ") + inPath);
        return false;
    }

    const QString outPath = resolveOutPath(QFileInfo(inPath).fileName(), inPath);

    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit status(QStringLiteral("Ошибка открытия выходного файла: ") + outPath);
        inFile.close();
        return false;
    }

    const qint64 total = inFile.size();
    qint64 processed = 0;

    if (total == 0) {
        outFile.close();
        inFile.close();
        emit progress(100);
        if (m_deleteInput)
            QFile::remove(inPath);
        m_processed.insert(inPath);
        return true;
    }

    QByteArray buf;
    buf.resize(CHUNK);

    while (processed < total) {
        if (m_stop.loadRelaxed()) {
            outFile.close();
            inFile.close();
            return false;
        }

        m_mutex.lock();
        while (m_pause.loadRelaxed() && !m_stop.loadRelaxed())
            m_cond.wait(&m_mutex);
        m_mutex.unlock();

        if (m_stop.loadRelaxed()) {
            outFile.close();
            inFile.close();
            return false;
        }

        const qint64 n = inFile.read(buf.data(), CHUNK);
        if (n < 0) {
            emit status(QStringLiteral("Ошибка чтения: ") + inPath);
            outFile.close();
            inFile.close();
            return false;
        }

        char *data = buf.data();
        for (qint64 i = 0; i < n; ++i)
            data[i] = static_cast<char>(static_cast<quint8>(data[i]) ^ m_keyBytes[(processed + i) & 7]);

        const qint64 w = outFile.write(data, n);
        if (w != n) {
            emit status(QStringLiteral("Ошибка записи (нет места?): ") + outPath);
            outFile.close();
            inFile.close();
            return false;
        }

        processed += n;
        emit progress(int(processed * 100 / total));
    }

    outFile.flush();
    outFile.close();
    inFile.close();

    if (m_deleteInput)
        QFile::remove(inPath);

    m_processed.insert(inPath);
    return true;
}

QString Worker::resolveOutPath(const QString &fileName, const QString &inPath)
{
    QDir outDir(m_outputDir);
    const QString outPath = outDir.absoluteFilePath(fileName);
    const bool collide = samePath(outPath, inPath);

    if (m_overwrite) {
        if (!collide)
            return outPath;
    } else {
        if (!QFile::exists(outPath) && !collide)
            return outPath;
    }

    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    int counter = 1;
    while (true) {
        const QString candidateName = suffix.isEmpty()
            ? QStringLiteral("%1(%2)").arg(base).arg(counter)
            : QStringLiteral("%1(%2).%3").arg(base).arg(counter).arg(suffix);
        const QString candidate = outDir.absoluteFilePath(candidateName);
        if (!QFile::exists(candidate) && !samePath(candidate, inPath))
            return candidate;
        ++counter;
    }
}

void Worker::requestStop()
{
    m_stop.storeRelaxed(1);
    m_mutex.lock();
    m_cond.wakeAll();
    m_mutex.unlock();
}

void Worker::requestPause()
{
    m_pause.storeRelaxed(1);
}

void Worker::requestResume()
{
    m_pause.storeRelaxed(0);
    m_mutex.lock();
    m_cond.wakeAll();
    m_mutex.unlock();
}
