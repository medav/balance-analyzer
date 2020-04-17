
from model import *

class Analyzer():
    def __init__(self, filename):
        self.filename = filename
        self.loop_trip_counts = {}
        self.cond_trip_counts = {}
        self.constraints = []
        self.nodes = self.BuildGraph(filename)
        self.entry = self.nodes[0]
        self.exit = self.FindExit()
        # self.wait = self.FindWait()
        # self.RemoveLoops()
        # self.ComputeDataflow()

    def FindExit(self):
        # The exit node is the only control node with no targets
        for n in self.nodes:
            if type(n) is ControlNode and len(n.targets) == 0:
                return n

    def PostProcess(self, entry=None, stop_before=None):
        """ Post Process the CFG

        This is operates recursively, almost like a post-order tree traversal.
        While the CFG is potentially / likely a cyclic graph, this routine will
        break cycles along the way and turn this into a straight line graph
        (potentially with conditional branches which reconverge).
        """

        if entry is None:
            entry = self.entry

        if stop_before is None:
            stop_before = self.exit

        if entry == stop_before:
            return

        if type(entry) is ControlNode and entry.IsLoopEntry():
            tc = Int('loop_{}'.format(entry.Name()))
            self.loop_trip_counts[entry] = tc
            loop_body = entry.GetLoopBody()
            self.PostProcess(loop_body)
            self.ApplyTripCount(loop_body, tc, entry)

        elif type(entry) is ControlNode and entry.IsConditional():
            tc = Int('cond_{}'.format(entry.Name()))
            self.cond_trip_counts[entry] = tc
            cond_exit = entry.GetCondExit()
            true_branch = entry.GetTrueBranch()
            false_branch = entry.GetFalseBranch()
            self.PostProcess(true_branch, cond_exit)
            self.PostProcess(false_branch, cond_exit)
            self.ApplyTripCount(true_branch, tc, cond_exit)
            self.ApplyTripCount(false_branch, tc, cond_exit)

        else:
            assert len(entry.targets) == 1, 'Not a conditional or loop?'
            self.PostProcess(entry.targets[0], stop_before)

    def DumpDot(self, f):
        print('digraph G {', file=f)
        for n in self.nodes:
            print('\t{} [label = "{}"];'.format(n.Name(), n.TypeName()))
            for t in n.targets:
                print(
                    '\t{} -> {};'.format(n.Name(), t.Name()),
                    file=f)

        print('}', file=f)

    @staticmethod
    def ComputeSymbolicNumElems(num_iters, stride=None, access_size=None):
        return 1

    @staticmethod
    def BuildNode(line):
        parts = list(filter(
            lambda p: len(p) > 0,
            map(lambda p: p.strip(), line.split(','))))

        bb_id = int(parts[0])
        inst_id = int(parts[1])

        if parts[2] == 'control':
            return ControlNode(
                bb_id,
                inst_id,
                list(map(int, parts[3:])))

        if parts[2] == 'SB_CONFIG':
            return ConfigNode(bb_id, inst_id)

        elif parts[2] == 'SB_WAIT':
            return WaitNode(bb_id, inst_id)

        elif parts[2] == 'SB_MEM_PORT_STREAM':
            return PortNode(
                bb_id,
                inst_id,
                int(parts[3]),
                Analyzer.ComputeSymbolicNumElems(parts[6], parts[4], parts[5]))

        elif parts[2] == 'SB_CONSTANT':
            return PortNode(
                bb_id,
                inst_id,
                int(parts[3]),
                Analyzer.ComputeSymbolicNumElems(parts[4]))

        elif parts[2] == 'SB_DISCARD':
            return PortNode(
                bb_id,
                inst_id,
                int(parts[3]),
                Analyzer.ComputeSymbolicNumElems(parts[4]))

        elif parts[2] == 'SB_PORT_MEM_STREAM':
            return PortNode(
                bb_id,
                inst_id,
                int(parts[3]),
                Analyzer.ComputeSymbolicNumElems(parts[6], parts[4], parts[5]))

        assert False, line

    @staticmethod
    def BuildGraph(filename):
        nodes = []
        entry_points = {}
        exit_points = []
        num_bbs = 0

        # Initially just build all the nodes
        with open(filename) as f:
            for line in f:
                n = Analyzer.BuildNode(line)
                num_bbs = max(n.bb_id, num_bbs)
                nodes.append(n)

        num_bbs += 1

        #
        # Now link the nodes by their control flow
        #

        # First find basic block entry nodes and link instructions inside each
        # block.
        for bb_id in range(num_bbs):
            bb = list(filter(lambda n: n.bb_id == bb_id, nodes))
            sorted(bb, key=lambda n: n.inst_id)

            entry_points[bb_id] = bb[0]
            exit_points.append(bb[-1])

            # Link up instructions in side each basic block
            for i in range(len(bb) - 1):
                n1 = bb[i]
                n2 = bb[i + 1]
                n1.targets.append(n2)
                n2.num_sources += 1

        # Now link basic blocks by their control nodes
        for bb_id in range(num_bbs):
            cn = exit_points[bb_id]
            for target_id in cn.target_bbs:
                cn.targets.append(entry_points[target_id])

        return nodes


