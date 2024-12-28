//
// Created by Adam Kovari on 14.12.2024.
//

#include "Codegen.h"

/*
using namespace llvm;

int main() {
    LLVMContext Context;
    Module *Module = new Module("MyModule", Context);

    // Create a function named "main"
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(Context), false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, "main", Module);

    // Create a basic block
    BasicBlock *BB = BasicBlock::Create(Context, "entry", F);
    IRBuilder<> Builder(BB);

    // Create instructions
    Value *RetVal = Builder.getInt32(42);
    Builder.ret(RetVal);

    // Print the module to the console
    Module->dump();

    // Clean up
    delete Module;
    return 0;
}
*/
