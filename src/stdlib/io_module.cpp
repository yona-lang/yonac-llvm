#include "stdlib/io_module.h"
#include "stdlib/native_args.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "common.h"

namespace yona::stdlib {

using namespace std;
using namespace yona::interp::runtime;

IOModule::IOModule() : NativeModule({"Std", "IO"}) {}

void IOModule::initialize() {
    // Register all IO functions
    module->exports["print"] = make_native_function("print", 1, print);
    module->exports["println"] = make_native_function("println", 1, println);
    module->exports["readFile"] = make_native_function("readFile", 1, readFile);
    module->exports["writeFile"] = make_native_function("writeFile", 2, writeFile);
    module->exports["appendFile"] = make_native_function("appendFile", 2, appendFile);
    module->exports["fileExists"] = make_native_function("fileExists", 1, fileExists);
    module->exports["deleteFile"] = make_native_function("deleteFile", 1, deleteFile);
    module->exports["readLine"] = make_native_function("readLine", 0, readLine);
    module->exports["readChar"] = make_native_function("readChar", 0, readChar);
}

RuntimeObjectPtr IOModule::print(const vector<RuntimeObjectPtr>& args) {
    NATIVE_ARGS_EXACT("print", 1);
    cout << *ARG_RAW(0);
    return make_unit();
}

RuntimeObjectPtr IOModule::println(const vector<RuntimeObjectPtr>& args) {
    NATIVE_FUNC_OPT_1("println", 0);
    if (!nargs.empty()) {
        cout << *ARG_RAW(0) << endl;
    } else {
        cout << endl;
    }
    return make_unit();
}

RuntimeObjectPtr IOModule::readFile(const vector<RuntimeObjectPtr>& args) {
    IO_FUNC_FILE("readFile");

    ifstream file(filename);
    if (!file.is_open()) {
        return make_error("file_not_found", make_string(filename));
    }

    stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return make_ok(make_string(buffer.str()));
}

RuntimeObjectPtr IOModule::writeFile(const vector<RuntimeObjectPtr>& args) {
    IO_FUNC_FILE_CONTENT("writeFile");

    ofstream file(filename);
    if (!file.is_open()) {
        return make_error("file_write_error", make_string(filename));
    }

    file << content;
    file.close();

    return make_ok(make_unit());
}

RuntimeObjectPtr IOModule::appendFile(const vector<RuntimeObjectPtr>& args) {
    IO_FUNC_FILE_CONTENT("appendFile");

    ofstream file(filename, ios::app);
    if (!file.is_open()) {
        return make_error("file_write_error", make_string(filename));
    }

    file << content;
    file.close();

    return make_ok(make_unit());
}

RuntimeObjectPtr IOModule::fileExists(const vector<RuntimeObjectPtr>& args) {
    IO_FUNC_FILE("fileExists");
    return make_bool(filesystem::exists(filename));
}

RuntimeObjectPtr IOModule::deleteFile(const vector<RuntimeObjectPtr>& args) {
    IO_FUNC_FILE("deleteFile");

    try {
        bool removed = filesystem::remove(filename);
        return make_ok(make_bool(removed));
    } catch (const filesystem::filesystem_error& e) {
        return make_error("file_delete_error", make_string(e.what()));
    }
}

RuntimeObjectPtr IOModule::readLine(const vector<RuntimeObjectPtr>& args) {
    NATIVE_FUNC_0("readLine");
    string line;
    if (getline(cin, line)) {
        return make_string(line);
    } else {
        // Return empty string on EOF
        return make_string("");
    }
}

RuntimeObjectPtr IOModule::readChar(const vector<RuntimeObjectPtr>& args) {
    NATIVE_FUNC_0("readChar");
    char ch = cin.get();
    if (cin.good()) {
        return make_char(ch);
    } else {
        // Return null character on EOF
        return make_char('\0');
    }
}

} // namespace yona::stdlib
