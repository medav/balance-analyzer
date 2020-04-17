

class DataflowNode:
    def __init__(self, bb_id, inst_id, trip_count=1):
        self.bb_id = bb_id
        self.inst_id = inst_id
        self.trip_count = trip_count
        self.targets = []
        self.num_sources = 0

    def IsReachable(self, target):
        seen = set()
        work_list = [self]

        while len(work_list) > 0:
            n = work_list.pop(0)
            seen.add(n)

            if n is target:
                 return True

            for t in n.targets:
                if t not in seen:
                    work_list.add(t)

        return False

    def Name(self):
        return 'bb{}_i{}'.format(self.bb_id, self.inst_id)

class PortNode(DataflowNode):
    def __init__(self, bb_id, inst_id, port, num_elem):
        super().__init__(bb_id, inst_id)
        self.port = port
        self.num_elem = num_elem

    def TypeName(self):
        return 'StreamCmd'

class ConfigNode(DataflowNode):
    def __init__(self, bb_id, inst_id):
        super().__init__(bb_id, inst_id)

    def TypeName(self):
        return 'Config'

class WaitNode(DataflowNode):
    def __init__(self, bb_id, inst_id):
        super().__init__(bb_id, inst_id)

    def TypeName(self):
        return 'Wait'

class ControlNode(DataflowNode):
    def __init__(self, bb_id, inst_id, target_bbs):
        super().__init__(bb_id, inst_id)
        self.target_bbs = target_bbs

    def IsConditional(self):
        # A conditional is identified by any control node with 1 source and two
        # targets. This should theoretically suffice for capturing any if
        # expression in C
        return self.num_sources == 1 and len(self.targets) == 2

    def GetCondExit(self):
        assert self.IsConditional()

        tb = self.GetTrueBranch()
        fb = self.GetFalseBranch()

        seen = set()
        work_list = [tb, fb]

        while len(work_list) > 0:
            n = work_list.pop(0)

            if n in seen:
                return n

            for t in n.targets:
                work_list.append(t)

        raise RuntimeError('Could not find If-Condition Exit!')

    def GetTrueBranch(self):
        assert self.IsConditional()
        return self.targets[0]

    def GetFalseBranch(self):
        assert self.IsConditional()
        return self.targets[1]

    def IsLoopEntry(self):
        # A loop entry will be a control node with 2 sources and 2 targets.
        # In addition, one of the targets must be able to reach this node.
        return \
            self.num_sources == 2 and
            len(self.targets) == 2 and
            (self.targets[0].IsReachable(self) or
                self.targets[1].IsReachable(self))

    def GetLoopBody(self):
        assert self.IsLoopEntry()
        if self.targets[0].IsReachable(self):
            return self.targets[0]
        else:
            return self.targets[1]

    def TypeName(self):
        return 'Ctrl'
