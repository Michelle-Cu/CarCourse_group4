import csv
from collections import deque
import os

# --- Configuration & Setup ---
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
CSV_FILE = os.path.join(BASE_DIR, "maze.csv")

DIRECTIONS = ['N', 'S', 'W', 'E']
DIR_COLUMNS = {'N': 'North', 'S': 'South', 'W': 'West', 'E': 'East'}

# --- Helper Functions ---

def load_maze(csv_path: str) -> dict:
    maze = {}
    with open(csv_path, newline='') as f:
        reader = csv.DictReader(f)
        for row in reader:
            node = int(row['index'])
            neighbors = {}
            for d in DIRECTIONS:
                val = row[DIR_COLUMNS[d]].strip()
                neighbors[d] = int(val) if val else None
            maze[node] = neighbors
    return maze

def get_coordinates(node):
    """
    Adjust the '6' to match the actual width of your maze grid.
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

def get_old_point_value(node):
    """Calculates base treasure points (30 * distance from Node 1)."""
    x, y = get_coordinates(node)
    x1, y1 = get_coordinates(1)
    return 30 * (abs(x - x1) + abs(y - y1))

# --- Updated Functions ---

def function1(treasures):
    print("--- Treasure Points (Base Values) ---")
    for t in treasures:
        val = get_old_point_value(t)
        print(f"Treasure {t}: {val}")

def function2(maze, treasures):
    print("\n--- Treasure Density Values ---")
    
    # 1. Pre-calculate all old point values
    treasure_values = {t: get_old_point_value(t) for t in treasures}
    
    # 2. Calculate New Point Value
    for t in treasures:
        old_val = treasure_values[t]
        density_bonus = 0
        
        for other in treasures:
            if t == other: continue
            
            dist = get_road_distance(maze, t, other)
            
            # Ensure we don't divide by zero and only include reachable nodes
            if dist > 0 and dist != float('inf'):
                # Formula: sum of (other_node_point / distance^2)
                density_bonus += (treasure_values[other] / (dist ** 2) )
        
        new_value = old_val + density_bonus
        print(f"Point {t}: {new_value:.2f}")

# --- Main Execution ---

if __name__ == "__main__":
    maze = load_maze(CSV_FILE)
    
    # Identify treasures (dead ends)
    treasures = [n for n, neighbors in maze.items() 
                 if sum(1 for v in neighbors.values() if v is not None) == 1]
    
    function1(treasures)
    function2(maze, treasures)