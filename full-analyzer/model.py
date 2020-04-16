

class DataflowNode:
    def __init__(self, bb_id, inst_id, trip_count=1):
        self.bb_id = bb_id
        self.inst_id = inst_id
        self.trip_count = trip_count
        self.targets = []
        self.num_sources = 0

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

    def TypeName(self):
        return 'Ctrl'
