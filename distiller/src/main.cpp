
#include <iostream>
#include <memory>
#include <unordered_set>

#include "llvm/ADT/APSInt.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

static cl::OptionCategory balance_cat{"balance analyzer options"};

static cl::opt<std::string> input_path {
    cl::Positional,
    cl::desc{"<Module to analyze>"},
    cl::value_desc{"bitcode filename"},
    cl::init(""),
    cl::Required,
    cl::cat{balance_cat}};

static cl::opt<int> num_ports {
    cl::Positional,
    cl::desc{"<Number of Ports>"},
    cl::value_desc{""},
    cl::init(1),
    cl::Required,
    cl::cat{balance_cat}};

static llvm::Function * SB_CONFIG;
static llvm::Function * SB_WAIT;
static llvm::Function * SB_MEM_PORT_STREAM;
static llvm::Function * SB_CONSTANT;
static llvm::Function * SB_PORT_MEM_STREAM;
static llvm::Function * SB_DISCARD;

int main(int argc, char **argv) {

    sys::PrintStackTraceOnErrorSignal(argv[0]);
    PrettyStackTraceProgram X(argc, argv);
    llvm_shutdown_obj shutdown;
    cl::HideUnrelatedOptions(balance_cat);
    cl::ParseCommandLineOptions(argc, argv);

    // Construct an IR file from the filename passed on the command line.
    SMDiagnostic err;
    LLVMContext context;
    std::unique_ptr<Module> module = llvm::parseIRFile(input_path.getValue(), err, context);

    if (!module.get()) {
        errs() << "Error reading bitcode file: " << input_path << "\n";
        err.print(argv[0], errs());
        return -1;
    }

    auto * main_func = module->getFunction("main");

    if (!main_func) {
        llvm::report_fatal_error("Unable to find main function.");
    }

    SB_CONFIG = module->getFunction("SB_CONFIG");
    SB_WAIT = module->getFunction("SB_WAIT");
    SB_MEM_PORT_STREAM = module->getFunction("SB_MEM_PORT_STREAM");
    SB_CONSTANT = module->getFunction("SB_CONSTANT");
    SB_PORT_MEM_STREAM = module->getFunction("SB_PORT_MEM_STREAM");
    SB_DISCARD = module->getFunction("SB_DISCARD");


    return 0;
}
