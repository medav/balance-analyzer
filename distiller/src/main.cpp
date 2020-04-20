
#include <iostream>
#include <memory>
#include <unordered_map>
#include <fstream>

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

#include "dfa.h"

using namespace llvm;
using namespace analysis;

using BbIdMap = std::unordered_map<llvm::BasicBlock*, int>;

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

static cl::opt<std::string> out_filename {
    cl::Positional,
    cl::desc{"<Output Filename>"},
    cl::value_desc{""},
    cl::init(""),
    cl::Required,
    cl::cat{balance_cat}};

static llvm::Function *SB_CONFIG;
static llvm::Function *SB_WAIT;
static llvm::Function *SB_MEM_PORT_STREAM;
static llvm::Function *SB_CONSTANT;
static llvm::Function *SB_PORT_MEM_STREAM;
static llvm::Function *SB_DISCARD;

static std::ofstream out_file;

llvm::Function* getCalledFunction(llvm::CallSite cs) {
    auto* calledValue = cs.getCalledValue()->stripPointerCasts();
    return llvm::dyn_cast<llvm::Function>(calledValue);
}

const int ExtractConstant(const llvm::Value * val) {
    if (auto* constant = llvm::dyn_cast<llvm::ConstantInt>(val)) {
        return constant->getValue().getLimitedValue();
    }
    return -1;
}

void ProcessCall(int bb_id, int inst_id, llvm::CallSite& cs) {
    llvm::Function * func = getCalledFunction(cs);
    if (func->isDeclaration()) return;

    if (func == SB_CONFIG) {
        out_file << bb_id << "," << inst_id << ","
            << "SB_CONFIG"
            << std::endl;
    }
    else if (func == SB_MEM_PORT_STREAM) {
        out_file << bb_id << "," << inst_id << ","
            << "SB_MEM_PORT_STREAM,"
            /* << "port = " */ << ExtractConstant(cs.getArgument(4)) << ","
            /* << "stride = " */ << ExtractConstant(cs.getArgument(1)) << ","
            /* << "access_size = " */ << ExtractConstant(cs.getArgument(2)) << ","
            /* << "nstrides = " */ << ExtractConstant(cs.getArgument(3))
            << std::endl;
    }
    else if (func == SB_CONSTANT) {
        out_file << bb_id << "," << inst_id << ","
            << "SB_CONSTANT,"
            /* << "port = " */ << ExtractConstant(cs.getArgument(0)) << ","
            /* << "nelems = " */ << ExtractConstant(cs.getArgument(2))
            << std::endl;
    }
    else if (func == SB_PORT_MEM_STREAM) {
        out_file << bb_id << "," << inst_id << ","
            << "SB_PORT_MEM_STREAM,"
            /* << "port = " */ << ExtractConstant(cs.getArgument(0)) << ","
            /* << "stride = " */ << ExtractConstant(cs.getArgument(1)) << ","
            /* << "access_size = " */ << ExtractConstant(cs.getArgument(2)) << ","
            /* << "nstrides = " */ << ExtractConstant(cs.getArgument(3))
            << std::endl;
    }
    else if (func == SB_DISCARD) {
        out_file << bb_id << "," << inst_id << ","
            << "SB_DISCARD,"
            /* << "port = " */ << ExtractConstant(cs.getArgument(0)) << ","
            /* << "nelems = " */ << ExtractConstant(cs.getArgument(1))
            << std::endl;
    }
    else if (func == SB_WAIT) {
        out_file << bb_id << "," << inst_id << ","
            << "SB_WAIT" << std::endl;
    }
}

void ProcessInst(int bb_id, int inst_id, auto& i) {
    llvm::CallSite cs(&i);
    if (cs.getInstruction()) {
        ProcessCall(bb_id, inst_id, cs);
        return;
    }


}

void ProcessBasicBlock(BbIdMap& bb_id_map, auto * bb) {
    int bb_id = bb_id_map[bb];
    int inst_id = 0;

    for (auto& i : Forward::getInstructions(*bb)) {
        ProcessInst(bb_id, inst_id++, i);
    }

    int num_successors = 0;

    for (auto* s : Forward::getSuccessors(*bb)) {
        num_successors++;
    }

    out_file << bb_id << "," << inst_id << ",control,";
    for (auto* s : Forward::getSuccessors(*bb)) {
        out_file << bb_id_map[s] << ",";
    }
    out_file << std::endl;

}

int main(int argc, char **argv) {
    sys::PrintStackTraceOnErrorSignal(argv[0]);
    PrettyStackTraceProgram X(argc, argv);
    llvm_shutdown_obj shutdown;
    cl::HideUnrelatedOptions(balance_cat);
    cl::ParseCommandLineOptions(argc, argv);
    int i = 0;

    // Construct an IR file from the filename passed on the command line.
    SMDiagnostic err;
    LLVMContext context;
    std::unique_ptr<Module> module = llvm::parseIRFile(input_path.getValue(), err, context);

    out_file.open(out_filename.getValue(), std::ios::trunc);

    if (!module.get()) {
        errs() << "Error reading bitcode file: " << input_path << "\n";
        err.print(argv[0], errs());
        return -1;
    }

    auto *main_func = module->getFunction("main");

    if (!main_func) {
        llvm::report_fatal_error("Unable to find main function.");
    }

    SB_CONFIG = module->getFunction("SB_CONFIG");
    SB_WAIT = module->getFunction("SB_WAIT");
    SB_MEM_PORT_STREAM = module->getFunction("SB_MEM_PORT_STREAM");
    SB_CONSTANT = module->getFunction("SB_CONSTANT");
    SB_PORT_MEM_STREAM = module->getFunction("SB_PORT_MEM_STREAM");
    SB_DISCARD = module->getFunction("SB_DISCARD");

    auto traversal = Forward::getFunctionTraversal(*main_func);
    BasicBlockWorklist labeling_work(traversal.begin(), traversal.end());

    BbIdMap bb_id_map;
    std::set<llvm::BasicBlock*> bb_seen;

    while (!labeling_work.empty()) {
        auto* bb = labeling_work.take();

        if (bb_seen.count(bb) > 0) {
            continue;
        }

        bb_id_map[bb] = i++;

        for (auto* s : Forward::getSuccessors(*bb)) {
            labeling_work.add(s);
        }

        bb_seen.insert(bb);
    }

    BasicBlockWorklist process_work(traversal.begin(), traversal.end());
    bb_seen.clear();

    while (!process_work.empty()) {
        auto* bb = process_work.take();

        if (bb_seen.count(bb) > 0) {
            continue;
        }

        ProcessBasicBlock(bb_id_map, bb);

        for (auto* s : Forward::getSuccessors(*bb)) {
            process_work.add(s);
        }

        bb_seen.insert(bb);
    }

    out_file.close();

    return 0;
}
