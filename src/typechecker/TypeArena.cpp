#include "typechecker/TypeArena.h"

namespace yona::compiler::typechecker {

MonoTypePtr TypeArena::alloc(MonoType t) {
    storage_.push_back(std::move(t));
    return &storage_.back();
}

MonoTypePtr TypeArena::fresh_var(int level) {
    return alloc(MonoType::make_var(next_id_++, level));
}

MonoTypePtr TypeArena::make_con(TyCon con) {
    return alloc(MonoType::make_con(con));
}

MonoTypePtr TypeArena::make_arrow(MonoTypePtr param, MonoTypePtr ret) {
    return alloc(MonoType::make_arrow(param, ret));
}

MonoTypePtr TypeArena::make_app(const std::string& name, std::vector<MonoTypePtr> args) {
    return alloc(MonoType::make_app(name, std::move(args)));
}

MonoTypePtr TypeArena::make_tuple(std::vector<MonoTypePtr> elems) {
    return alloc(MonoType::make_tuple(std::move(elems)));
}

} // namespace yona::compiler::typechecker
