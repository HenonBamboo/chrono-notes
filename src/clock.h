#ifndef CLOCK_H
#define CLOCK_H

#include <QDateTime>

class Clock {
public:
    virtual ~Clock() = default;
    virtual QDateTime now() const = 0;

    qint64 nowMSecsSinceEpoch() const {
        return now().toMSecsSinceEpoch();
    }
};

class SystemClock final : public Clock {
public:
    QDateTime now() const override;
};

class TestClock final : public Clock {
public:
    explicit TestClock(const QDateTime &initial = QDateTime::currentDateTime());

    QDateTime now() const override;
    void setNow(const QDateTime &value);
    void advanceMSecs(qint64 milliseconds);

private:
    QDateTime now_;
};

#endif
