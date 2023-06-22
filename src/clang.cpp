#include "clang.hpp"

#ifdef _MSC_VER
#pragma warning(push, 1)
#pragma warning(disable: 4624)
#pragma warning(disable: 4291)
#endif

#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <llvm/Support/TargetSelect.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <vector>
#include <memory>

static char const* const filename = "test.cpp";
static char const* const code = "int test(int x) { return x + 1; }";

std::string test_clang()
{
    {
        static bool did_init = false;
        if(!did_init)
        {
            did_init = true;
            LLVMInitializeAVRAsmPrinter();
            LLVMInitializeAVRTargetMC();
            LLVMInitializeAVRTargetInfo();
            LLVMInitializeAVRTarget();
        }
    }

    std::vector<char const*> args;
    args.push_back("-Os");
    args.push_back(filename);

    std::string err_string;
    llvm::raw_string_ostream err_stream(err_string);
    auto diag_opts = llvm::makeIntrusiveRefCnt<clang::DiagnosticOptions>();

    llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diag_ids;
    auto diag_engine = std::make_unique<clang::DiagnosticsEngine>(
        diag_ids, diag_opts,
        new clang::TextDiagnosticPrinter(err_stream, diag_opts.get()));

    auto ci = std::make_shared<clang::CompilerInvocation>();
    clang::CompilerInvocation::CreateFromArgs(
        *ci, llvm::ArrayRef(&args[0], &args[0] + args.size()), *diag_engine);

    {
        auto buffer = llvm::MemoryBuffer::getMemBufferCopy(code);
        ci->getPreprocessorOpts().addRemappedFile(filename, buffer.get());
        buffer.release();
    }

    clang::CompilerInstance cinst;
    cinst.setInvocation(ci);
    cinst.createDiagnostics();

    //auto fm = std::make_unique<clang::FileManager>(
    //    clang::FileSystemOptions{""},
    //    llvm::makeIntrusiveRefCnt<clang_vfs>());
    //cinst.setFileManager(fm.get());

    auto const target_opts = std::make_shared<clang::TargetOptions>();
    target_opts->Triple = "avr";
    target_opts->CPU = "atmega32u4";
    ci->TargetOpts = target_opts;
    auto* target_info = clang::TargetInfo::CreateTargetInfo(*diag_engine, target_opts);
    cinst.setTarget(target_info);

    //auto compiler_action = std::make_unique<clang::EmitAssemblyAction>();
    auto compiler_action = std::make_unique<clang::EmitObjAction>();
    cinst.ExecuteAction(*compiler_action);

    return err_string;
}
