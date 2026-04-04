import csv
from collections import deque

DIRECTIONS = ['N', 'S', 'W', 'E']
DIR_COLUMNS = {
    'N': 'North', 
    'S': 'South', 
    'W': 'West', 
    'E': 'East'
}

TURN_ACTION = {
    'N': {'N': 'f', 'S': 'b', 'W': 'l', 'E': 'r'},
    'S': {'S': 'f', 'N': 'b', 'E': 'l', 'W': 'r'},
    'W': {'W': 'f', 'E': 'b', 'S': 'l', 'N': 'r'},
    'E': {'E': 'f', 'W': 'b', 'N': 'l', 'S': 'r'},
}

def load_maze(csv_path: str) -> dict:
    maze = {}
    with open(csv_path, newline = '') as f:
        reader = csv.DictReader(f)
        for row in reader:
            node = int(row['index'])
            neighbors = {}
            for d in DIRECTIONS:
                val = row[DIR_COLUMNS[d]].strip()
                neighbors[d] = int(val) if val else None
            maze[node] = neighbors
    return maze

def bfs(maze: dict, start: int, end: int) -> list[int] | None:
    queue = deque([[start]])
    visited = {start}

    while queue:
        path = queue.popleft()
        current = path[-1]

        if current == end:
            return path

        for direction, neighbor in maze[current].items():
            if neighbor is not None and neighbor not in visited:
                visited.add(neighbor)
                queue.append(path + [neighbor])

    return None

def get_direction(maze: dict, from_node: int, to_node: int) -> str:
    for d, neighbor in maze[from_node].items():
        if neighbor == to_node:
            return d
    raise ValueError(f"No direct connection from {from_node} to {to_node}")
    
def path_to_moves(maze: dict, path: list[int], initial_heading: str = None) -> str:
    if len(path) < 2:
        return ""
        
    moves = []
    first_dir = get_direction(maze, path[0], path[1])

    heading = initial_heading if initial_heading else first_dir

    for i in range(len(path) - 1):
        move_dir = get_direction(maze, path[i], path[i+1])
        action = TURN_ACTION[heading][move_dir]
        moves.append(action)
        heading = move_dir
        
    return ''.join(moves)


def solve(csv_path: str, start: int, end: int, initial_heading: str = None) -> str:
    maze = load_maze(csv_path)
    path = bfs(maze, start, end)

    if path is None:
        raise ValueError(f"No path found from {start} to {end}")
        
    print(f"Node path : {' → '.join(map(str, path))}")
    moves = path_to_moves(maze, path, initial_heading)
    return moves

if __name__ == "__main__":
    CSV_FILE = "0404maze.csv"
    start = int(input("Start: \n"))
    end = int(input("End: \n"))

    result = solve(CSV_FILE, start, end)
    print(f"Moves: {result}")


