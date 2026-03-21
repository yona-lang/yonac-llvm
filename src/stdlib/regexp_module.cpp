#include "stdlib/regexp_module.h"
#include "stdlib/native_args.h"
#include <regex>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

RegexpModule::RegexpModule() : NativeModule({"Std", "Regexp"}) {}

void RegexpModule::initialize() {
    module->exports["match"] = make_native_function("match", 2, match);
    module->exports["matchAll"] = make_native_function("matchAll", 2, matchAll);
    module->exports["replace"] = make_native_function("replace", 3, replace);
    module->exports["split"] = make_native_function("split", 2, split);
    module->exports["test"] = make_native_function("test", 2, test);
}

RuntimeObjectPtr RegexpModule::match(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("match", 2);
    string pattern_str = ARG_STRING(0);
    string input = ARG_STRING(1);

    try {
        regex pattern(pattern_str);
        smatch m;
        if (regex_search(input, m, pattern)) {
            return make_some(make_string(m[0].str()));
        }
        return make_none();
    } catch (const regex_error& e) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
            "match: invalid regex pattern: " + string(e.what()));
    }
}

RuntimeObjectPtr RegexpModule::matchAll(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("matchAll", 2);
    string pattern_str = ARG_STRING(0);
    string input = ARG_STRING(1);

    try {
        regex pattern(pattern_str);
        auto result = make_shared<SeqValue>();
        auto it = sregex_iterator(input.begin(), input.end(), pattern);
        auto end = sregex_iterator();
        for (; it != end; ++it) {
            result->fields.push_back(make_string((*it)[0].str()));
        }
        return make_shared<RuntimeObject>(Seq, result);
    } catch (const regex_error& e) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
            "matchAll: invalid regex pattern: " + string(e.what()));
    }
}

RuntimeObjectPtr RegexpModule::replace(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("replace", 3);
    string pattern_str = ARG_STRING(0);
    string replacement = ARG_STRING(1);
    string input = ARG_STRING(2);

    try {
        regex pattern(pattern_str);
        return make_string(regex_replace(input, pattern, replacement));
    } catch (const regex_error& e) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
            "replace: invalid regex pattern: " + string(e.what()));
    }
}

RuntimeObjectPtr RegexpModule::split(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("split", 2);
    string pattern_str = ARG_STRING(0);
    string input = ARG_STRING(1);

    try {
        regex pattern(pattern_str);
        auto result = make_shared<SeqValue>();
        sregex_token_iterator it(input.begin(), input.end(), pattern, -1);
        sregex_token_iterator end;
        for (; it != end; ++it) {
            result->fields.push_back(make_string(it->str()));
        }
        return make_shared<RuntimeObject>(Seq, result);
    } catch (const regex_error& e) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
            "split: invalid regex pattern: " + string(e.what()));
    }
}

RuntimeObjectPtr RegexpModule::test(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("test", 2);
    string pattern_str = ARG_STRING(0);
    string input = ARG_STRING(1);

    try {
        regex pattern(pattern_str);
        return make_bool(regex_search(input, pattern));
    } catch (const regex_error& e) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME,
            "test: invalid regex pattern: " + string(e.what()));
    }
}

} // namespace yona::stdlib
