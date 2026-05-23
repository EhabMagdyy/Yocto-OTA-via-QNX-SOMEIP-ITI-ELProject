#pragma once
#include <QObject>
#include <QProcess>

class OtaProcess : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)

public:
    explicit OtaProcess(QObject* parent = nullptr);
    ~OtaProcess() override;          /* ← ADD THIS */
    bool running() const;
    int progress() const;

    Q_INVOKABLE void start(const QString& script, const QStringList& args);
    Q_INVOKABLE void kill();

signals:
    void logOutput(const QString& text);
    void runningChanged();
    void progressChanged(int progress);

private:
    QProcess* m_process;
    int m_progress = 0;
};