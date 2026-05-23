#include "backend.hpp"
#include <QRegularExpression>
#include <QFile>
#include <signal.h>
#include <unistd.h>

/* Recursively kill a process and all its children via /proc */
static void killProcessTree(qint64 pid)
{
    QString childrenPath = QString("/proc/%1/task/%1/children").arg(pid);
    QFile f(childrenPath);
    if (f.open(QIODevice::ReadOnly)) {
        for (const QByteArray &bp : f.readAll().split(' ')) {
            qint64 cpid = bp.trimmed().toLongLong();
            if (cpid > 0 && cpid != pid) {
                killProcessTree(cpid);
                ::kill(static_cast<pid_t>(cpid), SIGKILL);
            }
        }
    }
    ::kill(static_cast<pid_t>(pid), SIGKILL);
}

OtaProcess::OtaProcess(QObject* parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    /* ── Stdout: real logs → terminal ── */
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, [this]() {
        QString out = QString::fromUtf8(m_process->readAllStandardOutput());
        if (!out.trimmed().isEmpty())
            emit logOutput(out);
    });

    /* ── Stderr: pv -n percentages → progress bar only ── */
    connect(m_process, &QProcess::readyReadStandardError,
            this, [this]() {
        QString err = QString::fromUtf8(m_process->readAllStandardError());
        for (const QString &line : err.split('\n', Qt::SkipEmptyParts)) {
            QString trimmed = line.trimmed();
            bool ok;
            int p = trimmed.toInt(&ok);
            if (ok && p >= 0 && p <= 100) {
                if (p != m_progress) {
                    m_progress = p;
                    emit progressChanged(m_progress);
                }
            } else if (!trimmed.isEmpty()) {
                emit logOutput(trimmed + "\n");
            }
        }
    });

    connect(m_process, &QProcess::finished,
            this, [this](int exitCode) {
        emit logOutput(QString("\n--- Process finished (exit code: %1) ---").arg(exitCode));
        if (exitCode == 0 && m_progress != 100) {
            m_progress = 100;
            emit progressChanged(100);
        }
        emit runningChanged();
    });

    connect(m_process, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError) {
        m_progress = 0;
        emit progressChanged(0);
        emit runningChanged();
    });
}

OtaProcess::~OtaProcess()
{
    if (running()) {
        killProcessTree(m_process->processId());
        m_process->waitForFinished(1500);
    }
}

bool OtaProcess::running() const
{
    return m_process->state() != QProcess::NotRunning;
}

int OtaProcess::progress() const
{
    return m_progress;
}

void OtaProcess::start(const QString& script, const QStringList& args)
{
    if (running())
        return;

    m_progress = 0;
    emit progressChanged(0);
    m_process->start(script, args);
    emit runningChanged();
}

void OtaProcess::kill()
{
    if (!running())
        return;

    qint64 pid = m_process->processId();   /* ← FIXED: Q_PID → qint64 */
    if (pid > 0)
        killProcessTree(pid);

    m_process->waitForFinished(2000);
    emit runningChanged();
}