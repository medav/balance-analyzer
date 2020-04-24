
from z3 import *

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

    def FindExit(self):
        # The exit node is the only control node with no targets
        for n in self.nodes:
            if type(n) is ControlNode and len(n.targets) == 0:
                return n

    def ApplyTripCount(self, entry, tc, stop_before):
        seen = set()
        work_list = [entry]

        while len(work_list) > 0:
            n = work_list.pop(0)
            seen.add(n)
            n.trip_count = tc * n.trip_count

            if n is stop_before:
                continue

            for t in n.targets:
                if t not in seen:
                    work_list.append(t)


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
            loop_exit = entry.GetLoopExit()
            reentry = entry.GetLoopReentry()

            # Before anything else, we post process the loop body. This is the
            # "pre-order" part of this traversal.
            self.PostProcess(loop_body, entry)
            self.ApplyTripCount(loop_body, tc, entry)

            # Now Post Process the rest of the graph starting at the loop exit.
            self.PostProcess(loop_exit, stop_before)

            # Finally, unlink the loop.
            reentry.targets = [loop_exit]
            loop_exit.sources = [reentry]
            entry.targets = [loop_body]
            loop_body.sources = [entry]
            entry.sources.remove(reentry)

        elif type(entry) is ControlNode and entry.IsConditional():
            tc_t = Int('cond_{}_t'.format(entry.Name()))
            tc_f = Int('cond_{}_f'.format(entry.Name()))

            # Record the branch trip counts. This table is used to generate
            # logic constraints when performing model checking
            self.cond_trip_counts[entry] = (entry, tc_t, tc_f)

            cond_exit = entry.GetCondExit()
            true_branch = entry.GetTrueBranch()
            false_branch = entry.GetFalseBranch()

            self.PostProcess(true_branch, cond_exit)
            self.PostProcess(false_branch, cond_exit)

            # Apply the trip counts to the two branches
            self.ApplyTripCount(true_branch, tc_t, cond_exit)
            self.ApplyTripCount(false_branch, tc_f, cond_exit)

            # Post process the rest of the graph
            self.PostProcess(cond_exit, stop_before)

            # Re-link the branches to be in-line
            pred = cond_exit.sources[0]
            cond_exit.sources.remove(pred)
            if true_branch.IsReachable(pred):
                entry.targets.remove(false_branch)
                pred.targets = [false_branch]
                false_branch.sources = [pred]
            else:
                entry.targets.remove(true_branch)
                pred.targets = [true_branch]
                true_branch.sources = [pred]

        else:
            #assert len(entry.targets) == 1, 'Not a conditional or loop?'
            if len(entry.targets) != 1:
                print('Error! Expecting a conditional or loop!')
                print('name = ', entry.Name())
                print('sources = ', entry.sources)
                print('targets = ', entry.targets)
                raise RuntimeError('Not a conditional or loop?')
            self.PostProcess(entry.targets[0], stop_before)

    def DumpDot(self, f):
        print('digraph G {', file=f)
        for n in self.nodes:
            print('\t{} [label = "{}"];'.format(n.Name(), n.Name()), file=f)
            for t in n.targets:
                print(
                    '\t{} -> {};'.format(n.Name(), t.Name()),
                    file=f)

        print('}', file=f)

    @staticmethod
    def ComputeSymbolicNumElems(num_iters, stride=None, access_size=None):
        return int(num_iters)

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
                n2.sources.append(n1)

        # Now link basic blocks by their control nodes
        for bb_id in range(num_bbs):
            cn = exit_points[bb_id]
            for target_id in cn.target_bbs:
                if target_id == bb_id:
                    continue

                cn.targets.append(entry_points[target_id])
                entry_points[target_id].sources.append(cn)

        return nodes

    def CollectDataflow(self, node, num_ports):
        if node == self.entry:
            val =  node.Apply(None, num_ports)
            return val

        if len(node.sources) != 1:
            print('name = ', node.Name())
            print('sources = ', node.sources)
            print('targets = ', node.targets)

        assert len(node.sources) == 1, \
            'Node has {} sources! (Should have 1)'.format(len(node.sources))

        s = node.sources[0]
        val = self.CollectDataflow(s, num_ports)
        return node.Apply(val, num_ports)
