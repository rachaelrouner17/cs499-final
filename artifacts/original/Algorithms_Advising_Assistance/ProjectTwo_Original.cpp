//CS 300 Project Two
// Rachael Baker

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
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

    while (getline(file, line)) {
        line = trim(line);
        if (line.empty()) continue;

        auto tokens = splitCSV(line);
        if (tokens.size() < 2) continue;

        Course c;
        c.number = toUpper(tokens[0]);
        c.title = tokens[1];

        for (size_t i = 2; i < tokens.size(); ++i) {
            if (!tokens[i].empty()) {
                c.prereqs.push_back(toUpper(tokens[i]));
            }
        }

        courses[c.number] = c;
    }

    cout << "Loaded " << courses.size() << " courses.\n";
    return true;
}

// Displays the menu options
static void printMenu() {
    cout << "1. Load Data Structure.\n";
    cout << "2. Print Course List.\n";
    cout << "3. Print Course.\n";
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

        } else if (choice == 9) {
            cout << "Thank you for using the course planner!\n";

        } else {
            cout << choice << " is not a valid option.\n";
        }
    }

    return 0;
}
