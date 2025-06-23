#include "common.h"

#include "runtime.h"

namespace yona {
template <typename T> void Frame<T>::write(const string &name, T value) { locals_[name] = std::move(value); }

template <typename T> void Frame<T>::write(const string &name, any value) { write(name, any_cast<T>(value)); }

template <typename T> T Frame<T>::lookup(SourceInfo source_token, const string &name) {
  auto value = try_lookup(source_token, name);
  if (value.has_value()) {
    return value.value();
  }
  throw yona_error(std::move(source_token), yona_error::RUNTIME, "Undefined variable: " + name);
}

template <typename T> optional<T> Frame<T>::try_lookup(SourceInfo source_token, const string &name) noexcept{
  auto it = locals_.find(name);
  if (it != locals_.end()) {
    return it->second;
  }
  if (parent) {
    return parent->try_lookup(source_token, name);
  }
  return nullopt;
}

template <typename T> void Frame<T>::merge(const Frame &other) {
  for (const auto &[name, value] : other.locals_) {
    write(name, value);
  }
}

template struct Frame<shared_ptr<interp::runtime::RuntimeObject>>;
} // namespace yona
