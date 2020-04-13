
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

    // Invariant: PortAssignments are always guaranteed to be in "normal form"
    // (I.e. at least one value is 0).

    PortAssignment(const std::vector<int>& _port_values)
        : port_values(_port_values) {

        int min = *std::min_element(port_values.begin(), port_values.end());
        for (int& i : port_values) {
            i -= min;
        }
    }

    bool operator==(const PortAssignment& other) const {
        int i;

        if (port_values.size() != other.port_values.size()) {
            return false;
        }

        for (i = 0; i < port_values.size(); i++) {
            if (port_values[i] != other.port_values[i]) {
                return false;
            }
        }

        return true;
    }

    std::size_t hash() const {
        int sum = 0;

        for (int i : port_values) {
            sum += i;
        }

        return sum;
    }

    bool Balanced() const {
        // Only need to check against 0 since the only normal form balanced
        // assignment is <0, 0, ...>
        for (int i : port_values) {
            if (i != 0) {
                return false;
            }
        }

        return true;
    }
};

namespace std {

    template <>
    struct hash<PortAssignment> {
        std::size_t operator()(const PortAssignment& k) const {
            using std::size_t;
            using std::hash;
            return k.hash();
        }
    };

}

struct AssignmentSet {
    std::unordered_set<PortAssignment> assignments;

    AssignmentSet() { }

    AssignmentSet(std::unordered_set<PortAssignment>&& _assignments)
        : assignments(_assignments) { }

    AssignmentSet operator+(const AssignmentSet& other) const {
        std::unordered_set<PortAssignment> new_assignments = this->assignments;

        new_assignments.insert(
            other.assignments.begin(),
            other.assignments.end());

        return AssignmentSet(std::move(new_assignments));
    }

    bool operator==(const AssignmentSet& other) const {
        return false;
    }

    bool AlwaysBalanced() const {
        return assignments.size() == 1 && assignments.begin()->Balanced();
    }
};

using AssignmentSetState = analysis::AbstractState<AssignmentSet>;
using AssignmentSetResult = analysis::DataflowResult<AssignmentSet>;

class AssignmentSetCombine : public analysis::Meet<AssignmentSet, AssignmentSetCombine> {
public:
    AssignmentSet meetPair(AssignmentSet &s1, AssignmentSet &s2) const {
        return s1 + s2;
    }
};

class AssignmentSetExtend
{
    llvm::Function* getCalledFunction(llvm::CallSite cs) {
        auto* calledValue = cs.getCalledValue()->stripPointerCasts();
        return llvm::dyn_cast<llvm::Function>(calledValue);
    }

    const llvm::APInt ExtractConstant(const llvm::Value * val) {
        if (auto* constant = llvm::dyn_cast<llvm::ConstantInt>(val)) {
            return constant->getValue();
        }
        assert(false);
    }

public:
    void operator()(llvm::Value &i, AssignmentSetState &state) {
        llvm::CallSite cs(&i);
        if (!cs.getInstruction()) return;

        llvm::Function * func = getCalledFunction(cs);
        if (func->isDeclaration()) return;

        if (func == SB_CONFIG) {
            std::cout << "SB_CONFIG("
                << ")" << std::endl;
        }
        else if (func == SB_MEM_PORT_STREAM) {
            std::cout << "SB_MEM_PORT_STREAM("
                << "port = " << ExtractConstant(cs.getArgument(4)).getLimitedValue() << ", "
                << "stride = " << ExtractConstant(cs.getArgument(1)).getLimitedValue() << ", "
                << "access_size = " << ExtractConstant(cs.getArgument(2)).getLimitedValue() << ", "
                << "nstrides = " << ExtractConstant(cs.getArgument(3)).getLimitedValue()
                << ")" << std::endl;
        }
        else if (func == SB_CONSTANT) {
            std::cout << "SB_CONSTANT("
                << "port = " << ExtractConstant(cs.getArgument(0)).getLimitedValue() << ", "
                << "nelems = " << ExtractConstant(cs.getArgument(2)).getLimitedValue()
                << ")" << std::endl;
        }
        else if (func == SB_PORT_MEM_STREAM) {
            std::cout << "SB_PORT_MEM_STREAM("
                << "port = " << ExtractConstant(cs.getArgument(0)).getLimitedValue() << ", "
                << "stride = " << ExtractConstant(cs.getArgument(1)).getLimitedValue() << ", "
                << "access_size = " << ExtractConstant(cs.getArgument(2)).getLimitedValue() << ", "
                << "nstrides = " << ExtractConstant(cs.getArgument(3)).getLimitedValue()
                << ")" << std::endl;
        }
        else if (func == SB_DISCARD) {
            std::cout << "SB_DISCARD("
                << "port = " << ExtractConstant(cs.getArgument(0)).getLimitedValue() << ", "
                << "nelems = " << ExtractConstant(cs.getArgument(1)).getLimitedValue()
                << ")" << std::endl;
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
