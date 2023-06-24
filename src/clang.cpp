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
static char const* const code = "int test(int x, int y) { return x / y + 1; }";

/*

E:\ProjectBuilds\llvm_subproject\Release\bin\clang.exe -c --target=avr -mmcu=atmega32u4 -Os -DF_CPU=16000000L -D__AVR_ARCH__ -DUSB_VID=0x2341 -DUSB_PID=0x8036 -DUSB_MANUFACTURER="ArduboyInc" -DUSB_PRODUCT="Arduboy" -DARDUBOY_10 -DCART_CS_SDA -ffunction-sections -fdata-sections -fno-threadsafe-statics -fno-exceptions -isystem E:\Brown\Downloads\toolchain\include\std  -isystem E:\Brown\Downloads\toolchain\include\core -D__ATTR_PROGMEM__=__attribute__((section(\".progmem1.data\"))) test.cpp

E:\ProjectBuilds\llvm_subproject\Release\bin\ld.lld.exe -TE:\Brown\Downloads\toolchain\avr5.xn --static --defsym=_start=0 --defsym=__DATA_REGION_ORIGIN__=0x800100 --defsym=__TEXT_REGION_LENGTH__=32768 --defsym=__DATA_REGION_LENGTH__=2560 --gc-sections E:\Brown\Downloads\toolchain\lib\crtatmega32u4.o test.o -LE:\Brown\Downloads\toolchain\lib --start-group -lgcc -lm -lc -latmega32u4 -lcore --end-group -o test.elf

*/

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
    args.push_back("-ffunction-sections");
    args.push_back("-fdata-sections");
    //args.push_back("-fno-exceptions");
    args.push_back("-fno-threadsafe-statics");
    args.push_back("-D__AVR_ARCH__");
    args.push_back("-DF_CPU=16000000L");
    args.push_back("-DUSB_VID=0x2341");
    args.push_back("-DUSB_PID=0x8036");
    args.push_back("-DUSB_MANUFACTURER=\"Arduboy Inc\"");
    args.push_back("-DUSB_PRODUCT=\"Arduboy\"");
    args.push_back("-DARDUBOY_10");
    args.push_back("-DCART_CS_SDA");
    args.push_back("-D__ATTR_PROGMEM__=__attribute__((section(\".progmem1.data\")))");
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

    ci->getLangOpts()->setExceptionHandling(
        clang::LangOptions::ExceptionHandlingKind::None);

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

    auto compiler_action = std::make_unique<clang::EmitAssemblyAction>();
    //auto compiler_action = std::make_unique<clang::EmitObjAction>();
    cinst.ExecuteAction(*compiler_action);

    return err_string;
}
