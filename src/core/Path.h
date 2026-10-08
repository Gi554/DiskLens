#pragma once

#include "core/Scanner.h"
#include <QString>

namespace disklens {
inline QString displayPath(const fs::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.native());
#else
    return QString::fromUtf8(path.u8string().c_str());
#endif
}
inline fs::path nativePath(const QString& path) {
#ifdef _WIN32
    return fs::path(path.toStdWString());
#else
    return fs::u8path(path.toUtf8().constData());
#endif
}
inline QString formatBytes(std::uintmax_t bytes) {
    static const char* units[] = {"o", "Kio", "Mio", "Gio", "Tio", "Pio"};
    double amount = static_cast<double>(bytes);
    int unit = 0;
    while (amount >= 1024 && unit < 5) {
        amount /= 1024;
        ++unit;
    }
    return QString::number(amount, 'f', unit == 0 ? 0 : 1) + " " + units[unit];
}
} // namespace disklens
