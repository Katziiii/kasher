#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QUuid>

namespace Protocol {

namespace Msg {
    constexpr const char* Op       = "op";
    constexpr const char* Cursor   = "cursor";
    constexpr const char* Join     = "join";
    constexpr const char* Sync     = "sync";
    constexpr const char* Joined   = "participant_joined";
    constexpr const char* Left     = "participant_left";
    constexpr const char* Ack      = "ack";
    constexpr const char* FileOpen = "file_open";
}

namespace OpType {
    constexpr const char* Insert   = "insert";
    constexpr const char* Delete   = "delete";
}

inline QString genUuid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

inline QString genColor() {
    static const QStringList colors = {
        QStringLiteral("#e74c3c"), QStringLiteral("#3498db"),
        QStringLiteral("#2ecc71"), QStringLiteral("#f39c12"),
        QStringLiteral("#9b59b6"), QStringLiteral("#1abc9c"),
        QStringLiteral("#e67e22"), QStringLiteral("#e91e63"),
    };
    static int idx = 0;
    return colors[idx++ % colors.size()];
}

} // namespace Protocol
