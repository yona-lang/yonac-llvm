#include "stdlib/file_module.h"
#include "stdlib/native_args.h"
#include <filesystem>

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;
namespace fs = std::filesystem;

FileModule::FileModule() : NativeModule({"Std", "File"}) {}

void FileModule::initialize() {
    module->exports["exists"] = make_native_function("exists", 1, exists);
    module->exports["isDir"] = make_native_function("isDir", 1, isDir);
    module->exports["isFile"] = make_native_function("isFile", 1, isFile);
    module->exports["listDir"] = make_native_function("listDir", 1, listDir);
    module->exports["mkdir"] = make_native_function("mkdir", 1, mkdir);
    module->exports["remove"] = make_native_function("file_remove", 1, remove);
    module->exports["basename"] = make_native_function("basename", 1, basename);
    module->exports["dirname"] = make_native_function("dirname", 1, dirname);
    module->exports["extension"] = make_native_function("extension", 1, extension);
    module->exports["join"] = make_native_function("file_join", 2, join);
    module->exports["absolute"] = make_native_function("absolute", 1, absolute);
}

RuntimeObjectPtr FileModule::exists(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("exists", 1);
    string path = ARG_STRING(0);
    return make_bool(fs::exists(path));
}

RuntimeObjectPtr FileModule::isDir(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isDir", 1);
    string path = ARG_STRING(0);
    return make_bool(fs::is_directory(path));
}

RuntimeObjectPtr FileModule::isFile(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("isFile", 1);
    string path = ARG_STRING(0);
    return make_bool(fs::is_regular_file(path));
}

RuntimeObjectPtr FileModule::listDir(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("listDir", 1);
    string path = ARG_STRING(0);

    if (!fs::is_directory(path)) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "listDir: path is not a directory: " + path);
    }

    auto result = make_shared<SeqValue>();
    for (const auto& entry : fs::directory_iterator(path)) {
        result->fields.push_back(make_string(entry.path().filename().string()));
    }
    return make_shared<RuntimeObject>(Seq, result);
}

RuntimeObjectPtr FileModule::mkdir(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("mkdir", 1);
    string path = ARG_STRING(0);
    try {
        return make_bool(fs::create_directories(path));
    } catch (const fs::filesystem_error& e) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "mkdir: " + string(e.what()));
    }
}

RuntimeObjectPtr FileModule::remove(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("file_remove", 1);
    string path = ARG_STRING(0);
    try {
        return make_bool(fs::remove(path));
    } catch (const fs::filesystem_error& e) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "remove: " + string(e.what()));
    }
}

RuntimeObjectPtr FileModule::basename(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("basename", 1);
    string path = ARG_STRING(0);
    return make_string(fs::path(path).filename().string());
}

RuntimeObjectPtr FileModule::dirname(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("dirname", 1);
    string path = ARG_STRING(0);
    return make_string(fs::path(path).parent_path().string());
}

RuntimeObjectPtr FileModule::extension(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("extension", 1);
    string path = ARG_STRING(0);
    return make_string(fs::path(path).extension().string());
}

RuntimeObjectPtr FileModule::join(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("file_join", 2);
    string path1 = ARG_STRING(0);
    string path2 = ARG_STRING(1);
    return make_string((fs::path(path1) / fs::path(path2)).string());
}

RuntimeObjectPtr FileModule::absolute(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("absolute", 1);
    string path = ARG_STRING(0);
    try {
        return make_string(fs::absolute(path).string());
    } catch (const fs::filesystem_error& e) {
        throw yona_error(EMPTY_SOURCE_LOCATION, yona_error::Type::RUNTIME, "absolute: " + string(e.what()));
    }
}

} // namespace yona::stdlib
