import csv
import logging
import math
from enum import IntEnum
from typing import List
from collections import deque

import numpy as np
import pandas

from node import Direction, Node
from pathlib import Path

log = logging.getLogger(__name__)


class Action(IntEnum):
    ADVANCE = 1
    TURN_LEFT = 2
    U_TURN = 3
    TURN_RIGHT = 4
    HALT = 5


class Maze:
    def __init__(self, filepath: str):
        # TODO : read file and implement a data structure you like
        # For example, when parsing raw_data, you may create several Node objects.
        # Then you can store these objects into self.nodes.
        # Finally, add to nd_dict by {key(index): value(corresponding node)}
        self.raw_data = pandas.read_csv(filepath).values
        self.nodes = []
        self.node_dict = dict()  # key: index, value: the correspond node

        # First pass: create all Node objects
        for row in self.raw_data:
            index = int(row[0])
            node = Node(index)
            self.nodes.append(node)
            self.node_dict[index] = node

        # (neighbor_col, distance_col, Direction int) matching Direction enum in node.py
        # NORTH=1, SOUTH=2, WEST=3, EAST=4
        DIR_MAP = [
            (1, 5, 1),  # North neighbor, ND distance
            (2, 6, 2),  # South neighbor, SD distance
            (3, 7, 3),  # West neighbor,  WD distance
            (4, 8, 4),  # East neighbor,  ED distance
        ]

        # Second pass: set successors now that all nodes exist
        for row in self.raw_data:
            node = self.node_dict[int(row[0])]
            for neighbor_col, dist_col, direction in DIR_MAP:
                neighbor_val = row[neighbor_col]
                dist_val = row[dist_col]
                if not pandas.isna(neighbor_val):
                    neighbor_node = self.node_dict[int(neighbor_val)]
                    dist = int(dist_val) if not pandas.isna(dist_val) else 1
                    node.set_successor(neighbor_node, direction, dist)
        self.explored = set() # track visited nodes

    def get_start_point(self):
        if len(self.node_dict) < 2:
            log.error("Error: the start point is not included.")
            return 0
        return self.node_dict[1]

    def get_node_dict(self):
        return self.node_dict

    def BFS(self, node: Node):
        # TODO : design your data structure here for your algorithm
        # Tips : return a sequence of nodes from the node to the nearest unexplored deadend
        self.explored.add(node.get_index())
    
        queue = deque([[node]])
        visited = {node.get_index()}

        while queue:
            path = queue.popleft()
            current = path[-1]

            # dead-end = 1 successor, not starting node, not already explored
            if len(current.get_successors()) == 1 and current != node and current.get_index() not in self.explored:
                for n in path:
                    self.explored.add(n.get_index())
                return path

            for succ, direction, dist in current.get_successors():
                if succ.get_index() not in visited:  # ← removed self.explored check here
                    visited.add(succ.get_index())
                    queue.append(path + [succ])
        return None

    def BFS_2(self, node_from: Node, node_to: Node):
        # TODO : similar to BFS but with fixed start point and end point
        # Tips : return a sequence of nodes of the shortest path

        queue = deque([[node_from]])
        visited = {node_from.get_index()}

        while queue:
            path = queue.popleft()
            current = path[-1]

            if current == node_to:
                return path

            for succ, direction, dist in current.get_successors():
                if succ.get_index() not in visited:
                    visited.add(succ.get_index())
                    queue.append(path + [succ])

        return None

    def getAction(self, car_dir, node_from: Node, node_to: Node):
        # TODO : get the car action
        # Tips : return an action and the next direction of the car if the node_to is the Successor of node_to
        # If not, print error message and return 0
        if not node_from.is_successor(node_to):
            print(f"Error: Node {node_to.get_index()} is not a successor of Node {node_from.get_index()}")
            return 0, car_dir
        
        # get the absolute direction from node_from to node_to
        move_dir = node_from.get_direction(node_to)

        # same table as BFSchecklist.py but mapped to Action enum
        TURN_ACTION = {
            Direction.NORTH: {Direction.NORTH: Action.ADVANCE,   Direction.SOUTH: Action.U_TURN,    Direction.WEST: Action.TURN_LEFT,  Direction.EAST: Action.TURN_RIGHT},
            Direction.SOUTH: {Direction.SOUTH: Action.ADVANCE,   Direction.NORTH: Action.U_TURN,    Direction.EAST: Action.TURN_LEFT,  Direction.WEST: Action.TURN_RIGHT},
            Direction.WEST:  {Direction.WEST:  Action.ADVANCE,   Direction.EAST:  Action.U_TURN,    Direction.SOUTH: Action.TURN_LEFT, Direction.NORTH: Action.TURN_RIGHT},
            Direction.EAST:  {Direction.EAST:  Action.ADVANCE,   Direction.WEST:  Action.U_TURN,    Direction.NORTH: Action.TURN_LEFT, Direction.SOUTH: Action.TURN_RIGHT},
        }

        action = TURN_ACTION[car_dir][move_dir]
        return action, move_dir  # move_dir becomes the new car heading
        # return None

    def getActions(self, nodes: List[Node]):
        # TODO : given a sequence of nodes, return the corresponding action sequence
        # Tips : iterate through the nodes and use getAction() in each iteration
        if len(nodes) < 2:
            return []
        
        actions = []
        car_dir = nodes[0].get_direction(nodes[1])  # initial heading = direction of first move
        
        for i in range(len(nodes) - 1):
            action, car_dir = self.getAction(car_dir, nodes[i], nodes[i+1])
            actions.append(action)
        
        return actions
        # return None

    def actions_to_str(self, actions):
        # cmds should be a string sequence like "fbrl....", use it as the input of BFS checklist #1
        cmd = "fbrls"
        cmds = ""
        for action in actions:
            cmds += cmd[action - 1]
        log.info(cmds)
        return cmds

    def strategy(self, node: Node):
        return self.BFS(node)

    def strategy_2(self, node_from: Node, node_to: Node):
        return self.BFS_2(node_from, node_to)


# Temporary test to ensure __init__ works properly


# if __name__ == "__main__":
#     m = Maze(str(Path(__file__).resolve().parent / "data" / "small_maze.csv"))
#     for idx, node in m.node_dict.items():
#         print(f"Node {idx}: successors = {[(s[0].get_index(), str(s[1]), s[2]) for s in node.get_successors()]}")

if __name__ == "__main__":
    # BASE = Path(__file__).resolve().parent
    # m = Maze(str(BASE / "small_maze.csv"))
    m = Maze(str(Path(__file__).resolve().parent / "data" / "small_maze.csv"))
    
    node3 = m.node_dict[3]
    node2 = m.node_dict[2]
    node5 = m.node_dict[5]
    
    print(node3.get_direction(node2))  # should print Direction.EAST
    print(node3.get_direction(node5))  # should print Direction.WEST
    print(node3.get_direction(m.node_dict[1]))  # should print error, return 0

    # car is at node3, heading SOUTH, wants to go to node2 (EAST)
    # should return TURN_LEFT, new heading EAST
    action, new_dir = m.getAction(Direction.SOUTH, node3, node2)
    print(action, new_dir)

    # car is at node3, heading EAST, wants to go to node2 (EAST)
    # should return ADVANCE, new heading EAST
    action, new_dir = m.getAction(Direction.EAST, node3, node2)
    print(action, new_dir)

    # path 1→2→3→5→6
    path = [m.node_dict[1], m.node_dict[2], m.node_dict[3], m.node_dict[5], m.node_dict[6]]
    actions = m.getActions(path)
    print(m.actions_to_str(actions))  # should print "frfr" matching BFSchecklist result!