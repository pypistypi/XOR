#ifndef WORKER_H
#define WORKER_H

#include <QObject>
#include <QStringList>
#include <QString>
#include <QAtomicInt>
#include <QMutex>
#include <QWaitCondition>
#include <QSet>

class Worker : public QObject
{
    Q_OBJECT
public:
    explicit Worker(QObject *parent = nullptr);

    void configure(const QStringList &masks,
                   bool deleteInput,
                   const QString &outputDir,
                   const QString &searchDir,
                   bool overwrite,
                   bool timerMode,
                   const quint8 key[8]);

public slots:
    void process();
    void requestStop();
    void requestPause();
    void requestResume();

signals:
    void fileStarted(QString name);
    void progress(int percent);
    void status(QString text);
    void finished();

private:
    bool processFile(const QString &inPath);
    QString resolveOutPath(const QString &fileName, const QString &inPath);

    QStringList m_masks;
    bool m_deleteInput;
    QString m_outputDir;
    QString m_searchDir;
    bool m_overwrite;
    bool m_timerMode;
    quint8 m_keyBytes[8];

    QAtomicInt m_stop;
    QAtomicInt m_pause;
    QMutex m_mutex;
    QWaitCondition m_cond;
    QSet<QString> m_processed;
};

#endif
