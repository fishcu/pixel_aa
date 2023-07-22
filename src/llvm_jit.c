#include <iostream>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Host.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

using namespace llvm;
using namespace clang;
using namespace clang::tooling;

int main(int argc, const char* argv[]) {
    // Initialize LLVM components.
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();

    // Create an LLVM context and a module.
    LLVMContext context;
    std::unique_ptr<Module> module = std::make_unique<Module>("my_module", context);

    // Parse the C file and generate LLVM IR.
    std::unique_ptr<FrontendActionFactory> factory(newFrontendActionFactory<clang::EmitLLVMOnlyAction>());
    int result = ToolInvocation::runToolOnCode(factory->create(), argv[1]);

    if (result != 0) {
        std::cerr << "Error: Failed to parse the C file.\n";
        return 1;
    }

    // Create a JIT engine and compile the module.
    std::string error;
    EngineBuilder builder(std::move(module));
    builder.setErrorStr(&error);
    std::unique_ptr<ExecutionEngine> engine(builder.create());
    if (!engine) {
        std::cerr << "Error: Could not create ExecutionEngine: " << error << "\n";
        return 1;
    }

    // Find the address of the function to execute (e.g., "main" function).
    void* mainFuncPtr = engine->getFunctionAddress("main");

    // Call the JIT-compiled function.
    int (*JITMain)() = reinterpret_cast<int (*)()>(mainFuncPtr);
    int mainResult = JITMain();

    std::cout << "Result: " << mainResult << "\n";

    return 0;
}
