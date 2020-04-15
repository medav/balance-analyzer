

class DataflowNode:
    def __init__(self, bb_id, inst_id, trip_count=1):
        self.bb_id = bb_id
        self.inst_id = inst_id
        self.trip_count = trip_count
        self.targets = []

class PortNode(DataflowNode):
    def __init__(self, bb_id, inst_id, port, num_elem):
        super().__init__(bb_id, inst_id)
        self.port = port
        self.num_elem = num_elem

class ConfigNode(DataflowNode):
    def __init__(self, bb_id, inst_id):
        super().__init__(bb_id, inst_id)

class WaitNode(DataflowNode):
    def __init__(self, bb_id, inst_id):
        super().__init__(bb_id, inst_id)

class ControlNode(DataflowNode):
    def __init__(self, bb_id, inst_id, target_bbs):
        super().__init__(bb_id, inst_id)
        self.target_bbs = target_bbs
