
from z3 import *

from model import *

import os
import subprocess

def CompileDot(dotfile, outdir='edits'):
    if not os.path.exists(outdir):
        os.mkdir(outdir)

    subprocess.check_output([
        'dot',
        '-Tpng',
        dotfile,
        '-o',
        '{}/{}'.format(outdir, dotfile.replace('.dot', '.png'))
    ])

class Analyzer():
    def __init__(self, filename):
        self.filename = filename
        self.loop_trip_counts = {}
        self.cond_trip_counts = []
        self.constraints = []
        self.nodes = self.BuildGraph(filename)
        self.entry = self.nodes[0]
        self.exit = self.FindExit()
        self.edit = 0
        self.indent = 0
        self.debug = False

    def FindExit(self):
        # The exit node is the only control node with no targets
        for n in self.nodes:
            if type(n) is ControlNode and len(n.targets) == 0:
                return n

    def CreateTripCount(self, tc_name):
        tc = Int(tc_name)
        self.constraints.append(tc >= 0)
        return tc

    def ApplyTripCount(self, entry, tc, stop_before):
        seen = set()
        work_list = [entry]

        while len(work_list) > 0:
            n = work_list.pop(0)

            if n is stop_before:
                continue

            seen.add(n)
            n.trip_count = tc * n.trip_count

            for t in n.targets:
                if t not in seen and t is not stop_before:
                    work_list.append(t)

    def GenConstraints(self):
        cs = self.constraints + [
            tb.trip_count + fb.trip_count == c.trip_count
            for (c, tb, fb) in self.cond_trip_counts
        ]

        return And(*cs)

    def NewEditFile(self, node):
        filename = 'edit{}-{}.dot'.format(self.edit, node.Name())
        self.edit += 1
        return filename

    def DumpEditDot(self, node):
        if not self.debug:
            return

        dotfile = self.NewEditFile(node)
        with open(dotfile, 'w') as f:
            self.DumpDot(f)

        self.Debug('Capturing Edit! File = {}'.format(dotfile))
        CompileDot(dotfile)

    def Debug(self, msg):
        if not self.debug:
            return

        print(' |  '*self.indent, msg)

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

        self.Debug('PostProcess({}) {{'.format(entry.Name()))
        self.indent += 1

        # In cases where we are processing a loop, the reentry node will have
        # its target links removed to force processing to terminate. This will
        # result in len(targets) == 0
        if len(entry.targets) == 0:
            self.indent -= 1
            self.Debug('}')
            return


        if type(entry) is ControlNode and entry.IsLoopEntry():
            tc = self.CreateTripCount('loop_{}'.format(entry.Name()))
            self.loop_trip_counts[entry] = tc

            self.Debug('Processing loop')

            loop_body = entry.GetLoopBody()
            loop_exit = entry.GetLoopExit()
            reentry = entry.GetLoopReentry()

            self.Debug('Body: {}'.format(loop_body.Name()))
            self.Debug('Exit: {}'.format(loop_exit.Name()))
            self.Debug('Re-Entry: {}'.format(reentry.Name()))

            # Temporarily set the reentry to have no targets. This ensures the
            # recursive call doesn't accidentally jump out and cause weird
            # things to happen to the CFG.
            reentry.targets = []

            # Before anything else, we post process the loop body. This is the
            # "pre-order" part of this traversal.
            self.Debug('Loop Body')
            self.PostProcess(loop_body, entry)
            self.ApplyTripCount(loop_body, tc, entry)

            # Finally, unlink the loop.
            reentry.targets = [loop_exit]
            loop_exit.sources = [reentry]
            entry.targets = [loop_body]
            loop_body.sources = [entry]
            entry.sources.remove(reentry)

            self.DumpEditDot(entry)

            # Now Post Process the rest of the graph starting at the loop exit.
            self.PostProcess(loop_exit, stop_before)

        elif type(entry) is ControlNode and entry.IsConditional():
            tc_t = self.CreateTripCount('cond_{}_t'.format(entry.Name()))
            tc_f = self.CreateTripCount('cond_{}_f'.format(entry.Name()))

            self.Debug('Processing Conditional')

            cond_exit = entry.GetCondExit()
            true_branch = entry.GetTrueBranch()
            false_branch = entry.GetFalseBranch()

            self.Debug('True Branch: {}'.format(true_branch.Name()))
            self.Debug('False Branch: {}'.format(false_branch.Name()))
            self.Debug('Exit: {}'.format(cond_exit.Name()))

            if false_branch is cond_exit:
                false_branch = DummyNode(entry, cond_exit)
                self.nodes.append(false_branch)

            if true_branch is cond_exit:
                true_branch = DummyNode(entry, cond_exit)
                self.nodes.append(true_branch)

            # Record the branch trip counts. This table is used to generate
            # logic constraints when performing model checking
            self.cond_trip_counts.append((entry, true_branch, false_branch))

            self.Debug('True Branch')
            self.PostProcess(true_branch, cond_exit)
            self.Debug('False Branch')
            self.PostProcess(false_branch, cond_exit)

            # Apply the trip counts to the two branches
            self.ApplyTripCount(true_branch, tc_t, cond_exit)
            self.ApplyTripCount(false_branch, tc_f, cond_exit)

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

            self.DumpEditDot(entry)

            # Post process the rest of the graph
            self.PostProcess(cond_exit, stop_before)

        else:
            #assert len(entry.targets) == 1, 'Not a conditional or loop?'
            if len(entry.targets) != 1:
                print('Error! Expecting a conditional or loop!')
                print('name = ', entry.Name())
                print('sources = ', entry.sources)
                print('targets = ', entry.targets)
                raise RuntimeError('Not a conditional or loop?')
            self.PostProcess(entry.targets[0], stop_before)

        self.indent -= 1
        self.Debug('}')

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
        if access_size is not None:
            return int(num_iters) * int(access_size) // 8
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
