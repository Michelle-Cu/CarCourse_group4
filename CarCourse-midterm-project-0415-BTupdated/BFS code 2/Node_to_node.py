import csv
from collections import deque
import os

# --- Configuration & Setup (MUST BE AT THE TOP) ---
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
CSV_FILE = os.path.join(BASE_DIR, "maze.csv")

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

# --- Helper Functions ---

def load_maze(csv_path: str) -> dict:
    maze = {}
    with open(csv_path, newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            node = int(row['index'])
            neighbors = {}
            # We use the DIR_COLUMNS global variable here
            for d in DIRECTIONS:
                val = row[DIR_COLUMNS[d]].strip()
                neighbors[d] = int(val) if val else None
            maze[node] = neighbors
    return maze

def get_coordinates(node):
    """
    Assumes a grid layout (e.g., 6 columns wide). 
    Change the '6' if your maze layout has a different width.
    """
    x = node % 6
    y = node // 6
    return x, y

def get_road_distance(maze, start, end):
    queue = deque([(start, 0)])
    visited = {start}
    while queue:
        curr, dist = queue.popleft()
        if curr == end: return dist
        for neighbor in maze[curr].values():
            if neighbor and neighbor not in visited:
                visited.add(neighbor)
                queue.append((neighbor, dist + 1))
    return float('inf')

# --- New Functions ---

def function1(maze, treasures):
    print("--- Treasure Amounts (Coordinate Distance) ---")
    x1, y1 = get_coordinates(1) # Reference point is Node 1
    for t in treasures:
        x, y = get_coordinates(t)
        amount = 30 * (abs(x - x1) + abs(y - y1))
        print(f"Treasure at {t}: {amount}")

def function2(maze, treasures):
    print("\n--- Treasure Density Values ---")
    for t in treasures:
        # Base value calculation
        x, y = get_coordinates(t)
        old_val = 30 * (x + y) 
        
        density_bonus = 0
        for other in maze.keys():
            if t == other: continue
            dist = get_road_distance(maze, t, other)
            if dist > 0 and dist != float('inf'):
                density_bonus += (other / (dist ** 5))
        
        new_value = old_val + density_bonus
        print(f"Point {t}: {new_value:.2f}")

# --- Main Execution ---

if __name__ == "__main__":
    # 1. Load the maze
    maze = load_maze(CSV_FILE)
    
    # 2. Identify treasures (nodes with only 1 neighbor)
    treasures = [n for n, neighbors in maze.items() 
                 if sum(1 for v in neighbors.values() if v is not None) == 1]
    
    # 3. Run your new functions
    function1(maze, treasures)
    function2(maze, treasures)