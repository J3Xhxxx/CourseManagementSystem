#pragma once

#include "core/services/TeachingClassService.h"
#include <QString>

namespace planner {
using namespace std;

struct ImportSummary {
    size_t rows = 0;
    size_t addedTeachingClasses = 0;
    size_t addedMeetings = 0;
    size_t skippedDuplicates = 0;
};

class CsvImporter {
public:
    static ImportSummary importFile(const QString& path, TeachingClassService& target);
    static ImportSummary importText(const QString& csv, TeachingClassService& target);
};

}
