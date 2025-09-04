//
// Created by Adam Kovari on 23.12.2024.
//

#include "runtime.h"

#include <ranges>

namespace yona::interp::runtime {
namespace { // Anonymous namespace for helper functions
std::ostream &printInt(std::ostream &strm, const RuntimeObject &obj) {
  strm << obj.get<int>();
  return strm;
}

std::ostream &printFloat(std::ostream &strm, const RuntimeObject &obj) {
  strm << obj.get<double>();
  return strm;
}

std::ostream &printByte(std::ostream &strm, const RuntimeObject &obj) {
  strm << static_cast<int>(obj.get<byte>());
  return strm;
}

std::ostream &printChar(std::ostream &strm, const RuntimeObject &obj) {
  strm << wchar_to_utf8(obj.get<wchar_t>());
  return strm;
}

std::ostream &printString(std::ostream &strm, const RuntimeObject &obj) {
  strm << obj.get<std::string>();
  return strm;
}

std::ostream &printBool(std::ostream &strm, const RuntimeObject &obj) {
  strm << (obj.get<bool>() ? "true" : "false");
  return strm;
}

std::ostream &printUnit(std::ostream &strm, const RuntimeObject &obj) {
  strm << "()";
  return strm;
}

std::ostream &printSymbol(std::ostream &strm, const RuntimeObject &obj) {
  strm << ":" << obj.get<std::shared_ptr<SymbolValue>>()->name;
  return strm;
}

std::ostream &printDict(std::ostream &strm, const RuntimeObject &obj) {
  strm << "{";
  const auto fields = obj.get<std::shared_ptr<DictValue>>()->fields;
  std::size_t i = 0;
  for (const auto &field : fields) {
    strm << *field.first << ": " << *field.second;
    if (i++ < fields.size() - 1) {
      strm << ", ";
    }
  }
  strm << "}";
  return strm;
}

std::ostream &printSeq(std::ostream &strm, const RuntimeObject &obj) {
  strm << "[";
  const auto fields = obj.get<std::shared_ptr<SeqValue>>()->fields;
  std::size_t i = 0;
  for (const auto &field : fields) {
    strm << *field;
    if (i++ < fields.size() - 1) {
      strm << ", ";
    }
  }
  strm << "]";
  return strm;
}

std::ostream &printSet(std::ostream &strm, const RuntimeObject &obj) {
  strm << "{";
  const auto fields = obj.get<std::shared_ptr<SetValue>>()->fields;
  std::size_t i = 0;
  for (const auto &field : fields) {
    strm << *field;
    if (i++ < fields.size() - 1) {
      strm << ", ";
    }
  }
  strm << "}";
  return strm;
}

std::ostream &printTuple(std::ostream &strm, const RuntimeObject &obj) {
  strm << "(";
  const auto fields = obj.get<std::shared_ptr<TupleValue>>()->fields;
  std::size_t i = 0;
  for (const auto &field : fields) {
    strm << *field;
    if (i++ < fields.size() - 1) {
      strm << ", ";
    }
  }
  strm << ")";
  return strm;
}

std::ostream &printRecord(std::ostream &strm, const RuntimeObject &obj) {
  const auto &record = obj.get<std::shared_ptr<RecordValue>>();
  strm << record->type_name << "(";
  for (std::size_t i = 0; i < record->field_names.size(); i++) {
    strm << record->field_names[i] << "=" << *record->field_values[i];
    if (i < record->field_names.size() - 1) {
      strm << ", ";
    }
  }
  strm << ")";
  return strm;
}

std::ostream &printFQN(std::ostream &strm, const RuntimeObject &obj) {
  const auto parts = obj.get<std::shared_ptr<FqnValue>>()->parts;
  std::size_t i = 0;
  for (const auto &part : parts) {
    strm << part;
    if (i++ < parts.size() - 1) {
      strm << "\\";
    }
  }
  return strm;
}

std::ostream &printFunction(std::ostream &strm, const RuntimeObject &obj) {
  const auto &function = obj.get<std::shared_ptr<FunctionValue>>();
  strm << function->fqn;
  return strm;
}

std::ostream &printModule(std::ostream &strm, const RuntimeObject &obj) {
  const auto &module = obj.get<std::shared_ptr<ModuleValue>>();
  const auto &exports = module->exports;
  const auto &record_types = module->record_types;
  std::size_t i = 0;

  strm << module->fqn << "(exports=";

  for (const auto &function : exports | views::keys) {
    strm << function;
    if (i++ < exports.size() - 1) {
      strm << ", ";
    }
  }
  i = 0;

  strm << ", records=";

  for (const auto &record : record_types | views::keys) {
    strm << record;
    if (i++ < record_types.size() - 1) {
      strm << ", ";
    }
  }
  strm << ")";
  return strm;
}
} // namespace

std::ostream &operator<<(std::ostream &strm, const RuntimeObject &obj) {
  switch (obj.type) {
  case Int:
    return printInt(strm, obj);
  case Float:
    return printFloat(strm, obj);
  case Byte:
    return printByte(strm, obj);
  case Char:
    return printChar(strm, obj);
  case String:
    return printString(strm, obj);
  case Bool:
    return printBool(strm, obj);
  case Unit:
    return printUnit(strm, obj);
  case Symbol:
    return printSymbol(strm, obj);
  case Dict:
    return printDict(strm, obj);
  case Seq:
    return printSeq(strm, obj);
  case Set:
    return printSet(strm, obj);
  case Tuple:
    return printTuple(strm, obj);
  case Record:
    return printRecord(strm, obj);
  case FQN:
    return printFQN(strm, obj);
  case Module:
    return printModule(strm, obj);
  case Function:
    return printFunction(strm, obj);
  case Promise:
    return strm << "<Promise>";
  case Error:
    return strm << "<Error>";
  }
  return strm;
}
} // namespace yona::interp::runtime
