from enum import IntEnum


# You can get the enumeration based on integer value, or make comparison
# ex: d = Direction(1), then d would be Direction.NORTH
# ex: print(Direction.SOUTH == 1) should return False
class Direction(IntEnum):
    NORTH = 1
    SOUTH = 2
    WEST = 3
    EAST = 4


# Construct class Node and its member functions
# You may add more member functions to meet your needs
class Node:
    def __init__(self, index: int = 0):
        self.index = index
        # store successor as (Node, direction to node, distance)
        self.successors = []

    def get_index(self):
        return self.index

    def get_successors(self):
        return self.successors

    def set_successor(self, successor, direction, length=1):
        self.successors.append((successor, Direction(direction), int(length)))
        print(f"For Node {self.index}, a successor {self.successors[-1]} is set.")
        return

    def get_direction(self, node):
        # TODO : if node is adjacent to the present node, return the direction of node from the present node
        # For example, if the direction of node from the present node is EAST, then return Direction.EAST = 4
        # However, if node is not adjacent to the present node, print error message and return 0
        for succ in self.successors:
            if succ[0] == node:
                return succ[1]  # returns the Direction enum
        print(f"Error: Node {node.get_index()} is not adjacent to Node {self.index}")
        return 0

    def is_successor(self, node):
        for succ in self.successors:
            if succ[0] == node:
                return True
        return False


# Temporary test to ensure get_direction works properly
# if __name__ == "__main__":d
#     m = Maze("small_maze.csv")
    
#     node3 = m.node_dict[3]
#     node2 = m.node_dict[2]
#     node5 = m.node_dict[5]
    
#     print(node3.get_direction(node2))  # should print Direction.EAST = 4
#     print(node3.get_direction(node5))  # should print Direction.WEST = 3
#     print(node3.get_direction(m.node_dict[1]))  # should print error, return 0

