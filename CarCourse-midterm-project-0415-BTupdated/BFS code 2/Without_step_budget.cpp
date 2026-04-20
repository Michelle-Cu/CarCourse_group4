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

using namespace std; // This brings all standard library symbols into the global scope

struct NodeData {
    int id;
    map<string, int> neighbors;
};

// --- Maze Helper Functions ---
map<int, NodeData> load_maze(const string& filename) {
    map<int, NodeData> maze;
    ifstream file(filename);
    string line, word;
    
    // Skip header
    getline(file, line);
    
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
    queue<pair<int, int>> q;
    q.push({start, 0});
    vector<int> visited = {start};
    
    while (!q.empty()) {
        auto [curr, dist] = q.front();
        q.pop();
        if (curr == end) return dist;
        
        for (auto const& [dir, neighbor] : maze.at(curr).neighbors) {
            if (find(visited.begin(), visited.end(), neighbor) == visited.end()) {
                visited.push_back(neighbor);
                q.push({neighbor, dist + 1});
            }
        }
    }
    return numeric_limits<int>::max(); 
}

// --- Logic ---
double get_old_point_value(int node) {
    int x = node % 6; 
    int y = node / 6;
    int x1 = 1 % 6, y1 = 1 / 6;
    return 30.0 * (abs(x - x1) + abs(y - y1));
}

int main() {
    auto maze = load_maze("maze.csv");
    vector<int> treasures;
    for (auto const& [id, data] : maze) {
        if (data.neighbors.size() == 1) treasures.push_back(id);
    }

    // Pre-calculate P'
    map<int, double> p_prime;
    for (int t : treasures) {
        double bonus = 0;
        for (int other : treasures) {
            if (t == other) continue;
            int dist = get_road_distance(maze, t, other);
            if (dist != numeric_limits<int>::max()) {
                bonus += (get_old_point_value(other) / (static_cast<double>(dist) * dist));
            }
        }
        p_prime[t] = get_old_point_value(t) + bonus;
    }

    // Tracking variables
    map<int, bool> collected;
    vector<int> visit_sequence;
    
    int current_node;
    cout << "Enter start treasure node: ";
    cin >> current_node;
    
    visit_sequence.push_back(current_node);
    collected[current_node] = true;

    // Main loop to find path
    while(true) {
        int best_node = -1;
        double max_ratio = -1.0;

        for (int t : treasures) {
            if (collected[t]) continue;
            
            int dist = get_road_distance(maze, current_node, t);
            if (dist != numeric_limits<int>::max() && dist > 0) {
                double ratio = p_prime[t] / pow(dist, 2);
                if (ratio > max_ratio) {
                    max_ratio = ratio;
                    best_node = t;
                }
            }
        }

        if (best_node == -1) break; // No more reachable treasures

        // Update state
        collected[best_node] = true;
        visit_sequence.push_back(best_node);
        current_node = best_node;
    }

    // Print the final sequence
    cout << "\nTreasure collection sequence: ";
    for (size_t i = 0; i < visit_sequence.size(); ++i) {
        cout << visit_sequence[i] << (i == visit_sequence.size() - 1 ? "" : " -> ");
    }
    cout << endl;

    return 0;
}