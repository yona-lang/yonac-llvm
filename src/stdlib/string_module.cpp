#include "stdlib/string_module.h"
#include "stdlib/native_args.h"
#include <algorithm>
#include <sstream>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

StringModule::StringModule() : NativeModule({"Std", "String"}) {}

void StringModule::initialize() {
    module->exports["length"] = make_native_function("length", 1, length);
    module->exports["toUpperCase"] = make_native_function("toUpperCase", 1, toUpperCase);
    module->exports["toLowerCase"] = make_native_function("toLowerCase", 1, toLowerCase);
    module->exports["trim"] = make_native_function("trim", 1, trim);
    module->exports["split"] = make_native_function("split", 2, split);
    module->exports["join"] = make_native_function("join", 2, join);
    module->exports["substring"] = make_native_function("substring", 3, substring);
    module->exports["indexOf"] = make_native_function("indexOf", 2, indexOf);
    module->exports["contains"] = make_native_function("contains", 2, contains);
    module->exports["replace"] = make_native_function("replace", 3, replace);
    module->exports["startsWith"] = make_native_function("startsWith", 2, startsWith);
    module->exports["endsWith"] = make_native_function("endsWith", 2, endsWith);
    module->exports["toString"] = make_native_function("toString", 1, toString);
    module->exports["chars"] = make_native_function("chars", 1, chars);
}

RuntimeObjectPtr StringModule::length(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("length", 1);
    return make_int(static_cast<int>(ARG_STRING(0).size()));
}

RuntimeObjectPtr StringModule::toUpperCase(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("toUpperCase", 1);
    string s = ARG_STRING(0);
    transform(s.begin(), s.end(), s.begin(), ::toupper);
    return make_string(s);
}

RuntimeObjectPtr StringModule::toLowerCase(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("toLowerCase", 1);
    string s = ARG_STRING(0);
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return make_string(s);
}

RuntimeObjectPtr StringModule::trim(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("trim", 1);
    string s = ARG_STRING(0);
    auto start = s.find_first_not_of(" \t\n\r");
    auto end = s.find_last_not_of(" \t\n\r");
    if (start == string::npos) return make_string("");
    return make_string(s.substr(start, end - start + 1));
}

RuntimeObjectPtr StringModule::split(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("split", 2);
    string delimiter = ARG_STRING(0);
    string s = ARG_STRING(1);

    auto result = make_shared<SeqValue>();
    size_t pos = 0;
    while ((pos = s.find(delimiter)) != string::npos) {
        result->fields.push_back(make_string(s.substr(0, pos)));
        s.erase(0, pos + delimiter.length());
    }
    result->fields.push_back(make_string(s));
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr StringModule::join(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("join", 2);
    string delimiter = ARG_STRING(0);

    if (!nargs.is_type(1, Seq)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::TYPE, "join: second argument must be a sequence");
    }
    auto seq = args[1]->get<shared_ptr<SeqValue>>();

    ostringstream oss;
    for (size_t i = 0; i < seq->fields.size(); i++) {
        if (i > 0) oss << delimiter;
        oss << *seq->fields[i];
    }
    return make_string(oss.str());
}

RuntimeObjectPtr StringModule::substring(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("substring", 3);
    string s = ARG_STRING(0);
    int start = ARG_INT(1);
    int len = ARG_INT(2);

    if (start < 0 || start > static_cast<int>(s.size())) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "substring: start index out of bounds");
    }
    return make_string(s.substr(start, len));
}

RuntimeObjectPtr StringModule::indexOf(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("indexOf", 2);
    string needle = ARG_STRING(0);
    string haystack = ARG_STRING(1);

    auto pos = haystack.find(needle);
    return make_int(pos == string::npos ? -1 : static_cast<int>(pos));
}

RuntimeObjectPtr StringModule::contains(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("contains", 2);
    string needle = ARG_STRING(0);
    string haystack = ARG_STRING(1);
    return make_bool(haystack.find(needle) != string::npos);
}

RuntimeObjectPtr StringModule::replace(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("replace", 3);
    string from = ARG_STRING(0);
    string to = ARG_STRING(1);
    string s = ARG_STRING(2);

    size_t pos = 0;
    while ((pos = s.find(from, pos)) != string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
    return make_string(s);
}

RuntimeObjectPtr StringModule::startsWith(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("startsWith", 2);
    string prefix = ARG_STRING(0);
    string s = ARG_STRING(1);
    return make_bool(s.substr(0, prefix.size()) == prefix);
}

RuntimeObjectPtr StringModule::endsWith(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("endsWith", 2);
    string suffix = ARG_STRING(0);
    string s = ARG_STRING(1);
    if (suffix.size() > s.size()) return make_bool(false);
    return make_bool(s.substr(s.size() - suffix.size()) == suffix);
}

RuntimeObjectPtr StringModule::toString(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("toString", 1);
    ostringstream oss;
    oss << *args[0];
    return make_string(oss.str());
}

RuntimeObjectPtr StringModule::chars(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("chars", 1);
    string s = ARG_STRING(0);
    auto result = make_shared<SeqValue>();
    for (char c : s) {
        result->fields.push_back(make_string(string(1, c)));
    }
    return make_shared<RuntimeObject>(Seq, result);
}

} // namespace yona::stdlib
