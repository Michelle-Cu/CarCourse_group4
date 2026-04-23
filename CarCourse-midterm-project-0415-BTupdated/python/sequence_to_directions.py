from maze import Maze, Action
from node import Direction
from pathlib import Path

def plan_route(maze_file: str, node_sequence: list):
    """
    Given a list of node indices to visit in order,
    compute the full action sequence to walk that route.
    """
    m = Maze(str(Path(__file__).resolve().parent / "data" / maze_file))
    
    action_names = {1: "ADVANCE", 2: "TURN_LEFT", 3: "U_TURN", 4: "TURN_RIGHT", 5: "HALT"}
    
    all_actions = []
    all_nodes = [node_sequence[0]]
    car_dir = None

    # print(f"=== Route Plan: {' → '.join(map(str, node_sequence))} ===\n")

    for i in range(len(node_sequence) - 1):
        start = m.node_dict[node_sequence[i]]
        end   = m.node_dict[node_sequence[i + 1]]

        path = m.BFS_2(start, end)
        if path is None:
            print(f"❌ No path found from node {node_sequence[i]} to node {node_sequence[i+1]}!")
            return

        actions, car_dir = m.getActions(path, initial_dir=car_dir)
        action_str = m.actions_to_str(actions)
        action_nums = [int(a) for a in actions]

        # print(f"--- Segment {i+1}: node {node_sequence[i]} → node {node_sequence[i+1]} ---")
        # print(f"Node path:     {' → '.join(str(n.get_index()) for n in path)}")
        # print(f"Actions (str): {action_str}")
        # print(f"Actions (num): {action_nums}")
        # for j, (node, action) in enumerate(zip(path, actions)):
        #     print(f"  Step {j+1}: at node {node.get_index()} → {action_names[int(action)]} ({int(action)})")
        # print(f"  Step {len(path)}: arrive at node {path[-1].get_index()}")
        print()

        all_nodes.extend(n.get_index() for n in path[1:])
        all_actions.extend(actions)

    print("=== Summary ===")
    print(f"Dead-end sequence:  {node_sequence}")
    print(f"Nodes visited:      {all_nodes}")
    print(f"Full actions (str): {m.actions_to_str(all_actions)}")
    print(f"Full actions (num): {[int(a) for a in all_actions]}")
    print(f"Total steps:        {len(all_actions)}")


if __name__ == "__main__":
    plan_route(
        maze_file="big_maze_114.csv",
        node_sequence=[25, 43, 45, 48, 36, 24, 12, 6, 30, 1, 7, 19, 31] # fill this
    )