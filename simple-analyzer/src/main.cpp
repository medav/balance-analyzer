
#include <iostream>
#include <memory>

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

struct PortAssignment {
    std::vector<int> port_values;


    bool Balanced() const {
        for (int i : port_values) {
            if (i != 0) {
                return false;
            }
        }

        return true;
    }
};

struct AssignmentSet {
    std::vector<PortAssignment> assignments;

    AssignmentSet() { }

    AssignmentSet(std::vector<PortAssignment>&& _assignments)
        : assignments(_assignments) { }

    AssignmentSet operator+(const AssignmentSet& other) const {
        std::vector<PortAssignment> new_list;
        new_list.reserve(this->assignments.size() + other.assignments.size());
        new_list.insert(new_list.end(), this->assignments.begin(), this->assignments.end());
        new_list.insert(new_list.end(), other.assignments.begin(), other.assignments.end());
        return AssignmentSet(std::move(new_list));
    }

    void Simplify() {
        // TODO: Dedupe and reduce <N, N, ...> to <0, 0, ...>
    }

    bool operator==(const AssignmentSet& other) const {
        return false;
    }

    bool AlwaysBalanced() const {
        return assignments.size() == 1 && assignments.front().Balanced();
    }
};

using AssignmentSetState = analysis::AbstractState<AssignmentSet>;
using AssignmentSetResult = analysis::DataflowResult<AssignmentSet>;

class AssignmentSetCombine : public analysis::Meet<AssignmentSet, AssignmentSetCombine> {
public:
    AssignmentSet meetPair(AssignmentSet &s1, AssignmentSet &s2) const {
        AssignmentSet result = s1 + s2;
        result.Simplify();
        return result;
    }
};

class AssignmentSetExtend
{
    llvm::Function* getCalledFunction(llvm::CallSite cs) {
        auto* calledValue = cs.getCalledValue()->stripPointerCasts();
        return llvm::dyn_cast<llvm::Function>(calledValue);
    }

public:
    void operator()(llvm::Value &i, AssignmentSetState &state) {
        llvm::CallSite cs(&i);
        if (!cs.getInstruction()) return;

        llvm::Function * func = getCalledFunction(cs);
        if (func->isDeclaration()) return;

        if (func == SB_CONFIG) {
            std::cout << "SB_CONFIG!" << std::endl;
        }
    }
};

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


    using Value    = AssignmentSet;
    using Transfer = AssignmentSetExtend;
    using Meet     = AssignmentSetCombine;
    using Analysis = analysis::DataflowAnalysis<Value, Transfer, Meet>;
    Analysis analysis{*module, main_func};
    auto results = analysis.computeDataflow();
    // for (auto& [context, contextResults] : results) {
    //     for (auto& [function, functionResults] : contextResults) {
    //         printConstantArguments(functionResults);
    //     }
    // }

    return 0;
}
