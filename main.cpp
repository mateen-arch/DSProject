#include <iostream>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#define INF 99999
using namespace std;
using namespace sf;

struct Route {
    string destination;
    string date;
    string depTime;
    string arrTime;
    int cost;
    string company;
    int travelTime;
};

class Port {
public:
    string name;
    int cost;
    Port(string n, int c) {
        name = n;
        cost = c;
    }
};

class Graph {
public:
    vector<Port> ports;               
    vector<vector<Route>> routes;    
    
    void addPort(const string& name, int cost) {
        ports.push_back(Port(name, cost));   
        routes.push_back(vector<Route>());   
    }
    
    void addRoute(const string& src, const Route& route) {
        int srcIdx = -1;
        for (int i = 0; i < ports.size(); i++) {
            if (ports[i].name == src) {
                srcIdx = i;
                break;
            }
        }
        if (srcIdx == -1) {
            cout << "Source port not found: " << src << ", adding it automatically." << endl;
            addPort(src, 0); 
            srcIdx = ports.size() - 1;
        }
        routes[srcIdx].push_back(route);
    }

    int getPortIndex(const string& name) const {
        for (int i = 0; i < ports.size(); i++) {
            if (ports[i].name == name) {
                return i;
            }
        }
        return -1;
    }
    
    void parsePorts(const string& filename) {
        ifstream file(filename);
        if (!file) {
            cout << "Error opening port file!" << endl;
            return;
        }
        string name;
        int cost;
        while (file >> name >> cost) {
            addPort(name, cost);
        }
    }
    
    void printPorts() {
        if (ports.empty()) {
            cout << "No ports loaded!" << endl;
            return;
        }
        cout << "=== Ports Loaded ===" << endl;
        for (size_t i = 0; i < ports.size(); i++) {
            cout << i+1 << ". " << ports[i].name 
                 << " | Cost: " << ports[i].cost << endl;
        }
        cout << "==================" << endl;
    }
int timeToMinutes(const string& time) {
    int hours = stoi(time.substr(0, 2));
    int minutes = stoi(time.substr(3, 2));
    return hours * 60 + minutes;
}
int calculateTravelTime(const string& depTime, const string& arrTime) {
    int depMinutes = timeToMinutes(depTime);
    int arrMinutes = timeToMinutes(arrTime);
    
    int travelTime = arrMinutes - depMinutes;
    if (travelTime < 0) {
        travelTime += 24 * 60;  
    }
    
    return travelTime;
}
void parseRoute(const string& filename) {
    ifstream file(filename);
    if (!file) {
        cout << "Error opening routes file!" << endl;
        return;
    }
    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string origin, destination, date, depTime, arrTime, company;
        int cost;
        if (!(ss >> origin >> destination >> date >> depTime >> arrTime >> cost)) {
            cout << "Route line format error: " << line << endl;
            continue;
        }
        getline(ss, company);
        if (!company.empty() && company[0] == ' ')
            company.erase(0, 1);
        int travelTime = calculateTravelTime(depTime, arrTime);
        
        Route r = {destination, date, depTime, arrTime, cost, company, travelTime};
        addRoute(origin, r);
    }
    cout << "Routes loaded:" << endl;
    for (int i = 0; i < ports.size(); i++) {
        cout << ports[i].name << " has " << routes[i].size() << " routes" << endl;
    }
}
struct ShortestRouteResult {
    vector<int> dist;
    vector<int> parent;
    int srcIdx;
    bool found;
};

ShortestRouteResult findShortestRouteData(Port* src) {
    int n = ports.size();

    vector<int> dist(n, INF);
    vector<bool> visited(n, false);
    vector<int> parent(n, -1);

    // Find index of source port
    int srcIdx = -1;
    for (int i = 0; i < n; i++) {
        if (ports[i].name == src->name) {
            srcIdx = i;
            break;
        }
    }

    if (srcIdx == -1) {
        cout << "Source port not found!" << endl;
        return {dist, parent, -1, false};
    }

    dist[srcIdx] = 0;

    // Dijkstra for shortest time (travelTime)
    for (int count = 0; count < n - 1; count++) {
        int u = -1;
        int minDist = INF;

        // Find unvisited port with smallest distance
        for (int i = 0; i < n; i++) {
            if (!visited[i] && dist[i] < minDist) {
                minDist = dist[i];
                u = i;
            }
        }

        if (u == -1 || dist[u] == INF)
            break;

        visited[u] = true;

        // Relax edges
        for (const Route& route : routes[u]) {

            int v = -1;
            for (int i = 0; i < n; i++) {
                if (ports[i].name == route.destination) {
                    v = i;
                    break;
                }
            }

            if (v != -1 && !visited[v]) {
                int newDist = dist[u] + route.travelTime;

                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    parent[v] = u;
                }
            }
        }
    }

    return {dist, parent, srcIdx, true};
}

    ShortestRouteResult findCheapestRoute(Port* src){
            int n = ports.size();
    vector<int> dist(n, INF);
    vector<bool> visited(n, false);
    vector<int> parent(n, -1);
    
    int srcIdx = -1;
    for (int i = 0; i < n; i++) {
        if (ports[i].name == src->name) {
            srcIdx = i;
            break;
        }
    }
    
    if (srcIdx == -1) {
        cout << "Source port not found!" << endl;
        return {dist, parent, -1, false};
    }
    
    dist[srcIdx] = 0;
    
    for (int count = 0; count < n - 1; count++) {
        int u = -1;
        int minDist = INF;
        
        for (int i = 0; i < n; i++) {
            if (!visited[i] && dist[i] < minDist) {
                minDist = dist[i];
                u = i;
            }
        }
        
        if (u == -1 || dist[u] == INF) break;
        visited[u] = true;
        
        for (const Route& route : routes[u]) {
            int v = -1;
            for (int i = 0; i < n; i++) {
                if (ports[i].name == route.destination) {
                    v = i;
                    break;
                }
            }
            
            if (v != -1 && !visited[v]) {
                int newDist = dist[u] + route.cost + ports[v].cost;  
                if (newDist < dist[v]) {
                    dist[v] = newDist;
                    parent[v] = u;
                }
            }
        }
    }
    
    return {dist, parent, srcIdx, true};
    }
    string buildPathString(const vector<int>& parent, int destIdx) {
    vector<int> path;
    for (int v = destIdx; v != -1; v = parent[v]) {
        path.push_back(v);
    }
    reverse(path.begin(), path.end());

    string s;
    for (size_t i = 0; i < path.size(); i++) {
        s += ports[path[i]].name;
        if (i < path.size() - 1) s += " -> ";
    }
    return s;
}
struct UserPreferences {
    vector<string> preferredCompanies;
    vector<string> avoidPorts;
    int maxVoyageTime;  // in minutes
    bool hasCompanyFilter;
    bool hasPortFilter;
    bool hasTimeFilter;
};
bool routeMatchesPreferences(const Route& route, const UserPreferences& prefs) const {
    // Check company preference
    if (prefs.hasCompanyFilter) {
        bool companyFound = false;
        for (const string& company : prefs.preferredCompanies) {
            if (route.company == company) {
                companyFound = true;
                break;
            }
        }
        if (!companyFound) return false;
    }
    
    // Check voyage time
    if (prefs.hasTimeFilter) {
        if (route.travelTime > prefs.maxVoyageTime) {
            return false;
        }
    }
    
    return true;
}

bool portIsAvoid(const string& portName, const UserPreferences& prefs) const {
    if (!prefs.hasPortFilter) return false;
    
    for (const string& avoidPort : prefs.avoidPorts) {
        if (portName == avoidPort) {
            return true;
        }
    }
    return false;
}

// Get all unique shipping companies
vector<string> getAllShippingCompanies() const {
    set<string> companies;
    for (int i = 0; i < routes.size(); i++) {
        for (const Route& route : routes[i]) {
            companies.insert(route.company);
        }
    }
    return vector<string>(companies.begin(), companies.end());
}
    void displayGraph() {
        printPorts();
        for (int i = 0; i < ports.size(); i++) {
            cout << "Routes from port: " << ports[i].name << " -> ";
            if (routes[i].empty()) {
                cout << "No routes";
            } else {
                for (int j = 0; j < routes[i].size(); j++) {
                    cout << routes[i][j].destination;
                    if (j != routes[i].size() - 1) cout << " -> ";
                }
            }
            cout << endl;
        }
    }
};

struct CompleteRoute {
    vector<int> portPath;        // Indices of ports in the path
    vector<Route> routeLegs;     // Actual route objects for each leg
    int totalCost;
    int totalTime;
    int layoverCount;
};

// Function prototypes - add these BEFORE main()
vector<CompleteRoute> findAllPossibleRoutes(Graph& graph, int originIdx, int destIdx);
vector<string> formatAllRoutes(Graph& graph, int originIdx, int destIdx);

const int MAX_LAYOVERS = 3;

bool findCheapestEnumeratedRoute(Graph& graph, int originIdx, int destIdx, CompleteRoute& cheapestOut);

void dfsEnumerateRoutes(Graph& graph,
                        int currentIdx,
                        int destIdx,
                        vector<int>& currentPath,
                        vector<Route>& currentLegs,
                        vector<CompleteRoute>& results,
                        int maxLegs) {
    if (currentLegs.size() > static_cast<size_t>(maxLegs)) {
        return;
    }

    if (currentIdx == destIdx && !currentLegs.empty()) {
        CompleteRoute cr;
        cr.portPath = currentPath;
        cr.routeLegs = currentLegs;
        cr.totalCost = 0;
        cr.totalTime = 0;
        for (const Route& leg : currentLegs) {
            cr.totalCost += leg.cost;
            cr.totalTime += leg.travelTime;
        }
        for (size_t i = 1; i < currentPath.size(); i++) {
            cr.totalCost += graph.ports[currentPath[i]].cost;
        }
        cr.layoverCount = currentPath.size() >= 2 ? static_cast<int>(currentPath.size()) - 2 : 0;
        results.push_back(cr);
        return;
    }

    if (currentLegs.size() == static_cast<size_t>(maxLegs)) {
        return;
    }

    for (const Route& route : graph.routes[currentIdx]) {
        int nextIdx = graph.getPortIndex(route.destination);
        if (nextIdx == -1) continue;

        if (find(currentPath.begin(), currentPath.end(), nextIdx) != currentPath.end()) {
            continue;
        }

        currentPath.push_back(nextIdx);
        currentLegs.push_back(route);
        dfsEnumerateRoutes(graph, nextIdx, destIdx, currentPath, currentLegs, results, maxLegs);
        currentLegs.pop_back();
        currentPath.pop_back();
    }
}

vector<CompleteRoute> findAllPossibleRoutes(Graph& graph, int originIdx, int destIdx) {
    vector<CompleteRoute> allRoutes;
    if (originIdx < 0 || originIdx >= graph.ports.size() ||
        destIdx < 0 || destIdx >= graph.ports.size()) {
        return allRoutes;
    }

    vector<int> currentPath = {originIdx};
    vector<Route> currentLegs;
    int maxLegs = MAX_LAYOVERS + 1;
    dfsEnumerateRoutes(graph, originIdx, destIdx, currentPath, currentLegs, allRoutes, maxLegs);

    return allRoutes;
}

bool findCheapestEnumeratedRoute(Graph& graph, int originIdx, int destIdx, CompleteRoute& cheapestOut) {
    vector<CompleteRoute> allRoutes = findAllPossibleRoutes(graph, originIdx, destIdx);
    
    if (allRoutes.empty()) {
        return false;
    }

    auto cmp = [](const CompleteRoute& a, const CompleteRoute& b) {
        if (a.totalCost == b.totalCost) {
            return a.totalTime < b.totalTime;
        }
        return a.totalCost < b.totalCost;
    };

    auto bestIt = min_element(allRoutes.begin(), allRoutes.end(), cmp);
    cheapestOut = *bestIt;
    return true;
}

// Format all routes for display - IMPROVED VERSION
vector<string> formatAllRoutes(Graph& graph, int originIdx, int destIdx) {
    vector<string> display;

    string originName = graph.ports[originIdx].name;
    string destName = graph.ports[destIdx].name;

    // Find all possible routes
    vector<CompleteRoute> allRoutes = findAllPossibleRoutes(graph, originIdx, destIdx);

    if (allRoutes.empty()) {
        display.push_back("=== NO ROUTES FOUND ===");
        display.push_back("");
        display.push_back("No direct or connected routes available");
        display.push_back("between " + originName + " and " + destName + ".");
        display.push_back("");
        display.push_back("This could mean:");
        display.push_back("- No direct routes exist");
        display.push_back("- No single-layover connections available");
        display.push_back("- Routes may require 2+ layovers");
        return display;
    }

    // Separate routes by layover count
    vector<CompleteRoute> directRoutes, singleLayoverRoutes, multiLayoverRoutes;
    for (const auto& route : allRoutes) {
        if (route.layoverCount == 0) directRoutes.push_back(route);
        else if (route.layoverCount == 1) singleLayoverRoutes.push_back(route);
        else multiLayoverRoutes.push_back(route);
    }

    auto formatPath = [&](const vector<int>& path) {
        string s;
        for (size_t i = 0; i < path.size(); i++) {
            s += graph.ports[path[i]].name;
            if (i < path.size() - 1) s += " -> ";
        }
        return s;
    };

    auto formatRouteDetails = [&](const CompleteRoute& cr) {
        vector<string> lines;
        for (size_t i = 0; i < cr.routeLegs.size(); i++) {
            const Route& leg = cr.routeLegs[i];
            string from = graph.ports[cr.portPath[i]].name;
            string to = graph.ports[cr.portPath[i + 1]].name;
            lines.push_back("  Leg " + to_string(i + 1) + ": " + from + " -> " + to);
            lines.push_back("    Date: " + leg.date);
            lines.push_back("    Time: " + leg.depTime + " -> " + leg.arrTime);
            lines.push_back("    Cost: $" + to_string(leg.cost));
            lines.push_back("    Company: " + leg.company);
        }
        return lines;
    };

    // Lambda to display routes of a given category
    auto displayRouteCategory = [&](const vector<CompleteRoute>& routes, const string& title) {
        if (!routes.empty()) {
            display.push_back(title);
            display.push_back("");
            for (size_t i = 0; i < routes.size(); i++) {
                display.push_back("Route " + to_string(i + 1) + ": " + formatPath(routes[i].portPath));
                display.push_back("  Total Cost: $" + to_string(routes[i].totalCost));
                display.push_back("  Total Travel Time: " + 
                    to_string(routes[i].totalTime / 60) + "h " + 
                    to_string(routes[i].totalTime % 60) + "m");
                display.push_back("");
                vector<string> legLines = formatRouteDetails(routes[i]);
                display.insert(display.end(), legLines.begin(), legLines.end());
                display.push_back("");
            }
        }
    };

    // Display all route categories
    displayRouteCategory(directRoutes, "=== DIRECT ROUTES ===");
    displayRouteCategory(singleLayoverRoutes, "=== CONNECTED ROUTES (1 Layover) ===");
    displayRouteCategory(multiLayoverRoutes, "=== MULTI-LAYOVER ROUTES (2+ Layovers) ===");

    // Summary
    display.push_back("=== SUMMARY ===");
    display.push_back("");
    display.push_back("Total Direct Routes: " + to_string(directRoutes.size()));
    display.push_back("Total 1-Layover Routes: " + to_string(singleLayoverRoutes.size()));
    display.push_back("Total 2+ Layover Routes: " + to_string(multiLayoverRoutes.size()));
    display.push_back("Total Routes Available: " + to_string(allRoutes.size()));
    display.push_back("Max Layovers Considered: " + to_string(MAX_LAYOVERS));
    display.push_back("");

    // Highlight cheapest and fastest routes
    if (!allRoutes.empty()) {
        auto cheapestIt = min_element(allRoutes.begin(), allRoutes.end(),
                                      [](const CompleteRoute& a, const CompleteRoute& b) {
                                          return a.totalCost < b.totalCost;
                                      });
        auto fastestIt = min_element(allRoutes.begin(), allRoutes.end(),
                                     [](const CompleteRoute& a, const CompleteRoute& b) {
                                         return a.totalTime < b.totalTime;
                                     });

        display.push_back("=== EXTRA ROUTE DETAILS ===");
        display.push_back("");
        display.push_back("Cheapest Option: $" + to_string(cheapestIt->totalCost));
        display.push_back("  Path: " + formatPath(cheapestIt->portPath));
        display.push_back("");
        display.push_back("Fastest Option: " + 
                          to_string(fastestIt->totalTime / 60) + "h " + 
                          to_string(fastestIt->totalTime % 60) + "m");
        display.push_back("  Path: " + formatPath(fastestIt->portPath));
    }
    display.push_back(""); 
    display.push_back("=== HIGHLIGHTED ROUTES COLOUR SCHEME ==="); 
    display.push_back(""); 
    display.push_back("Magenta = Direct Routes"); 
    display.push_back("Orange = 1-Layover Routes"); 
    display.push_back("Blue = Multi-Layover Routes"); 
    display.push_back("");
    return display;
}

class Button {
private:
    RectangleShape shape;
    Text text;
    Color normalColor;
    Color hoverColor;
    Color clickColor;

public:
    Button(Vector2f position, Vector2f size, string buttonText, Font& font) {
        shape.setSize(size);
        shape.setPosition(position);
        
        normalColor = Color(70, 130, 180);
        hoverColor = Color(100, 160, 210);
        clickColor = Color(50, 100, 150);
        shape.setFillColor(normalColor);
        shape.setOutlineThickness(2);
        shape.setOutlineColor(Color::White);
        
        text.setFont(font);
        text.setString(buttonText);
        text.setCharacterSize(24);
        text.setFillColor(Color::White);
        
        FloatRect textBounds = text.getLocalBounds();
        text.setOrigin(textBounds.left + textBounds.width / 2.0f,
                       textBounds.top + textBounds.height / 2.0f);
        text.setPosition(position.x + size.x / 2.0f,
                        position.y + size.y / 2.0f);
    }
    
    void draw(RenderWindow& window) {
        window.draw(shape);
        window.draw(text);
    }
    
    bool isMouseOver(RenderWindow& window) {
        Vector2i mousePos = Mouse::getPosition(window);
        FloatRect bounds = shape.getGlobalBounds();
        return bounds.contains(static_cast<Vector2f>(mousePos));
    }
    
    void update(RenderWindow& window) {
        if (isMouseOver(window)) {
            if (Mouse::isButtonPressed(Mouse::Left)) {
                shape.setFillColor(clickColor);
            } else {
                shape.setFillColor(hoverColor);
            }
        } else {
            shape.setFillColor(normalColor);
        }
    }
    
    bool isClicked(RenderWindow& window, Event& event) {
        if (event.type == Event::MouseButtonReleased && 
            event.mouseButton.button == Mouse::Left) {
            return isMouseOver(window);
        }
        return false;
    }
};
class RouteDisplayWindow {
private:
    RectangleShape background;
    RectangleShape titleBar;
    Text titleText;
    vector<Text> routeLines;
    vector<RectangleShape> routeBoxes;
    Button* closeButton;
    bool isVisible;
    Font* font;
    float scrollOffset;
    float maxScrollOffset;
    float windowWidth;
    float windowHeight;
    
public:
RouteDisplayWindow(Font& f, float winWidth, float winHeight) 
    : font(&f), isVisible(false), closeButton(nullptr), scrollOffset(0), maxScrollOffset(0), 
      windowWidth(winWidth), windowHeight(winHeight) {
    
    float width = windowWidth * 0.4f;  // Smaller - 40% of screen
    float height = windowHeight * 0.7f; // Smaller - 70% of screen
    float x = windowWidth - width - 20; // Position on RIGHT side instead of center
    float y = (windowHeight - height) / 2;
        
        background.setSize(Vector2f(width, height));
        background.setPosition(x, y);
        background.setFillColor(Color(240, 240, 240, 220)); // Added alpha = 220 for slight transparency
        background.setOutlineThickness(3);
        background.setOutlineColor(Color(50, 50, 50));
        
        titleBar.setSize(Vector2f(width, 50));
        titleBar.setPosition(x, y);
        titleBar.setFillColor(Color(70, 130, 180));
        
        titleText.setFont(f);
        titleText.setString("Available Routes");
        titleText.setCharacterSize(24);
        titleText.setFillColor(Color::White);
        titleText.setPosition(x + 10, y + 13);
        
        closeButton = new Button(Vector2f(x + width - 50, y + 10), Vector2f(40, 40), "X", f);
    }
    
    ~RouteDisplayWindow() {
        delete closeButton;
    }
    
    void show(const string& from, const string& to, const vector<string>& routes) {
        isVisible = true;
        scrollOffset = 0;
        routeLines.clear();
        routeBoxes.clear();
        
        titleText.setString("Routes: " + from + " -> " + to);
        
        float x = (windowWidth - (windowWidth * 0.7f)) / 2;
        float y = (windowHeight - (windowHeight * 0.85f)) / 2;
        float yPos = y + 70;
        
        if (routes.empty()) {
            Text text;
            text.setFont(*font);
            text.setString("No routes found between these ports.");
            text.setCharacterSize(22);
            text.setFillColor(Color(200, 50, 50));
            text.setOutlineThickness(1);
            text.setOutlineColor(Color::White);
            FloatRect bounds = text.getLocalBounds();
            text.setOrigin(bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
            text.setPosition(windowWidth / 2, windowHeight / 2);
            routeLines.push_back(text);
        } else {
            for (size_t i = 0; i < routes.size(); i++) {
                Text text;
                text.setFont(*font);
                text.setString(routes[i]);
                text.setCharacterSize(17);
                
                // Color coding for different types of info
                if (routes[i].find("===") != string::npos || 
                    routes[i].find("DIRECT") != string::npos || 
                    routes[i].find("CONNECTED") != string::npos) {
                    // Headers
                    text.setFillColor(Color(50, 100, 150));
                    text.setStyle(Text::Bold);
                    text.setCharacterSize(19);
                } else if (routes[i].find("->") != string::npos) {
                    // Route paths
                    text.setFillColor(Color(0, 100, 0));
                    text.setStyle(Text::Bold);
                } else if (routes[i].empty()) {
                    // Empty lines for spacing
                    text.setCharacterSize(10);
                } else {
                    // Details
                    text.setFillColor(Color::Black);
                }
                
                text.setPosition(x + 20, yPos);
                routeLines.push_back(text);
                
                if (!routes[i].empty() && routes[i].find("===") == string::npos) {
                    yPos += 25;
                } else {
                    yPos += 15;
                }
            }
        }
        
        float contentHeight = routeLines.size() * 25;
        float visibleHeight = windowHeight * 0.85f - 80;
        maxScrollOffset = max(0.0f, contentHeight - visibleHeight);
    }
    
    void hide() {
        isVisible = false;
        routeLines.clear();
        routeBoxes.clear();
        scrollOffset = 0;
        maxScrollOffset = 0;
    }
    
    bool checkClose(RenderWindow& window, Event& event) {
        if (isVisible && closeButton->isClicked(window, event)) {
            hide();
            return true;
        }
        return false;
    }
    
    void scroll(float delta) {
        if (!isVisible) return;
        scrollOffset -= delta * 30.0f;
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset > maxScrollOffset) scrollOffset = maxScrollOffset;
    }
    
    void update(RenderWindow& window) {
        if (isVisible) {
            closeButton->update(window);
        }
    }
    
    void draw(RenderWindow& window) {
        if (isVisible) {
            window.draw(background);
            window.draw(titleBar);
            window.draw(titleText);
            
            float y = (windowHeight - (windowHeight * 0.85f)) / 2;
            float maxY = y + windowHeight * 0.85f - 10;
            
            for (size_t i = 0; i < routeLines.size(); i++) {
                Text& line = routeLines[i];
                Vector2f originalPos = line.getPosition();
                float newY = originalPos.y - scrollOffset;
                if (newY >= y + 60 && newY <= maxY) {
                    line.setPosition(originalPos.x, newY);
                    window.draw(line);
                    line.setPosition(originalPos);
                }
            }
            
            closeButton->draw(window);
            
            if (maxScrollOffset > 0) {
                float x = (windowWidth - (windowWidth * 0.7f)) / 2;
                float width = windowWidth * 0.7f;
                RectangleShape scrollBar(Vector2f(10, 120));
                float scrollBarY = y + 70 + (scrollOffset / maxScrollOffset) * (windowHeight * 0.85f - 200);
                scrollBar.setPosition(x + width - 20, scrollBarY);
                scrollBar.setFillColor(Color(100, 100, 100, 150));
                window.draw(scrollBar);
            }
        }
    }
    
    bool visible() const { return isVisible; }
    
    bool isMouseOverWindow(RenderWindow& window) const {
        if (!isVisible) return false;
        Vector2i mousePos = Mouse::getPosition(window);
        FloatRect bounds = background.getGlobalBounds();
        return bounds.contains(static_cast<Vector2f>(mousePos));
    }
};
class InfoWindow {
private:
    RectangleShape background;
    RectangleShape titleBar;
    Text titleText;
    vector<Text> contentLines;
    Button* closeButton;
    bool isVisible;
    Font* font;
    float scrollOffset;
    float maxScrollOffset;
    float windowWidth;
    float windowHeight;
    
public:
    InfoWindow(Font& f, float winWidth, float winHeight) : font(&f), isVisible(false), closeButton(nullptr), scrollOffset(0), maxScrollOffset(0), windowWidth(winWidth), windowHeight(winHeight) {
        float infoWidth = windowWidth * 0.5f;
        float infoHeight = windowHeight * 0.7f;
        float infoX = (windowWidth - infoWidth) / 2;
        float infoY = (windowHeight - infoHeight) / 2;
        
        background.setSize(Vector2f(infoWidth, infoHeight));
        background.setPosition(infoX, infoY);
        background.setFillColor(Color(240, 240, 240));
        background.setOutlineThickness(3);
        background.setOutlineColor(Color(50, 50, 50));
        
        titleBar.setSize(Vector2f(infoWidth, 50));
        titleBar.setPosition(infoX, infoY);
        titleBar.setFillColor(Color(70, 130, 180));
        
        titleText.setFont(f);
        titleText.setCharacterSize(24);
        titleText.setFillColor(Color::White);
        titleText.setPosition(infoX + 10, infoY + 13);
        
        closeButton = new Button(Vector2f(infoX + infoWidth - 50, infoY + 10), Vector2f(40, 40), "X", f);
    }
    
    ~InfoWindow() {
        delete closeButton;
    }
    
    void show(const string& portName, const vector<string>& info) {
        isVisible = true;
        scrollOffset = 0;
        titleText.setString(portName + " - Port Information");
        
        contentLines.clear();
        float infoX = (windowWidth - (windowWidth * 0.5f)) / 2;
        float infoY = (windowHeight - (windowHeight * 0.7f)) / 2;
        float yPos = infoY + 70;
        
        for (const string& line : info) {
            Text text;
            text.setFont(*font);
            text.setString(line);
            text.setCharacterSize(18);
            text.setFillColor(Color::Black);
            text.setPosition(infoX + 15, yPos);
            contentLines.push_back(text);
            yPos += 28;
        }
        float contentHeight = contentLines.size() * 28;
        float visibleHeight = windowHeight * 0.7f - 80;
        maxScrollOffset = max(0.0f, contentHeight - visibleHeight);
    }
    
    void hide() {
        isVisible = false;
        contentLines.clear();
        scrollOffset = 0;
        maxScrollOffset = 0;
    }
    
    bool checkClose(RenderWindow& window, Event& event) {
        if (isVisible && closeButton->isClicked(window, event)) {
            hide();
            return true;
        }
        return false;
    }
    
    void handleMouseWheel(RenderWindow& window) {
        if (!isVisible) return;
        
        Vector2i mousePos = Mouse::getPosition(window);
        FloatRect bounds = background.getGlobalBounds();
        
        if (bounds.contains(static_cast<Vector2f>(mousePos))) {
            // Check for mouse wheel movement
            // We'll handle this in the event loop
        }
    }
    
    void scroll(float delta) {
        if (!isVisible) return;
        
        scrollOffset -= delta * 30.0f; 
        if (scrollOffset < 0) scrollOffset = 0;
        if (scrollOffset > maxScrollOffset) scrollOffset = maxScrollOffset;
    }
    
    void update(RenderWindow& window) {
        if (isVisible) {
            closeButton->update(window);
        }
    }
    
    void draw(RenderWindow& window) {
        if (isVisible) {
            window.draw(background);
            window.draw(titleBar);
            window.draw(titleText);
            
            float infoY = (windowHeight - (windowHeight * 0.7f)) / 2;
            float maxY = infoY + windowHeight * 0.7f - 10;
            
            for (size_t i = 0; i < contentLines.size(); i++) {
                Text& line = contentLines[i];
                Vector2f originalPos = line.getPosition();
                float newY = originalPos.y - scrollOffset;
                if (newY >= infoY + 60 && newY <= maxY) {
                    line.setPosition(originalPos.x, newY);
                    window.draw(line);
                    line.setPosition(originalPos); 
                }
            }
            closeButton->draw(window);
            if (maxScrollOffset > 0) {
                float infoX = (windowWidth - (windowWidth * 0.5f)) / 2;
                float infoWidth = windowWidth * 0.5f;
                RectangleShape scrollBar(Vector2f(10, 120));
                float scrollBarY = infoY + 70 + (scrollOffset / maxScrollOffset) * (windowHeight * 0.7f - 200);
                scrollBar.setPosition(infoX + infoWidth - 20, scrollBarY);
                scrollBar.setFillColor(Color(100, 100, 100, 150));
                window.draw(scrollBar);
            }
        }
    }
    bool visible() const { return isVisible; }
    bool isMouseOverWindow(RenderWindow& window) const {
        if (!isVisible) return false;
        Vector2i mousePos = Mouse::getPosition(window);
        FloatRect bounds = background.getGlobalBounds();
        return bounds.contains(static_cast<Vector2f>(mousePos));
    }
};
class Location {
public:
    CircleShape pin;
    Text label;
    string name;
    Vector2f position;
    Color normalColor;
    Color hoverColor;
    
    Location(string portName, Vector2f pos, Font& font) {
        name = portName;
        position = pos;
        
        pin.setRadius(18);
        pin.setOrigin(18, 18);
        pin.setPosition(pos);
        
        normalColor = Color(220, 50, 50);
        hoverColor = Color(255, 100, 100);
        pin.setFillColor(normalColor);
        pin.setOutlineThickness(3);
        pin.setOutlineColor(Color::White);
        
        label.setFont(font);
        label.setString(portName);
        label.setCharacterSize(20);
        label.setFillColor(Color::White);
        label.setOutlineThickness(2);
        label.setOutlineColor(Color::Black);
        
        FloatRect textBounds = label.getLocalBounds();
        label.setOrigin(textBounds.left + textBounds.width / 2, textBounds.top + textBounds.height);
        label.setPosition(pos.x, pos.y - 30);
    }
    
    bool isMouseOver(RenderWindow& window) {
        Vector2i mousePos = Mouse::getPosition(window);
        FloatRect bounds = pin.getGlobalBounds();
        return bounds.contains(static_cast<Vector2f>(mousePos));
    }
    
    bool isClicked(RenderWindow& window, Event& event) {
        if (event.type == Event::MouseButtonReleased && 
            event.mouseButton.button == Mouse::Left) {
            return isMouseOver(window);
        }
        return false;
    }
    
    void update(RenderWindow& window) {
        if (isMouseOver(window)) {
            pin.setFillColor(hoverColor);
            pin.setRadius(22);
            pin.setOrigin(22, 22);
        } else {
            pin.setFillColor(normalColor);
            pin.setRadius(18);
            pin.setOrigin(18, 18);
        }
    }
    
    void draw(RenderWindow& window) {
        window.draw(pin);
        window.draw(label);
    }
};
class NavigationMenu {
private:
    vector<Button*> buttons;
    bool isVisible;
    Text titleText;
    float windowWidth;
    float windowHeight;
    
public:
    NavigationMenu(Font& font, float winWidth, float winHeight) : isVisible(false), windowWidth(winWidth), windowHeight(winHeight) {
        titleText.setFont(font);
        titleText.setString("Route Navigation System");
        titleText.setCharacterSize(48);
        titleText.setFillColor(Color::White);
        titleText.setOutlineThickness(2);
        titleText.setOutlineColor(Color::Black);
        
        FloatRect textBounds = titleText.getLocalBounds();
        titleText.setOrigin(textBounds.left + textBounds.width / 2.0f,
                           textBounds.top + textBounds.height / 2.0f);
        titleText.setPosition(windowWidth / 2, windowHeight * 0.08f);
        
        float buttonWidth = 400;
        float buttonHeight = 70;
        float centerX = (windowWidth - buttonWidth) / 2;
        float startY = windowHeight * 0.18f;
        float spacing = buttonHeight + 15;
        
        buttons.push_back(new Button(Vector2f(centerX, startY), Vector2f(buttonWidth, buttonHeight), "Search for Routes", font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing), Vector2f(buttonWidth, buttonHeight), "Book Cargo Routes", font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 2), Vector2f(buttonWidth, buttonHeight), "Display Ports Info", font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 3), Vector2f(buttonWidth, buttonHeight), "Shortest Route", font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 4), Vector2f(buttonWidth, buttonHeight), "Cheapest Route", font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 5), Vector2f(buttonWidth, buttonHeight), "Filter Preferences", font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 6), Vector2f(buttonWidth, buttonHeight), "Process Layovers", font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 7), Vector2f(buttonWidth, buttonHeight), "Track Multi-Leg Journeys", font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 8.5f), Vector2f(buttonWidth, buttonHeight), "Back to Main Menu", font));
    }
    
    ~NavigationMenu() {
        for (auto btn : buttons) {
            delete btn;
        }
    }
    
    void show() {
        isVisible = true;
    }
    
    void hide() {
        isVisible = false;
    }
    
    bool visible() const {
        return isVisible;
    }
    
    void update(RenderWindow& window) {
        if (isVisible) {
            for (auto btn : buttons) {
                btn->update(window);
            }
        }
    }
    
    void draw(RenderWindow& window) {
        if (isVisible) {
            window.draw(titleText);
            for (auto btn : buttons) {
                btn->draw(window);
            }
        }
    }
    
    int checkClick(RenderWindow& window, Event& event) {
        if (!isVisible) return -1;
        
        for (int i = 0; i < buttons.size(); i++) {
            if (buttons[i]->isClicked(window, event)) {
                return i;
            }
        }
        return -1;
    }
};

class RouteTooltip {
private:
    RectangleShape background;
    vector<Text> lines;
    Font* font;
    bool isVisible;
    
public:
    RouteTooltip(Font& f) : font(&f), isVisible(false) {
        background.setFillColor(Color(40, 40, 40, 230));
        background.setOutlineThickness(2);
        background.setOutlineColor(Color(255, 165, 0));
    }
    
    void show(Vector2f mousePos, const vector<string>& details) {
        isVisible = true;
        lines.clear();
        
        float maxWidth = 0;
        float yOffset = 5;
        
        for (const string& detail : details) {
            Text text;
            text.setFont(*font);
            text.setString(detail);
            text.setCharacterSize(14);
            text.setFillColor(Color::White);
            
            FloatRect bounds = text.getLocalBounds();
            if (bounds.width > maxWidth) maxWidth = bounds.width;
            
            text.setPosition(mousePos.x + 15, mousePos.y + yOffset);
            lines.push_back(text);
            yOffset += 18;
        }
        
        // Position background
        background.setSize(Vector2f(maxWidth + 20, yOffset + 5));
        background.setPosition(mousePos.x + 10, mousePos.y);
    }
    
    void hide() {
        isVisible = false;
        lines.clear();
    }
    
    void draw(RenderWindow& window) {
        if (isVisible) {
            window.draw(background);
            for (auto& line : lines) {
                window.draw(line);
            }
        }
    }
    
    bool visible() const { return isVisible; }
};

class RouteEdge {
public:
    Vector2f start;
    Vector2f end;
    Route routeInfo;
    string sourceName;
    RectangleShape line;
    Color normalColor;
    Color hoverColor;
    bool isHovered;
    
    RouteEdge(Vector2f s, Vector2f e, const Route& r, const string& src) 
        : start(s), end(e), routeInfo(r), sourceName(src), isHovered(false) {
        
        // Calculate line properties
        Vector2f direction = end - start;
        float length = sqrt(direction.x * direction.x + direction.y * direction.y);
        float angle = atan2(direction.y, direction.x) * 180 / 3.14159f;
        
        // Create line as a thin rectangle (thicker and fully opaque for clarity)
        line.setSize(Vector2f(length, 2.5));
        line.setPosition(start);
        line.setRotation(angle);
        
        // Color based on cost (bright red lines for visibility, fully opaque)
        int costLevel = min(routeInfo.cost / 100, 255);
        normalColor = Color(255, 50, 50, 255); // Bright red, fully opaque
        hoverColor = Color(255, 165, 0, 255); // Orange when selected
        line.setFillColor(normalColor);
    }
    
    bool isMouseOver(RenderWindow& window) {
        Vector2i mousePos = Mouse::getPosition(window);
        Vector2f mousePosF = static_cast<Vector2f>(mousePos);
        
        // Check if mouse is near the line (within 15 pixels for easier clicking)
        Vector2f dir = end - start;
        float lineLength = sqrt(dir.x * dir.x + dir.y * dir.y);
        
        if (lineLength == 0) return false;
        
        // Project mouse position onto line
        Vector2f toMouse = mousePosF - start;
        float t = (toMouse.x * dir.x + toMouse.y * dir.y) / (lineLength * lineLength);
        t = max(0.0f, min(1.0f, t));
        
        Vector2f projection = start + dir * t;
        Vector2f diff = mousePosF - projection;
        float distance = sqrt(diff.x * diff.x + diff.y * diff.y);
        
        return distance < 15.0f;
    }
    
    void update(RenderWindow& window) {
        // Just check if mouse is over, don't change color automatically
        isHovered = isMouseOver(window);
    }
    
    bool isClicked(RenderWindow& window, Event& event) {
        if (event.type == Event::MouseButtonReleased && 
            event.mouseButton.button == Mouse::Left) {
            return isMouseOver(window);
        }
        return false;
    }
    
    void setSelected(bool selected) {
        if (selected) {
            line.setFillColor(hoverColor);
        } else {
            line.setFillColor(normalColor);
        }
    }
    
    void draw(RenderWindow& window) {
        window.draw(line);
    }
    
    vector<string> getRouteDetails() {
        vector<string> details;
        details.push_back("Route: " + sourceName + " -> " + routeInfo.destination);
        details.push_back("Date: " + routeInfo.date);
        details.push_back("Departure: " + routeInfo.depTime);
        details.push_back("Arrival: " + routeInfo.arrTime);
        details.push_back("Cost: $" + to_string(routeInfo.cost));
        details.push_back("Company: " + routeInfo.company);
        return details;
    }
};

void loadMapView(Texture& backgroundTexture, Sprite& backgroundSprite, 
                 vector<Location>& locations, vector<RouteEdge>& edges,
                 RenderWindow& window, Font& font, Graph& graph) {
    if (backgroundTexture.loadFromFile("map.png")) {
        backgroundSprite.setTexture(backgroundTexture);
        Vector2u textureSize = backgroundTexture.getSize();
        Vector2u windowSize = window.getSize();
        
        float scaleX = (float)windowSize.x / textureSize.x;
        float scaleY = (float)windowSize.y / textureSize.y;
        
        backgroundSprite.setScale(scaleX, scaleY);
        locations.clear();
        edges.clear();
        int numPorts = graph.ports.size();
        
        // Define safe zones spread across the entire screen with better distribution
        float screenWidth = windowSize.x;
        float screenHeight = windowSize.y;
        float margin = 80; // Margin from edges
        
        vector<pair<Vector2f, Vector2f>> safeZones = {
            // Top-left region
            {Vector2f(margin, margin), Vector2f(screenWidth * 0.25f, screenHeight * 0.25f)},
            // Top-center region
            {Vector2f(screenWidth * 0.35f, margin), Vector2f(screenWidth * 0.65f, screenHeight * 0.25f)},
            // Top-right region
            {Vector2f(screenWidth * 0.75f, margin), Vector2f(screenWidth - margin, screenHeight * 0.25f)},
            
            // Middle-left region
            {Vector2f(margin, screenHeight * 0.30f), Vector2f(screenWidth * 0.25f, screenHeight * 0.50f)},
            // Center region
            {Vector2f(screenWidth * 0.35f, screenHeight * 0.30f), Vector2f(screenWidth * 0.65f, screenHeight * 0.50f)},
            // Middle-right region
            {Vector2f(screenWidth * 0.75f, screenHeight * 0.30f), Vector2f(screenWidth - margin, screenHeight * 0.50f)},
            
            // Bottom-left region
            {Vector2f(margin, screenHeight * 0.55f), Vector2f(screenWidth * 0.25f, screenHeight * 0.75f)},
            // Bottom-center region
            {Vector2f(screenWidth * 0.35f, screenHeight * 0.55f), Vector2f(screenWidth * 0.65f, screenHeight * 0.75f)},
            // Bottom-right region
            {Vector2f(screenWidth * 0.75f, screenHeight * 0.55f), Vector2f(screenWidth - margin, screenHeight * 0.75f)},
            
            // Lower-left region
            {Vector2f(margin, screenHeight * 0.78f), Vector2f(screenWidth * 0.30f, screenHeight - margin)},
            // Lower-center region
            {Vector2f(screenWidth * 0.35f, screenHeight * 0.78f), Vector2f(screenWidth * 0.65f, screenHeight - margin)},
            // Lower-right region
            {Vector2f(screenWidth * 0.70f, screenHeight * 0.78f), Vector2f(screenWidth - margin, screenHeight - margin)}
        };
        
        int portsPerZone = (numPorts + safeZones.size() - 1) / safeZones.size();
        
        for (int i = 0; i < numPorts; i++) {
            int zoneIdx = i / portsPerZone;
            if (zoneIdx >= safeZones.size()) zoneIdx = safeZones.size() - 1;
            
            int portInZone = i % portsPerZone;
            
            Vector2f zoneMin = safeZones[zoneIdx].first;
            Vector2f zoneMax = safeZones[zoneIdx].second;
            
            // Distribute ports within the zone
            int cols = min(3, portsPerZone); 
            int col = portInZone % cols;
            int row = portInZone / cols;
            
            float zoneWidth = zoneMax.x - zoneMin.x;
            float zoneHeight = zoneMax.y - zoneMin.y;
            
            float x = zoneMin.x + (col * zoneWidth / max(1.0f, (float)(cols - 1)));
            float y = zoneMin.y + (row * zoneHeight / max(1.0f, (float)((portsPerZone + cols - 1) / cols - 1)));
            
            // Add slight random offset for natural distribution
            if (portsPerZone > 1) {
                x += (i % 5 - 2) * 15; 
                y += ((i * 3) % 5 - 2) * 15;   
            }
            
            // Ensure ports stay within their zone
            if (x < zoneMin.x) x = zoneMin.x;
            if (x > zoneMax.x) x = zoneMax.x;
            if (y < zoneMin.y) y = zoneMin.y;
            if (y > zoneMax.y) y = zoneMax.y;
            
            locations.push_back(Location(graph.ports[i].name, Vector2f(x, y), font));
        }
        
        // Create edges (routes) between ports
        for (int i = 0; i < graph.ports.size(); i++) {
            string sourceName = graph.ports[i].name;
            Vector2f sourcePos = locations[i].position;
            
            // For each route from this port
            for (const Route& route : graph.routes[i]) {
                // Find the destination port's position
                for (int j = 0; j < graph.ports.size(); j++) {
                    if (graph.ports[j].name == route.destination) {
                        Vector2f destPos = locations[j].position;
                        edges.push_back(RouteEdge(sourcePos, destPos, route, sourceName));
                        break;
                    }
                }
            }
        }
    } else {
        cout << "Error: Could not load map.png" << endl;
    }
}
vector<string> getPortInfo(const Location& location, Graph& graph) {
    vector<string> info;
    int portIdx = -1;
    for (int i = 0; i < graph.ports.size(); i++) {
        if (graph.ports[i].name == location.name) {
            portIdx = i;
            break;
        }
    }
    
    if (portIdx == -1) {
        info.push_back("Port not found in database!");
        return info;
    }
    info.push_back("Port Name: " + graph.ports[portIdx].name);
    info.push_back("");
    info.push_back("Port Charge: $" + to_string(graph.ports[portIdx].cost));
    info.push_back("");
        info.push_back("========== PORT STATISTICS ==========");
    info.push_back("");
    
    // Count incoming and outgoing routes
    int outgoingRoutes = graph.routes[portIdx].size();
    int incomingRoutes = 0;
    
    for (int i = 0; i < graph.routes.size(); i++) {
        for (const Route& route : graph.routes[i]) {
            if (route.destination == graph.ports[portIdx].name) {
                incomingRoutes++;
            }
        }
    }
    
    info.push_back("Outgoing Routes: " + to_string(outgoingRoutes));
    info.push_back("Incoming Routes: " + to_string(incomingRoutes));
    info.push_back("Total Connections: " + to_string(outgoingRoutes + incomingRoutes));
    info.push_back("");
    info.push_back("========== LOCATION ==========");
    info.push_back("");
    info.push_back("Coordinates: (" + to_string((int)location.position.x) + ", " + 
                   to_string((int)location.position.y) + ")");
    info.push_back("");
    info.push_back("========== AVAILABLE ROUTES ==========");
    if (graph.routes[portIdx].empty()) {
        info.push_back("No routes available from this port.");
    } else {
        for (int i = 0; i < graph.routes[portIdx].size(); i++) {
            const Route& r = graph.routes[portIdx][i];
            info.push_back("");
            info.push_back("Route " + to_string(i+1) + ":");
            info.push_back("  Destination: " + r.destination);
            info.push_back("  Date: " + r.date);
            info.push_back("  Departure: " + r.depTime + " -> Arrival: " + r.arrTime);
            info.push_back("  Cost: $" + to_string(r.cost));
            info.push_back("  Company: " + r.company);
        }
    }
    
    return info;
}
void showSettings() {
    // Settings can be displayed in an info window or separate UI
    // For now, this is a placeholder for future SFML-based settings
}
class FilterPreferencesMenu {
private:
    vector<Button*> buttons;
    Text titleText;
    bool isVisible;
    Font* font;
    float windowWidth;
    float windowHeight;
    Texture* oceanTexture;    
    Sprite oceanBackground; 
public:
    FilterPreferencesMenu(Font& f, float winWidth, float winHeight, Texture& oceanTex) 
        : font(&f), isVisible(false), windowWidth(winWidth), windowHeight(winHeight), 
          oceanTexture(&oceanTex) {
        
        titleText.setFont(f);
        titleText.setCharacterSize(48);
        titleText.setFillColor(Color::White);
        titleText.setOutlineThickness(2);
        titleText.setOutlineColor(Color::Black);
        
        // Setup ocean background
        oceanBackground.setTexture(*oceanTexture);
        Vector2u textureSize = oceanTexture->getSize();
        float scaleX = (float)winWidth / textureSize.x;
        float scaleY = (float)winHeight / textureSize.y;
        oceanBackground.setScale(scaleX, scaleY);
    }
    
    ~FilterPreferencesMenu() {
        for (auto btn : buttons) {
            delete btn;
        }
    }
    
    void createMainMenu() {
        clearButtons();
        titleText.setString("Filter Preferences");
        
        float buttonWidth = 400;
        float buttonHeight = 70;
        float centerX = (windowWidth - buttonWidth) / 2;
        float startY = windowHeight * 0.2f;
        float spacing = buttonHeight + 15;
        
        buttons.push_back(new Button(Vector2f(centerX, startY), Vector2f(buttonWidth, buttonHeight), "Shipping Companies", *font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing), Vector2f(buttonWidth, buttonHeight), "Avoid Specific Ports", *font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 2), Vector2f(buttonWidth, buttonHeight), "Voyage Time", *font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 3), Vector2f(buttonWidth, buttonHeight), "Custom Preferences", *font));
        buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 4.5f), Vector2f(buttonWidth, buttonHeight), "Back", *font));
    }
    void createTimeMenuForCustom() {
    clearButtons();
    titleText.setString("Set Maximum Voyage Time (in hours)");
    
    float buttonWidth = 300;
    float buttonHeight = 60;
    float centerX = (windowWidth - buttonWidth) / 2;
    float startY = windowHeight * 0.2f;
    float spacing = buttonHeight + 15;
    
    buttons.push_back(new Button(Vector2f(centerX, startY), Vector2f(buttonWidth, buttonHeight), "8 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing), Vector2f(buttonWidth, buttonHeight), "16 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 2), Vector2f(buttonWidth, buttonHeight), "24 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 3), Vector2f(buttonWidth, buttonHeight), "48 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 4), Vector2f(buttonWidth, buttonHeight), "No Limit", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 5.5f), Vector2f(buttonWidth, buttonHeight), "Done", *font));
}
void createCompanyMenu(const vector<string>& companies) {
    clearButtons();
    titleText.setString("Select Shipping Companies");
    
    float buttonWidth = 250;
    float buttonHeight = 60;
    float startX = 100;
    float startY = windowHeight * 0.15f;
    float spacingX = buttonWidth + 30;
    float spacingY = buttonHeight + 20;
    
    int cols = 4;  // Number of columns
    int col = 0;
    int row = 0;
    
    for (const string& company : companies) {
        float x = startX + (col * spacingX);
        float y = startY + (row * spacingY);
        
        buttons.push_back(new Button(Vector2f(x, y), Vector2f(buttonWidth, buttonHeight), company, *font));
        
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }
    }
    
    // Add Done button at the bottom
    float doneX = (windowWidth - 250) / 2;
    float doneY = startY + ((row + 1) * spacingY) + 30;
    buttons.push_back(new Button(Vector2f(doneX, doneY), Vector2f(250, 60), "Done", *font));
}
    
 void createPortMenu(const vector<Port>& ports) {
    clearButtons();
    titleText.setString("Select Ports to Avoid");
    
    float buttonWidth = 250;
    float buttonHeight = 60;
    float startX = 100;
    float startY = windowHeight * 0.15f;
    float spacingX = buttonWidth + 30;
    float spacingY = buttonHeight + 10;  // <-- Reduced spacing to fit more
    
    int cols = 4;
    int col = 0;
    int row = 0;
    
    for (const Port& port : ports) {
        float x = startX + (col * spacingX);
        float y = startY + (row * spacingY);
        
        buttons.push_back(new Button(Vector2f(x, y), Vector2f(buttonWidth, buttonHeight), port.name, *font));
        
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }
    }
    
    // Add Done button at the bottom with less space
    float doneX = (windowWidth - 250) / 2;
    float doneY = startY + ((row + 1) * spacingY);  // <-- Moved up slightly
    buttons.push_back(new Button(Vector2f(doneX, doneY), Vector2f(250, 60), "Done", *font));
}
    void createTimeMenu() {
    clearButtons();
    titleText.setString("Set Maximum Voyage Time (in hours)");
    
    float buttonWidth = 300;
    float buttonHeight = 60;
    float centerX = (windowWidth - buttonWidth) / 2;
    float startY = windowHeight * 0.3f;
    float spacing = buttonHeight + 20;
    
    // Display preset time options
    buttons.push_back(new Button(Vector2f(centerX, startY), Vector2f(buttonWidth, buttonHeight), "8 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing), Vector2f(buttonWidth, buttonHeight), "16 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 2), Vector2f(buttonWidth, buttonHeight), "24 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 3), Vector2f(buttonWidth, buttonHeight), "48 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 4), Vector2f(buttonWidth, buttonHeight), "No Limit", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 5.5f), Vector2f(buttonWidth, buttonHeight), "Back", *font));
}
    void clearButtons() {
        for (auto btn : buttons) {
            delete btn;
        }
        buttons.clear();
    }
    
    void show() {
        isVisible = true;
        createMainMenu();
    }
    
    void hide() {
        isVisible = false;
        clearButtons();
    }
    
    bool visible() const {
        return isVisible;
    }
    
    void update(RenderWindow& window) {
        if (isVisible) {
            for (auto btn : buttons) {
                btn->update(window);
            }
        }
    }
    
    void draw(RenderWindow& window) {
        if (isVisible) {
            // Draw ocean background
            window.draw(oceanBackground);
            
            // Draw semi-transparent overlay for better readability
            RectangleShape overlay(Vector2f(windowWidth, windowHeight));
            overlay.setFillColor(Color(0, 0, 0, 100));  // Semi-transparent black
            window.draw(overlay);
            
            FloatRect bounds = titleText.getLocalBounds();
            titleText.setOrigin(bounds.left + bounds.width / 2, bounds.top);
            titleText.setPosition(windowWidth / 2, 40);
            window.draw(titleText);
            
            for (auto btn : buttons) {
                btn->draw(window);
            }
        }
    }
    
    int checkClick(RenderWindow& window, Event& event) {
        if (!isVisible) return -1;
        
        for (int i = 0; i < buttons.size(); i++) {
            if (buttons[i]->isClicked(window, event)) {
                return i;
            }
        }
        return -1;
    }
    
    int getButtonCount() const {
        return buttons.size();
    }
};
void customShipPreferences(const Graph::UserPreferences& prefs, 
                          vector<Location>& locations, 
                          vector<RouteEdge>& edges, 
                          const Graph& g) {
    
    cout << "\n=== Applying Custom Ship Preferences ===" << endl;
    
    // Color ports based on preferences
    for (int i = 0; i < locations.size(); i++) {
        bool isAvoidPort = g.portIsAvoid(locations[i].name, prefs);
        
        if (isAvoidPort) {
            // Grey - avoid/filtered out port
            locations[i].pin.setFillColor(Color(150, 150, 150));      // Grey
            locations[i].pin.setOutlineColor(Color(100, 100, 100));
            locations[i].label.setFillColor(Color(100, 100, 100));
            cout << "Port " << locations[i].name << " marked as AVOID (Grey)" << endl;
        } else {
            // Bright Green - preferred/allowed port
            locations[i].pin.setFillColor(Color(0, 255, 0));          // Bright Green
            locations[i].pin.setOutlineColor(Color(0, 180, 0));
            locations[i].label.setFillColor(Color::White);
            cout << "Port " << locations[i].name << " marked as ALLOWED (Green)" << endl;
        }
    }
    
    // Color routes based on preferences
    int allowedRoutes = 0;
    int blockedRoutes = 0;
    
    for (auto& edge : edges) {
        // Check if source or destination port is avoided
        bool sourceAvoid = g.portIsAvoid(edge.sourceName, prefs);
        bool destAvoid = g.portIsAvoid(edge.routeInfo.destination, prefs);
        
        if (sourceAvoid || destAvoid) {
            // Grey with low opacity - blocked/filtered route
            edge.line.setFillColor(Color(150, 150, 150, 100));
            blockedRoutes++;
            cout << "Route blocked: " << edge.sourceName << " -> " << edge.routeInfo.destination 
                 << " (Avoid port)" << endl;
        }
        else if (g.routeMatchesPreferences(edge.routeInfo, prefs)) {
            // Bright Green - matches preferences
            edge.line.setFillColor(Color(0, 255, 0, 255));
            allowedRoutes++;
            cout << "Route allowed: " << edge.sourceName << " -> " << edge.routeInfo.destination 
                 << " (Matches preferences)" << endl;
        } else {
            // Grey - doesn't match preferences
            edge.line.setFillColor(Color(150, 150, 150, 100));
            blockedRoutes++;
            cout << "Route not preferred: " << edge.sourceName << " -> " << edge.routeInfo.destination 
                 << " (Different company/time)" << endl;
        }
    }
    
    cout << "\nRoute Summary:" << endl;
    cout << "Allowed routes: " << allowedRoutes << endl;
    cout << "Blocked/Not preferred routes: " << blockedRoutes << endl;
    
    // Print applied preferences
    cout << "\nApplied Preferences:" << endl;
    
    if (prefs.hasCompanyFilter) {
        cout << "Preferred Companies: ";
        for (const string& company : prefs.preferredCompanies) {
            cout << company << " ";
        }
        cout << endl;
    }
    
    if (prefs.hasPortFilter) {
        cout << "Avoid Ports: ";
        for (const string& port : prefs.avoidPorts) {
            cout << port << " ";
        }
        cout << endl;
    }
    
    if (prefs.hasTimeFilter) {
        cout << "Max Voyage Time: " << prefs.maxVoyageTime << " minutes (" 
             << (prefs.maxVoyageTime / 60) << "h " << (prefs.maxVoyageTime % 60) << "m)" << endl;
    }
    
    cout << "======================================\n" << endl;
}
enum GameState {
    MAIN_MENU,
    NAVIGATION_MENU,
    MAP_VIEW,
    SEARCH_ROUTES,
    BOOK_CARGO,
    SHORTEST_ROUTE_SELECT,
    CHEAPEST_ROUTE_SELECT,
    FILTER_PREFERENCES,
    FILTER_MAIN_MENU,       
    FILTER_COMPANIES,        
    FILTER_PORTS,            
    FILTER_TIME,          
    FILTER_CUSTOM,           
    FILTER_RESULT_MAP,       
    PROCESS_LAYOVERS,
    TRACK_MULTI_LEG,
    FILTER_CUSTOM_COMPANIES,   
    FILTER_CUSTOM_PORTS,       
    FILTER_CUSTOM_TIME,        
    SUBGRAPH
};


int main() { 
    Graph g;
    g.parsePorts("PortCharges.txt");
    g.parseRoute("Routes.txt");
    vector<string> availableCompanies = g.getAllShippingCompanies();
cout << "Available companies: ";
for (const string& company : availableCompanies) {
    cout << company << " ";
}
cout << endl;
    Font font;
    if (!font.loadFromFile("arial.ttf")) {
        cout << "Error: Could not load arial.ttf, trying system font..." << endl;
        if (!font.loadFromFile("C:/Windows/Fonts/arial.ttf")) {
            cout << "Error: Could not load any font!" << endl;
            return -1;
        }
    }
    
    // Create fullscreen window
    RenderWindow window(VideoMode::getDesktopMode(), "Port Route Map", Style::Fullscreen);
    Vector2u windowSize = window.getSize();
    float windowWidth = windowSize.x;
    float windowHeight = windowSize.y;
    
    // Load ocean background texture
    Texture oceanTexture; 
    Sprite oceanSprite;
    RectangleShape oceanBackground(Vector2f(windowWidth, windowHeight));
    oceanBackground.setFillColor(Color(20, 60, 100));
    
    bool oceanLoaded = false;
    if (oceanTexture.loadFromFile("ocean.png")) { 
        cout << "Ocean image loaded successfully!" << endl;
        oceanSprite.setTexture(oceanTexture);
        Vector2u oceanTextureSize = oceanTexture.getSize();
        float scaleX = (float)windowWidth / oceanTextureSize.x;
        float scaleY = (float)windowHeight / oceanTextureSize.y;
        oceanSprite.setScale(scaleX, scaleY);
        oceanLoaded = true;
    } else { 
        cout << "Warning: Could not load ocean.png, using solid color background" << endl; 
    }
    
    // Map background
    Texture mapTexture;
    Sprite mapSprite;
    
    vector<Location> locations;
    vector<RouteEdge> edges;
    RouteTooltip routeTooltip(font);
    InfoWindow infoWindow(font, windowWidth, windowHeight);

    // Initialize the route display window
    RouteDisplayWindow* routeDisplayWindow = new RouteDisplayWindow(font, windowWidth, windowHeight);
    
    int selectedEdgeIndex = -1;
    
    // Shortest route variables
    vector<int> selectedPorts;
    Graph::ShortestRouteResult shortestRouteResult;
    vector<RouteEdge> shortestPathEdges;
    bool shortestRouteCalculated = false;
    
    // Cheapest route variables
    vector<RouteEdge> cheapestPathEdges;
    bool cheapestRouteCalculated = false;
    
    // Filter preferences variables
Graph::UserPreferences userPreferences = {
    {},      // preferredCompanies
    {},      // avoidPorts
    999999,  // maxVoyageTime
    false,   // hasCompanyFilter
    false,   // hasPortFilter
    false    // hasTimeFilter
};

int filterMenuState = 0;  // 0=main, 1=companies, 2=ports, 3=time, 4=custom
vector<Button*> filterButtons;
Text filterMenuTitle;

    // Search routes variables
    vector<RouteEdge> highlightedRoutes;
    bool routesDisplayed = false;

    Text instructionText;
    instructionText.setFont(font);
    instructionText.setCharacterSize(24);
    instructionText.setFillColor(Color::White);
    instructionText.setOutlineThickness(2);
    instructionText.setOutlineColor(Color::Black);
    
    // Main menu welcome text
    Text welcomeText;
    welcomeText.setFont(font);
    welcomeText.setString("Welcome to OceanRoute Nav");
    welcomeText.setCharacterSize(60);
    welcomeText.setFillColor(Color::White);
    welcomeText.setOutlineThickness(3);
    welcomeText.setOutlineColor(Color::Black);
    FloatRect welcomeBounds = welcomeText.getLocalBounds();
    welcomeText.setOrigin(welcomeBounds.left + welcomeBounds.width / 2.0f,
                         welcomeBounds.top + welcomeBounds.height / 2.0f);
    welcomeText.setPosition(windowWidth / 2, windowHeight * 0.15f);
    
    // Buttons
    float buttonWidth = 400;
    float buttonHeight = 70;
    float centerX = (windowWidth - buttonWidth) / 2;
    
    Button startButton(Vector2f(centerX, windowHeight * 0.35f), Vector2f(buttonWidth, buttonHeight), "Start Route Navigation", font);
    Button settingsButton(Vector2f(centerX, windowHeight * 0.50f), Vector2f(buttonWidth, buttonHeight), "Settings", font);
    Button exitButton(Vector2f(centerX, windowHeight * 0.65f), Vector2f(buttonWidth, buttonHeight), "Exit", font);
    NavigationMenu navMenu(font, windowWidth, windowHeight);
    FilterPreferencesMenu filterMenu(font, windowWidth, windowHeight, oceanTexture);
    GameState currentState = MAIN_MENU;
    
    while (window.isOpen()) { 
        Event event; 
        while (window.pollEvent(event)) { 
            if (event.type == Event::Closed) 
                window.close();
            
            // Mouse wheel scrolling for InfoWindow
            if (event.type == Event::MouseWheelScrolled) {
                if (infoWindow.isMouseOverWindow(window)) {
                    infoWindow.scroll(event.mouseWheelScroll.delta);
                }
                // Add scroll support for route display window
                if (routeDisplayWindow->isMouseOverWindow(window)) {
                    routeDisplayWindow->scroll(event.mouseWheelScroll.delta);
                }
            }
            
            // Check InfoWindow close
            if (infoWindow.checkClose(window, event)) {
                if (selectedEdgeIndex >= 0 && selectedEdgeIndex < edges.size()) {
                    edges[selectedEdgeIndex].setSelected(false);
                    selectedEdgeIndex = -1;
                }
                continue; 
            }
            
            // Check RouteDisplayWindow close
            if (routeDisplayWindow->checkClose(window, event)) {
                highlightedRoutes.clear();
                routesDisplayed = false;
                continue;
            }
            
            // Main Menu handling
            if (currentState == MAIN_MENU) {
                if (event.type == Event::KeyPressed && event.key.code == Keyboard::Escape) {
                    window.close();
                }
                if (startButton.isClicked(window, event)) {
                    currentState = NAVIGATION_MENU;
                    navMenu.show();
                }
                if (settingsButton.isClicked(window, event)) {
                    showSettings();
                }
                if (exitButton.isClicked(window, event)) {
                    window.close();
                }
            }
            
            // Navigation Menu handling
            else if (currentState == NAVIGATION_MENU) {
                int clicked = navMenu.checkClick(window, event);
                if (clicked == 0) { // Search for Routes
                    loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
                    selectedPorts.clear();
                    highlightedRoutes.clear();
                    routesDisplayed = false;
                    currentState = SEARCH_ROUTES;
                    navMenu.hide();
                }
                else if (clicked == 1) { // Book Cargo
                    currentState = BOOK_CARGO;
                    navMenu.hide();
                }
                else if (clicked == 2) { // Display Ports Info
                    loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
                    selectedEdgeIndex = -1;
                    currentState = MAP_VIEW;
                    navMenu.hide();
                }
                else if (clicked == 3) { // Shortest Route
                    loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
                    selectedEdgeIndex = -1;
                    selectedPorts.clear();
                    shortestPathEdges.clear();
                    shortestRouteCalculated = false;
                    currentState = SHORTEST_ROUTE_SELECT;
                    navMenu.hide();
                }
                else if (clicked == 4) { // Cheapest Route
                    loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
                    selectedEdgeIndex = -1;
                    selectedPorts.clear();
                    cheapestPathEdges.clear();
                    cheapestRouteCalculated = false;
                    currentState = CHEAPEST_ROUTE_SELECT;
                    navMenu.hide();
                }
else if (clicked == 5) { 
    currentState = FILTER_PREFERENCES;
    filterMenu.show();
    navMenu.hide();
}
                else if (clicked == 6) { // Process Layovers
                    currentState = PROCESS_LAYOVERS;
                    navMenu.hide();
                }
                else if (clicked == 7) { // Track Multi-Leg
                    currentState = TRACK_MULTI_LEG;
                    navMenu.hide();
                }
                else if (clicked == 8) { // Back to Main Menu
                    currentState = MAIN_MENU;
                    navMenu.hide();
                }
            }
            
            // Map View handling
            else if (currentState == MAP_VIEW && !infoWindow.visible()) {
                bool locationClicked = false;
                for (auto& location : locations) {
                    if (location.isClicked(window, event)) {
                        if (selectedEdgeIndex >= 0 && selectedEdgeIndex < edges.size()) {
                            edges[selectedEdgeIndex].setSelected(false);
                            selectedEdgeIndex = -1;
                        }
                        
                        vector<string> portInfo = getPortInfo(location, g);
                        infoWindow.show(location.name, portInfo);
                        locationClicked = true;
                        break;
                    }
                }
                
                if (!locationClicked) {
                    for (int i = 0; i < edges.size(); i++) {
                        if (edges[i].isClicked(window, event)) {
                            if (selectedEdgeIndex >= 0 && selectedEdgeIndex < edges.size()) {
                                edges[selectedEdgeIndex].setSelected(false);
                            }
                            
                            selectedEdgeIndex = i;
                            edges[i].setSelected(true);
                            
                            vector<string> routeDetails = edges[i].getRouteDetails();
                            infoWindow.show("Route Information", routeDetails);
                            break;
                        }
                    }
                }
            }
            
            // SEARCH_ROUTES handling - multi-route highlight support
// SEARCH_ROUTES handling - Like Shortest/Cheapest Route

else if (currentState == SEARCH_ROUTES && !infoWindow.visible()) {
    bool locationClicked = false;
    for (int i = 0; i < locations.size(); i++) {
        if (locations[i].isClicked(window, event)) {
            bool alreadySelected = false;
            for (int idx : selectedPorts) {
                if (idx == i) {
                    alreadySelected = true;
                    break;
                }
            }
            
            if (!alreadySelected && selectedPorts.size() < 2) {
                selectedPorts.push_back(i);
                locations[i].pin.setOutlineColor(Color::Magenta);
                cout << "Selected port: " << locations[i].name << endl;
            } else if (alreadySelected && selectedPorts.size() <= 2) {
                selectedPorts.erase(remove(selectedPorts.begin(), selectedPorts.end(), i), selectedPorts.end());
                locations[i].pin.setOutlineColor(Color::White);
                cout << "Deselected port: " << locations[i].name << endl;
                
                // Clear routes when deselecting
                highlightedRoutes.clear();
                routesDisplayed = false;
            }
            
            if (selectedPorts.size() == 2) {
                cout << "Two ports selected, finding all routes..." << endl;
                int srcIdx = selectedPorts[0];
                int destIdx = selectedPorts[1];
                
                cout << "Searching routes from " << g.ports[srcIdx].name 
                     << " to " << g.ports[destIdx].name << endl;
                
                // Highlight all routes between these ports
                highlightedRoutes.clear();
                
                // Find all possible routes
                vector<CompleteRoute> allRoutes = findAllPossibleRoutes(g, srcIdx, destIdx);
                
                cout << "Found " << allRoutes.size() << " total routes" << endl;
                vector<string> routeInfo = formatAllRoutes(g, srcIdx, destIdx);
                if (allRoutes.empty()) {
                    infoWindow.show("Search Results", routeInfo);
                } else {
                    // Highlight each route found with distinct colors
                    for (const CompleteRoute& cr : allRoutes) {
                        Color legColor;
                        if (cr.layoverCount == 0) {
                            legColor = Color::Magenta;
                        } else if (cr.layoverCount == 1) {
                            legColor = Color(255, 140, 0, 255);
                        } else {
                            legColor = Color(65, 105, 225, 255);
                        }

                        for (size_t legIdx = 0; legIdx < cr.routeLegs.size(); legIdx++) {
                            int fromIdx = cr.portPath[legIdx];
                            int toIdx = cr.portPath[legIdx + 1];
                            RouteEdge edge(locations[fromIdx].position, locations[toIdx].position,
                                           cr.routeLegs[legIdx], g.ports[fromIdx].name);
                            edge.line.setFillColor(legColor);
                            edge.line.setSize(Vector2f(edge.line.getSize().x, 6));
                            highlightedRoutes.push_back(edge);
                        }
                    }
                    
                    cout << "Highlighted " << highlightedRoutes.size() << " route segments" << endl;
                    routesDisplayed = true;
                    
                    // Create summary info for info window
                    vector<string> info;
                    info.push_back("Routes Found!");
                    info.push_back("");
                    info.push_back("From: " + g.ports[srcIdx].name);
                    info.push_back("To: " + g.ports[destIdx].name);
                    info.push_back("");
                    
                    // Count route types
                    int directCount = 0;
                    int singleLayoverCount = 0;
                    int multiLayoverCount = 0;
                    
                    for (const auto& route : allRoutes) {
                        if (route.layoverCount == 0) directCount++;
                        else if (route.layoverCount == 1) singleLayoverCount++;
                        else multiLayoverCount++;
                    }
                    
                    info.push_back("=== SUMMARY ===");
                    info.push_back("Total Routes: " + to_string(allRoutes.size()));
                    info.push_back("");
                    info.push_back("Direct Routes: " + to_string(directCount));
                    info.push_back("1-Layover Routes: " + to_string(singleLayoverCount));
                    info.push_back("Multi-Layover Routes: " + to_string(multiLayoverCount));
                    info.push_back("");
                    
                    // Find cheapest and fastest
                    int cheapestCost = INF;
                    int fastestTime = INF;
                    info.push_back("=== HIGHLIGHTED ROUTE DETAILS ===");
                    info.push_back("Magenta = Direct Routes");
                    info.push_back("Orange = 1-Layover Routes");
                    info.push_back("Blue = Multi-Layover Routes");
                    info.push_back("");
                    
                    infoWindow.show("Search Results", routeInfo);
                }
            }
            
            locationClicked = true;
            break;
        }
    }
    
    // Allow clicking on highlighted routes for detailed info
    if (!locationClicked && routesDisplayed) {
        for (size_t i = 0; i < highlightedRoutes.size(); i++) {
            if (highlightedRoutes[i].isClicked(window, event)) {
                vector<string> routeDetails = highlightedRoutes[i].getRouteDetails();
                infoWindow.show("Route Information", routeDetails);
                break;
            }
        }
    }
}
            // Shortest Route handling
            else if (currentState == SHORTEST_ROUTE_SELECT && !infoWindow.visible()) {
                bool locationClicked = false;
                for (int i = 0; i < locations.size(); i++) {
                    if (locations[i].isClicked(window, event)) {
                        bool alreadySelected = false;
                        for (int idx : selectedPorts) {
                            if (idx == i) {
                                alreadySelected = true;
                                break;
                            }
                        }
                        
                        if (!alreadySelected && selectedPorts.size() < 2) {
                            selectedPorts.push_back(i);
                            locations[i].pin.setOutlineColor(Color::Yellow);
                            cout << "Selected port: " << locations[i].name << endl;
                        } else if (alreadySelected && selectedPorts.size() <= 2) {
                            selectedPorts.erase(remove(selectedPorts.begin(), selectedPorts.end(), i), selectedPorts.end());
                            locations[i].pin.setOutlineColor(Color::White);
                            cout << "Deselected port: " << locations[i].name << endl;
                        }
                        
                        if (selectedPorts.size() == 2) {
                            int srcIdx = selectedPorts[0];
                            int destIdx = selectedPorts[1];
                            
                            shortestRouteResult = g.findShortestRouteData(&g.ports[srcIdx]);
                            
if (shortestRouteResult.found && shortestRouteResult.dist[destIdx] != INF) {
    vector<int> path;
    int curr = destIdx;
    while (curr != -1) {
        path.push_back(curr);
        curr = shortestRouteResult.parent[curr];
    }
    reverse(path.begin(), path.end());

    shortestPathEdges.clear();

    vector<Route> legs; // collect route legs

    for (int i = 0; i < path.size() - 1; i++) {
        int from = path[i];
        int to = path[i + 1];

        for (const Route& route : g.routes[from]) {
            if (route.destination == g.ports[to].name) {
                // Store route leg info
                legs.push_back(route);

                RouteEdge edge(locations[from].position, locations[to].position,
                               route, g.ports[from].name);
                edge.line.setFillColor(Color::Green);
                shortestPathEdges.push_back(edge);
                break;
            }
        }
    }

    shortestRouteCalculated = true;

    vector<string> info;
    info.push_back("Shortest Route Found!");
    info.push_back("");
    info.push_back("From: " + g.ports[srcIdx].name);
    info.push_back("To: " + g.ports[destIdx].name);
    info.push_back("");

    int totalTime = shortestRouteResult.dist[destIdx];
    info.push_back("Layovers: " + to_string((int)path.size() - 2));
    info.push_back("Total Travel Time: "
                   + to_string(totalTime / 60) + "h "
                   + to_string(totalTime % 60) + "m");
    info.push_back("");

    // Path string
    info.push_back("Path:");
    string pathStr;
    for (int i = 0; i < path.size(); i++) {
        pathStr += g.ports[path[i]].name;
        if (i < path.size() - 1) pathStr += " -> ";
    }
    info.push_back(pathStr);
    info.push_back("");

    // Detailed leg info
    info.push_back("Route Details:");
    for (size_t i = 0; i < legs.size(); i++) {
        const Route& leg = legs[i];
        string fromName = g.ports[path[i]].name;
        string toName = g.ports[path[i + 1]].name;

        info.push_back("  Leg " + to_string(i + 1) + ": " + fromName + " -> " + toName);
        info.push_back("    Date: " + leg.date);
        info.push_back("    Time: " + leg.depTime + " -> " + leg.arrTime);
        info.push_back("    Travel Duration: "
                        + to_string(leg.travelTime / 60) + "h "
                        + to_string(leg.travelTime % 60) + "m");
        info.push_back("    Company: " + leg.company);
        info.push_back("");
    }

    infoWindow.show("Shortest Route Result", info);
}
 else {
                                selectedPorts.clear();
                                for (auto& loc : locations) {
                                    loc.pin.setOutlineColor(Color::White);
                                }
                            }
                        }
                        
                        locationClicked = true;
                        break;
                    }
                }
                
                if (!locationClicked && shortestRouteCalculated) {
                    for (int i = 0; i < shortestPathEdges.size(); i++) {
                        if (shortestPathEdges[i].isClicked(window, event)) {
                            vector<string> routeDetails = shortestPathEdges[i].getRouteDetails();
                            infoWindow.show("Route Information", routeDetails);
                            break;
                        }
                    }
                }
            }
            else if (currentState == FILTER_RESULT_MAP && !infoWindow.visible()) {
    bool locationClicked = false;
    
    // Check if clicked on a port/location first (higher priority)
    for (auto& location : locations) {
        if (location.isClicked(window, event)) {
            // Only show info if port is NOT greyed out (is allowed)
            if (!g.portIsAvoid(location.name, userPreferences)) {
                vector<string> portInfo = getPortInfo(location, g);
                infoWindow.show(location.name, portInfo);
                locationClicked = true;
                break;
            }
        }
    }
    
    // If no location clicked, then check for route edge clicks
    if (!locationClicked) {
        for (int i = 0; i < edges.size(); i++) {
            if (edges[i].isClicked(window, event)) {
                // Only show info if route is NOT greyed out (is allowed)
                bool sourceAvoid = g.portIsAvoid(edges[i].sourceName, userPreferences);
                bool destAvoid = g.portIsAvoid(edges[i].routeInfo.destination, userPreferences);
                
                if (!sourceAvoid && !destAvoid && g.routeMatchesPreferences(edges[i].routeInfo, userPreferences)) {
                    vector<string> routeDetails = edges[i].getRouteDetails();
                    infoWindow.show("Route Information", routeDetails);
                    break;
                }
            }
        }
    }
}
            // Cheapest Route handling
            else if (currentState == CHEAPEST_ROUTE_SELECT && !infoWindow.visible()) {
                bool locationClicked = false;
                for (int i = 0; i < locations.size(); i++) {
                    if (locations[i].isClicked(window, event)) {
                        bool alreadySelected = false;
                        for (int idx : selectedPorts) {
                            if (idx == i) {
                                alreadySelected = true;
                                break;
                            }
                        }
                        
                        if (!alreadySelected && selectedPorts.size() < 2) {
                            selectedPorts.push_back(i);
                            locations[i].pin.setOutlineColor(Color::Cyan);
                            cout << "Selected port: " << locations[i].name << endl;
                        } else if (alreadySelected && selectedPorts.size() <= 2) {
                            selectedPorts.erase(remove(selectedPorts.begin(), selectedPorts.end(), i), selectedPorts.end());
                            locations[i].pin.setOutlineColor(Color::White);
                            cout << "Deselected port: " << locations[i].name << endl;
                        }
                        
                        if (selectedPorts.size() == 2) {
                            int srcIdx = selectedPorts[0];
                            int destIdx = selectedPorts[1];
                            
                            CompleteRoute cheapestRoute;
                            bool cheapestFound = findCheapestEnumeratedRoute(g, srcIdx, destIdx, cheapestRoute);
                            
                            if (cheapestFound) {
                                cheapestPathEdges.clear();
                                
                                for (size_t i = 0; i < cheapestRoute.routeLegs.size(); i++) {
                                    int from = cheapestRoute.portPath[i];
                                    int to = cheapestRoute.portPath[i + 1];
                                    
                                    RouteEdge edge(locations[from].position, locations[to].position, 
                                                   cheapestRoute.routeLegs[i], g.ports[from].name);
                                    edge.line.setFillColor(Color::Cyan);
                                    cheapestPathEdges.push_back(edge);
                                }
                                
                                cheapestRouteCalculated = true;
                                
                                vector<string> info;
                                info.push_back("Cheapest Route Found!");
                                info.push_back("");
                                info.push_back("From: " + g.ports[srcIdx].name);
                                info.push_back("To: " + g.ports[destIdx].name);
                                info.push_back("");
                                info.push_back("Layovers: " + to_string(cheapestRoute.layoverCount));
                                info.push_back("Total Cost: $" + to_string(cheapestRoute.totalCost));
                                info.push_back("Total Travel Time: " + to_string(cheapestRoute.totalTime / 60) + "h " +
                                               to_string(cheapestRoute.totalTime % 60) + "m");
                                info.push_back("");
                                info.push_back("Path: ");
                                string pathStr = "";
                                for (size_t i = 0; i < cheapestRoute.portPath.size(); i++) {
                                    pathStr += g.ports[cheapestRoute.portPath[i]].name;
                                    if (i < cheapestRoute.portPath.size() - 1) pathStr += " -> ";
                                }
                                info.push_back(pathStr);
                                info.push_back("");
                                info.push_back("Route Details:");
                                for (size_t i = 0; i < cheapestRoute.routeLegs.size(); i++) {
                                    const Route& leg = cheapestRoute.routeLegs[i];
                                    string fromName = g.ports[cheapestRoute.portPath[i]].name;
                                    string toName = g.ports[cheapestRoute.portPath[i + 1]].name;
                                    info.push_back("  Leg " + to_string(i + 1) + ": " + fromName + " -> " + toName);
                                    info.push_back("    Date: " + leg.date);
                                    info.push_back("    Time: " + leg.depTime + " -> " + leg.arrTime);
                                    info.push_back("    Cost: $" + to_string(leg.cost));
                                    info.push_back("    Company: " + leg.company);
                                    info.push_back("");
                                }
                                info.push_back("Port Charges (excluding origin):");
                                for (size_t i = 1; i < cheapestRoute.portPath.size(); i++) {
                                    int portIdx = cheapestRoute.portPath[i];
                                    info.push_back("  " + g.ports[portIdx].name + ": $" + to_string(g.ports[portIdx].cost));
                                }
                                
                                infoWindow.show("Cheapest Route Result", info);
                            } else {
                                cout << "No route found between selected ports!" << endl;
                                selectedPorts.clear();
                                for (auto& loc : locations) {
                                    loc.pin.setOutlineColor(Color::White);
                                }
                            }
                        }
                        
                        locationClicked = true;
                        break;
                    }
                }
                
                if (!locationClicked && cheapestRouteCalculated) {
                    for (int i = 0; i < cheapestPathEdges.size(); i++) {
                        if (cheapestPathEdges[i].isClicked(window, event)) {
                            vector<string> routeDetails = cheapestPathEdges[i].getRouteDetails();
                            infoWindow.show("Route Information", routeDetails);
                            break;
                        }
                    }
                }
            }
            else if (currentState == FILTER_PREFERENCES) {
                int clicked = filterMenu.checkClick(window, event);
                if (clicked == 0) {
                    currentState = FILTER_COMPANIES;
                    filterMenu.createCompanyMenu(availableCompanies);
                    cout << "Showing shipping companies filter menu" << endl;
                }
                else if (clicked == 1) {
                    currentState = FILTER_PORTS;
                    filterMenu.createPortMenu(g.ports);
                    cout << "Showing avoid ports filter menu" << endl;
                }
else if (clicked == 2) {
    currentState = FILTER_TIME;
    filterMenu.createTimeMenu();  // <-- Add this line
    cout << "Showing voyage time filter menu" << endl;
}
    else if (clicked == 3) {
        currentState = FILTER_CUSTOM_COMPANIES;  // <-- Changed from FILTER_CUSTOM
        filterMenu.createCompanyMenu(availableCompanies);
        cout << "Showing custom preferences menu (step 1: companies)" << endl;
    }
                else if (clicked == 4) {
                    currentState = NAVIGATION_MENU;
                    navMenu.show();
                    filterMenu.hide();
                    cout << "Back to navigation menu" << endl;
                }
            }
else if (currentState == FILTER_COMPANIES) {
    int clicked = filterMenu.checkClick(window, event);
    if (clicked != -1) {
        if (clicked == filterMenu.getButtonCount() - 1) {
            // Done button
            userPreferences.hasCompanyFilter = true;
            
            // Load map and apply filters
            loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
            customShipPreferences(userPreferences, locations, edges, g);
            
            currentState = FILTER_RESULT_MAP;
            cout << "Company filter applied, showing filtered map" << endl;
        } else if (clicked < availableCompanies.size()) {
            // Company selected
            string selectedCompany = availableCompanies[clicked];
            auto it = find(userPreferences.preferredCompanies.begin(), 
                          userPreferences.preferredCompanies.end(), selectedCompany);
            if (it == userPreferences.preferredCompanies.end()) {
                userPreferences.preferredCompanies.push_back(selectedCompany);
                cout << "Selected company: " << selectedCompany << endl;
            } else {
                userPreferences.preferredCompanies.erase(it);
                cout << "Deselected company: " << selectedCompany << endl;
            }
        }
    }
}
else if (currentState == FILTER_PORTS) {
    int clicked = filterMenu.checkClick(window, event);
    if (clicked != -1) {
        if (clicked == filterMenu.getButtonCount() - 1) {
            // Done button
            userPreferences.hasPortFilter = true;
            
            // Load map and apply filters
            loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
            customShipPreferences(userPreferences, locations, edges, g);
            
            currentState = FILTER_RESULT_MAP;
            cout << "Port filter applied, showing filtered map" << endl;
        } else if (clicked < g.ports.size()) {
            // Port selected
            string selectedPort = g.ports[clicked].name;
            auto it = find(userPreferences.avoidPorts.begin(), 
                          userPreferences.avoidPorts.end(), selectedPort);
            if (it == userPreferences.avoidPorts.end()) {
                userPreferences.avoidPorts.push_back(selectedPort);
                cout << "Avoid port: " << selectedPort << endl;
            } else {
                userPreferences.avoidPorts.erase(it);
                cout << "Include port: " << selectedPort << endl;
            }
        }
    }
}
else if (currentState == FILTER_TIME) {
    int clicked = filterMenu.checkClick(window, event);
    if (clicked != -1) {
        bool timeSelected = false;
        
        if (clicked == 0) {
            userPreferences.maxVoyageTime = 8 * 60;  // 8 hours in minutes
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 8 hours, showing filtered map" << endl;
        }
        else if (clicked == 1) {
            userPreferences.maxVoyageTime = 16 * 60;  // 16 hours
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 16 hours, showing filtered map" << endl;
        }
        else if (clicked == 2) {
            userPreferences.maxVoyageTime = 24 * 60;  // 24 hours
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 24 hours, showing filtered map" << endl;
        }
        else if (clicked == 3) {
            userPreferences.maxVoyageTime = 48 * 60;  // 48 hours
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 48 hours, showing filtered map" << endl;
        }
        else if (clicked == 4) {
            userPreferences.hasTimeFilter = false;
            userPreferences.maxVoyageTime = 999999;
            timeSelected = true;
            cout << "No voyage time limit" << endl;
        }
        else if (clicked == 5) {
            // Back button
            currentState = FILTER_PREFERENCES;
            filterMenu.createMainMenu();
        }
        
        // If time was selected, show the map
        if (timeSelected) {
            loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
            customShipPreferences(userPreferences, locations, edges, g);
            currentState = FILTER_RESULT_MAP;
        }
    }
    
    if (event.type == Event::KeyPressed && event.key.code == Keyboard::Escape) {
        currentState = FILTER_PREFERENCES;
        filterMenu.createMainMenu();
    }
}
else if (currentState == FILTER_CUSTOM_COMPANIES) {
    int clicked = filterMenu.checkClick(window, event);
    if (clicked != -1) {
        if (clicked == filterMenu.getButtonCount() - 1) {
            // Done button - move to ports
            userPreferences.hasCompanyFilter = true;
            filterMenu.createPortMenu(g.ports);
            currentState = FILTER_CUSTOM_PORTS;
            cout << "Company preferences set, now showing ports" << endl;
        } else if (clicked < availableCompanies.size()) {
            // Company selected
            string selectedCompany = availableCompanies[clicked];
            auto it = find(userPreferences.preferredCompanies.begin(), 
                          userPreferences.preferredCompanies.end(), selectedCompany);
            if (it == userPreferences.preferredCompanies.end()) {
                userPreferences.preferredCompanies.push_back(selectedCompany);
                cout << "Selected company: " << selectedCompany << endl;
            } else {
                userPreferences.preferredCompanies.erase(it);
                cout << "Deselected company: " << selectedCompany << endl;
            }
        }
    }
}

else if (currentState == FILTER_CUSTOM_PORTS) {
    int clicked = filterMenu.checkClick(window, event);
    if (clicked != -1) {
        if (clicked == filterMenu.getButtonCount() - 1) {
            // Done button - move to time
            userPreferences.hasPortFilter = true;
            filterMenu.createTimeMenuForCustom();
            currentState = FILTER_CUSTOM_TIME;
            cout << "Port preferences set, now showing voyage time" << endl;
        } else if (clicked < g.ports.size()) {
            // Port selected
            string selectedPort = g.ports[clicked].name;
            auto it = find(userPreferences.avoidPorts.begin(), 
                          userPreferences.avoidPorts.end(), selectedPort);
            if (it == userPreferences.avoidPorts.end()) {
                userPreferences.avoidPorts.push_back(selectedPort);
                cout << "Avoid port: " << selectedPort << endl;
            } else {
                userPreferences.avoidPorts.erase(it);
                cout << "Include port: " << selectedPort << endl;
            }
        }
    }
}

else if (currentState == FILTER_CUSTOM_TIME) {
    int clicked = filterMenu.checkClick(window, event);
    if (clicked != -1) {
        bool timeSelected = false;
        
        if (clicked == 0) {
            userPreferences.maxVoyageTime = 8 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 8 hours" << endl;
        }
        else if (clicked == 1) {
            userPreferences.maxVoyageTime = 16 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 16 hours" << endl;
        }
        else if (clicked == 2) {
            userPreferences.maxVoyageTime = 24 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 24 hours" << endl;
        }
        else if (clicked == 3) {
            userPreferences.maxVoyageTime = 48 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 48 hours" << endl;
        }
        else if (clicked == 4) {
            userPreferences.hasTimeFilter = false;
            userPreferences.maxVoyageTime = 999999;
            timeSelected = true;
            cout << "No voyage time limit" << endl;
        }
        else if (clicked == 5) {
            // Done button - show filtered map
            timeSelected = true;
            cout << "All custom preferences applied, showing filtered map" << endl;
        }
        
        if (timeSelected) {
            loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
            customShipPreferences(userPreferences, locations, edges, g);
            currentState = FILTER_RESULT_MAP;
        }
    }
}
            
            // ESC key handling for all states
// ESC key handling - find this section and ensure it clears properly
if ((currentState == MAP_VIEW || currentState == SEARCH_ROUTES || currentState == BOOK_CARGO || 
     currentState == FILTER_PREFERENCES || currentState == FILTER_COMPANIES || 
     currentState == FILTER_PORTS || currentState == FILTER_TIME || currentState == FILTER_CUSTOM || currentState == PROCESS_LAYOVERS ||
     currentState == TRACK_MULTI_LEG || currentState == SUBGRAPH ||
     currentState == SHORTEST_ROUTE_SELECT || currentState == CHEAPEST_ROUTE_SELECT || 
     currentState == FILTER_CUSTOM_COMPANIES || currentState == FILTER_CUSTOM_PORTS || 
     currentState == FILTER_CUSTOM_TIME) && 
    event.type == Event::KeyPressed && event.key.code == Keyboard::Escape) {
    
    selectedPorts.clear();
    shortestPathEdges.clear();
    cheapestPathEdges.clear();
    highlightedRoutes.clear(); // This should clear the routes
    shortestRouteCalculated = false;
    cheapestRouteCalculated = false;
    routesDisplayed = false;
    userPreferences.hasCompanyFilter = false;
    userPreferences.hasPortFilter = false;
    userPreferences.hasTimeFilter = false;
    userPreferences.preferredCompanies.clear();
    userPreferences.avoidPorts.clear();
    // Close any open windows
    routeDisplayWindow->hide();
    infoWindow.hide();
    
    for (auto& loc : locations) {
        loc.pin.setOutlineColor(Color::White);
    }
    
    currentState = NAVIGATION_MENU;
    navMenu.show();
}
        if (currentState == FILTER_RESULT_MAP && event.type == Event::KeyPressed && event.key.code == Keyboard::Escape) {
    currentState = FILTER_PREFERENCES;
    filterMenu.createMainMenu();
    
    // Reset preferences
    userPreferences.preferredCompanies.clear();
    userPreferences.avoidPorts.clear();
    userPreferences.hasCompanyFilter = false;
    userPreferences.hasPortFilter = false;
    userPreferences.hasTimeFilter = false;
    cout << "Back to filter preferences" << endl;
}
        // UPDATE SECTION
        if (currentState == MAIN_MENU) {
            startButton.update(window);
            settingsButton.update(window);
            exitButton.update(window);
        }
        else if (currentState == NAVIGATION_MENU) {
            navMenu.update(window);
        }
        else if (currentState == MAP_VIEW) {
            for (auto& edge : edges) {
                edge.update(window);
            }
            for (auto& location : locations) {
                location.update(window);
            }
            infoWindow.update(window);
        }
        else if (currentState == FILTER_RESULT_MAP) {
    for (auto& edge : edges) {
        edge.update(window);
    }
    for (auto& location : locations) {
        location.update(window);
    }
    infoWindow.update(window);
}
        else if (currentState == SEARCH_ROUTES) {
            for (auto& edge : edges) {
                edge.update(window);
            }
            for (auto& edge : highlightedRoutes) {
                edge.update(window);
            }
            for (auto& location : locations) {
                location.update(window);
            }
            infoWindow.update(window);
        }
        else if (currentState == SHORTEST_ROUTE_SELECT) {
            for (auto& edge : edges) {
                edge.update(window);
            }
            for (auto& edge : shortestPathEdges) {
                edge.update(window);
            }
            for (auto& location : locations) {
                location.update(window);
            }
            infoWindow.update(window);
        }
        else if (currentState == CHEAPEST_ROUTE_SELECT) {
            for (auto& edge : edges) {
                edge.update(window);
            }
            for (auto& edge : cheapestPathEdges) {
                edge.update(window);
            }
            for (auto& location : locations) {
                location.update(window);
            }
            infoWindow.update(window);
        }
else if (currentState == FILTER_PREFERENCES || currentState == FILTER_COMPANIES || 
         currentState == FILTER_PORTS || currentState == FILTER_TIME || 
         currentState == FILTER_CUSTOM_COMPANIES || currentState == FILTER_CUSTOM_PORTS || 
         currentState == FILTER_CUSTOM_TIME) {
    filterMenu.update(window);
}
        
        // RENDERING SECTION
        window.clear(); 
        
        if (oceanLoaded) {
            window.draw(oceanSprite);
        } else {
            window.draw(oceanBackground);
        }
        
        if (currentState == MAIN_MENU) {
            window.draw(welcomeText);
            startButton.draw(window);
            settingsButton.draw(window);
            exitButton.draw(window);
        }
        else if (currentState == NAVIGATION_MENU) {
            navMenu.draw(window);
        }
        else if (currentState == MAP_VIEW) {
            window.draw(mapSprite);
            for (auto& edge : edges) {
                edge.draw(window);
            }
            for (auto& location : locations) {
                location.draw(window);
            }
            infoWindow.draw(window);
                instructionText.setFont(font);
    instructionText.setString("Display Ports Info - Click ports/routes for details | Press ESC to go back");
    instructionText.setCharacterSize(20);
    instructionText.setFillColor(Color::White);
    instructionText.setOutlineThickness(2);
    instructionText.setOutlineColor(Color::Black);
    FloatRect bounds = instructionText.getLocalBounds();
    instructionText.setOrigin(bounds.left + bounds.width, bounds.top + bounds.height);
    instructionText.setPosition(windowWidth - 20, windowHeight - 20);
    window.draw(instructionText);
}
else if (currentState == SEARCH_ROUTES) {
    window.draw(mapSprite);
    
    // Draw all original edges (very dimmed)
    for (auto& edge : edges) {
        RectangleShape tempLine = edge.line;
        tempLine.setFillColor(Color(150, 150, 150, 50));
        window.draw(tempLine);
    }
    
    // Draw highlighted routes
    for (size_t i = 0; i < highlightedRoutes.size(); i++) {
        window.draw(highlightedRoutes[i].line);
    }
    
    // Draw locations
    for (auto& location : locations) {
        location.draw(window);
    }
    
    // Draw info window (NOT route display window)
    infoWindow.draw(window);
    
    // Instructions
    instructionText.setFont(font);
    instructionText.setString("Search for Routes - Click ports/routes for details | Press ESC to go back");
    instructionText.setCharacterSize(20);
    instructionText.setFillColor(Color::White);
    instructionText.setOutlineThickness(2);
    instructionText.setOutlineColor(Color::Black);
    FloatRect bounds = instructionText.getLocalBounds();
    instructionText.setOrigin(bounds.left + bounds.width, bounds.top + bounds.height);
    instructionText.setPosition(windowWidth - 20, windowHeight - 20);
    window.draw(instructionText);
}
        else if (currentState == SHORTEST_ROUTE_SELECT) {
            window.draw(mapSprite);
            
            for (auto& edge : edges) {
                edge.line.setFillColor(Color(100, 100, 100, 100));
                edge.draw(window);
            }
            
            for (auto& edge : shortestPathEdges) {
                edge.draw(window);
            }
            
            for (auto& location : locations) {
                location.draw(window);
            }
            
    infoWindow.draw(window);
    
    instructionText.setFont(font);
    if (selectedPorts.size() < 2) {
        instructionText.setString("Click on 2 ports to calculate shortest route (Yellow highlight)");
    } else {
        instructionText.setString("Calculating shortest route...");
    }
    instructionText.setCharacterSize(24);
    instructionText.setFillColor(Color::White);
    instructionText.setOutlineThickness(2);
    instructionText.setOutlineColor(Color::Black);
    FloatRect bounds = instructionText.getLocalBounds();
    instructionText.setOrigin(bounds.left + bounds.width, bounds.top + bounds.height);
    instructionText.setPosition(windowWidth - 20, windowHeight - 20);
    window.draw(instructionText);
}
 else if (currentState == CHEAPEST_ROUTE_SELECT) {
    window.draw(mapSprite);
    
    for (auto& edge : edges) {
        edge.line.setFillColor(Color(100, 100, 100, 100));
        edge.draw(window);
    }
    
    for (auto& edge : shortestPathEdges) {
        edge.draw(window);
    }
    
    for (auto& location : locations) {
        location.draw(window);
    }
    
    infoWindow.draw(window);
    
    instructionText.setFont(font);
    if (selectedPorts.size() < 2) {
        instructionText.setString("Click on 2 ports to calculate cheapest route (Cyan highlight)");
    } else {
        instructionText.setString("Calculating cheapest route...");
    }
    instructionText.setCharacterSize(24);
    instructionText.setFillColor(Color::White);
    instructionText.setOutlineThickness(2);
    instructionText.setOutlineColor(Color::Black);
    FloatRect bounds = instructionText.getLocalBounds();
    instructionText.setOrigin(bounds.left + bounds.width, bounds.top + bounds.height);
    instructionText.setPosition(windowWidth - 20, windowHeight - 20);
    window.draw(instructionText);
}
else if (currentState == FILTER_PREFERENCES || currentState == FILTER_COMPANIES || 
         currentState == FILTER_PORTS || currentState == FILTER_TIME || 
         currentState == FILTER_CUSTOM_COMPANIES || currentState == FILTER_CUSTOM_PORTS || 
         currentState == FILTER_CUSTOM_TIME) {
    filterMenu.draw(window);
}
        else if (currentState == BOOK_CARGO) {
            Text placeholder;
            placeholder.setFont(font);
            placeholder.setString("Book Cargo Routes\n\n(Press ESC to go back)");
            placeholder.setCharacterSize(40);
            placeholder.setFillColor(Color::White);
            FloatRect bounds = placeholder.getLocalBounds();
            placeholder.setOrigin(bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
            placeholder.setPosition(windowWidth / 2, windowHeight / 2);
            window.draw(placeholder);
        }
        else if (currentState == PROCESS_LAYOVERS) {
            Text placeholder;
            placeholder.setFont(font);
            placeholder.setString("Process Layovers\n\n(Press ESC to go back)");
            placeholder.setCharacterSize(40);
            placeholder.setFillColor(Color::White);
            FloatRect bounds = placeholder.getLocalBounds();
            placeholder.setOrigin(bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
            placeholder.setPosition(windowWidth / 2, windowHeight / 2);
            window.draw(placeholder);
        }
        else if (currentState == TRACK_MULTI_LEG) {
            Text placeholder;
            placeholder.setFont(font);
            placeholder.setString("Track Multi-Leg Journeys\n\n(Press ESC to go back)");
            placeholder.setCharacterSize(40);
            placeholder.setFillColor(Color::White);
            FloatRect bounds = placeholder.getLocalBounds();
            placeholder.setOrigin(bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
            placeholder.setPosition(windowWidth / 2, windowHeight / 2);
            window.draw(placeholder);
        }
        else if (currentState == SUBGRAPH) {
            Text placeholder;
            placeholder.setFont(font);
            placeholder.setString("Generate Subgraph\n\n(Press ESC to go back)");
            placeholder.setCharacterSize(40);
            placeholder.setFillColor(Color::White);
            FloatRect bounds = placeholder.getLocalBounds();
            placeholder.setOrigin(bounds.left + bounds.width / 2, bounds.top + bounds.height / 2);
            placeholder.setPosition(windowWidth / 2, windowHeight / 2);
            window.draw(placeholder);
        }
        else if (currentState == FILTER_RESULT_MAP) {
    window.draw(mapSprite);
    
    // Draw all edges
    for (auto& edge : edges) {
        edge.draw(window);
    }
    
    // Draw all locations (with filter colors applied)
    for (auto& location : locations) {
        location.draw(window);
    }
    
    infoWindow.draw(window);
    
    // Draw instruction text
    instructionText.setFont(font);
    instructionText.setString("Filtered Map View - Click ports/routes for details | Press ESC to go back");
    instructionText.setCharacterSize(20);
    instructionText.setFillColor(Color::White);
    instructionText.setOutlineThickness(2);
    instructionText.setOutlineColor(Color::Black);
    FloatRect bounds = instructionText.getLocalBounds();
instructionText.setOrigin(bounds.left + bounds.width, bounds.top + bounds.height);
instructionText.setPosition(windowWidth - 20, windowHeight - 20);  // Bottom right
    window.draw(instructionText);
}
        window.display(); 
    } 
}
    return 0; 
}