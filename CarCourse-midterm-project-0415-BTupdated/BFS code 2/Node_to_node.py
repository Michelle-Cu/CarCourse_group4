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
            neighbors = {d: (int(row[DIR_COLUMNS[d]]) if row[DIR_COLUMNS[d]] else None) 
                         for d in DIRECTIONS}
            maze[node] = neighbors
    return maze

def get_coordinates(node):
    # Adjust '6' if your maze has a different grid width
    x = node % 6 
    y = node // 6
    return x, y

def get_road_distance(maze, start, end):
    """Calculates shortest distance by road (edges) between two nodes."""
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
    """Base treasure points: 30 * Manhattan distance from Node 1."""
    x, y = get_coordinates(node)
    x1, y1 = get_coordinates(1)
    return 30 * (abs(x - x1) + abs(y - y1))

def calculate_all_new_values(maze, treasures):
    """Calculates P' (New Point Value) for all treasure nodes."""
    # Pre-calculate base values
    base_values = {t: get_old_point_value(t) for t in treasures}
    new_values = {}
    
    for t in treasures:
        density_bonus = 0
        for other in treasures:
            if t == other: continue
            dist = get_road_distance(maze, t, other)
            if dist > 0 and dist != float('inf'):
                density_bonus += (base_values[other] / (dist ** 2))
        new_values[t] = base_values[t] + density_bonus
    return new_values

def find_best_target(start_node, treasures, new_values, maze):
    """Calculates P'/r for all other treasures and returns the best target."""
    if start_node not in treasures:
        return None, "Node is not a treasure node."

    best_node = None
    max_ratio = -1.0

    print(f"\n--- Calculating P'/r from Start: {start_node} ---")
    for t in treasures:
        if t == start_node: continue
        
        dist = get_road_distance(maze, start_node, t)
        if dist > 0 and dist != float('inf'):
            ratio = new_values[t] / dist / dist
            #            print(f"Target {t}: Ratio = {ratio:.2f} (P'={new_values[t]:.2f}, r={dist})")
            
            if ratio > max_ratio:
                max_ratio = ratio
                best_node = t
                
    return best_node, max_ratio

# --- Main Execution ---

if __name__ == "__main__":
    maze = load_maze(CSV_FILE)
    
    # Identify treasures (nodes with only 1 neighbor)
    treasures = [n for n, neighbors in maze.items() 
                 if sum(1 for v in neighbors.values() if v is not None) == 1]
    
    # Pre-calculate all P' values once
    p_prime_values = calculate_all_new_values(maze, treasures)
    
    # User Input
    try:
        user_node = int(input(f"Enter a start treasure node {treasures}: "))
        target, ratio = find_best_target(user_node, treasures, p_prime_values, maze)
        
        if target:
            print(f"\nResult: The best target node is {target} with a ratio of {ratio:.2f}")
        else:
            print("No valid target found.")
    except ValueError:
        print("Invalid input. Please enter a number.")