#include "common.h"
#include "runtime.h"

namespace yona {
template <typename T> void Frame<T>::write(const string &name, T value) { locals_[name] = std::move(value); }

template <typename T> void Frame<T>::write(const string &name, any value) { write(name, any_cast<T>(value)); }

template <typename T> T Frame<T>::lookup(SourceInfo source_token, const string &name) {
  if (locals_.contains(name)) {
    return locals_[name];
  } else if (parent) {
    return parent->lookup(source_token, name);
  }
  throw yona_error(source_token, yona_error::RUNTIME, "Undefined variable: " + name);
}

template struct Frame<shared_ptr<interp::runtime::RuntimeObject>>;
} // namespace yona
