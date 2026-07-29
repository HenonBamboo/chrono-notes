#include "clock.h"

QDateTime SystemClock::now() const {
    return QDateTime::currentDateTime();
}

TestClock::TestClock(const QDateTime &initial)
    : now_(initial) {
}

QDateTime TestClock::now() const {
    return now_;
}

void TestClock::setNow(const QDateTime &value) {
    now_ = value;
}

void TestClock::advanceMSecs(qint64 milliseconds) {
    now_ = now_.addMSecs(milliseconds);
}
