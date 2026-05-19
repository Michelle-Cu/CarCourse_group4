#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <queue>
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
        
        if (row.size() < 5) continue;
        
        int id = stoi(row[0]);
        NodeData data;
        data.id = id;
        
        vector<string> dirs = {"N", "S", "W", "E"};
        for (int i = 0; i < 4; ++i) {
            if (!row[i + 1].empty() && row[i+1].find_first_not_of(" \t\r\n") != string::npos) {
                data.neighbors[dirs[i]] = stoi(row[i + 1]);
            }
        }
        maze[id] = data;
    }
    return maze;
}

// Standard BFS to find shortest distance
int get_road_distance(const map<int, NodeData>& maze, int start, int end) {
    if (start == end) return 0;
    queue<pair<int, int>> q;
    q.push({start, 0});
    map<int, bool> visited;
    visited[start] = true;
    
    while (!q.empty()) {
        auto [curr, dist] = q.front();
        q.pop();
        
        if (maze.find(curr) == maze.end()) continue;

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

    cout << "\nStarting traversal (Greedy Nearest Neighbor)..." << endl;

    while (k > 0) {
        int best_node = -1;
        int min_dist = numeric_limits<int>::max();

        cout << "\n--- Current Node: " << current_node << " | Budget Left: " << k << " ---" << endl;

        // BFS-based selection: Find the closest uncollected treasure
        for (int t : treasures) {
            if (collected[t]) continue;

            int dist = get_road_distance(maze, current_node, t);
            
            if (dist <= k && dist < min_dist) {
                min_dist = dist;
                best_node = t;
            }
        }

        if (best_node == -1) {
            cout << "No more reachable treasures within budget." << endl;
            break; 
        }

        k -= min_dist;
        collected[best_node] = true;
        visit_sequence.push_back(best_node);
        current_node = best_node;
        
        cout << ">>> Moving to nearest treasure " << best_node << " (Spent " << min_dist << " steps)" << endl;
    }

    cout << "\nFinal sequence: ";
    for (size_t i = 0; i < visit_sequence.size(); ++i) {
        cout << visit_sequence[i] << (i == visit_sequence.size() - 1 ? "" : " -> ");
    }
    cout << "\nBudget remaining: " << k << endl;

    return 0;
}