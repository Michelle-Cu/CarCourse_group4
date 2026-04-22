#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <queue>
#include <cmath>
#include <limits>
#include <algorithm>

using namespace std;

struct NodeData {
    int id;
    map<string, int> neighbors;
};

// --- Maze Helper Functions ---
map<int, NodeData> load_maze(const string& filename) {
    map<int, NodeData> maze;
    ifstream file(filename);
    string line, word;
    
    if (!file.is_open()) return maze;
    getline(file, line); // Skip header
    
    while (getline(file, line)) {
        stringstream ss(line);
        vector<string> row;
        while (getline(ss, word, ',')) row.push_back(word);
        
        int id = stoi(row[0]);
        NodeData data;
        data.id = id;
        
        vector<string> dirs = {"N", "S", "W", "E"};
        for (int i = 0; i < 4; ++i) {
            if (!row[i + 1].empty()) data.neighbors[dirs[i]] = stoi(row[i + 1]);
        }
        maze[id] = data;
    }
    return maze;
}

int get_road_distance(const map<int, NodeData>& maze, int start, int end) {
    if (start == end) return 0;
    queue<pair<int, int>> q;
    q.push({start, 0});
    map<int, bool> visited;
    visited[start] = true;
    
    while (!q.empty()) {
        auto [curr, dist] = q.front();
        q.pop();
        
        for (auto const& [dir, neighbor] : maze.at(curr).neighbors) {
            if (neighbor == end) return dist + 1;
            if (!visited[neighbor]) {
                visited[neighbor] = true;
                q.push({neighbor, dist + 1});
            }
        }
    }
    return numeric_limits<int>::max(); 
}

// Manhattan distance relative to the user's defined starting point
double get_manhattan_value(int node, int start_node) {
    int x = node % 6; 
    int y = node / 6;
    int x1 = start_node % 6; 
    int y1 = start_node / 6;
    return 30.0 * (abs(x - x1) + abs(y - y1));
}

// Calculates P' = (Manhattan value) + sum(other_Manhattan / dist^2) for unvisited nodes
double get_dynamic_p_prime(int target_node, const vector<int>& treasures, 
                           const map<int, bool>& collected, 
                           const map<int, NodeData>& maze, int start_node) {
    
    double base = get_manhattan_value(target_node, start_node);
    double bonus = 0;
    
    for (int other : treasures) {
        // Exclude self and already collected treasures
        if (target_node == other || (collected.count(other) && collected.at(other))) continue;
        
        int dist = get_road_distance(maze, target_node, other);
        if (dist > 0 && dist != numeric_limits<int>::max()) {
            bonus += (get_manhattan_value(other, start_node) / (static_cast<double>(dist) * dist));
        }
    }
    return base + bonus;
}

int main() {
    auto maze = load_maze("big_maze_114.csv");
    if (maze.empty()) {
        cerr << "Error: Could not load maze file." << endl;
        return 1;
    }

    vector<int> treasures;
    for (auto const& [id, data] : maze) {
        if (data.neighbors.size() == 1) treasures.push_back(id);
    }

    int start_node, k;
    cout << "Enter start treasure node: ";
    cin >> start_node;
    cout << "Enter total movement budget (k): ";
    cin >> k;

    map<int, bool> collected;
    vector<int> visit_sequence;
    
    int current_node = start_node;
    visit_sequence.push_back(current_node);
    collected[current_node] = true;

    cout << "\nStarting traversal..." << endl;

    while (k > 0) {
        int best_node = -1;
        double max_ratio = -1.0;
        int best_dist = 0;

        cout << "\n--- Current Node: " << current_node << " | Budget Left: " << k << " ---" << endl;

        for (int t : treasures) {
            if (collected[t]) continue;

            int dist = get_road_distance(maze, current_node, t);
            
            if (dist <= k && dist != numeric_limits<int>::max()) {
                double p_prime = get_dynamic_p_prime(t, treasures, collected, maze, start_node);
                double ratio = p_prime / static_cast<double>(dist == 0 ? 1 : dist);
                
                cout << "  > Target " << t << ": Dist=" << dist 
                     << " | P'=" << p_prime 
                     << " | Ratio=" << ratio << endl;
                
                if (ratio > max_ratio) {
                    max_ratio = ratio;
                    best_node = t;
                    best_dist = dist;
                }
            }
        }

        if (best_node == -1) {
            cout << "No more reachable treasures within budget." << endl;
            break; 
        }

        k -= best_dist;
        collected[best_node] = true;
        visit_sequence.push_back(best_node);
        current_node = best_node;
        
        cout << ">>> Moving to node " << best_node << " (Spent " << best_dist << " steps)" << endl;
    }

    cout << "\nFinal sequence: ";
    for (size_t i = 0; i < visit_sequence.size(); ++i) {
        cout << visit_sequence[i] << (i == visit_sequence.size() - 1 ? "" : " -> ");
    }
    cout << "\nBudget remaining: " << k << endl;

    return 0;
}