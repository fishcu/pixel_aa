#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/LinkAllIR.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Scalar.h>

#include <iostream>
#include <string>

using namespace llvm;

// Function to generate LLVM IR from C code
std::string generateLLVMIR(const std::string& cCode, int argc, char* argv[]) {
    // Initialize LLVM
    llvm::InitLLVM X(argc, argv);

    // Parse the C code to an LLVM module
    LLVMContext context;
    SMDiagnostic error;
    std::unique_ptr<Module> module =
        parseIR(MemoryBufferRef(cCode, "C_code"), error, context);

    // Verify the module
    if (!module) {
        std::string errorString;
        raw_string_ostream errorStream(errorString);
        error.print("LLVM IR generation", errorStream);
        return errorStream.str();
    }

    // Run optimizations on the module (optional)
    /*
    PassManagerBuilder builder;
    builder.OptLevel = 3;
    builder.SizeLevel = 0;
    legacy::FunctionPassManager fpm(module.get());
    legacy::PassManager mpm;
    builder.populateFunctionPassManager(fpm);
    builder.populateModulePassManager(mpm);
    fpm.doInitialization();
    for (Function& func : *module) fpm.run(func);
    fpm.doFinalization();
    mpm.run(*module);
    */

    // Verify the optimized module
    if (verifyModule(*module, &errs())) {
        return "LLVM IR verification failed after optimization.\n";
    }

    // Convert the module to a string containing LLVM IR
    std::string llvmIR;
    raw_string_ostream llvmIROut(llvmIR);
    llvmIROut << *module;
    llvmIROut.flush();

    return llvmIR;
}

int main(int argc, char* argv[]) {
    std::string cCode = R"(
        #include <stdio.h>
        int main() {
            printf("Hello, world!\n");
            return 0;
        }
    )";

    std::string llvmIR = generateLLVMIR(cCode, argc, argv);
    std::cout << "Generated LLVM IR:\n" << llvmIR << "\n";

    return 0;
}
