
from model import *

class Analyzer():
    def __init__(self, filename):
        self.filename = filename
        self.nodes = self.BuildGraph(filename)
        self.entry = self.nodes[0]
        self.exit = self.FindExit()
        self.wait = self.FindWait()
        self.RemoveLoops()
        self.ComputeDataflow()

    def FindExit(self):
        # The exit node is the only control node with no targets
        for n in nodes:
            if type(n) is ControlNode and len(n.targets) == 0:
                return n

    def RemoveLoops(self):
        # 1. Remove useless ControlNodes
        # 2. Collapse PortNodes
        # 3. Loop ellison / trip count insertion
        pass

    @staticmethod
    def ComputeSymbolicNumElems(num_iters, stride=None, access_size=None):
        return 1

    @staticmethod
    def BuildNode(line):
        parts = filter(
            lambda p: len(p) > 0,
            map(lambda p: p.strip(), line.split(','))

        bb_id = int(parts[0])
        if parts[1] == 'control':
            return ControlNode(bb_id, inst_id, parts[2:])

        inst_id = int(parts[1])
        if parts[2] == 'SB_CONFIG':
            return ConfigNode(bb_id, inst_id)

        elif parts[2] == 'SB_WAIT':
            return WaitNode(bb_id, inst_id)

        elif parts[2] == 'SB_MEM_PORT_STREAM':
            return PortNode(
                bb_id,
                inst_id,
                int(parts[3]),
                ComputeSymbolicNumElems(parts[6], parts[4], parts[5]))

        elif parts[2] == 'SB_CONSTANT':
            return PortNode(
                bb_id,
                inst_id,
                int(parts[3]),
                ComputeSymbolicNumElems(parts[4]))

        elif parts[2] == 'SB_DISCARD':
            return PortNode(
                bb_id,
                inst_id,
                int(parts[3]),
                ComputeSymbolicNumElems(parts[4]))

        elif parts[2] == 'SB_PORT_MEM_STREAM':
            return PortNode(
                bb_id,
                inst_id,
                int(parts[3]),
                ComputeSymbolicNumElems(parts[6], parts[4], parts[5]))

    @staticmethod
    def BuildGraph(filename):
        nodes = []
        entry_points = {}
        exit_points = []
        num_bbs = 0

        # Initially just build all the nodes
        with open(filename) as f:
            for line in f:
                n = BuildNode(line)
                num_bbs = max(n.bb_id, num_bbs)
                nodes.append(n)

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

        # Now link basic blocks by their control nodes
        for bb_id in range(num_bbs):
            cn = exit_points[bb_id]
            for target_id in cn.target_bbs:
                cn.targets.append(entry_points[target_id])

        return nodes


