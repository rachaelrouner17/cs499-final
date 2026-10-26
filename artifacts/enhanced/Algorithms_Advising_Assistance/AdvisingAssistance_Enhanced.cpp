//CS 300 Project Two
// Rachael Baker

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// Represents a single course record
struct Course {
    string number;
    string title;
    vector<string> prereqs;
};

// Trim whitespace from both ends of a string
static string trim(const string& s) {
    const string ws = " \t\r\n";
    const auto start = s.find_first_not_of(ws);
    if (start == string::npos) return "";
    const auto end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

// Convert string to uppercase for consistent keys
static string toUpper(string s) {
    for (char& ch : s) {
        ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
    }
    return s;
}

// Split a CSV line into fields
static vector<string> splitCSV(const string& line) {
    vector<string> parts;
    stringstream ss(line);
    string item;

    while (getline(ss, item, ',')) {
        parts.push_back(trim(item));
    }
    return parts;
}

// Stores each prerequisite as a node with edges to the courses it unlocks.
// For example, an edge from CS200 to CS300 means CS200 is required before CS300.
using DependencyGraph = map<string, vector<string>>;

// Builds the prerequisite graph after the course records have been loaded.
// Every course is included, even if it has no prerequisites, so disconnected
// courses still appear in the graph and can be tested.
// Time complexity: O(V + E); space complexity: O(V + E).
static DependencyGraph buildDependencyGraph(const map<string, Course>& courses) {
    DependencyGraph graph;

    for (const auto& entry : courses) {
        graph[entry.first];
    }

    for (const auto& entry : courses) {
        const Course& course = entry.second;
        for (const string& prerequisite : course.prereqs) {
            if (courses.find(prerequisite) != courses.end()) {
                graph[prerequisite].push_back(course.number);
            }
        }
    }

    // A repeated prerequisite should create one edge, not duplicate work.
    for (auto& entry : graph) {
        auto& dependents = entry.second;
        sort(dependents.begin(), dependents.end());
        dependents.erase(unique(dependents.begin(), dependents.end()), dependents.end());
    }

    return graph;
}

// Prints the graph using the same prerequisite-to-dependent convention used
// by buildDependencyGraph. This makes the data structure easy to verify.
static void printDependencyGraph(const DependencyGraph& graph,
                                 const map<string, Course>& courses) {
    cout << "Dependency graph (prerequisite -> dependent course):\n";

    for (const auto& entry : graph) {
        cout << entry.first;
        auto courseIt = courses.find(entry.first);
        if (courseIt != courses.end()) {
            cout << ", " << courseIt->second.title;
        }

        if (entry.second.empty()) {
            cout << " -> (no dependent courses)\n";
            continue;
        }

        cout << " -> ";
        for (size_t i = 0; i < entry.second.size(); ++i) {
            if (i > 0) cout << ", ";
            cout << entry.second[i];
        }
        cout << "\n";
    }
}

// Uses three states to detect a back edge during depth-first search:
// 0 means unvisited, 1 means the node is on the current search path, and
// 2 means all of the node's dependents have already been checked.
// Time complexity: O(V + E); space complexity: O(V).
static bool hasCycle(const DependencyGraph& graph) {
    map<string, int> state;

    for (const auto& entry : graph) {
        state[entry.first] = 0;
    }

    function<bool(const string&)> visit = [&](const string& course) {
        state[course] = 1;

        auto graphIt = graph.find(course);
        if (graphIt != graph.end()) {
            for (const string& dependent : graphIt->second) {
                if (state[dependent] == 1) {
                    return true;
                }
                if (state[dependent] == 0 && visit(dependent)) {
                    return true;
                }
            }
        }

        state[course] = 2;
        return false;
    };

    for (const auto& entry : graph) {
        if (state[entry.first] == 0 && visit(entry.first)) {
            return true;
        }
    }

    return false;
}

// Produces a prerequisite-first ordering using Kahn's topological-sort
// algorithm. A dependent course is not released until all incoming
// prerequisite edges have been processed.
// Time complexity: O(V + E); space complexity: O(V).
static vector<string> topologicalOrder(const DependencyGraph& graph) {
    map<string, int> indegree;
    for (const auto& entry : graph) {
        indegree[entry.first] = 0;
    }

    for (const auto& entry : graph) {
        for (const string& dependent : entry.second) {
            ++indegree[dependent];
        }
    }

    queue<string> ready;
    for (const auto& entry : indegree) {
        if (entry.second == 0) {
            ready.push(entry.first);
        }
    }

    vector<string> order;
    while (!ready.empty()) {
        string course = ready.front();
        ready.pop();
        order.push_back(course);

        for (const string& dependent : graph.at(course)) {
            if (--indegree[dependent] == 0) {
                ready.push(dependent);
            }
        }
    }

    return order;
}

// Displays the topological order and reports when a cycle prevents a complete
// ordering from being produced.
static void printTopologicalOrder(const DependencyGraph& graph,
                                  const map<string, Course>& courses) {
    vector<string> order = topologicalOrder(graph);
    if (order.size() != graph.size()) {
        cout << "A complete course order is not possible because the graph contains a cycle.\n";
        return;
    }

    cout << "Prerequisite-first course order:\n";
    for (size_t i = 0; i < order.size(); ++i) {
        const string& number = order[i];
        cout << (i + 1) << ". " << number;
        auto courseIt = courses.find(number);
        if (courseIt != courses.end()) {
            cout << ", " << courseIt->second.title;
        }
        cout << "\n";
    }
}

// Recursively walks prerequisite links to collect every prerequisite needed
// before a course, not just the courses listed on its CSV row.
// Time complexity: O(V + E) over the reachable prerequisite subgraph;
// space complexity: O(V).
static void collectPrerequisites(const map<string, Course>& courses,
                                 const string& courseNumber,
                                 set<string>& visited,
                                 vector<string>& result) {
    auto courseIt = courses.find(courseNumber);
    if (courseIt == courses.end()) {
        return;
    }

    for (const string& prerequisite : courseIt->second.prereqs) {
        if (visited.insert(prerequisite).second) {
            collectPrerequisites(courses, prerequisite, visited, result);
            result.push_back(prerequisite);
        }
    }
}

// Prints prerequisites in dependency order so the direct and indirect
// requirements are easy to follow.
static void printAllPrerequisites(const map<string, Course>& courses,
                                  const string& userInput) {
    string courseNumber = toUpper(trim(userInput));
    if (courses.find(courseNumber) == courses.end()) {
        cout << "Course '" << courseNumber << "' not found.\n";
        return;
    }

    set<string> visited;
    vector<string> prerequisites;
    collectPrerequisites(courses, courseNumber, visited, prerequisites);

    cout << "All prerequisites for " << courseNumber << ":\n";
    if (prerequisites.empty()) {
        cout << "None\n";
        return;
    }

    for (const string& prerequisite : prerequisites) {
        cout << "- " << prerequisite;
        auto prerequisiteIt = courses.find(prerequisite);
        if (prerequisiteIt != courses.end()) {
            cout << ", " << prerequisiteIt->second.title;
        }
        cout << "\n";
    }
}

// Loads course data from a CSV file into the map
static bool loadCourses(const string& filename,
                        map<string, Course>& courses) {
    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Error: Could not open file '" << filename << "'.\n";
        return false;
    }

    courses.clear();
    string line;
    size_t lineNumber = 0;
    bool validationPassed = true;
    set<string> courseNumbers;

    while (getline(file, line)) {
        ++lineNumber;
        line = trim(line);
        if (line.empty()) continue;

        auto tokens = splitCSV(line);
        if (tokens.size() < 2 || tokens[0].empty() || tokens[1].empty()) {
            cout << "Validation error on line " << lineNumber
                 << ": each record needs a course number and title.\n";
            validationPassed = false;
            continue;
        }

        Course c;
        c.number = toUpper(tokens[0]);
        c.title = tokens[1];

        if (!courseNumbers.insert(c.number).second) {
            cout << "Validation error on line " << lineNumber
                 << ": duplicate course number '" << c.number << "'.\n";
            validationPassed = false;
            continue;
        }

        for (size_t i = 2; i < tokens.size(); ++i) {
            if (!tokens[i].empty()) {
                c.prereqs.push_back(toUpper(tokens[i]));
            }
        }

        courses[c.number] = c;
    }

    // A prerequisite must refer to a course that was loaded. Reporting this
    // here prevents an invalid relationship from disappearing during graph
    // construction.
    for (const auto& entry : courses) {
        for (const string& prerequisite : entry.second.prereqs) {
            if (courses.find(prerequisite) == courses.end()) {
                cout << "Validation error: " << entry.first
                     << " references missing prerequisite '" << prerequisite << "'.\n";
                validationPassed = false;
            }
        }
    }

    if (!validationPassed) {
        courses.clear();
        cout << "Course data was not loaded because validation failed.\n";
        return false;
    }

    cout << "Loaded " << courses.size() << " courses.\n";
    return true;
}

// Displays the menu options
static void printMenu() {
    cout << "1. Load Data Structure.\n";
    cout << "2. Print Course List.\n";
    cout << "3. Print Course.\n";
    cout << "4. Print Dependency Graph.\n";
    cout << "5. Check for Circular Prerequisites.\n";
    cout << "6. Print Prerequisite-First Course Order.\n";
    cout << "7. Print All Prerequisites for a Course.\n";
    cout << "9. Exit\n";
}

// Prints all courses in sorted alphanumeric order
static void printCourseList(const map<string, Course>& courses) {
    cout << "Here is a sample schedule:\n";
    for (const auto& kv : courses) {
        cout << kv.second.number << ", "
             << kv.second.title << "\n";
    }
}

// Prints a single course with prerequisite numbers and titles
static void printCourseInfo(const map<string, Course>& courses,
                            const string& userInput) {
    string key = toUpper(trim(userInput));
    auto it = courses.find(key);

    if (it == courses.end()) {
        cout << "Course '" << key << "' not found.\n";
        return;
    }

    const Course& c = it->second;
    cout << c.number << ", " << c.title << "\n";

    if (c.prereqs.empty()) {
        cout << "Prerequisites: None\n";
        return;
    }

    cout << "Prerequisites:\n";
    for (const auto& pre : c.prereqs) {
        cout << " - " << pre;
        auto pit = courses.find(pre);
        if (pit != courses.end()) {
            cout << ", " << pit->second.title;
        }
        cout << "\n";
    }
}

// Program entry point and main control loop
int main() {
    map<string, Course> courses;
    DependencyGraph graph;
    bool loaded = false;

    cout << "Welcome to the course planner.\n";

    int choice = 0;
    while (choice != 9) {
        printMenu();
        cout << "What would you like to do? ";

        string input;
        getline(cin >> ws, input);

        try {
            choice = stoi(trim(input));
        } catch (...) {
            cout << "Invalid input.\n";
            continue;
        }

        if (choice == 1) {
            cout << "Enter file name: ";
            string filename;
            getline(cin >> ws, filename);
            loaded = loadCourses(filename, courses);
            if (loaded) {
                graph = buildDependencyGraph(courses);
            }

        } else if (choice == 2) {
            if (!loaded) {
                cout << "Please load data first.\n";
                continue;
            }
            printCourseList(courses);

        } else if (choice == 3) {
            if (!loaded) {
                cout << "Please load data first.\n";
                continue;
            }
            cout << "What course do you want to know about? ";
            string courseNum;
            getline(cin >> ws, courseNum);
            printCourseInfo(courses, courseNum);

        } else if (choice == 4) {
            if (!loaded) {
                cout << "Please load data first.\n";
                continue;
            }
            printDependencyGraph(graph, courses);

        } else if (choice == 5) {
            if (!loaded) {
                cout << "Please load data first.\n";
                continue;
            }
            if (hasCycle(graph)) {
                cout << "Circular prerequisite detected.\n";
            } else {
                cout << "No circular prerequisites detected.\n";
            }

        } else if (choice == 6) {
            if (!loaded) {
                cout << "Please load data first.\n";
                continue;
            }
            printTopologicalOrder(graph, courses);

        } else if (choice == 7) {
            if (!loaded) {
                cout << "Please load data first.\n";
                continue;
            }
            cout << "What course do you want prerequisites for? ";
            string courseNum;
            getline(cin >> ws, courseNum);
            printAllPrerequisites(courses, courseNum);

        } else if (choice == 9) {
            cout << "Thank you for using the course planner!\n";

        } else {
            cout << choice << " is not a valid option.\n";
        }
    }

    return 0;
}
