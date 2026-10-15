#pragma once

#include "core/services/TeachingClassService.h"
#include <QByteArray>
#include <QString>

namespace planner {

class JsonProjectStore {
public:
    static constexpr int formatVersion = 4;
    static QByteArray serialize(const TeachingClassService& project);
    static TeachingClassService deserialize(const QByteArray& json);
    static void save(const QString& path, const TeachingClassService& project);
    static TeachingClassService load(const QString& path);
};

} // namespace planner
