
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
	
	const PortAssignment AddAtPort(int portNum, int value) {		
		std::vector<int> copy = port_values;
		copy[portNum] += value; 
				
		return PortAssignment(copy);
	}
	
	bool isBalanced() {
		return (*std::max_element(port_values.begin(), port_values.end()) == 0);
	}
};

std::ostream& operator<<(std::ostream& os, const PortAssignment& p) {
	os << '<';
	
	int c = 0;
	for (auto i = p.port_values.begin(); i != p.port_values.end(); i++) {
		
		if (c > 0) {
			os << ", ";
		}
		os << (*i);
		c++;
	}
	os << '>';
	
	return os;
}

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
	
	void AddAtPort(int portNum, int value) {
		std::unordered_set<PortAssignment> updated;
		for (auto i : assignments) {
			PortAssignment p = i.AddAtPort(portNum, value);
			updated.insert(p);
		}
		assignments = updated;
	}
	
	bool isBalanced() {
		for (auto a : assignments) {
			if (a.isBalanced()) {
				return true;
			}
		}
		return false;
	}
};


std::ostream& operator<<(std::ostream& os, const AssignmentSet& a) {	
	for (auto i = a.assignments.begin(); i != a.assignments.end(); i++) {
		os << (*i) << '\n';
	}
	
	return os;
}


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
				
			PortAssignment bottom(std::vector<int> (num_ports,0));
			
			std::unordered_set<PortAssignment> pas;
			pas.insert(bottom);
			
			AssignmentSet as;
			as.assignments = pas;
			
			state[&i] = as;
			
			std::cout << state[&i] << '\n';
        }
        else if (func == SB_MEM_PORT_STREAM) {
			std::cout << "SB_MEM_PORT_STREAM("
                << "port = " << ExtractConstant(cs.getArgument(4)).getLimitedValue() << ", "
                << "stride = " << ExtractConstant(cs.getArgument(1)).getLimitedValue() << ", "
                << "access_size = " << ExtractConstant(cs.getArgument(2)).getLimitedValue() << ", "
                << "nstrides = " << ExtractConstant(cs.getArgument(3)).getLimitedValue()
                << ")" << std::endl;
				
			// number of elements = nstrides * access_size / 8
			// ports are numbered starting at 1
			
			int port = ExtractConstant(cs.getArgument(4)).getLimitedValue();
			int access_size = ExtractConstant(cs.getArgument(2)).getLimitedValue();
			int nstrides = ExtractConstant(cs.getArgument(3)).getLimitedValue();
			
			
			// This does not work 
			// (want to access most recently added, not guaranteed to be at end)
			/*
			for (auto j = state.begin(); j != state.end(); j++) {
				if (std::next(j) == state.end()) {
					state[&i] = state[&i] + j->second;
				}
			}
			*/
			
			for (auto j : state) {
				state[&i] = state[&i] + j.second;
			}			
			
			state[&i].AddAtPort(port-1, nstrides * access_size / 8);
			std::cout << state[&i] << '\n';
        }
        else if (func == SB_CONSTANT) {
			std::cout << "SB_CONSTANT("
                << "port = " << ExtractConstant(cs.getArgument(0)).getLimitedValue() << ", "
                << "nelems = " << ExtractConstant(cs.getArgument(2)).getLimitedValue()
                << ")" << std::endl;
				
			int port = ExtractConstant(cs.getArgument(0)).getLimitedValue();
			int nelems = ExtractConstant(cs.getArgument(2)).getLimitedValue();
			
			
			for (auto j : state) {
				state[&i] = state[&i] + j.second;
			}
			
			state[&i].AddAtPort(port-1, nelems);
			std::cout << state[&i] << '\n';
        }
        else if (func == SB_PORT_MEM_STREAM) {
			std::cout << "SB_PORT_MEM_STREAM("
                << "port = " << ExtractConstant(cs.getArgument(0)).getLimitedValue() << ", "
                << "stride = " << ExtractConstant(cs.getArgument(1)).getLimitedValue() << ", "
                << "access_size = " << ExtractConstant(cs.getArgument(2)).getLimitedValue() << ", "
                << "nstrides = " << ExtractConstant(cs.getArgument(3)).getLimitedValue()
                << ")" << std::endl;
				
			// number of elements = nstrides * access_size / 8
			// ports are numbered starting at 1
			
			int port = ExtractConstant(cs.getArgument(0)).getLimitedValue();
			int access_size = ExtractConstant(cs.getArgument(2)).getLimitedValue();
			int nstrides = ExtractConstant(cs.getArgument(3)).getLimitedValue();
			
			for (auto j : state) {
				state[&i] = state[&i] + j.second;
			}
			
			state[&i].AddAtPort(port-1, nstrides * access_size / 8);
			std::cout << state[&i] << '\n';
        }
        else if (func == SB_DISCARD) {
			std::cout << "SB_DISCARD("
                << "port = " << ExtractConstant(cs.getArgument(0)).getLimitedValue() << ", "
                << "nelems = " << ExtractConstant(cs.getArgument(1)).getLimitedValue()
                << ")" << std::endl;
				
			int port = ExtractConstant(cs.getArgument(0)).getLimitedValue();
			int nelems = ExtractConstant(cs.getArgument(1)).getLimitedValue();
			
			for (auto j : state) {
				state[&i] = state[&i] + j.second;
			}
			
			
			state[&i].AddAtPort(port-1, nelems);
			std::cout << state[&i] << '\n';	
        }
		/*
		else if (func == SB_WAIT) {
			for (auto i : state) {
				state[func] = state[func] + i.second;
			}
		}
		*/
    }
};

static void
printWaitBalance(AssignmentSetResult& functionResults) {
	for (auto& [value,localState] : functionResults) {
		auto* inst = llvm::dyn_cast<llvm::Instruction>(value);
		if (!inst) {
		  continue;
		}

		llvm::CallSite cs{inst};
		if (!cs.getInstruction()) {
		  continue;
		}
			
		std::string fnName = "";
		
		//identify somehow what function is being called here?
		
		if (fnName == "SB_WAIT") {
			auto& state = analysis::getIncomingState(functionResults, *inst);
			
			bool hasBalancedOption = std::any_of(cs.arg_begin(), cs.arg_end(),
				[&state] (auto& use) { return state[use.get()].isBalanced(); });
			if (!hasBalancedOption) {
				continue;
			}
			else {
				llvm::outs() << "SB_WAIT is possibly balanced.";
				llvm::outs() << "\n\n";
			}
		}
	}
}

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
	
    for (auto& [context, contextResults] : results) {
        for (auto& [function, functionResults] : contextResults) {
			
			printWaitBalance(functionResults);
			
            /*
			if (function == SB_WAIT) {
				std::cout << "function wait\n";
			}
			*/
			/*
			if (functionResults == SB_WAIT) {
				std::cout << "functionResults wait\n";
			}
			*/
        }
    }

    return 0;
}
