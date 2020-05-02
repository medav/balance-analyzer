
class DataflowNode:
    def __init__(self, bb_id, inst_id):
        self.bb_id = bb_id
        self.inst_id = inst_id
        self.trip_count = 1
        self.targets = []
        self.sources = []

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
                    work_list.append(t)

        return False

    def Name(self):
        return 'bb{}_i{}'.format(self.bb_id, self.inst_id)

    def __repr__(self):
        return self.Name()

    def Apply(self, val, num_ports):
        return val

class PortNode(DataflowNode):
    def __init__(self, bb_id, inst_id, port, num_elem):
        super().__init__(bb_id, inst_id)
        self.port = port
        self.num_elem = num_elem

    def TypeName(self):
        return 'StreamCmd'

    def Apply(self, val, num_ports):
        val[self.port - 1] += self.trip_count * self.num_elem
        return val

class ConfigNode(DataflowNode):
    def __init__(self, bb_id, inst_id):
        super().__init__(bb_id, inst_id)

    def TypeName(self):
        return 'Config'

    def Apply(self, val, num_ports):
        return [0 for _ in range(num_ports)]

class DummyNode(DataflowNode):
    def __init__(self, pred, succ):
        super().__init__(pred.bb_id, -1)
        self.sources = [pred]
        self.targets = [succ]

        assert succ in pred.targets
        assert pred in succ.sources

        pred.targets.remove(succ)
        pred.targets.append(self)

        succ.sources.remove(pred)
        succ.sources.append(self)

        self.name = pred.Name() + '_' + succ.Name()

    def TypeName(self):
        return 'Dummy'

    def Name(self):
        return self.name

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
        return len(self.sources) == 1 and len(self.targets) == 2

    def GetCondExit(self):
        assert self.IsConditional()

        tb = self.GetTrueBranch()
        fb = self.GetFalseBranch()

        seen_t = set()
        seen_f = set()
        wl_t = [tb]
        wl_f = [fb]

        seen = [seen_t, seen_f]
        wls = [wl_t, wl_f]
        i = 0

        while len(wl_t) > 0 or len(wl_f) > 0:
            if len(wls[i % 2]) > 0:
                n = wls[i % 2].pop(0)

                intersection = seen_t.intersection(seen_f)
                if len(intersection) > 0:
                    assert len(intersection) == 1
                    return list(intersection)[0]

                for t in n.targets:
                    if n not in seen:
                        wls[i % 2].append(t)


                seen[i % 2].add(n)

            i += 1

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
            len(self.sources) == 2 and \
            len(self.targets) == 2 and \
            (self.targets[0].IsReachable(self) or \
                self.targets[1].IsReachable(self))

    def GetLoopBody(self):
        assert self.IsLoopEntry()
        if self.targets[0].IsReachable(self):
            return self.targets[0]
        else:
            return self.targets[1]

    def GetLoopExit(self):
        assert self.IsLoopEntry()
        if self.targets[0].IsReachable(self):
            return self.targets[1]
        else:
            return self.targets[0]

    def GetLoopReentry(self):
        assert self.IsLoopEntry()
        body = self.GetLoopBody()
        if body.IsReachable(self.sources[0]):
            return self.sources[0]
        else:
            return self.sources[1]

    def TypeName(self):
        return 'Ctrl'
