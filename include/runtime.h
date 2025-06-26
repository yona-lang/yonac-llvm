//
// Created by Adam Kovari on 23.12.2024.
//

#pragma once

#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>
#include "yona_export.h"
#include "types.h"

// Disable MSVC warning C4251 about STL types in exported class interfaces
// This is safe for our use case as both the DLL and clients use the same STL
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251)
#endif

namespace yona::interp::runtime {
using namespace std;

struct SetValue;
struct SeqValue;
struct DictValue;
struct TupleValue;
struct RecordValue;
struct FunctionValue;
struct ModuleValue;
struct SymbolValue;
struct ApplyValue;
struct RuntimeObject;
struct FqnValue;

// Helper function to convert wide character to UTF-8
inline string wchar_to_utf8(wchar_t wc) {
    string result;
    uint32_t codepoint = static_cast<uint32_t>(wc);

    if (codepoint <= 0x7F) {
        result += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        result += static_cast<char>(0xC0 | (codepoint >> 6));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        result += static_cast<char>(0xE0 | (codepoint >> 12));
        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        result += static_cast<char>(0xF0 | (codepoint >> 18));
        result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    return result;
}

using RuntimeObjectData =
    variant<int /*Int*/, double /*Float*/, byte /*Byte*/, wchar_t /*Char*/, string /*String*/, bool /*Bool*/, nullptr_t /*Unit*/,
            shared_ptr<SymbolValue> /*Symbol*/, shared_ptr<TupleValue> /*Tuple*/, shared_ptr<RecordValue> /*Record*/,
            shared_ptr<DictValue> /*Dict*/, shared_ptr<SeqValue> /*Seq*/, shared_ptr<SetValue> /*Set*/,
            shared_ptr<FqnValue> /*FQN*/, shared_ptr<ModuleValue> /*Module*/, shared_ptr<FunctionValue> /*Function*/,
            shared_ptr<ApplyValue> /*Apply*/>;

using RuntimeObjectPtr = shared_ptr<RuntimeObject>;

enum RuntimeObjectType { Int, Float, Byte, Char, String, Bool, Unit, Symbol, Dict, Seq, Set, Tuple, Record, FQN, Module, Function };

inline string RuntimeObjectTypes[] = {"Int",  "Float", "Byte", "Char",  "String", "Bool",   "Unit",    "Symbol",
                                      "Dict", "Seq",   "Set",  "Tuple", "Record", "FQN",    "Module", "Function"};

struct SymbolValue {
  string name;
};

struct DictValue {
  vector<pair<RuntimeObjectPtr, RuntimeObjectPtr>> fields{};
};

struct SeqValue {
  vector<RuntimeObjectPtr> fields{};
};

struct SetValue {
  vector<RuntimeObjectPtr> fields{};
};

struct ApplyValue {
  RuntimeObjectPtr func;
  RuntimeObjectPtr arg;
};

struct TupleValue {
  vector<RuntimeObjectPtr> fields{};
};

struct RecordValue {
  string type_name;                         // Record type name (e.g., "Person")
  vector<string> field_names;               // Field names in order
  vector<RuntimeObjectPtr> field_values;    // Field values in same order

  // Helper to get field value by name
  RuntimeObjectPtr get_field(const string& name) const {
    for (size_t i = 0; i < field_names.size(); i++) {
      if (field_names[i] == name) {
        return field_values[i];
      }
    }
    return nullptr;
  }

  // Helper to set field value by name
  bool set_field(const string& name, RuntimeObjectPtr value) {
    for (size_t i = 0; i < field_names.size(); i++) {
      if (field_names[i] == name) {
        field_values[i] = value;
        return true;
      }
    }
    return false;
  }
};

struct FqnValue {
  vector<string> parts;
};

struct FunctionValue {
  shared_ptr<FqnValue> fqn;
  function<RuntimeObjectPtr(const vector<RuntimeObjectPtr> &)> code; // nullptr - nomatch
  size_t arity; // Number of parameters expected
  optional<compiler::types::Type> type; // Function type information

  // For partial application - stores already applied arguments
  vector<RuntimeObjectPtr> partial_args;
};

// Record type definition (metadata about a record type)
struct RecordTypeInfo {
  string name;
  vector<string> field_names;
  vector<compiler::types::Type> field_types;
};

struct ModuleValue {
  shared_ptr<FqnValue> fqn;

  // Record type definitions in this module
  unordered_map<string, RecordTypeInfo> record_types;

  // Export table: maps exported names to functions (this is the only function storage)
  unordered_map<string, shared_ptr<FunctionValue>> exports;

  // Keep the AST alive as long as the module is alive
  // This is needed because functions capture pointers to AST nodes
  shared_ptr<void> ast_keeper;

  // Module source file path (for debugging and reloading)
  string source_path;

  // Helper to get exported function by name
  shared_ptr<FunctionValue> get_export(const string& name) const {
    auto it = exports.find(name);
    return it != exports.end() ? it->second : nullptr;
  }

  // Helper to get record type info
  RecordTypeInfo* get_record_type(const string& name) {
    auto it = record_types.find(name);
    return it != record_types.end() ? &it->second : nullptr;
  }

  const RecordTypeInfo* get_record_type(const string& name) const {
    auto it = record_types.find(name);
    return it != record_types.end() ? &it->second : nullptr;
  }
};

struct RuntimeObject {
  RuntimeObjectType type;
  RuntimeObjectData data;
  optional<compiler::types::Type> static_type; // Type information from type checker

  RuntimeObject(RuntimeObjectType t, RuntimeObjectData d) : type(t), data(std::move(d)) {}

  RuntimeObject(RuntimeObjectType t, RuntimeObjectData d, compiler::types::Type st)
    : type(t), data(std::move(d)), static_type(st) {}

  template <typename T> T &get() { return std::get<T>(data); }

  template <typename T> const T &get() const { return std::get<T>(data); }

  friend bool operator==(const RuntimeObject &lhs, const RuntimeObject &rhs) { return lhs.type == rhs.type && lhs.data == rhs.data; }
  friend bool operator!=(const RuntimeObject &lhs, const RuntimeObject &rhs) { return !(lhs == rhs); }
};

YONA_API std::ostream &operator<<(std::ostream &strm, const RuntimeObject &obj);

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace yona::interp::runtime
