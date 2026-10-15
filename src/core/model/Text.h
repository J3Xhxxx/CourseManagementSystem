#pragma once

#include <stdexcept>
#include <string>

namespace planner::detail {
using namespace std;

inline string requiredText(string text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        throw invalid_argument("必填项不能为空");
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// UTF-8 字节长度作为前缀，字段本身包含冒号等分隔符时也不会产生 ID 歧义。
inline string identityPart(const string& text) {
    return to_string(text.size()) + ":" + text;
}

}
