#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
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

struct Port {
  string name;
  int cost;
  vector<string> weatherConditions;
  Port(string n, int c) {
    name = n;
    cost = c;
    weatherConditions = {};
  }
  Port(string n, int c, vector<string> weather) {
    name = n;
    cost = c;
    weatherConditions = weather;
  }
};
struct CompleteRoute {
  vector<int> portPath;
  vector<Route> routeLegs;
  int totalCost;
  int totalTime;
  int layoverCount;
};
struct BookedRoute {
  string bookingID;
  CompleteRoute route;
  string originPort;
  string destPort;
  string bookingDate;
  string customerName;

  BookedRoute(const CompleteRoute& r, const string& from, const string& to,
              const string& date, const string& customer = "Guest")
      : route(r),
        originPort(from),
        destPort(to),
        bookingDate(date),
        customerName(customer) {
    bookingID = "BK" + date.substr(0, 2) + date.substr(3, 2) +
                date.substr(6, 4) + to_string(rand() % 10000);
  }
};
struct Ship {
  string shipID;
  CompleteRoute route;
  string originPort;
  string destPort;
  int currentLegIndex;
  Vector2f position;
  float animationProgress;
  bool isMoving;
  bool isDocked;
  int queuePosition;
  float dockingTimeRemaining;
  Color shipColor;

  Ship(const string& id, const CompleteRoute& r, const string& from,
       const string& to)
      : shipID(id),
        route(r),
        originPort(from),
        destPort(to),
        currentLegIndex(0),
        animationProgress(0.0f),
        isMoving(false),
        isDocked(false),
        queuePosition(-1),
        dockingTimeRemaining(0.0f) {
    shipColor =
        Color(rand() % 156 + 100, rand() % 156 + 100, rand() % 156 + 100);
  }
};
struct ShortestRouteResult {
  vector<int> dist;
  vector<int> parent;
  int srcIdx;
  bool found;
};

struct UserPreferences {
  vector<string> preferredCompanies;
  vector<string> avoidPorts;
  int maxVoyageTime;
  bool hasCompanyFilter;
  bool hasPortFilter;
  bool hasTimeFilter;
};

struct FilteredGraphData {
  vector<int> allowedPorts;
  vector<vector<Route>> filteredRoutes;
};
struct JourneyLeg {
  int fromPortIdx;
  int toPortIdx;
  Route route;
  bool isValid;
};
struct PortDockingQueue {
  string portName;
  int portIndex;
  vector<Ship*> queue;
  int maxDockingSlots;
  int currentlyDocked;
  float averageDockingTime;
  PortDockingQueue()
      : portName(""),
        portIndex(-1),
        maxDockingSlots(2),
        currentlyDocked(0),
        averageDockingTime(8.0f) {}

  PortDockingQueue(const string& name, int idx, int slots = 2,
                   float avgTime = 8.0f)
      : portName(name),
        portIndex(idx),
        maxDockingSlots(slots),
        currentlyDocked(0),
        averageDockingTime(avgTime) {}

  void addShip(Ship* ship) {
    queue.push_back(ship);
    ship->queuePosition = queue.size() - 1;
  }

  bool canDockNow() const { return currentlyDocked < maxDockingSlots; }

  void processQueue(float deltaTime) {
    for (auto it = queue.begin(); it != queue.end();) {
      Ship* ship = *it;

      if (ship->isDocked) {
        ship->dockingTimeRemaining -= deltaTime;

        if (ship->dockingTimeRemaining <= 0) {
          ship->isDocked = false;
          currentlyDocked--;
          it = queue.erase(it);

          ship->currentLegIndex++;
          ship->isMoving = true;
          ship->animationProgress = 0.0f;
          continue;
        }
      }
      ++it;
    }

    for (auto& ship : queue) {
      if (!ship->isDocked && canDockNow()) {
        ship->isDocked = true;
        ship->dockingTimeRemaining = averageDockingTime;
        currentlyDocked++;
        break;
      }
    }

    for (size_t i = 0; i < queue.size(); i++) {
      queue[i]->queuePosition = i;
    }
  }
};

const int INF = 1e9;
const int MAX_LAYOVERS = 3;

int dateToInt(const string& date) {
  int day = stoi(date.substr(0, 2));
  int month = stoi(date.substr(3, 2));
  int year = stoi(date.substr(6, 4));
  return year * 10000 + month * 100 + day;
}

int timeToMinutes(const string& time) {
  int hours = stoi(time.substr(0, 2));
  int minutes = stoi(time.substr(3, 2));
  return hours * 60 + minutes;
}

bool isValidLegTransition(const Route& prevLeg, const Route& currentLeg) {
  int prevArrivalDate = dateToInt(prevLeg.date);
  int currentDepDate = dateToInt(currentLeg.date);
  int prevArrivalTime = timeToMinutes(prevLeg.arrTime);
  int currentDepTime = timeToMinutes(currentLeg.depTime);

  if (currentDepDate > prevArrivalDate) {
    return true;
  } else if (currentDepDate == prevArrivalDate) {
    return currentDepTime > prevArrivalTime;
  } else {
    return false;
  }
}

bool isValidRoutePath(const vector<int>& path, const vector<Route>& routeLegs) {
  if (routeLegs.size() < 1) return false;
  if (routeLegs.size() != path.size() - 1) return false;

  for (size_t i = 1; i < routeLegs.size(); i++) {
    if (!isValidLegTransition(routeLegs[i - 1], routeLegs[i])) {
      return false;
    }
  }
  return true;
}

class Graph {
 public:
  vector<Port> ports;
  vector<vector<Route>> routes;

  void addPort(const string& name, int cost) {
    ports.push_back(Port(name, cost));
    routes.push_back(vector<Route>());
  }

  void addRoute(const string& src, const Route& route) {
    int srcIdx = getPortIndex(src);
    if (srcIdx == -1) {
      cout << "Source port not found: " << src << ", adding it automatically."
           << endl;
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
  void parseWeatherData(const string& filename) {
    ifstream file(filename);
    if (!file) {
      cout << "Error opening weather file!" << endl;
      return;
    }

    string line;
    while (getline(file, line)) {
      if (line.empty()) continue;
      stringstream ss(line);
      string portName;
      ss >> portName;

      int portIdx = getPortIndex(portName);
      if (portIdx != -1) {
        string weather;
        while (ss >> weather) {
          ports[portIdx].weatherConditions.push_back(weather);
        }
      }
    }
    file.close();
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
    file.close();
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
      if (!(ss >> origin >> destination >> date >> depTime >> arrTime >>
            cost)) {
        cout << "Route line format error: " << line << endl;
        continue;
      }
      getline(ss, company);
      if (!company.empty() && company[0] == ' ') company.erase(0, 1);

      int travelTime = calculateTravelTime(depTime, arrTime);
      Route r = {destination, date,    depTime,   arrTime,
                 cost,        company, travelTime};
      addRoute(origin, r);
    }
    file.close();
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

  void printPorts() {
    if (ports.empty()) {
      cout << "No ports loaded!" << endl;
      return;
    }
    cout << "=== Ports Loaded ===" << endl;
    for (size_t i = 0; i < ports.size(); i++) {
      cout << i + 1 << ". " << ports[i].name << " | Cost: " << ports[i].cost
           << endl;
    }
    cout << "==================" << endl;
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

  ShortestRouteResult dijkstra(Port* src, bool findCheapest,
                               const UserPreferences* prefs = nullptr) {
    int n = ports.size();
    vector<int> dist(n, INF);
    vector<bool> visited(n, false);
    vector<int> parent(n, -1);
    vector<Route> lastRoute(n);

    int srcIdx = getPortIndex(src->name);
    if (srcIdx == -1) {
      cout << "Source port not found!" << endl;
      return {dist, parent, -1, false};
    }

    if (prefs && prefs->hasPortFilter &&
        portIsAvoid(ports[srcIdx].name, *prefs)) {
      cout << "Source port is in avoid list!" << endl;
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
        int v = getPortIndex(route.destination);
        if (v == -1 || visited[v]) continue;

        if (prefs) {
          if (prefs->hasPortFilter && portIsAvoid(ports[v].name, *prefs))
            continue;
          if (prefs->hasCompanyFilter &&
              !routeHasPreferredCompany(route, *prefs))
            continue;
          if (prefs->hasTimeFilter && route.travelTime > prefs->maxVoyageTime)
            continue;
        }

        bool validTiming = true;
        if (u != srcIdx && parent[u] != -1) {
          validTiming = isValidLegTransition(lastRoute[u], route);
        }

        if (!validTiming) continue;

        int newDist = findCheapest ? dist[u] + route.cost + ports[v].cost
                                   : dist[u] + route.travelTime;

        if (newDist < dist[v]) {
          dist[v] = newDist;
          parent[v] = u;
          lastRoute[v] = route;
        }
      }
    }

    return {dist, parent, srcIdx, true};
  }

  ShortestRouteResult findShortestRoute(
      Port* src, const UserPreferences* prefs = nullptr) {
    return dijkstra(src, false, prefs);
  }

  ShortestRouteResult findCheapestRoute(
      Port* src, const UserPreferences* prefs = nullptr) {
    return dijkstra(src, true, prefs);
  }

  void dfsEnumerateRoutes(int currentIdx, int destIdx, vector<int>& currentPath,
                          vector<Route>& currentLegs,
                          vector<CompleteRoute>& results, int maxLegs) {
    if (currentLegs.size() > static_cast<size_t>(maxLegs)) {
      return;
    }

    if (currentIdx == destIdx && !currentLegs.empty()) {
      if (!isValidRoutePath(currentPath, currentLegs)) {
        return;
      }

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
        cr.totalCost += ports[currentPath[i]].cost;
      }
      cr.layoverCount = currentPath.size() >= 2
                            ? static_cast<int>(currentPath.size()) - 2
                            : 0;

      results.push_back(cr);
      return;
    }

    if (currentLegs.size() == static_cast<size_t>(maxLegs)) {
      return;
    }

    for (const Route& route : routes[currentIdx]) {
      int nextIdx = getPortIndex(route.destination);
      if (nextIdx == -1) continue;

      if (find(currentPath.begin(), currentPath.end(), nextIdx) !=
          currentPath.end()) {
        continue;
      }

      bool validTiming = true;
      if (!currentLegs.empty()) {
        validTiming = isValidLegTransition(currentLegs.back(), route);
      }

      if (!validTiming) continue;

      currentPath.push_back(nextIdx);
      currentLegs.push_back(route);
      dfsEnumerateRoutes(nextIdx, destIdx, currentPath, currentLegs, results,
                         maxLegs);
      currentLegs.pop_back();
      currentPath.pop_back();
    }
  }
  bool portHasWeather(const string& portName, const string& weather) const {
    int idx = getPortIndex(portName);
    if (idx == -1) return false;

    for (const string& w : ports[idx].weatherConditions) {
      if (w == weather) return true;
    }
    return false;
  }
  vector<string> getAllWeatherConditions() const {
    set<string> conditions;
    for (const Port& port : ports) {
      for (const string& weather : port.weatherConditions) {
        conditions.insert(weather);
      }
    }
    return vector<string>(conditions.begin(), conditions.end());
  }
  vector<CompleteRoute> findAllPossibleRoutes(int originIdx, int destIdx) {
    vector<CompleteRoute> allRoutes;
    if (originIdx < 0 || originIdx >= ports.size() || destIdx < 0 ||
        destIdx >= ports.size()) {
      return allRoutes;
    }

    vector<int> currentPath = {originIdx};
    vector<Route> currentLegs;
    int maxLegs = MAX_LAYOVERS + 1;
    dfsEnumerateRoutes(originIdx, destIdx, currentPath, currentLegs, allRoutes,
                       maxLegs);

    return allRoutes;
  }

  bool findCheapestEnumeratedRoute(int originIdx, int destIdx,
                                   CompleteRoute& cheapestOut) {
    vector<CompleteRoute> allRoutes = findAllPossibleRoutes(originIdx, destIdx);

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

  bool portIsAvoid(const string& portName, const UserPreferences& prefs) const {
    if (!prefs.hasPortFilter) return false;
    for (const string& avoidPort : prefs.avoidPorts) {
      if (portName == avoidPort) return true;
    }
    return false;
  }

  bool routeHasPreferredCompany(const Route& route,
                                const UserPreferences& prefs) const {
    if (!prefs.hasCompanyFilter) return true;
    for (const string& company : prefs.preferredCompanies) {
      if (route.company == company) return true;
    }
    return false;
  }

  bool routeMatchesPreferences(const Route& route,
                               const UserPreferences& prefs) const {
    if (!routeHasPreferredCompany(route, prefs)) return false;
    if (prefs.hasTimeFilter && route.travelTime > prefs.maxVoyageTime)
      return false;
    return true;
  }

  FilteredGraphData applyFilters(const UserPreferences& prefs) const {
    FilteredGraphData result;
    result.filteredRoutes.resize(ports.size());

    for (int i = 0; i < ports.size(); i++) {
      if (!portIsAvoid(ports[i].name, prefs)) {
        result.allowedPorts.push_back(i);
      }
    }

    for (int i = 0; i < routes.size(); i++) {
      for (const Route& route : routes[i]) {
        if (!portIsAvoid(ports[i].name, prefs) &&
            !portIsAvoid(route.destination, prefs) &&
            routeMatchesPreferences(route, prefs)) {
          result.filteredRoutes[i].push_back(route);
        }
      }
    }

    return result;
  }

  vector<string> getAllShippingCompanies() const {
    set<string> companies;
    for (int i = 0; i < routes.size(); i++) {
      for (const Route& route : routes[i]) {
        companies.insert(route.company);
      }
    }
    return vector<string>(companies.begin(), companies.end());
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

  vector<string> formatRouteWithLegs(const vector<int>& path,
                                     const vector<Route>& routeLegs,
                                     float totalTime = 0, float totalCost = 0) {
    vector<string> details;
    details.push_back("=== ROUTE DETAILS ===");
    details.push_back("");

    string pathStr = "Path: ";
    for (size_t i = 0; i < path.size(); i++) {
      pathStr += ports[path[i]].name;
      if (i < path.size() - 1) pathStr += " -> ";
    }
    details.push_back(pathStr);
    details.push_back("");
    for (size_t i = 0; i < routeLegs.size(); i++) {
      details.push_back("--- LEG " + to_string(i + 1) + " ---");
      details.push_back("From: " + ports[path[i]].name);
      details.push_back("To: " + ports[path[i + 1]].name);
      details.push_back("Date: " + routeLegs[i].date);
      details.push_back("Departure: " + routeLegs[i].depTime);
      details.push_back("Arrival: " + routeLegs[i].arrTime);
      details.push_back("Cost: $" + to_string(routeLegs[i].cost));
      details.push_back(
          "Travel Time: " + to_string(routeLegs[i].travelTime / 60) + "h " +
          to_string(routeLegs[i].travelTime % 60) + "m");
      details.push_back("Company: " + routeLegs[i].company);
      details.push_back("");
      totalCost += routeLegs[i].cost;
      totalTime += routeLegs[i].travelTime;
    }

    details.push_back("=== SUMMARY ===");
    details.push_back("Total Cost: $" + to_string(totalCost));
    int hours = floor(totalTime / 60.0);
    int minutes = (int)fmod(totalTime, 60.0);

    details.push_back("Total Travel Time: " + to_string(hours) + "h " +
                      to_string(minutes) + "m");

    details.push_back("Number of Legs: " + to_string(routeLegs.size()));

    return details;
  }

  vector<string> formatAllRoutes(int originIdx, int destIdx) {
    vector<string> display;
    string originName = ports[originIdx].name;
    string destName = ports[destIdx].name;

    vector<CompleteRoute> allRoutes = findAllPossibleRoutes(originIdx, destIdx);

    if (allRoutes.empty()) {
      display.push_back("=== NO ROUTES FOUND ===");
      display.push_back("");
      display.push_back("No direct or connected routes available between " +
                        originName + " and " + destName + ".");
      return display;
    }

    vector<CompleteRoute> uniqueRoutes;
    for (const auto& route : allRoutes) {
      bool isDuplicate = false;
      for (const auto& existing : uniqueRoutes) {
        if (route.portPath == existing.portPath &&
            route.totalCost == existing.totalCost &&
            route.totalTime == existing.totalTime) {
          isDuplicate = true;
          break;
        }
      }
      if (!isDuplicate) {
        uniqueRoutes.push_back(route);
      }
    }

    CompleteRoute shortestRoute = uniqueRoutes[0];
    CompleteRoute cheapestRoute = uniqueRoutes[0];

    for (const auto& route : uniqueRoutes) {
      if (route.totalTime < shortestRoute.totalTime) {
        shortestRoute = route;
      }
      if (route.totalCost < cheapestRoute.totalCost) {
        cheapestRoute = route;
      }
    }

    vector<CompleteRoute> directRoutes, singleLayoverRoutes, multiLayoverRoutes;
    for (const auto& route : uniqueRoutes) {
      if (route.layoverCount == 0)
        directRoutes.push_back(route);
      else if (route.layoverCount == 1)
        singleLayoverRoutes.push_back(route);
      else
        multiLayoverRoutes.push_back(route);
    }

    auto formatPath = [this](const vector<int>& path) {
      string s;
      for (size_t i = 0; i < path.size(); i++) {
        s += ports[path[i]].name;
        if (i < path.size() - 1) s += " -> ";
      }
      return s;
    };

    display.push_back(
        "==================== ROUTE SEARCH RESULTS ====================");
    display.push_back("");
    display.push_back("Number of Available Routes: " +
                      to_string(uniqueRoutes.size()));
    display.push_back("");

    display.push_back(
        "==================== QUICK SUMMARY ====================");
    display.push_back("");
    display.push_back("FASTEST ROUTE:");
    display.push_back(formatPath(shortestRoute.portPath));
    display.push_back("Time: " + to_string(shortestRoute.totalTime / 60) +
                      "h " + to_string(shortestRoute.totalTime % 60) +
                      "m | Cost: $" + to_string(shortestRoute.totalCost));
    display.push_back("");
    display.push_back("CHEAPEST ROUTE:");
    display.push_back(formatPath(cheapestRoute.portPath));
    display.push_back("Cost: $" + to_string(cheapestRoute.totalCost) +
                      "  Time: " + to_string(cheapestRoute.totalTime / 60) +
                      "h " + to_string(cheapestRoute.totalTime % 60) + "m");
    display.push_back("");

    auto displayRouteCategory = [&](const vector<CompleteRoute>& routes,
                                    const string& title, const string& icon) {
      if (!routes.empty()) {
        display.push_back(icon + " " + title + " (" + to_string(routes.size()) +
                          ") ====================");
        display.push_back("");

        for (size_t i = 0; i < routes.size(); i++) {
          display.push_back("ROUTE " + to_string(i + 1));
          display.push_back("");
          display.push_back("Path: " + formatPath(routes[i].portPath));
          display.push_back("Cost: $" + to_string(routes[i].totalCost) +
                            "  Time: " + to_string(routes[i].totalTime / 60) +
                            "h " + to_string(routes[i].totalTime % 60) + "m");
          display.push_back("");

          for (size_t j = 0; j < routes[i].routeLegs.size(); j++) {
            const Route& leg = routes[i].routeLegs[j];
            display.push_back("---- LEG " + to_string(j + 1) + "----");
            display.push_back(ports[routes[i].portPath[j]].name + " -> " +
                              ports[routes[i].portPath[j + 1]].name);
            display.push_back("Date: " + leg.date + "   " + leg.depTime +
                              " - " + leg.arrTime);
            display.push_back("Cost: $" + to_string(leg.cost) +
                              "  Duration: " + to_string(leg.travelTime / 60) +
                              "h " + to_string(leg.travelTime % 60) + "m");
            display.push_back("Company: " + leg.company);
            if (j < routes[i].routeLegs.size() - 1) {
              display.push_back("");
            }
          }
          display.push_back("");
        }
      }
    };

    displayRouteCategory(directRoutes, "DIRECT ROUTES",
                         " ====================");
    displayRouteCategory(singleLayoverRoutes, "SINGLE LAYOVER ROUTES",
                         " ====================");
    displayRouteCategory(multiLayoverRoutes, "MULTI-LAYOVER ROUTES",
                         " ====================");

    display.push_back("==================== SUMMARY ====================");
    display.push_back("");
    display.push_back("Direct Routes: " + to_string(directRoutes.size()));
    display.push_back("Single Layover: " +
                      to_string(singleLayoverRoutes.size()));
    display.push_back("Multi-Layover: " + to_string(multiLayoverRoutes.size()));
    display.push_back("Total Unique Routes: " + to_string(uniqueRoutes.size()));
    display.push_back("");
    display.push_back("==================== COLOR LEGEND ====================");
    display.push_back("");
    display.push_back("MAGENTA = Direct Routes (No layovers)");
    display.push_back("ORANGE = Single Layover Routes");
    display.push_back("BLUE = Multi-Layover Routes (2+ stops)");
    display.push_back("");
    return display;
  }
  bool hasDirectRoute(int fromIdx, int toIdx) const {
    if (fromIdx < 0 || fromIdx >= ports.size() || toIdx < 0 ||
        toIdx >= ports.size()) {
      return false;
    }

    for (const Route& route : routes[fromIdx]) {
      if (getPortIndex(route.destination) == toIdx) {
        return true;
      }
    }
    return false;
  }

  vector<Route> getRoutesFromPort(int portIdx) const {
    if (portIdx < 0 || portIdx >= ports.size()) {
      return vector<Route>();
    }
    return routes[portIdx];
  }

  Route getRouteBetween(int fromIdx, int toIdx) const {
    for (const Route& route : routes[fromIdx]) {
      if (getPortIndex(route.destination) == toIdx) {
        return route;
      }
    }
    return Route{};
  }
};

struct MultiLegJourney {
  vector<int> portPath;
  vector<JourneyLeg> legs;
  int totalCost;
  int totalTime;
  bool isComplete;

  void calculateTotals(const Graph& g) {
    totalCost = 0;
    totalTime = 0;

    for (const auto& leg : legs) {
      if (leg.isValid) {
        totalCost += leg.route.cost;
        totalTime += leg.route.travelTime;
      }
    }

    for (size_t i = 1; i < portPath.size(); i++) {
      totalCost += g.ports[portPath[i]].cost;
    }
  }
};
class Button {
 public:
  RectangleShape shape;
  Text text;
  Color normalColor;
  Color hoverColor;
  Color clickColor;
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
    text.setPosition(position.x + size.x / 2.0f, position.y + size.y / 2.0f);
  }
  void setPosition(const sf::Vector2f& pos) {
    shape.setPosition(pos);
    FloatRect bounds = text.getLocalBounds();
    text.setPosition(
        pos.x + (shape.getSize().x - bounds.width) / 2.0f,
        pos.y + (shape.getSize().y - bounds.height) / 2.0f - bounds.top);
  }
  void setText(const string& newText) {
    text.setString(newText);
    FloatRect textBounds = text.getLocalBounds();
    text.setOrigin(textBounds.left + textBounds.width / 2.0f,
                   textBounds.top + textBounds.height / 2.0f);

    Vector2f shapePos = shape.getPosition();
    Vector2f shapeSize = shape.getSize();
    text.setPosition(shapePos.x + shapeSize.x / 2.0f,
                     shapePos.y + shapeSize.y / 2.0f);
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
class SubgraphMenu {
 public:
  vector<Button*> buttons;
  Text titleText;
  bool isVisible;
  Font* font;
  float windowWidth;
  float windowHeight;
  Texture* oceanTexture;
  Sprite oceanBackground;
  vector<string> selectedOptions;
  enum SubgraphMode { COMPANY_MODE, WEATHER_MODE };
  SubgraphMode currentMode;
  SubgraphMenu(Font& f, float winWidth, float winHeight, Texture& oceanTex)
      : font(&f),
        isVisible(false),
        windowWidth(winWidth),
        windowHeight(winHeight),
        oceanTexture(&oceanTex),
        currentMode(COMPANY_MODE) {
    titleText.setFont(f);
    titleText.setCharacterSize(48);
    titleText.setFillColor(Color::White);
    titleText.setOutlineThickness(2);
    titleText.setOutlineColor(Color::Black);
    oceanBackground.setTexture(*oceanTexture);
    Vector2u textureSize = oceanTexture->getSize();
    float scaleX = (float)winWidth / textureSize.x;
    float scaleY = (float)winHeight / textureSize.y;
    oceanBackground.setScale(scaleX, scaleY);
  }

  ~SubgraphMenu() { clearButtons(); }

  void clearButtons() {
    for (auto btn : buttons) {
      delete btn;
    }
    buttons.clear();
  }

  void createMainMenu() {
    clearButtons();
    titleText.setString("Generate Subgraph");
    selectedOptions.clear();

    float buttonWidth = 400;
    float buttonHeight = 70;
    float centerX = (windowWidth - buttonWidth) / 2;
    float startY = windowHeight * 0.2f;
    float spacing = buttonHeight + 15;

    buttons.push_back(new Button(Vector2f(centerX, startY),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Based on Company", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Based on Weather Conditions", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 4),
                                 Vector2f(buttonWidth, buttonHeight), "Back",
                                 *font));
  }

  void createCompanyMenu(const vector<string>& companies) {
    clearButtons();
    titleText.setString("Select Companies");
    currentMode = COMPANY_MODE;

    float buttonWidth = 250;
    float buttonHeight = 60;
    float startX = 100;
    float startY = windowHeight * 0.15f;
    float spacingX = buttonWidth + 30;
    float spacingY = buttonHeight + 20;

    int cols = 4;
    int col = 0;
    int row = 0;

    for (const string& company : companies) {
      float x = startX + (col * spacingX);
      float y = startY + (row * spacingY);

      Button* btn = new Button(
          Vector2f(x, y), Vector2f(buttonWidth, buttonHeight), company, *font);
      for (const string& selected : selectedOptions) {
        if (selected == company) {
          btn->shape.setFillColor(Color(0, 150, 0));
          break;
        }
      }
      buttons.push_back(btn);

      col++;
      if (col >= cols) {
        col = 0;
        row++;
      }
    }
    float actionY = startY + ((row + 1) * spacingY) + 40;
    float centerX = (windowWidth - 250) / 2;

    buttons.push_back(new Button(Vector2f(centerX - 270, actionY),
                                 Vector2f(250, 60), "Select All", *font));
    buttons.push_back(new Button(Vector2f(centerX, actionY), Vector2f(250, 60),
                                 "Clear All", *font));
    buttons.push_back(new Button(Vector2f(centerX + 270, actionY),
                                 Vector2f(250, 60), "Done", *font));
  }

  void createWeatherMenu(const vector<string>& weatherConditions) {
    clearButtons();
    titleText.setString("Select Weather to AVOID");
    currentMode = WEATHER_MODE;

    float buttonWidth = 250;
    float buttonHeight = 60;
    float startX = 100;
    float startY = windowHeight * 0.15f;
    float spacingX = buttonWidth + 30;
    float spacingY = buttonHeight + 20;

    int cols = 3;
    int col = 0;
    int row = 0;

    vector<string> weatherDisplayNames = {
        "Cyclone", "Fog", "Heavy Rain", "High Wind", "Snow", "Storm", "Clear"};

    for (size_t i = 0; i < weatherConditions.size(); i++) {
      float x = startX + (col * spacingX);
      float y = startY + (row * spacingY);

      string displayName = (i < weatherDisplayNames.size())
                               ? weatherDisplayNames[i]
                               : weatherConditions[i];
      Button* btn =
          new Button(Vector2f(x, y), Vector2f(buttonWidth, buttonHeight),
                     displayName, *font);
      for (const string& selected : selectedOptions) {
        if (selected == weatherConditions[i]) {
          btn->shape.setFillColor(Color(150, 0, 0));
          break;
        }
      }
      buttons.push_back(btn);

      col++;
      if (col >= cols) {
        col = 0;
        row++;
      }
    }

    float actionY = startY + ((row + 1) * spacingY) + 40;
    float centerX = (windowWidth - 250) / 2;

    buttons.push_back(new Button(Vector2f(centerX - 270, actionY),
                                 Vector2f(250, 60), "Select All", *font));
    buttons.push_back(new Button(Vector2f(centerX, actionY), Vector2f(250, 60),
                                 "Clear All", *font));
    buttons.push_back(new Button(Vector2f(centerX + 270, actionY),
                                 Vector2f(250, 60), "Done", *font));
  }

  void toggleOption(const string& option) {
    auto it = find(selectedOptions.begin(), selectedOptions.end(), option);
    if (it == selectedOptions.end()) {
      selectedOptions.push_back(option);
    } else {
      selectedOptions.erase(it);
    }
  }

  void selectAll(const vector<string>& allOptions) {
    selectedOptions = allOptions;
  }

  void clearAll() { selectedOptions.clear(); }

  void show() {
    isVisible = true;
    createMainMenu();
  }

  void hide() {
    isVisible = false;
    clearButtons();
  }

  bool visible() const { return isVisible; }

  void update(RenderWindow& window) {
    if (isVisible) {
      for (auto btn : buttons) {
        btn->update(window);
      }
    }
  }

  void draw(RenderWindow& window) {
    if (isVisible) {
      window.draw(oceanBackground);
      RectangleShape overlay(Vector2f(windowWidth, windowHeight));
      overlay.setFillColor(Color(0, 0, 0, 100));
      window.draw(overlay);

      FloatRect bounds = titleText.getLocalBounds();
      titleText.setOrigin(bounds.left + bounds.width / 2, bounds.top);
      titleText.setPosition(windowWidth / 2, 40);
      window.draw(titleText);

      if (!selectedOptions.empty()) {
        Text selectionText;
        selectionText.setFont(*font);
        selectionText.setCharacterSize(24);
        selectionText.setFillColor(Color::White);

        string info = "Selected: ";
        for (size_t i = 0; i < selectedOptions.size(); i++) {
          info += selectedOptions[i];
          if (i < selectedOptions.size() - 1) info += ", ";
        }
        selectionText.setString(info);
        selectionText.setPosition(20, 100);
        window.draw(selectionText);
      }

      for (auto btn : buttons) {
        btn->draw(window);
      }
    }
  }

  int checkClick(RenderWindow& window, Event& event,
                 const vector<string>& options) {
    if (!isVisible) return -1;

    for (int i = 0; i < buttons.size(); i++) {
      if (buttons[i]->isClicked(window, event)) {
        string buttonText = buttons[i]->text.getString();

        if (!options.empty() && i < options.size()) {
          toggleOption(options[i]);
          return i;
        }

        if (buttonText == "Select All") {
          return -2;
        }
        if (buttonText == "Clear All") {
          return -3;
        }
        if (buttonText == "Done") {
          return -4;
        }

        if (buttonText == "Based on Company") {
          return 0;
        }
        if (buttonText == "Based on Weather Conditions") {
          return 1;
        }
        if (buttonText == "Back") {
          return 3;
        }

        return i;
      }
    }
    return -1;
  }

  bool isOptionSelected(const string& option) const {
    return find(selectedOptions.begin(), selectedOptions.end(), option) !=
           selectedOptions.end();
  }

  vector<string> getSelectedOptions() const { return selectedOptions; }
  SubgraphMode getCurrentMode() const { return currentMode; }
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
  InfoWindow(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        scrollOffset(0),
        maxScrollOffset(0),
        windowWidth(winWidth),
        windowHeight(winHeight) {
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

    closeButton = new Button(Vector2f(infoX + infoWidth - 45, infoY + 5),
                             Vector2f(40, 40), "X", f);
  }

  ~InfoWindow() { delete closeButton; }

  void show(const string& portName, const vector<string>& info) {
    isVisible = true;
    scrollOffset = 0;
    titleText.setString(portName + " - Information");

    contentLines.clear();
    float infoWidth = windowWidth * 0.5f;
    float infoHeight = windowHeight * 0.7f;
    float infoX = (windowWidth - infoWidth) / 2;
    float infoY = (windowHeight - infoHeight) / 2;
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
    float visibleHeight = infoHeight - 80;
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
    if (!isVisible) return;

    float infoWidth = windowWidth * 0.5f;
    float infoHeight = windowHeight * 0.7f;
    float infoX = (windowWidth - infoWidth) / 2;
    float infoY = (windowHeight - infoHeight) / 2;

    background.setSize(Vector2f(infoWidth, infoHeight));
    background.setPosition(infoX, infoY);

    titleBar.setSize(Vector2f(infoWidth, 50));
    titleBar.setPosition(infoX, infoY);

    titleText.setPosition(infoX + 10, infoY + 13);

    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);
    closeButton->draw(window);

    float visibleTop = infoY + 60;
    float visibleBottom = infoY + infoHeight - 20;

    for (auto& line : contentLines) {
      Vector2f orig = line.getPosition();
      float newY = orig.y - scrollOffset;

      if (newY >= visibleTop && newY <= visibleBottom) {
        line.setPosition(orig.x, newY);
        window.draw(line);
        line.setPosition(orig);
      }
    }

    if (maxScrollOffset > 0) {
      float trackX = infoX + infoWidth - 16;
      float trackY = infoY + 60;
      float trackH = infoHeight - 80;

      RectangleShape track(Vector2f(8, trackH));
      track.setPosition(trackX, trackY);
      track.setFillColor(Color(200, 200, 200));
      track.setOutlineColor(Color(120, 120, 120));
      track.setOutlineThickness(1);
      window.draw(track);

      float ratio = trackH / (trackH + maxScrollOffset);
      float thumbH = max(35.0f, trackH * ratio);

      float thumbY =
          trackY + (scrollOffset * (trackH - thumbH) / maxScrollOffset);

      RectangleShape thumb(Vector2f(8, thumbH));
      thumb.setPosition(trackX, thumbY);
      thumb.setFillColor(Color(100, 100, 100));
      window.draw(thumb);
    }
  }

  bool visible() const { return isVisible; }
  void handleResize(float newWidth, float newHeight) {
    windowWidth = newWidth;
    windowHeight = newHeight;
    float width = windowWidth * 0.45f;
    float height = windowHeight * 0.70f;
    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    titleBar.setSize(Vector2f(width, 50));
    titleBar.setPosition(x, y);
    titleText.setPosition(x + 10, y + 13);

    if (closeButton) {
      closeButton->setPosition(Vector2f(x + width - 45, y + 5));
    }
  }
  bool isMouseOverWindow(RenderWindow& window) const {
    if (!isVisible) return false;
    Vector2i mousePos = Mouse::getPosition(window);
    float infoWidth = windowWidth * 0.5f;
    float infoHeight = windowHeight * 0.7f;
    float infoX = (windowWidth - infoWidth) / 2;
    float infoY = (windowHeight - infoHeight) / 2;
    FloatRect bounds(infoX, infoY, infoWidth, infoHeight);
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
    label.setOrigin(textBounds.left + textBounds.width / 2,
                    textBounds.top + textBounds.height);
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
  bool isAllowed() const {
    Color color = pin.getFillColor();
    return (color.r > 200 && color.g > 200 && color.b < 100);
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
  NavigationMenu(Font& font, float winWidth, float winHeight)
      : isVisible(false), windowWidth(winWidth), windowHeight(winHeight) {
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

    buttons.push_back(new Button(Vector2f(centerX, startY),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Search for Routes", font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Book Cargo Routes", font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 2),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Display Ports Info", font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 3),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Shortest Route", font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 4),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Cheapest Route", font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 5),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Generate Subgraphs", font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 6),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Process Layovers", font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 7),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Track Multi-Leg Journeys", font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 8.5f),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Back to Main Menu", font));
  }

  ~NavigationMenu() {
    for (auto btn : buttons) {
      delete btn;
    }
  }

  void show() { isVisible = true; }

  void hide() { isVisible = false; }

  bool visible() const { return isVisible; }

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
  void handleResize(float newWidth, float newHeight) {
    windowWidth = newWidth;
    windowHeight = newHeight;

    FloatRect textBounds = titleText.getLocalBounds();
    titleText.setOrigin(textBounds.left + textBounds.width / 2.0f,
                        textBounds.top + textBounds.height / 2.0f);
    titleText.setPosition(windowWidth / 2, windowHeight * 0.08f);

    float buttonWidth = 400;
    float buttonHeight = 70;
    float centerX = (windowWidth - buttonWidth) / 2;
    float startY = windowHeight * 0.18f;
    float spacing = buttonHeight + 15;

    for (int i = 0; i < buttons.size(); i++) {
      float yPos = startY + (i * spacing);
      if (i == buttons.size() - 1) {
        yPos = startY + (8.5f * spacing);
      }
      buttons[i]->setPosition(Vector2f(centerX, yPos));
    }
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
class FilterSidebar {
 private:
  RectangleShape background;
  RectangleShape header;
  Text headerText;
  vector<Button*> filterButtons;
  Font* font;
  bool isVisible;
  bool isExpanded;
  float sidebarWidth;
  float sidebarHeight;
  float windowWidth;
  float windowHeight;

  Button* toggleButton;

  Text companiesHeader;
  Text portsHeader;
  Text timeHeader;

  vector<Text> selectionTexts;

 public:
  FilterSidebar(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        isExpanded(true),
        windowWidth(winWidth),
        windowHeight(winHeight) {
    sidebarWidth = 280;
    sidebarHeight = winHeight;

    background.setSize(Vector2f(sidebarWidth, sidebarHeight));
    background.setPosition(0, 0);
    background.setFillColor(Color(40, 40, 40, 230));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(100, 100, 100));

    header.setSize(Vector2f(sidebarWidth, 60));
    header.setPosition(0, 0);
    header.setFillColor(Color(70, 130, 180));

    headerText.setFont(f);
    headerText.setString("Filter Options");
    headerText.setCharacterSize(24);
    headerText.setFillColor(Color::White);
    headerText.setPosition(15, 18);

    toggleButton =
        new Button(Vector2f(sidebarWidth - 50, 10), Vector2f(40, 40), "<", f);

    setupButtons();
  }

  ~FilterSidebar() {
    delete toggleButton;
    for (auto btn : filterButtons) {
      delete btn;
    }
  }

  void setupButtons() {
    for (auto btn : filterButtons) {
      delete btn;
    }
    filterButtons.clear();

    float buttonWidth = windowWidth * 0.6f;
    float buttonHeight = windowHeight * 0.1f;
    float startX = background.getPosition().x + windowWidth * 0.05f;
    float startY = background.getPosition().y + windowHeight * 0.15f;
    float spacing = buttonHeight * 0.3f;

    filterButtons.push_back(new Button(Vector2f(startX, startY),
                                       Vector2f(buttonWidth, buttonHeight),
                                       "Select Companies", *font));

    filterButtons.push_back(
        new Button(Vector2f(startX, startY + (buttonHeight + spacing)),
                   Vector2f(buttonWidth, buttonHeight), "Avoid Ports", *font));

    filterButtons.push_back(new Button(
        Vector2f(startX, startY + 2 * (buttonHeight + spacing)),
        Vector2f(buttonWidth, buttonHeight), "Max Voyage Time", *font));

    filterButtons.push_back(new Button(
        Vector2f(startX, startY + 3 * (buttonHeight + spacing) + 20),
        Vector2f(buttonWidth, buttonHeight), "Apply Filters", *font));

    filterButtons.push_back(new Button(
        Vector2f(startX, startY + 4 * (buttonHeight + spacing) + 20),
        Vector2f(buttonWidth, buttonHeight), "Clear Filters", *font));
  }

  void show() { isVisible = true; }

  void hide() { isVisible = false; }

  bool visible() const { return isVisible; }

  void toggleExpand() {
    isExpanded = !isExpanded;
    if (isExpanded) {
      toggleButton->setText("<");
    } else {
      toggleButton->setText(">");
    }
  }

  void update(RenderWindow& window) {
    if (!isVisible) return;

    toggleButton->update(window);

    if (isExpanded) {
      for (auto btn : filterButtons) {
        btn->update(window);
      }
    }
  }

  void draw(RenderWindow& window, const UserPreferences& prefs) {
    if (!isVisible) return;

    if (isExpanded) {
      window.draw(background);
      window.draw(header);
      window.draw(headerText);

      float textY = 250;

      Text companiesLabel;
      companiesLabel.setFont(*font);
      companiesLabel.setString("Selected Companies:");
      companiesLabel.setCharacterSize(16);
      companiesLabel.setFillColor(Color::White);
      companiesLabel.setPosition(15, textY);
      window.draw(companiesLabel);
      textY += 25;

      if (prefs.hasCompanyFilter && !prefs.preferredCompanies.empty()) {
        for (const string& company : prefs.preferredCompanies) {
          Text companyText;
          companyText.setFont(*font);
          companyText.setString("- " + company);
          companyText.setCharacterSize(14);
          companyText.setFillColor(Color(150, 255, 150));
          companyText.setPosition(20, textY);
          window.draw(companyText);
          textY += 20;
        }
      } else {
        Text noneText;
        noneText.setFont(*font);
        noneText.setString("  None");
        noneText.setCharacterSize(14);
        noneText.setFillColor(Color(200, 200, 200));
        noneText.setPosition(20, textY);
        window.draw(noneText);
        textY += 20;
      }

      textY += 15;

      Text portsLabel;
      portsLabel.setFont(*font);
      portsLabel.setString("Avoided Ports:");
      portsLabel.setCharacterSize(16);
      portsLabel.setFillColor(Color::White);
      portsLabel.setPosition(15, textY);
      window.draw(portsLabel);
      textY += 25;

      if (prefs.hasPortFilter && !prefs.avoidPorts.empty()) {
        for (const string& port : prefs.avoidPorts) {
          Text portText;
          portText.setFont(*font);
          portText.setString("- " + port);
          portText.setCharacterSize(14);
          portText.setFillColor(Color(255, 150, 150));
          portText.setPosition(20, textY);
          window.draw(portText);
          textY += 20;
        }
      } else {
        Text noneText;
        noneText.setFont(*font);
        noneText.setString("  None");
        noneText.setCharacterSize(14);
        noneText.setFillColor(Color(200, 200, 200));
        noneText.setPosition(20, textY);
        window.draw(noneText);
        textY += 20;
      }

      textY += 15;

      Text timeLabel;
      timeLabel.setFont(*font);
      timeLabel.setString("Max Voyage Time:");
      timeLabel.setCharacterSize(16);
      timeLabel.setFillColor(Color::White);
      timeLabel.setPosition(15, textY);
      window.draw(timeLabel);
      textY += 25;

      Text timeText;
      timeText.setFont(*font);
      if (prefs.hasTimeFilter) {
        int hours = prefs.maxVoyageTime / 60;
        timeText.setString("  " + to_string(hours) + " hours");
        timeText.setFillColor(Color(150, 200, 255));
      } else {
        timeText.setString("  No limit");
        timeText.setFillColor(Color(200, 200, 200));
      }
      timeText.setCharacterSize(14);
      timeText.setPosition(20, textY);
      window.draw(timeText);

      for (auto btn : filterButtons) {
        btn->draw(window);
      }
    } else {
      RectangleShape collapsedBar(Vector2f(50, sidebarHeight));
      collapsedBar.setPosition(0, 0);
      collapsedBar.setFillColor(Color(40, 40, 40, 230));
      collapsedBar.setOutlineThickness(3);
      collapsedBar.setOutlineColor(Color(100, 100, 100));
      window.draw(collapsedBar);

      Text verticalText;
      verticalText.setFont(*font);
      verticalText.setString("FILTERS");
      verticalText.setCharacterSize(18);
      verticalText.setFillColor(Color::White);
      verticalText.setRotation(90);
      verticalText.setPosition(30, 100);
      window.draw(verticalText);
    }

    toggleButton->draw(window);
  }

  int checkClick(RenderWindow& window, Event& event) {
    if (!isVisible) return -1;

    if (toggleButton->isClicked(window, event)) {
      toggleExpand();
      return -2;
    }

    if (!isExpanded) return -1;

    for (int i = 0; i < filterButtons.size(); i++) {
      if (filterButtons[i]->isClicked(window, event)) {
        return i;
      }
    }

    return -1;
  }

  bool isMouseOverSidebar(RenderWindow& window) const {
    if (!isVisible) return false;

    Vector2i mousePos = Mouse::getPosition(window);
    float width = isExpanded ? sidebarWidth : 50;

    return (mousePos.x >= 0 && mousePos.x <= width && mousePos.y >= 0 &&
            mousePos.y <= sidebarHeight);
  }
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
    Vector2f direction = end - start;
    float length = sqrt(direction.x * direction.x + direction.y * direction.y);
    float angle = atan2(direction.y, direction.x) * 180 / 3.14159f;
    line.setSize(Vector2f(length, 2.5));
    line.setPosition(start);
    line.setRotation(angle);

    int costLevel = min(routeInfo.cost / 100, 255);
    normalColor = Color(255, 50, 50, 255);
    hoverColor = Color(255, 165, 0, 255);
    line.setFillColor(normalColor);
  }

  bool isMouseOver(RenderWindow& window) {
    Vector2i mousePos = Mouse::getPosition(window);
    Vector2f mousePosF = static_cast<Vector2f>(mousePos);
    Vector2f dir = end - start;
    float lineLength = sqrt(dir.x * dir.x + dir.y * dir.y);

    if (lineLength == 0) return false;
    Vector2f toMouse = mousePosF - start;
    float t =
        (toMouse.x * dir.x + toMouse.y * dir.y) / (lineLength * lineLength);
    t = max(0.0f, min(1.0f, t));

    Vector2f projection = start + dir * t;
    Vector2f diff = mousePosF - projection;
    float distance = sqrt(diff.x * diff.x + diff.y * diff.y);

    return distance < 15.0f;
  }

  void update(RenderWindow& window) { isHovered = isMouseOver(window); }

  bool isClicked(RenderWindow& window, Event& event) {
    if (event.type == Event::MouseButtonReleased &&
        event.mouseButton.button == Mouse::Left) {
      return isMouseOver(window);
    }
    return false;
  }
  bool isAllowed() const { return line.getFillColor().a > 150; }
  void setSelected(bool selected) {
    if (selected) {
      line.setFillColor(hoverColor);
    } else {
      line.setFillColor(normalColor);
    }
  }

  void draw(RenderWindow& window) { window.draw(line); }

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

    float screenWidth = windowSize.x;
    float screenHeight = windowSize.y;
    float margin = 80;

    vector<pair<Vector2f, Vector2f>> safeZones = {
        {Vector2f(margin, margin),
         Vector2f(screenWidth * 0.25f, screenHeight * 0.25f)},
        {Vector2f(screenWidth * 0.35f, margin),
         Vector2f(screenWidth * 0.65f, screenHeight * 0.25f)},
        {Vector2f(screenWidth * 0.75f, margin),
         Vector2f(screenWidth - margin, screenHeight * 0.25f)},

        {Vector2f(margin, screenHeight * 0.30f),
         Vector2f(screenWidth * 0.25f, screenHeight * 0.50f)},
        {Vector2f(screenWidth * 0.35f, screenHeight * 0.30f),
         Vector2f(screenWidth * 0.65f, screenHeight * 0.50f)},
        {Vector2f(screenWidth * 0.75f, screenHeight * 0.30f),
         Vector2f(screenWidth - margin, screenHeight * 0.50f)},

        {Vector2f(margin, screenHeight * 0.55f),
         Vector2f(screenWidth * 0.25f, screenHeight * 0.75f)},
        {Vector2f(screenWidth * 0.35f, screenHeight * 0.55f),
         Vector2f(screenWidth * 0.65f, screenHeight * 0.75f)},
        {Vector2f(screenWidth * 0.75f, screenHeight * 0.55f),
         Vector2f(screenWidth - margin, screenHeight * 0.75f)},

        {Vector2f(margin, screenHeight * 0.78f),
         Vector2f(screenWidth * 0.30f, screenHeight - margin)},
        {Vector2f(screenWidth * 0.35f, screenHeight * 0.78f),
         Vector2f(screenWidth * 0.65f, screenHeight - margin)},
        {Vector2f(screenWidth * 0.70f, screenHeight * 0.78f),
         Vector2f(screenWidth - margin, screenHeight - margin)}};

    int portsPerZone = (numPorts + safeZones.size() - 1) / safeZones.size();

    for (int i = 0; i < numPorts; i++) {
      int zoneIdx = i / portsPerZone;
      if (zoneIdx >= safeZones.size()) zoneIdx = safeZones.size() - 1;

      int portInZone = i % portsPerZone;

      Vector2f zoneMin = safeZones[zoneIdx].first;
      Vector2f zoneMax = safeZones[zoneIdx].second;

      int cols = min(3, portsPerZone);
      int col = portInZone % cols;
      int row = portInZone / cols;

      float zoneWidth = zoneMax.x - zoneMin.x;
      float zoneHeight = zoneMax.y - zoneMin.y;

      float x = zoneMin.x + (col * zoneWidth / max(1.0f, (float)(cols - 1)));
      float y = zoneMin.y +
                (row * zoneHeight /
                 max(1.0f, (float)((portsPerZone + cols - 1) / cols - 1)));

      if (portsPerZone > 1) {
        x += (i % 5 - 2) * 15;
        y += ((i * 3) % 5 - 2) * 15;
      }

      if (x < zoneMin.x) x = zoneMin.x;
      if (x > zoneMax.x) x = zoneMax.x;
      if (y < zoneMin.y) y = zoneMin.y;
      if (y > zoneMax.y) y = zoneMax.y;

      locations.push_back(Location(graph.ports[i].name, Vector2f(x, y), font));
    }

    for (int i = 0; i < graph.ports.size(); i++) {
      string sourceName = graph.ports[i].name;
      Vector2f sourcePos = locations[i].position;

      for (const Route& route : graph.routes[i]) {
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

  info.push_back("========== WEATHER CONDITIONS ==========");
  info.push_back("");
  if (!graph.ports[portIdx].weatherConditions.empty()) {
    for (const string& weather : graph.ports[portIdx].weatherConditions) {
      info.push_back(weather);
    }
  } else {
    info.push_back("No weather data available");
  }
  info.push_back("");

  info.push_back("========== PORT STATISTICS ==========");
  info.push_back("");

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
  info.push_back("Total Connections: " +
                 to_string(outgoingRoutes + incomingRoutes));
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
      info.push_back("Route " + to_string(i + 1) + ":");
      info.push_back("  Destination: " + r.destination);
      info.push_back("  Date: " + r.date);
      info.push_back("  Departure: " + r.depTime + " -> Arrival: " + r.arrTime);
      info.push_back("  Cost: $" + to_string(r.cost));
      info.push_back("  Company: " + r.company);
      info.push_back("  Travel Time: " + to_string(r.travelTime / 60) + "h " +
                     to_string(r.travelTime % 60) + "m");
    }
  }

  return info;
}
void showSettings() {}
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
  FilterPreferencesMenu(Font& f, float winWidth, float winHeight,
                        Texture& oceanTex)
      : font(&f),
        isVisible(false),
        windowWidth(winWidth),
        windowHeight(winHeight),
        oceanTexture(&oceanTex) {
    titleText.setFont(f);
    titleText.setCharacterSize(48);
    titleText.setFillColor(Color::White);
    titleText.setOutlineThickness(2);
    titleText.setOutlineColor(Color::Black);

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

    buttons.push_back(new Button(Vector2f(centerX, startY),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Shipping Companies", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Avoid Specific Ports", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 2),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Voyage Time", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 3),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Custom Preferences", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 4.5f),
                                 Vector2f(buttonWidth, buttonHeight), "Back",
                                 *font));
  }
  void createTimeMenuForCustom() {
    clearButtons();
    titleText.setString("Set Maximum Voyage Time (in hours)");

    float buttonWidth = 300;
    float buttonHeight = 60;
    float centerX = (windowWidth - buttonWidth) / 2;
    float startY = windowHeight * 0.2f;
    float spacing = buttonHeight + 15;

    buttons.push_back(new Button(Vector2f(centerX, startY),
                                 Vector2f(buttonWidth, buttonHeight), "8 hours",
                                 *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "16 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 2),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "24 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 3),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "48 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 4),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "No Limit", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 5.5f),
                                 Vector2f(buttonWidth, buttonHeight), "Done",
                                 *font));
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

    int cols = 4;
    int col = 0;
    int row = 0;

    for (const string& company : companies) {
      float x = startX + (col * spacingX);
      float y = startY + (row * spacingY);

      buttons.push_back(new Button(
          Vector2f(x, y), Vector2f(buttonWidth, buttonHeight), company, *font));

      col++;
      if (col >= cols) {
        col = 0;
        row++;
      }
    }

    float doneX = (windowWidth - 250) / 2;
    float doneY = startY + ((row + 1) * spacingY) + 30;
    buttons.push_back(
        new Button(Vector2f(doneX, doneY), Vector2f(250, 60), "Done", *font));
  }

  void createPortMenu(const vector<Port>& ports) {
    clearButtons();
    titleText.setString("Select Ports to Avoid");

    float buttonWidth = 250;
    float buttonHeight = 60;
    float startX = 100;
    float startY = windowHeight * 0.15f;
    float spacingX = buttonWidth + 30;
    float spacingY = buttonHeight + 10;

    int cols = 4;
    int col = 0;
    int row = 0;

    for (const Port& port : ports) {
      float x = startX + (col * spacingX);
      float y = startY + (row * spacingY);

      buttons.push_back(new Button(Vector2f(x, y),
                                   Vector2f(buttonWidth, buttonHeight),
                                   port.name, *font));

      col++;
      if (col >= cols) {
        col = 0;
        row++;
      }
    }

    float doneX = (windowWidth - 250) / 2;
    float doneY = startY + ((row + 1) * spacingY);
    buttons.push_back(
        new Button(Vector2f(doneX, doneY), Vector2f(250, 60), "Done", *font));
  }
  void createTimeMenu() {
    clearButtons();
    titleText.setString("Set Maximum Voyage Time (in hours)");

    float buttonWidth = 300;
    float buttonHeight = 60;
    float centerX = (windowWidth - buttonWidth) / 2;
    float startY = windowHeight * 0.3f;
    float spacing = buttonHeight + 20;

    buttons.push_back(new Button(Vector2f(centerX, startY),
                                 Vector2f(buttonWidth, buttonHeight), "8 hours",
                                 *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "16 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 2),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "24 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 3),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "48 hours", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 4),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "No Limit", *font));
    buttons.push_back(new Button(Vector2f(centerX, startY + spacing * 5.5f),
                                 Vector2f(buttonWidth, buttonHeight), "Back",
                                 *font));
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

  bool visible() const { return isVisible; }

  void update(RenderWindow& window) {
    if (isVisible) {
      for (auto btn : buttons) {
        btn->update(window);
      }
    }
  }

  void draw(RenderWindow& window) {
    if (isVisible) {
      window.draw(oceanBackground);
      RectangleShape overlay(Vector2f(windowWidth, windowHeight));
      overlay.setFillColor(Color(0, 0, 0, 100));
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

  int getButtonCount() const { return buttons.size(); }
};
void customShipPreferences(const UserPreferences& prefs,
                           vector<Location>& locations,
                           vector<RouteEdge>& edges, const Graph& g) {
  cout << "\n=== Applying Custom Ship Preferences ===" << endl;

  for (int i = 0; i < locations.size(); i++) {
    bool isAvoidPort = g.portIsAvoid(locations[i].name, prefs);

    if (isAvoidPort) {
      locations[i].pin.setFillColor(Color(150, 150, 150));
      locations[i].pin.setOutlineColor(Color(100, 100, 100));
      locations[i].label.setFillColor(Color(100, 100, 100));
      cout << "Port " << locations[i].name << " marked as AVOID (Grey)" << endl;
    } else {
      locations[i].pin.setFillColor(Color(0, 255, 0));
      locations[i].pin.setOutlineColor(Color(0, 180, 0));
      locations[i].label.setFillColor(Color::White);
      cout << "Port " << locations[i].name << " marked as ALLOWED (Green)"
           << endl;
    }
  }

  int allowedRoutes = 0;
  int blockedRoutes = 0;

  for (auto& edge : edges) {
    bool sourceAvoid = g.portIsAvoid(edge.sourceName, prefs);
    bool destAvoid = g.portIsAvoid(edge.routeInfo.destination, prefs);

    if (sourceAvoid || destAvoid) {
      edge.line.setFillColor(Color(150, 150, 150, 100));
      blockedRoutes++;
      cout << "Route blocked: " << edge.sourceName << " -> "
           << edge.routeInfo.destination << " (Avoid port)" << endl;
    } else if (g.routeMatchesPreferences(edge.routeInfo, prefs)) {
      edge.line.setFillColor(Color(0, 255, 0, 255));
      allowedRoutes++;
      cout << "Route allowed: " << edge.sourceName << " -> "
           << edge.routeInfo.destination << " (Matches preferences)" << endl;
    } else {
      edge.line.setFillColor(Color(150, 150, 150, 100));
      blockedRoutes++;
      cout << "Route not preferred: " << edge.sourceName << " -> "
           << edge.routeInfo.destination << " (Different company/time)" << endl;
    }
  }

  cout << "\nRoute Summary:" << endl;
  cout << "Allowed routes: " << allowedRoutes << endl;
  cout << "Blocked/Not preferred routes: " << blockedRoutes << endl;

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
         << (prefs.maxVoyageTime / 60) << "h " << (prefs.maxVoyageTime % 60)
         << "m)" << endl;
  }

  cout << "======================================\n" << endl;
}

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
  RouteDisplayWindow(Font& f, float windowWidth, float windowHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        scrollOffset(0),
        maxScrollOffset(0),
        windowWidth(windowWidth),
        windowHeight(windowHeight) {
    float width = windowWidth * 0.45f;
    float height = windowHeight * 0.70f;

    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(Color(240, 240, 240, 225));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(50, 50, 50));

    titleBar.setSize(Vector2f(width, 50));
    titleBar.setPosition(x, y);
    titleBar.setFillColor(Color(70, 130, 180));

    titleText.setString("Available Routes");
    titleText.setFont(f);
    titleText.setCharacterSize(24);
    titleText.setFillColor(Color::White);
    titleText.setPosition(x + 10, y + 13);

    closeButton =
        new Button(Vector2f(x + width - 45, y + 5), Vector2f(40, 40), "X", f);
  }

  ~RouteDisplayWindow() { delete closeButton; }

  void show(const string& from, const string& to,
            const vector<string>& routes) {
    isVisible = true;
    scrollOffset = 0;
    routeLines.clear();
    routeBoxes.clear();

    titleText.setString("Routes: " + from + " -> " + to);

    float windowLeft = background.getPosition().x;
    float windowTop = background.getPosition().y;
    float windowWidth = background.getSize().x;
    float windowHeight = background.getSize().y;

    float leftX = windowLeft + 20;
    float yPos = windowTop + 70;

    if (routes.empty()) {
      Text text;
      text.setFont(*font);
      text.setString("No routes found between these ports.");
      text.setCharacterSize(22);
      text.setFillColor(Color(200, 50, 50));
      text.setOutlineThickness(1);
      text.setOutlineColor(Color::White);

      FloatRect bounds = text.getLocalBounds();
      text.setOrigin(bounds.left + bounds.width / 2,
                     bounds.top + bounds.height / 2);
      text.setPosition(windowLeft + windowWidth / 2,
                       windowTop + windowHeight / 2);

      routeLines.push_back(text);
    } else {
      for (size_t i = 0; i < routes.size(); i++) {
        Text text;
        text.setFont(*font);
        text.setString(routes[i]);
        text.setCharacterSize(17);

        const string& line = routes[i];

        if (line.find("===") != string::npos ||
            line.find("DIRECT") != string::npos ||
            line.find("CONNECTED") != string::npos ||
            line.find("MULTI-LAYOVER") != string::npos) {
          text.setFillColor(Color(50, 100, 150));
          text.setStyle(Text::Bold);
          text.setCharacterSize(19);
        } else if (line.find("->") != string::npos) {
          text.setFillColor(Color(0, 120, 0));
          text.setStyle(Text::Bold);
        } else if (line.empty()) {
          text.setCharacterSize(8);
        } else {
          text.setFillColor(Color::Black);
        }

        text.setPosition(leftX, yPos);
        routeLines.push_back(text);

        if (line.empty() || line.find("===") != string::npos)
          yPos += 15;
        else
          yPos += 25;
      }
    }

    float contentHeight = yPos - (windowTop + 70);
    float visibleHeight = windowHeight - 90;

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
    if (!isVisible) return;

    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);
    closeButton->draw(window);

    float boxX = background.getPosition().x;
    float boxY = background.getPosition().y;
    float boxW = background.getSize().x;
    float boxH = background.getSize().y;

    float visibleTop = boxY + 60;
    float visibleBottom = boxY + boxH - 20;

    for (auto& line : routeLines) {
      Vector2f orig = line.getPosition();
      float newY = orig.y - scrollOffset;

      if (newY >= visibleTop && newY <= visibleBottom) {
        string lineStr = line.getString();
        line.setFillColor(Color::Black);
        line.setPosition(orig.x, newY);
        window.draw(line);
        line.setPosition(orig);
        line.setStyle(Text::Regular);
      }
    }

    if (maxScrollOffset > 0) {
      float trackX = boxX + boxW - 18;
      float trackY = boxY + 60;
      float trackH = boxH - 80;

      RectangleShape track(Vector2f(8, trackH));
      track.setPosition(trackX, trackY);
      track.setFillColor(Color(200, 200, 200));
      track.setOutlineThickness(1);
      track.setOutlineColor(Color(120, 120, 120));
      window.draw(track);

      float ratio = trackH / (trackH + maxScrollOffset);
      float thumbH = max(40.0f, trackH * ratio);

      float thumbY;
      if (maxScrollOffset == 0)
        thumbY = trackY;
      else
        thumbY = trackY + (scrollOffset * (trackH - thumbH) / maxScrollOffset);

      RectangleShape thumb(Vector2f(8, thumbH));
      thumb.setPosition(trackX, thumbY);
      thumb.setFillColor(Color(100, 100, 100));
      window.draw(thumb);
    }
  }

  bool visible() const { return isVisible; }

  bool isMouseOverWindow(RenderWindow& window) const {
    if (!isVisible) return false;
    Vector2i mousePos = Mouse::getPosition(window);
    FloatRect bounds = background.getGlobalBounds();
    return bounds.contains(static_cast<Vector2f>(mousePos));
  }
  void handleResize(float newWidth, float newHeight) {
    windowWidth = newWidth;
    windowHeight = newHeight;
    float width = windowWidth * 0.45f;
    float height = windowHeight * 0.70f;
    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    titleBar.setSize(Vector2f(width, 50));
    titleBar.setPosition(x, y);
    titleText.setPosition(x + 10, y + 13);

    if (closeButton) {
      closeButton->setPosition(Vector2f(x + width - 45, y + 5));
    }
  }
};
class CompanySelectionWindow {
 private:
  RectangleShape background;
  RectangleShape titleBar;
  Text titleText;
  vector<Button*> companyButtons;
  Button* doneButton;
  Button* closeButton;
  bool isVisible;
  Font* font;
  float windowWidth;
  float windowHeight;
  vector<string> selectedCompanies;
  vector<string> currentCompanies;
  vector<string> currentSelections;

 public:
  CompanySelectionWindow(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        doneButton(nullptr),
        windowWidth(winWidth),
        windowHeight(winHeight) {
    float width = 900;
    float height = 650;

    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(Color(50, 50, 50, 240));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(100, 100, 100));

    titleBar.setSize(Vector2f(width, 60));
    titleBar.setPosition(x, y);
    titleBar.setFillColor(Color(70, 130, 180));

    titleText.setFont(f);
    titleText.setString("Select Shipping Companies");
    titleText.setCharacterSize(26);
    titleText.setFillColor(Color::White);
    titleText.setPosition(x + 15, y + 17);

    closeButton =
        new Button(Vector2f(x + width - 50, y + 10), Vector2f(40, 40), "X", f);
  }

  ~CompanySelectionWindow() {
    delete closeButton;
    delete doneButton;
    for (auto btn : companyButtons) delete btn;
  }
  void handleResize(float newWidth, float newHeight) {
    windowWidth = newWidth;
    windowHeight = newHeight;

    if (isVisible) {
      vector<string> tempSelections = selectedCompanies;
      vector<string> tempCompanies = currentCompanies;

      show(tempCompanies, tempSelections);
    }
  }
  void show(const vector<string>& companies,
            const vector<string>& currentlySelected) {
    isVisible = true;
    selectedCompanies = currentlySelected;
    currentSelections = currentlySelected;
    currentCompanies = companies;

    for (auto btn : companyButtons) delete btn;
    companyButtons.clear();
    delete doneButton;

    float popupWidth = 900;
    float popupHeight = 650;

    float x = (windowWidth - popupWidth) / 2;
    float y = (windowHeight - popupHeight) / 2;

    background.setSize(Vector2f(popupWidth, popupHeight));
    background.setPosition(x, y);
    titleBar.setSize(Vector2f(popupWidth, 60));
    titleBar.setPosition(x, y);
    titleText.setPosition(x + 15, y + 17);

    if (closeButton) {
      closeButton->setPosition(Vector2f(x + popupWidth - 50, y + 10));
    }

    float paddingX = 40;
    float paddingTop = 90;

    float buttonWidth = 200;
    float buttonHeight = 55;
    float spacing = 15;
    int cols = 4;

    float startX = x + paddingX;
    float startY = y + paddingTop;

    int col = 0, row = 0;

    for (const string& company : companies) {
      float bx = startX + col * (buttonWidth + spacing);
      float by = startY + row * (buttonHeight + spacing);

      companyButtons.push_back(new Button(Vector2f(bx, by),
                                          Vector2f(buttonWidth, buttonHeight),
                                          company, *font));

      if (++col >= cols) {
        col = 0;
        row++;
      }
    }

    float textY = startY + row * (buttonHeight + spacing) + 30;

    doneButton = new Button(Vector2f(x + popupWidth / 2 - 125, textY + 40),
                            Vector2f(250, 60), "Done", *font);
  }
  void hide() { isVisible = false; }
  bool visible() const { return isVisible; }
  void update(RenderWindow& window) {
    if (!isVisible) return;

    closeButton->update(window);
    doneButton->update(window);
    for (auto btn : companyButtons) btn->update(window);
  }

  void draw(RenderWindow& window, const vector<string>& companies) {
    if (!isVisible) return;

    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);

    float popupWidth = background.getSize().x;
    float popupHeight = background.getSize().y;
    float x = background.getPosition().x;
    float y = background.getPosition().y;

    float paddingX = 40;
    float paddingTop = 90;

    float buttonWidth = 200;
    float buttonHeight = 55;
    float spacing = 15;
    int cols = 4;

    float startX = x + paddingX;
    float startY = y + paddingTop;

    int row = companyButtons.size() / cols;

    for (size_t i = 0; i < companyButtons.size(); i++) {
      bool selected = (find(selectedCompanies.begin(), selectedCompanies.end(),
                            currentCompanies[i]) != selectedCompanies.end());

      if (selected) {
        RectangleShape highlight;
        highlight.setSize(companyButtons[i]->shape.getSize() + Vector2f(8, 8));
        highlight.setPosition(companyButtons[i]->shape.getPosition() -
                              Vector2f(4, 4));
        highlight.setFillColor(Color::Transparent);
        highlight.setOutlineThickness(4);
        highlight.setOutlineColor(Color(0, 255, 0));
        window.draw(highlight);
      }

      companyButtons[i]->draw(window);
    }

    Text selectionText;
    selectionText.setFont(*font);
    selectionText.setCharacterSize(20);
    selectionText.setFillColor(Color::White);
    selectionText.setString("Selected: " + to_string(selectedCompanies.size()) +
                            " companies");

    float textY = startY + (row + 1) * (buttonHeight + spacing) + 10;
    selectionText.setPosition(startX, textY);
    window.draw(selectionText);

    if (doneButton) doneButton->draw(window);
    if (closeButton) closeButton->draw(window);
  }

  int checkClick(RenderWindow& window, Event& event,
                 const vector<string>& companies) {
    if (!isVisible) return -1;

    if (closeButton && closeButton->isClicked(window, event)) {
      hide();
      return -2;
    }
    if (doneButton && doneButton->isClicked(window, event)) {
      return -3;
    }

    for (size_t i = 0; i < companyButtons.size(); i++) {
      if (companyButtons[i]->isClicked(window, event)) {
        auto it = find(selectedCompanies.begin(), selectedCompanies.end(),
                       currentCompanies[i]);
        if (it == selectedCompanies.end())
          selectedCompanies.push_back(currentCompanies[i]);
        else
          selectedCompanies.erase(it);

        currentSelections = selectedCompanies;
        return i;
      }
    }
    return -1;
  }

  vector<string> getSelectedCompanies() const { return selectedCompanies; }
};

class PortAvoidanceWindow {
 private:
  RectangleShape background;
  RectangleShape titleBar;
  Text titleText;
  vector<Button*> portButtons;
  Button* doneButton;
  Button* closeButton;
  bool isVisible;
  Font* font;
  float windowWidth;
  float windowHeight;
  vector<string> avoidedPorts;

 public:
  PortAvoidanceWindow(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        doneButton(nullptr),
        windowWidth(winWidth),
        windowHeight(winHeight) {
    float width = windowWidth * 0.65f;
    float height = windowHeight * 0.70f;

    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(Color(50, 50, 50, 240));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(100, 100, 100));

    titleBar.setSize(Vector2f(width, 60));
    titleBar.setPosition(x, y);
    titleBar.setFillColor(Color(70, 130, 180));

    titleText.setFont(f);
    titleText.setString("Select Ports to Avoid");
    titleText.setCharacterSize(30);
    titleText.setFillColor(Color::White);
    titleText.setPosition(x + 20, y + 15);

    closeButton =
        new Button(Vector2f(x + width - 50, y + 10), Vector2f(40, 40), "X", f);
  }

  ~PortAvoidanceWindow() {
    delete closeButton;
    delete doneButton;
    for (auto btn : portButtons) delete btn;
  }

  void show(const vector<Port>& ports, const vector<string>& currentlyAvoided) {
    isVisible = true;
    avoidedPorts = currentlyAvoided;

    for (auto btn : portButtons) delete btn;
    portButtons.clear();
    delete doneButton;

    int cols = 4;
    int total = ports.size();
    int rows = (total + cols - 1) / cols;

    float buttonHeight = 55;
    float spacingY = 15;
    float topMargin = 120;
    float bottomMargin = 150;

    float height = topMargin + rows * (buttonHeight + spacingY) + bottomMargin;
    float width = windowWidth * 0.65f;

    if (height < 700) height = 700;

    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    titleBar.setSize(Vector2f(width, 60));
    titleBar.setPosition(x, y);
    titleText.setPosition(x + 20, y + 15);
    closeButton->setPosition(Vector2f(x + width - 50, y + 10));

    float leftPadding = 50;
    float buttonWidth = (width - leftPadding * 2 - 3 * 20) / cols;
    float startX = x + leftPadding;
    float startY = y + 80;

    float spacingX = 20;

    int col = 0, row = 0;

    for (const Port& port : ports) {
      float px = startX + col * (buttonWidth + spacingX);
      float py = startY + row * (buttonHeight + spacingY);

      Button* btn =
          new Button(Vector2f(px, py), Vector2f(buttonWidth, buttonHeight),
                     port.name, *font);

      portButtons.push_back(btn);

      col++;
      if (col >= cols) {
        col = 0;
        row++;
      }
    }

    float doneY = startY + rows * (buttonHeight + spacingY) + 40;
    doneButton = new Button(Vector2f(x + (width - 250) / 2, doneY),
                            Vector2f(250, 70), "Done", *font);
  }

  void hide() { isVisible = false; }
  bool visible() const { return isVisible; }

  void update(RenderWindow& window) {
    if (!isVisible) return;
    closeButton->update(window);
    doneButton->update(window);
    for (auto b : portButtons) b->update(window);
  }

  void draw(RenderWindow& window, const vector<Port>& ports) {
    if (!isVisible) return;

    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);

    for (size_t i = 0; i < portButtons.size(); i++) {
      bool isAvoided = (find(avoidedPorts.begin(), avoidedPorts.end(),
                             ports[i].name) != avoidedPorts.end());

      if (isAvoided) {
        RectangleShape outline;
        outline.setSize(portButtons[i]->shape.getSize() + Vector2f(10, 10));
        outline.setPosition(portButtons[i]->shape.getPosition() -
                            Vector2f(5, 5));
        outline.setFillColor(Color::Transparent);
        outline.setOutlineThickness(4);
        outline.setOutlineColor(Color(255, 80, 80));
        window.draw(outline);
      }

      portButtons[i]->draw(window);
    }

    doneButton->draw(window);
    closeButton->draw(window);

    Text status;
    status.setFont(*font);
    status.setCharacterSize(22);
    status.setFillColor(Color::White);
    status.setString("Avoiding: " + to_string(avoidedPorts.size()) + " ports");

    float x = background.getPosition().x;
    float y = background.getPosition().y + background.getSize().y - 60;
    status.setPosition(x + 25, y);

    window.draw(status);
  }

  int checkClick(RenderWindow& window, Event& event,
                 const vector<Port>& ports) {
    if (!isVisible) return -1;

    if (closeButton->isClicked(window, event)) {
      hide();
      return -2;
    }

    if (doneButton->isClicked(window, event)) return -3;

    for (size_t i = 0; i < portButtons.size(); i++) {
      if (portButtons[i]->isClicked(window, event)) {
        string n = ports[i].name;
        auto it = find(avoidedPorts.begin(), avoidedPorts.end(), n);

        if (it == avoidedPorts.end())
          avoidedPorts.push_back(n);
        else
          avoidedPorts.erase(it);

        return i;
      }
    }

    return -1;
  }

  vector<string> getAvoidedPorts() const { return avoidedPorts; }
  void handleResize(float newWidth, float newHeight) {
    windowWidth = newWidth;
    windowHeight = newHeight;
  }
};
class PortAdditionWindow {
 private:
  RectangleShape background;
  RectangleShape titleBar;
  Text titleText;
  Text instructionText;
  vector<Button*> portButtons;
  Button* closeButton;
  bool isVisible;
  Font* font;
  float windowWidth;
  float windowHeight;
  int selectedPortIdx;

  int beforePortIdx;
  int afterPortIdx;
  bool isAddingBefore;
  int selectedJourneyIdx;
  const Graph* graphRef;
  vector<int> currentJourneyPorts;

 public:
  PortAdditionWindow(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        windowWidth(winWidth),
        windowHeight(winHeight),
        selectedPortIdx(-1),
        beforePortIdx(-1),
        afterPortIdx(-1),
        isAddingBefore(true),
        selectedJourneyIdx(-1),
        graphRef(nullptr) {
    float width = windowWidth * 0.7f;
    float height = windowHeight * 0.8f;
    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(Color(50, 50, 50, 240));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(100, 100, 100));

    titleBar.setSize(Vector2f(width, 60));
    titleBar.setPosition(x, y);
    titleBar.setFillColor(Color(70, 130, 180));

    titleText.setFont(f);
    titleText.setString("Select Port to Add");
    titleText.setCharacterSize(30);
    titleText.setFillColor(Color::White);
    titleText.setPosition(x + 20, y + 15);

    instructionText.setFont(f);
    instructionText.setCharacterSize(18);
    instructionText.setFillColor(Color(200, 200, 200));
    instructionText.setPosition(x + 20, y + 80);

    closeButton =
        new Button(Vector2f(x + width - 50, y + 10), Vector2f(40, 40), "X", f);
  }

  ~PortAdditionWindow() {
    delete closeButton;
    for (auto btn : portButtons) delete btn;
  }

  void show(const Graph& g, const vector<int>& journeyPorts,
            int selectedPortInJourney, bool addBefore) {
    isVisible = true;
    graphRef = &g;
    currentJourneyPorts = journeyPorts;
    selectedJourneyIdx = selectedPortInJourney;
    isAddingBefore = addBefore;
    selectedPortIdx = -1;

    if (selectedJourneyIdx >= 0 && selectedJourneyIdx < journeyPorts.size()) {
      int selectedPort = journeyPorts[selectedJourneyIdx];

      if (addBefore) {
        afterPortIdx = selectedPort;
        beforePortIdx = (selectedJourneyIdx > 0)
                            ? journeyPorts[selectedJourneyIdx - 1]
                            : -1;
      } else {
        beforePortIdx = selectedPort;
        afterPortIdx = (selectedJourneyIdx < journeyPorts.size() - 1)
                           ? journeyPorts[selectedJourneyIdx + 1]
                           : -1;
      }
    } else {
      beforePortIdx = -1;
      afterPortIdx = -1;
    }

    string instruction =
        "Green = Can be inserted | Grey = Cannot be inserted\n";
    if (addBefore) {
      instruction +=
          "Adding BEFORE: " + g.ports[journeyPorts[selectedJourneyIdx]].name;
    } else {
      instruction +=
          "Adding AFTER: " + g.ports[journeyPorts[selectedJourneyIdx]].name;
    }

    if (beforePortIdx != -1) {
      instruction += "\nPrevious port: " + g.ports[beforePortIdx].name;
    }
    if (afterPortIdx != -1) {
      instruction += "\nNext port: " + g.ports[afterPortIdx].name;
    }

    instructionText.setString(instruction);

    for (auto btn : portButtons) delete btn;
    portButtons.clear();

    float width = background.getSize().x;
    float height = background.getSize().y;
    float x = background.getPosition().x;
    float y = background.getPosition().y;

    int cols = 4;
    float buttonWidth = (width - 100) / cols;
    float buttonHeight = 55;
    float startX = x + 50;
    float startY = y + 180;
    float spacingX = 20;
    float spacingY = 15;

    int col = 0, row = 0;

    int validPortCount = 0;
    int totalPortCount = 0;

    for (int i = 0; i < g.ports.size(); i++) {
      bool inJourney = false;
      for (int journeyPort : currentJourneyPorts) {
        if (journeyPort == i) {
          inJourney = true;
          break;
        }
      }
      if (inJourney) continue;

      totalPortCount++;

      float px = startX + col * (buttonWidth + spacingX);
      float py = startY + row * (buttonHeight + spacingY);

      Button* btn =
          new Button(Vector2f(px, py), Vector2f(buttonWidth, buttonHeight),
                     g.ports[i].name, *font);

      bool isValid =
          canPortBeInserted(g, i, beforePortIdx, afterPortIdx, addBefore);

      if (isValid) validPortCount++;
      if (isValid) {
        btn->normalColor = Color(0, 150, 0);
        btn->hoverColor = Color(0, 200, 0);
        btn->shape.setFillColor(btn->normalColor);
      } else {
        btn->normalColor = Color(100, 100, 100);
        btn->hoverColor = Color(120, 120, 120);
        btn->shape.setFillColor(btn->normalColor);
      }

      portButtons.push_back(btn);

      col++;
      if (col >= cols) {
        col = 0;
        row++;
      }
    }

    Text statsText;
    statsText.setFont(*font);
    statsText.setCharacterSize(16);
    statsText.setFillColor(Color(200, 200, 200));
    statsText.setString("Valid ports: " + to_string(validPortCount) + "/" +
                        to_string(totalPortCount));
    statsText.setPosition(x + 20, y + height - 40);
  }

 private:
  bool canPortBeInserted(const Graph& g, int newPortIdx, int beforePortIdx,
                         int afterPortIdx, bool isAddingBefore) {
    if (beforePortIdx == -1 && afterPortIdx != -1) {
      return g.hasDirectRoute(newPortIdx, afterPortIdx);
    }

    if (beforePortIdx != -1 && afterPortIdx == -1) {
      return g.hasDirectRoute(beforePortIdx, newPortIdx);
    }

    if (beforePortIdx != -1 && afterPortIdx != -1) {
      return g.hasDirectRoute(beforePortIdx, newPortIdx) &&
             g.hasDirectRoute(newPortIdx, afterPortIdx);
    }

    return true;
  }

 public:
  void hide() {
    isVisible = false;
    selectedPortIdx = -1;
    currentJourneyPorts.clear();
  }

  bool visible() const { return isVisible; }

  void update(RenderWindow& window) {
    if (!isVisible) return;
    closeButton->update(window);
    for (auto btn : portButtons) btn->update(window);
  }

  void draw(RenderWindow& window) {
    if (!isVisible) return;

    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);
    window.draw(instructionText);

    for (auto btn : portButtons) {
      btn->draw(window);
    }

    if (!portButtons.empty()) {
      int validCount = 0;
      for (auto btn : portButtons) {
        if (btn->normalColor == Color(0, 150, 0)) {
          validCount++;
        }
      }

      Text statsText;
      statsText.setFont(*font);
      statsText.setCharacterSize(16);
      statsText.setFillColor(Color(200, 200, 200));
      statsText.setString("Valid ports: " + to_string(validCount) + "/" +
                          to_string(portButtons.size()));
      statsText.setPosition(
          background.getPosition().x + 20,
          background.getPosition().y + background.getSize().y - 40);
      window.draw(statsText);
    }

    closeButton->draw(window);
  }

  int checkClick(RenderWindow& window, Event& event, const Graph& g) {
    if (!isVisible) return -1;

    if (closeButton->isClicked(window, event)) {
      hide();
      return -2;
    }

    for (int i = 0; i < portButtons.size(); i++) {
      if (portButtons[i]->isClicked(window, event)) {
        string portName = portButtons[i]->text.getString();
        for (int j = 0; j < g.ports.size(); j++) {
          if (g.ports[j].name == portName) {
            selectedPortIdx = j;
            cout << "Selected port: " << portName << " (index " << j << ")"
                 << endl;
            return j;
          }
        }
      }
    }

    return -1;
  }

  int getSelectedPort() const { return selectedPortIdx; }
  int getSelectedJourneyIndex() const { return selectedJourneyIdx; }
  bool getIsAddingBefore() const { return isAddingBefore; }
};
class JourneyControlPanel {
 private:
  Button* addBeforeButton;
  Button* addAfterButton;
  Button* removePortButton;
  Button* clearJourneyButton;
  Button* saveJourneyButton;
  Text journeyStatsText;
  Font* font;
  float windowWidth;
  float windowHeight;
  bool isVisible;

 public:
  JourneyControlPanel(Font& f, float winWidth, float winHeight)
      : font(&f), windowWidth(winWidth), windowHeight(winHeight) {
    float buttonWidth = 200;
    float buttonHeight = 50;
    float rightMargin = 20;
    float bottomMargin = 20;
    float spacing = 10;

    float x = windowWidth - buttonWidth - rightMargin;
    float y = windowHeight - (5 * (buttonHeight + spacing)) - bottomMargin;

    addBeforeButton =
        new Button(Vector2f(x, y), Vector2f(buttonWidth, buttonHeight),
                   "Add Port Before", f);

    addAfterButton =
        new Button(Vector2f(x, y + buttonHeight + spacing),
                   Vector2f(buttonWidth, buttonHeight), "Add Port After", f);

    removePortButton =
        new Button(Vector2f(x, y + 2 * (buttonHeight + spacing)),
                   Vector2f(buttonWidth, buttonHeight), "Remove Port", f);

    clearJourneyButton =
        new Button(Vector2f(x, y + 3 * (buttonHeight + spacing)),
                   Vector2f(buttonWidth, buttonHeight), "Clear Journey", f);

    saveJourneyButton =
        new Button(Vector2f(x, y + 4 * (buttonHeight + spacing)),
                   Vector2f(buttonWidth, buttonHeight), "View Summary", f);

    journeyStatsText.setFont(f);
    journeyStatsText.setCharacterSize(18);
    journeyStatsText.setFillColor(Color::White);
    journeyStatsText.setOutlineThickness(1);
    journeyStatsText.setOutlineColor(Color::Black);
    journeyStatsText.setPosition(x, 20);
  }

  ~JourneyControlPanel() {
    delete addBeforeButton;
    delete addAfterButton;
    delete removePortButton;
    delete clearJourneyButton;
    delete saveJourneyButton;
  }

  void updateStats(const MultiLegJourney& journey, const Graph& g) {}

  void update(RenderWindow& window) {
    addBeforeButton->update(window);
    addAfterButton->update(window);
    removePortButton->update(window);
    clearJourneyButton->update(window);
    saveJourneyButton->update(window);
  }

  void draw(RenderWindow& window) {
    window.draw(journeyStatsText);
    addBeforeButton->draw(window);
    addAfterButton->draw(window);
    removePortButton->draw(window);
    clearJourneyButton->draw(window);
    saveJourneyButton->draw(window);
  }

  enum ControlAction {
    NONE = 0,
    ADD_BEFORE = 1,
    ADD_AFTER = 2,
    REMOVE_PORT = 3,
    CLEAR_JOURNEY = 4,
    VIEW_SUMMARY = 5
  };
  bool isMouseOver(RenderWindow& window) const {
    if (!isVisible) return false;
    Vector2i mousePos = Mouse::getPosition(window);
    float infoWidth = windowWidth * 0.5f;
    float infoHeight = windowHeight * 0.7f;
    float infoX = (windowWidth - infoWidth) / 2;
    float infoY = (windowHeight - infoHeight) / 2;
    FloatRect bounds(infoX, infoY, infoWidth, infoHeight);
    return bounds.contains(static_cast<Vector2f>(mousePos));
  }
  bool visible() const { return isVisible; }
  ControlAction checkClick(RenderWindow& window, Event& event) {
    if (addBeforeButton->isClicked(window, event)) return ADD_BEFORE;
    if (addAfterButton->isClicked(window, event)) return ADD_AFTER;
    if (removePortButton->isClicked(window, event)) return REMOVE_PORT;
    if (clearJourneyButton->isClicked(window, event)) return CLEAR_JOURNEY;
    if (saveJourneyButton->isClicked(window, event)) return VIEW_SUMMARY;
    return NONE;
  }
};
class VoyageTimeWindow {
 private:
  RectangleShape background;
  RectangleShape titleBar;
  Text titleText;
  vector<Button*> timeButtons;
  Button* closeButton;
  bool isVisible;
  Font* font;
  float windowWidth;
  float windowHeight;
  int selectedTime;

 public:
  VoyageTimeWindow(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        windowWidth(winWidth),
        windowHeight(winHeight),
        selectedTime(-1) {
    setupWindow();
  }

  ~VoyageTimeWindow() {
    delete closeButton;
    for (auto btn : timeButtons) {
      delete btn;
    }
  }

  void setupWindow() {
    float width = windowWidth * 0.6f;
    float height = windowHeight * 0.8f;

    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(Color(50, 50, 50, 240));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(100, 100, 100));

    titleBar.setSize(Vector2f(width, 80));
    titleBar.setPosition(x, y);
    titleBar.setFillColor(Color(70, 130, 180));

    titleText.setFont(*font);
    titleText.setString("Set Maximum Voyage Time");
    titleText.setCharacterSize(32);
    titleText.setFillColor(Color::White);
    titleText.setPosition(x + 25, y + 25);

    closeButton = new Button(Vector2f(x + width - 65, y + 15), Vector2f(50, 50),
                             "X", *font);

    setupButtons();
  }

  void setupButtons() {
    for (auto btn : timeButtons) {
      delete btn;
    }
    timeButtons.clear();

    float width = background.getSize().x;
    float x = background.getPosition().x;
    float y = background.getPosition().y;
    float buttonWidth = width * 0.8f;
    float buttonHeight = 70;
    float startX = x + (width - buttonWidth) / 2;
    float startY = y + 120;
    float spacing = 15;

    vector<string> timeOptions = {"8 Hours",  "16 Hours", "24 Hours",
                                  "48 Hours", "72 Hours", "No Time Limit"};

    for (int i = 0; i < timeOptions.size(); i++) {
      timeButtons.push_back(new Button(
          Vector2f(startX, startY + i * (buttonHeight + spacing)),
          Vector2f(buttonWidth, buttonHeight), timeOptions[i], *font));
    }
  }

  void show(int currentTime) {
    isVisible = true;
    selectedTime = currentTime;
    setupWindow();
  }

  void hide() { isVisible = false; }

  void handleResize(float newWidth, float newHeight) {
    windowWidth = newWidth;
    windowHeight = newHeight;
    if (isVisible) {
      setupWindow();
    }
  }

  bool visible() const { return isVisible; }

  void update(RenderWindow& window) {
    if (!isVisible) return;
    if (closeButton) closeButton->update(window);
    for (auto btn : timeButtons) {
      btn->update(window);
    }
  }

  void draw(RenderWindow& window) {
    if (!isVisible) return;

    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);

    Text instructionText;
    instructionText.setFont(*font);
    instructionText.setCharacterSize(18);
    instructionText.setFillColor(Color(200, 200, 200));
    instructionText.setPosition(background.getPosition().x + 25,
                                background.getPosition().y + 90);
    window.draw(instructionText);

    for (int i = 0; i < 7; i++) {
      if (i < timeButtons.size()) {
        if (i == selectedTime) {
          RectangleShape highlight;
          highlight.setSize(timeButtons[i]->shape.getSize() + Vector2f(10, 10));
          highlight.setPosition(timeButtons[i]->shape.getPosition() -
                                Vector2f(5, 5));
          highlight.setFillColor(Color::Transparent);
          highlight.setOutlineThickness(3);
          highlight.setOutlineColor(Color(0, 255, 0));
          window.draw(highlight);
        }
        timeButtons[i]->draw(window);
      }
    }

    if (timeButtons.size() >= 9) {
      timeButtons[7]->draw(window);

      timeButtons[8]->draw(window);
    }

    if (selectedTime != -1 && selectedTime < 7) {
      Text currentSelection;
      currentSelection.setFont(*font);
      currentSelection.setCharacterSize(20);
      currentSelection.setFillColor(Color(150, 255, 150));

      vector<string> timeDescriptions = {
          "8 hours maximum voyage time",  "16 hours maximum voyage time",
          "24 hours maximum voyage time", "48 hours maximum voyage time",
          "72 hours maximum voyage time", "No voyage time limit",
          "Custom time input required"};

      string selectionText = "Selected: " + timeDescriptions[selectedTime];
      currentSelection.setString(selectionText);
      currentSelection.setPosition(
          background.getPosition().x + 25,
          background.getPosition().y + background.getSize().y - 80);
      window.draw(currentSelection);
    }

    if (closeButton) closeButton->draw(window);
  }

  int checkClick(RenderWindow& window, Event& event) {
    if (!isVisible) return -1;

    if (closeButton && closeButton->isClicked(window, event)) {
      hide();
      return -2;
    }
    for (int i = 0; i < 7 && i < timeButtons.size(); i++) {
      if (timeButtons[i]->isClicked(window, event)) {
        selectedTime = i;
        return i;
      }
    }

    if (timeButtons.size() >= 9) {
      if (timeButtons[7]->isClicked(window, event)) {
        return -3;
      }
      if (timeButtons[8]->isClicked(window, event)) {
        return -4;
      }
    }

    return -1;
  }

  int getSelectedTimeMinutes() const {
    switch (selectedTime) {
      case 0:
        return 8 * 60;
      case 1:
        return 16 * 60;
      case 2:
        return 24 * 60;
      case 3:
        return 48 * 60;
      case 4:
        return 72 * 60;
      case 5:
        return 999999;
      case 6:
        return -1;
      default:
        return 999999;
    }
  }
  bool isCustomTimeSelected() const { return selectedTime == 6; }

  string getSelectedTimeString() const {
    vector<string> timeStrings = {"8 Hours",    "16 Hours", "24 Hours",
                                  "48 Hours",   "72 Hours", "No Time Limit",
                                  "Custom Time"};
    return (selectedTime >= 0 && selectedTime < timeStrings.size())
               ? timeStrings[selectedTime]
               : "Not Selected";
  }
};
class FilterPopupWindow {
 private:
  RectangleShape background;
  RectangleShape titleBar;
  Text titleText;
  vector<Button*> buttons;
  Button* closeButton;
  bool isVisible;
  Font* font;
  float windowWidth;
  float windowHeight;

 public:
  FilterPopupWindow(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        windowWidth(winWidth),
        windowHeight(winHeight) {
    setupWindow();
  }

  ~FilterPopupWindow() {
    delete closeButton;
    for (auto btn : buttons) delete btn;
  }

  void setupWindow() {
    float width = 900;
    float height = 700;
    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(Color(50, 50, 50, 240));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(100, 100, 100));

    titleBar.setSize(Vector2f(width, 70));
    titleBar.setPosition(x, y);
    titleBar.setFillColor(Color(70, 130, 180));

    titleText.setFont(*font);
    titleText.setString("Filter Options");
    titleText.setCharacterSize(32);
    titleText.setFillColor(Color::White);
    titleText.setPosition(x + 20, y + 20);

    closeButton = new Button(Vector2f(x + width - 55, y + 10), Vector2f(45, 45),
                             "X", *font);

    setupButtons();
  }

  void setupButtons() {
    for (auto btn : buttons) delete btn;
    buttons.clear();

    float width = 900;
    float height = 700;
    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    float buttonWidth = 600;
    float buttonHeight = 80;
    float startX = x + (width - buttonWidth) / 2;
    float startY = y + 120;
    float spacing = 30;

    buttons.push_back(new Button(Vector2f(startX, startY),
                                 Vector2f(buttonWidth, buttonHeight),
                                 "Select Shipping Companies", *font));
    buttons.push_back(new Button(
        Vector2f(startX, startY + (buttonHeight + spacing)),
        Vector2f(buttonWidth, buttonHeight), "Avoid Specific Ports", *font));
    buttons.push_back(new Button(
        Vector2f(startX, startY + 2 * (buttonHeight + spacing)),
        Vector2f(buttonWidth, buttonHeight), "Set Max Voyage Time", *font));
    buttons.push_back(new Button(
        Vector2f(startX, startY + 3 * (buttonHeight + spacing) + 20),
        Vector2f(buttonWidth, buttonHeight), "Apply Filters", *font));
  }

  void show() {
    isVisible = true;
    setupWindow();
  }

  void hide() { isVisible = false; }

  bool visible() const { return isVisible; }

  void update(RenderWindow& window) {
    if (!isVisible) return;
    if (closeButton) closeButton->update(window);
    for (auto btn : buttons) btn->update(window);
  }

  void draw(RenderWindow& window, const UserPreferences& prefs) {
    if (!isVisible) return;

    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);

    for (auto btn : buttons) btn->draw(window);

    if (closeButton) closeButton->draw(window);
  }

  int checkClick(RenderWindow& window, Event& event) {
    if (!isVisible) return -1;

    if (closeButton && closeButton->isClicked(window, event)) {
      hide();
      return -2;
    }

    for (int i = 0; i < buttons.size(); i++) {
      if (buttons[i]->isClicked(window, event)) return i;
    }

    return -1;
  }
  void handleResize(float newWidth, float newHeight) {
    windowWidth = newWidth;
    windowHeight = newHeight;
    setupWindow();
  }
  bool isMouseOverWindow(RenderWindow& window) const {
    if (!isVisible) return false;
    Vector2i mousePos = Mouse::getPosition(window);
    FloatRect bounds = background.getGlobalBounds();
    return bounds.contains(static_cast<Vector2f>(mousePos));
  }
};

void rebuildJourneyVisualization(MultiLegJourney& journey,
                                 vector<RouteEdge>& journeyPathEdges,
                                 vector<RouteEdge>& availableRouteEdges,
                                 vector<Location>& locations, Graph& g,
                                 int& selectedJourneyPortIdx,
                                 bool& showingAvailableRoutes) {
  journeyPathEdges.clear();
  journey.legs.clear();
  availableRouteEdges.clear();

  bool allRoutesValid = true;
  for (size_t i = 0; i < journey.portPath.size() - 1; i++) {
    int from = journey.portPath[i];
    int to = journey.portPath[i + 1];

    if (g.hasDirectRoute(from, to)) {
      Route route = g.getRouteBetween(from, to);

      JourneyLeg leg = {from, to, route, true};
      journey.legs.push_back(leg);

      RouteEdge edge(locations[from].position, locations[to].position, route,
                     g.ports[from].name);
      edge.line.setFillColor(Color(255, 105, 180));
      journeyPathEdges.push_back(edge);
    } else {
      allRoutesValid = false;

      Route dummyRoute = {"", "", "", "", 0, "", 0};
      JourneyLeg leg = {from, to, dummyRoute, false};
      journey.legs.push_back(leg);

      RouteEdge edge(locations[from].position, locations[to].position,
                     dummyRoute, g.ports[from].name);
      edge.line.setFillColor(Color(150, 150, 150, 100));
      edge.line.setOutlineColor(Color::Red);
      edge.line.setOutlineThickness(1);
      journeyPathEdges.push_back(edge);
    }
  }

  journey.calculateTotals(g);
  journey.isComplete = allRoutesValid && (journey.portPath.size() >= 2);

  if (!journey.portPath.empty()) {
    int lastPort = journey.portPath.back();
    vector<Route> newRoutes = g.getRoutesFromPort(lastPort);

    for (const Route& route : newRoutes) {
      int destIdx = g.getPortIndex(route.destination);
      if (destIdx != -1) {
        bool alreadyInJourney = false;
        for (int portIdx : journey.portPath) {
          if (portIdx == destIdx) {
            alreadyInJourney = true;
            break;
          }
        }

        if (!alreadyInJourney) {
          RouteEdge edge(locations[lastPort].position,
                         locations[destIdx].position, route,
                         g.ports[lastPort].name);
          edge.line.setFillColor(Color(255, 255, 0, 200));
          availableRouteEdges.push_back(edge);
        }
      }
    }

    showingAvailableRoutes = true;
    selectedJourneyPortIdx = journey.portPath.size() - 1;
  } else {
    showingAvailableRoutes = false;
    selectedJourneyPortIdx = -1;
  }

  for (auto& loc : locations) {
    bool inJourney = false;
    for (int portIdx : journey.portPath) {
      int locPortIdx = -1;
      for (int j = 0; j < g.ports.size(); j++) {
        if (g.ports[j].name == loc.name) {
          locPortIdx = j;
          break;
        }
      }

      if (locPortIdx == portIdx) {
        inJourney = true;
        break;
      }
    }

    if (inJourney) {
      int locPortIdx = -1;
      for (int j = 0; j < g.ports.size(); j++) {
        if (g.ports[j].name == loc.name) {
          locPortIdx = j;
          break;
        }
      }

      bool isSelected =
          (locPortIdx != -1 &&
           selectedJourneyPortIdx < journey.portPath.size() &&
           journey.portPath[selectedJourneyPortIdx] == locPortIdx);

      if (isSelected) {
        loc.pin.setOutlineColor(Color::Yellow);
      } else {
        loc.pin.setOutlineColor(Color(255, 105, 180));
      }
    } else {
      loc.pin.setOutlineColor(Color::White);
    }
  }
}
void displaySubgraph(Graph& g, vector<Location>& locations,
                     vector<RouteEdge>& edges, const SubgraphMenu& menu) {
  vector<string> selectedOptions = menu.getSelectedOptions();
  SubgraphMenu::SubgraphMode mode = menu.getCurrentMode();

  Color allowedPortColor = Color(255, 215, 0);
  Color blockedPortColor = Color(150, 150, 150);

  Color allowedRouteColor = Color(255, 215, 0, 200);
  Color blockedRouteColor = Color(100, 100, 100, 50);

  for (auto& loc : locations) {
    int portIdx = g.getPortIndex(loc.name);
    if (portIdx == -1) continue;

    bool portAllowed = true;

    if (mode == SubgraphMenu::COMPANY_MODE) {
      bool hasSelectedCompanyRoute = false;
      for (const Route& route : g.routes[portIdx]) {
        for (const string& company : selectedOptions) {
          if (route.company == company) {
            hasSelectedCompanyRoute = true;
            break;
          }
        }
        if (hasSelectedCompanyRoute) break;
      }
      if (!hasSelectedCompanyRoute) {
        for (int i = 0; i < g.routes.size(); i++) {
          for (const Route& route : g.routes[i]) {
            if (route.destination == loc.name) {
              for (const string& company : selectedOptions) {
                if (route.company == company) {
                  hasSelectedCompanyRoute = true;
                  break;
                }
              }
            }
            if (hasSelectedCompanyRoute) break;
          }
          if (hasSelectedCompanyRoute) break;
        }
      }

      portAllowed = hasSelectedCompanyRoute;

    } else if (mode == SubgraphMenu::WEATHER_MODE) {
      bool hasBadWeather = false;
      for (const string& weather : selectedOptions) {
        if (g.portHasWeather(loc.name, weather)) {
          hasBadWeather = true;
          break;
        }
      }
      portAllowed = !hasBadWeather;
    }

    loc.pin.setFillColor(portAllowed ? allowedPortColor : blockedPortColor);
    loc.label.setFillColor(portAllowed ? Color::White : Color(100, 100, 100));
  }

  for (auto& edge : edges) {
    int sourceIdx = g.getPortIndex(edge.sourceName);
    int destIdx = g.getPortIndex(edge.routeInfo.destination);

    if (sourceIdx == -1 || destIdx == -1) continue;

    bool routeAllowed = true;

    if (mode == SubgraphMenu::COMPANY_MODE) {
      bool companyMatch = false;
      for (const string& company : selectedOptions) {
        if (edge.routeInfo.company == company) {
          companyMatch = true;
          break;
        }
      }
      routeAllowed = companyMatch;

    } else if (mode == SubgraphMenu::WEATHER_MODE) {
      bool sourceHasBadWeather = false;
      bool destHasBadWeather = false;

      for (const string& weather : selectedOptions) {
        if (g.portHasWeather(edge.sourceName, weather)) {
          sourceHasBadWeather = true;
        }
        if (g.portHasWeather(edge.routeInfo.destination, weather)) {
          destHasBadWeather = true;
        }
        if (sourceHasBadWeather && destHasBadWeather) break;
      }

      routeAllowed = !(sourceHasBadWeather || destHasBadWeather);
    }

    if (routeAllowed) {
      edge.line.setSize(Vector2f(edge.line.getSize().x, 1.5f));
      edge.line.setFillColor(allowedRouteColor);
      edge.normalColor = allowedRouteColor;
    } else {
      edge.line.setSize(Vector2f(edge.line.getSize().x, 1.0f));
      edge.line.setFillColor(blockedRouteColor);
      edge.normalColor = blockedRouteColor;
    }
    edge.line.setOutlineThickness(0);
  }
}

class DateSelectionWindow {
 private:
  RectangleShape background;
  RectangleShape titleBar;
  Text titleText;
  Text instructionText;
  vector<Button*> dateButtons;
  Button* closeButton;
  Button* doneButton;
  bool isVisible;
  Font* font;
  float windowWidth;
  float windowHeight;
  string selectedDate;
  float scrollOffset;
  float maxScrollOffset;

 public:
  DateSelectionWindow(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        doneButton(nullptr),
        windowWidth(winWidth),
        windowHeight(winHeight),
        scrollOffset(0),
        maxScrollOffset(0) {
    float width = windowWidth * 0.6f;
    float height = windowHeight * 0.8f;
    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(Color(50, 50, 50, 240));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(100, 100, 100));

    titleBar.setSize(Vector2f(width, 70));
    titleBar.setPosition(x, y);
    titleBar.setFillColor(Color(70, 130, 180));

    titleText.setFont(f);
    titleText.setString("Select Departure Date");
    titleText.setCharacterSize(30);
    titleText.setFillColor(Color::White);
    titleText.setPosition(x + 20, y + 20);

    instructionText.setFont(f);
    instructionText.setCharacterSize(18);
    instructionText.setFillColor(Color(200, 200, 200));
    instructionText.setPosition(x + 20, y + 90);

    closeButton =
        new Button(Vector2f(x + width - 55, y + 10), Vector2f(45, 45), "X", f);
  }

  ~DateSelectionWindow() {
    delete closeButton;
    delete doneButton;
    for (auto btn : dateButtons) delete btn;
  }

  void show(const string& fromPort, const string& toPort,
            const vector<string>& availableDates) {
    isVisible = true;
    selectedDate = "";
    scrollOffset = 0;

    instructionText.setString("Select departure date for route:\n" + fromPort +
                              " -> " + toPort);

    for (auto btn : dateButtons) delete btn;
    dateButtons.clear();
    delete doneButton;

    float width = background.getSize().x;
    float x = background.getPosition().x;
    float y = background.getPosition().y;

    float buttonWidth = width * 0.8f;
    float buttonHeight = 60;
    float startX = x + (width - buttonWidth) / 2;
    float startY = y + 150;
    float spacing = 15;

    for (size_t i = 0; i < availableDates.size(); i++) {
      Button* btn = new Button(
          Vector2f(startX, startY + i * (buttonHeight + spacing)),
          Vector2f(buttonWidth, buttonHeight), availableDates[i], *font);
      dateButtons.push_back(btn);
    }

    float contentHeight = availableDates.size() * (buttonHeight + spacing);
    float visibleHeight = background.getSize().y - 250;
    maxScrollOffset = max(0.0f, contentHeight - visibleHeight);

    doneButton = new Button(
        Vector2f(x + (width - 250) / 2, y + background.getSize().y - 80),
        Vector2f(250, 60), "Done", *font);
  }

  void hide() {
    isVisible = false;
    selectedDate = "";
  }

  bool visible() const { return isVisible; }

  void scroll(float delta) {
    if (!isVisible) return;
    scrollOffset -= delta * 30.0f;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > maxScrollOffset) scrollOffset = maxScrollOffset;
  }

  void update(RenderWindow& window) {
    if (!isVisible) return;
    closeButton->update(window);
    if (doneButton) doneButton->update(window);
    for (auto btn : dateButtons) btn->update(window);
  }

  void draw(RenderWindow& window) {
    if (!isVisible) return;

    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);
    window.draw(instructionText);

    float visibleTop = background.getPosition().y + 140;
    float visibleBottom =
        background.getPosition().y + background.getSize().y - 100;

    for (auto btn : dateButtons) {
      Vector2f pos = btn->shape.getPosition();
      float newY = pos.y - scrollOffset;

      if (newY >= visibleTop && newY <= visibleBottom) {
        btn->shape.setPosition(pos.x, newY);
        btn->text.setPosition(pos.x + btn->shape.getSize().x / 2,
                              newY + btn->shape.getSize().y / 2);

        if (btn->text.getString() == selectedDate) {
          RectangleShape highlight;
          highlight.setSize(btn->shape.getSize() + Vector2f(10, 10));
          highlight.setPosition(btn->shape.getPosition() - Vector2f(5, 5));
          highlight.setFillColor(Color::Transparent);
          highlight.setOutlineThickness(4);
          highlight.setOutlineColor(Color(0, 255, 0));
          window.draw(highlight);
        }

        btn->draw(window);
        btn->shape.setPosition(pos);
        btn->text.setPosition(pos.x + btn->shape.getSize().x / 2,
                              pos.y + btn->shape.getSize().y / 2);
      }
    }

    if (maxScrollOffset > 0) {
      float trackX = background.getPosition().x + background.getSize().x - 18;
      float trackY = visibleTop;
      float trackH = visibleBottom - visibleTop;

      RectangleShape track(Vector2f(8, trackH));
      track.setPosition(trackX, trackY);
      track.setFillColor(Color(200, 200, 200));
      window.draw(track);

      float ratio = trackH / (trackH + maxScrollOffset);
      float thumbH = max(40.0f, trackH * ratio);
      float thumbY =
          trackY + (scrollOffset * (trackH - thumbH) / maxScrollOffset);

      RectangleShape thumb(Vector2f(8, thumbH));
      thumb.setPosition(trackX, thumbY);
      thumb.setFillColor(Color(100, 100, 100));
      window.draw(thumb);
    }

    if (doneButton) doneButton->draw(window);
    closeButton->draw(window);

    if (!selectedDate.empty()) {
      Text selectionText;
      selectionText.setFont(*font);
      selectionText.setCharacterSize(20);
      selectionText.setFillColor(Color(150, 255, 150));
      selectionText.setString("Selected: " + selectedDate);
      selectionText.setPosition(
          background.getPosition().x + 20,
          background.getPosition().y + background.getSize().y - 120);
      window.draw(selectionText);
    }
  }

  int checkClick(RenderWindow& window, Event& event) {
    if (!isVisible) return -1;

    if (closeButton->isClicked(window, event)) {
      hide();
      return -2;
    }

    if (doneButton && doneButton->isClicked(window, event)) {
      if (!selectedDate.empty()) {
        return -3;
      }
    }

    float visibleTop = background.getPosition().y + 140;
    float visibleBottom =
        background.getPosition().y + background.getSize().y - 100;

    for (size_t i = 0; i < dateButtons.size(); i++) {
      Vector2f pos = dateButtons[i]->shape.getPosition();
      float newY = pos.y - scrollOffset;

      if (newY >= visibleTop && newY <= visibleBottom) {
        dateButtons[i]->shape.setPosition(pos.x, newY);
        dateButtons[i]->text.setPosition(
            pos.x + dateButtons[i]->shape.getSize().x / 2,
            newY + dateButtons[i]->shape.getSize().y / 2);

        if (dateButtons[i]->isClicked(window, event)) {
          selectedDate = dateButtons[i]->text.getString();
          dateButtons[i]->shape.setPosition(pos);
          dateButtons[i]->text.setPosition(
              pos.x + dateButtons[i]->shape.getSize().x / 2,
              pos.y + dateButtons[i]->shape.getSize().y / 2);
          return i;
        }

        dateButtons[i]->shape.setPosition(pos);
        dateButtons[i]->text.setPosition(
            pos.x + dateButtons[i]->shape.getSize().x / 2,
            pos.y + dateButtons[i]->shape.getSize().y / 2);
      }
    }

    return -1;
  }

  string getSelectedDate() const { return selectedDate; }

  bool isMouseOverWindow(RenderWindow& window) const {
    if (!isVisible) return false;
    Vector2i mousePos = Mouse::getPosition(window);
    FloatRect bounds = background.getGlobalBounds();
    return bounds.contains(static_cast<Vector2f>(mousePos));
  }
};

class RouteBookingWindow {
 private:
  RectangleShape background;
  RectangleShape titleBar;
  Text titleText;
  vector<Text> routeDetails;
  Button* closeButton;
  vector<Button*> routeSelectionButtons;
  Button* bookButton;
  bool isVisible;
  Font* font;
  float windowWidth;
  float windowHeight;
  vector<CompleteRoute> availableRoutes;
  string originPort;
  string destPort;
  string selectedDate;
  float scrollOffset;
  float maxScrollOffset;

 public:
  int selectedRouteIndex;
  RouteBookingWindow(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        bookButton(nullptr),
        windowWidth(winWidth),
        windowHeight(winHeight),
        selectedRouteIndex(-1),
        scrollOffset(0),
        maxScrollOffset(0) {
    float width = windowWidth * 0.7f;
    float height = windowHeight * 0.85f;
    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(Color(240, 240, 240));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(50, 50, 50));

    titleBar.setSize(Vector2f(width, 60));
    titleBar.setPosition(x, y);
    titleBar.setFillColor(Color(70, 130, 180));

    titleText.setFont(f);
    titleText.setCharacterSize(28);
    titleText.setFillColor(Color::White);
    titleText.setPosition(x + 20, y + 16);

    closeButton =
        new Button(Vector2f(x + width - 50, y + 10), Vector2f(40, 40), "X", f);

    bookButton = new Button(Vector2f(x + width - 270, y + height - 80),
                            Vector2f(250, 60), "Book This Route", f);
    bookButton->normalColor = Color(0, 150, 0);
    bookButton->hoverColor = Color(0, 200, 0);
    bookButton->shape.setFillColor(bookButton->normalColor);
  }

  ~RouteBookingWindow() {
    delete closeButton;
    delete bookButton;
    for (auto btn : routeSelectionButtons) {
        delete btn;
    }
  }

void show(const string& from, const string& to, const string& date, 
          const vector<CompleteRoute>& routes, const Graph& g) {
    isVisible = true;
    originPort = from;
    destPort = to;
    selectedDate = date;
    availableRoutes = routes;
    selectedRouteIndex = -1;
    scrollOffset = 0;
    
    for (auto btn : routeSelectionButtons) {
        delete btn;
    }
    routeSelectionButtons.clear();
    
    titleText.setString("Available Routes: " + from + " -> " + to);
    
    routeDetails.clear();
    
    float x = background.getPosition().x;
    float y = background.getPosition().y;
    float windowWidth = background.getSize().x;
    float yPos = y + 80;
    
    if (routes.empty()) {
        Text noRoutes;
        noRoutes.setFont(*font);
        noRoutes.setString("No routes available on " + date);
        noRoutes.setCharacterSize(24);
        noRoutes.setFillColor(Color(200, 50, 50));
        noRoutes.setPosition(x + 20, yPos);
        routeDetails.push_back(noRoutes);
    } else {
        Text header;
        header.setFont(*font);
        header.setString("Date: " + date + " | Available Routes: " + to_string(routes.size()));
        header.setCharacterSize(20);
        header.setFillColor(Color(50, 50, 50));
        header.setStyle(Text::Bold);
        header.setPosition(x + 20, yPos);
        routeDetails.push_back(header);
        yPos += 45;
        
        for (size_t i = 0; i < routes.size(); i++) {
            const CompleteRoute& route = routes[i];
            Text routeHeader;
            routeHeader.setFont(*font);
            routeHeader.setString("=== ROUTE " + to_string(i + 1) + " ===");
            routeHeader.setCharacterSize(22);
            routeHeader.setFillColor(Color(70, 130, 180));
            routeHeader.setStyle(Text::Bold);
            routeHeader.setPosition(x + 20, yPos);
            routeDetails.push_back(routeHeader);
            yPos += 35;
            Button* selectBtn = new Button(
                Vector2f(x + windowWidth - 180, yPos),  
                Vector2f(160, 45),  
                "Select Route " + to_string(i + 1),
                *font
            );
            selectBtn->normalColor = Color(70, 130, 180);
            selectBtn->hoverColor = Color(100, 160, 210);
            selectBtn->shape.setFillColor(selectBtn->normalColor);
            routeSelectionButtons.push_back(selectBtn);
            
            yPos += 55;  
            string pathStr = "Path: ";
            for (size_t j = 0; j < route.portPath.size(); j++) {
                pathStr += g.ports[route.portPath[j]].name;
                if (j < route.portPath.size() - 1) pathStr += " -> ";
            }
            Text pathText;
            pathText.setFont(*font);
            pathText.setString(pathStr);
            pathText.setCharacterSize(18);
            pathText.setFillColor(Color(0, 100, 0));
            pathText.setStyle(Text::Bold);
            pathText.setPosition(x + 30, yPos);
            routeDetails.push_back(pathText);
            yPos += 30;
            Text summary;
            summary.setFont(*font);
            summary.setString("Cost: $" + to_string(route.totalCost) + 
                            " | Time: " + to_string(route.totalTime / 60) + "h " + 
                            to_string(route.totalTime % 60) + "m | Stops: " + 
                            to_string(route.layoverCount));
            summary.setCharacterSize(16);
            summary.setFillColor(Color::Black);
            summary.setPosition(x + 30, yPos);
            routeDetails.push_back(summary);
            yPos += 35;
            for (size_t j = 0; j < route.routeLegs.size(); j++) {
                const Route& leg = route.routeLegs[j];
                Text legHeader;
                legHeader.setFont(*font);
                legHeader.setString("  Leg " + to_string(j + 1) + ": " + 
                                   g.ports[route.portPath[j]].name + " -> " + 
                                   g.ports[route.portPath[j + 1]].name);
                legHeader.setCharacterSize(16);
                legHeader.setFillColor(Color(50, 50, 150));
                legHeader.setStyle(Text::Bold);
                legHeader.setPosition(x + 40, yPos);
                routeDetails.push_back(legHeader);
                yPos += 25;
                Text legDetails;
                legDetails.setFont(*font);
                legDetails.setString("    " + leg.depTime + " - " + leg.arrTime + 
                                    " | $" + to_string(leg.cost) + " | " + leg.company);
                legDetails.setCharacterSize(14);
                legDetails.setFillColor(Color(80, 80, 80));
                legDetails.setPosition(x + 40, yPos);
                routeDetails.push_back(legDetails);
                yPos += 25;
            }
            
            yPos += 40;  
        }
    }
    
    float contentHeight = yPos - (background.getPosition().y + 80);
    float visibleHeight = background.getSize().y - 160;
    maxScrollOffset = max(0.0f, contentHeight - visibleHeight);
}

  void hide() {
    isVisible = false;
    selectedRouteIndex = -1;
  }

  bool visible() const { return isVisible; }

  void scroll(float delta) {
    if (!isVisible) return;
    scrollOffset -= delta * 30.0f;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > maxScrollOffset) scrollOffset = maxScrollOffset;
  }

void update(RenderWindow& window) {
    if (!isVisible) return;
    closeButton->update(window);
    if (!availableRoutes.empty()) {
        bookButton->update(window);
    }
    
    float visibleTop = background.getPosition().y + 70;
    float visibleBottom = background.getPosition().y + background.getSize().y - 100;
    
    for (auto btn : routeSelectionButtons) {
        Vector2f btnPos = btn->shape.getPosition();
        float newY = btnPos.y - scrollOffset;
        
        if (newY >= visibleTop && newY <= visibleBottom) {
            btn->shape.setPosition(btnPos.x, newY);
            btn->text.setPosition(
                btnPos.x + btn->shape.getSize().x / 2,
                newY + btn->shape.getSize().y / 2
            );
            btn->update(window);
            btn->shape.setPosition(btnPos);
            btn->text.setPosition(
                btnPos.x + btn->shape.getSize().x / 2,
                btnPos.y + btn->shape.getSize().y / 2
            );
        }
    }
}

void draw(RenderWindow& window) {
    if (!isVisible) return;
    
    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);
    
    float visibleTop = background.getPosition().y + 70;
    float visibleBottom = background.getPosition().y + background.getSize().y - 100;
    
    for (size_t i = 0; i < routeSelectionButtons.size(); i++) {
        Vector2f btnPos = routeSelectionButtons[i]->shape.getPosition();
        float newY = btnPos.y - scrollOffset;
        
        if (newY >= visibleTop && newY <= visibleBottom) {
            routeSelectionButtons[i]->shape.setPosition(btnPos.x, newY);
            routeSelectionButtons[i]->text.setPosition(
                btnPos.x + routeSelectionButtons[i]->shape.getSize().x / 2,
                newY + routeSelectionButtons[i]->shape.getSize().y / 2
            );
            if ((int)i == selectedRouteIndex) {
                routeSelectionButtons[i]->shape.setFillColor(Color(0, 200, 0));
            } else {
                routeSelectionButtons[i]->shape.setFillColor(routeSelectionButtons[i]->normalColor);
            }
            
            routeSelectionButtons[i]->draw(window);
            
            routeSelectionButtons[i]->shape.setPosition(btnPos);
            routeSelectionButtons[i]->text.setPosition(
                btnPos.x + routeSelectionButtons[i]->shape.getSize().x / 2,
                btnPos.y + routeSelectionButtons[i]->shape.getSize().y / 2
            );
        }
    }
    
    for (auto& text : routeDetails) {
        Vector2f orig = text.getPosition();
        float newY = orig.y - scrollOffset;
        
        if (newY >= visibleTop && newY <= visibleBottom) {
            text.setPosition(orig.x, newY);
            window.draw(text);
            text.setPosition(orig);
        }
    }
    
    if (maxScrollOffset > 0) {
        float trackX = background.getPosition().x + background.getSize().x - 18;
        float trackY = visibleTop;
        float trackH = visibleBottom - visibleTop;
        
        RectangleShape track(Vector2f(8, trackH));
        track.setPosition(trackX, trackY);
        track.setFillColor(Color(200, 200, 200));
        window.draw(track);
        
        float ratio = trackH / (trackH + maxScrollOffset);
        float thumbH = max(40.0f, trackH * ratio);
        float thumbY = trackY + (scrollOffset * (trackH - thumbH) / maxScrollOffset);
        
        RectangleShape thumb(Vector2f(8, thumbH));
        thumb.setPosition(trackX, thumbY);
        thumb.setFillColor(Color(100, 100, 100));
        window.draw(thumb);
    }
    
    if (selectedRouteIndex != -1) {
        Text statusText;
        statusText.setFont(*font);
        statusText.setCharacterSize(18);
        statusText.setFillColor(Color(0, 150, 0));
        statusText.setString("Route " + to_string(selectedRouteIndex + 1) + " selected - Click 'Book This Route' to confirm");
        statusText.setPosition(background.getPosition().x + 20, 
                              background.getPosition().y + background.getSize().y - 120);
        window.draw(statusText);
    }
    
    if (!availableRoutes.empty()) {
        if (selectedRouteIndex == -1) {
            bookButton->normalColor = Color(100, 100, 100);
            bookButton->shape.setFillColor(Color(100, 100, 100));
        } else {
            bookButton->normalColor = Color(0, 150, 0); 
            bookButton->shape.setFillColor(Color(0, 150, 0));
        }
        bookButton->draw(window);
    }
    closeButton->draw(window);
}

  enum ClickResult { NONE = -1, CLOSE = -2, BOOK = -3 };

int checkClick(RenderWindow& window, Event& event) {
    if (!isVisible) return NONE;
    
    if (closeButton->isClicked(window, event)) {
        hide();
        return CLOSE;
    }
    
    if (!availableRoutes.empty() && bookButton->isClicked(window, event)) {
        if (selectedRouteIndex != -1) { 
            return BOOK;
        }
    }
    
    float visibleTop = background.getPosition().y + 70;
    float visibleBottom = background.getPosition().y + background.getSize().y - 100;
    
    for (size_t i = 0; i < routeSelectionButtons.size(); i++) {
        Vector2f btnPos = routeSelectionButtons[i]->shape.getPosition();
        float newY = btnPos.y - scrollOffset;
        
        if (newY >= visibleTop && newY <= visibleBottom) {
            routeSelectionButtons[i]->shape.setPosition(btnPos.x, newY);
            routeSelectionButtons[i]->text.setPosition(
                btnPos.x + routeSelectionButtons[i]->shape.getSize().x / 2,
                newY + routeSelectionButtons[i]->shape.getSize().y / 2
            );
            
            if (routeSelectionButtons[i]->isClicked(window, event)) {
                selectedRouteIndex = i;  
                cout << "Selected route " << (i + 1) << endl;
                
                routeSelectionButtons[i]->shape.setPosition(btnPos);
                routeSelectionButtons[i]->text.setPosition(
                    btnPos.x + routeSelectionButtons[i]->shape.getSize().x / 2,
                    btnPos.y + routeSelectionButtons[i]->shape.getSize().y / 2
                );
                return i;  
            }
            
            routeSelectionButtons[i]->shape.setPosition(btnPos);
            routeSelectionButtons[i]->text.setPosition(
                btnPos.x + routeSelectionButtons[i]->shape.getSize().x / 2,
                btnPos.y + routeSelectionButtons[i]->shape.getSize().y / 2
            );
        }
    }
    
    return NONE;
}

CompleteRoute getSelectedRoute() const {
    if (selectedRouteIndex >= 0 && selectedRouteIndex < availableRoutes.size()) {
        return availableRoutes[selectedRouteIndex];
    }
    return availableRoutes.empty() ? CompleteRoute() : CompleteRoute();  
}

  bool isMouseOverWindow(RenderWindow& window) const {
    if (!isVisible) return false;
    Vector2i mousePos = Mouse::getPosition(window);
    FloatRect bounds = background.getGlobalBounds();
    return bounds.contains(static_cast<Vector2f>(mousePos));
  }
};

class ShipSprite {
 private:
  Texture shipTexture;
  Sprite shipSprite;
  bool textureLoaded;

 public:
  ShipSprite() : textureLoaded(false) {
    if (shipTexture.loadFromFile("ship.png")) {
      shipSprite.setTexture(shipTexture);
      Vector2u size = shipTexture.getSize();
      shipSprite.setOrigin(size.x / 2.0f, size.y / 2.0f);
      shipSprite.setScale(0.05f, 0.05f);
      textureLoaded = true;
    } else {
      cout << "Warning: Could not load ship.png" << endl;
    }
  }

  void draw(RenderWindow& window, const Vector2f& position, float rotation,
            const Color& tint) {
    if (textureLoaded) {
      shipSprite.setPosition(position);
      shipSprite.setRotation(rotation);
      shipSprite.setColor(tint);
      window.draw(shipSprite);
    } else {
      CircleShape shipShape(10, 3);
      shipShape.setOrigin(10, 10);
      shipShape.setPosition(position);
      shipShape.setRotation(rotation);
      shipShape.setFillColor(tint);
      window.draw(shipShape);
    }
  }
};

class LayoverSimulation {
 private:
  vector<Ship> ships;
  map<int, PortDockingQueue> portQueues;
  bool isRunning;
  bool isPaused;
  Clock animationClock;
  float simulationSpeed;
  ShipSprite shipRenderer;

 public:
  LayoverSimulation()
      : isRunning(false), isPaused(false), simulationSpeed(1.0f) {}

  void initializeFromBookedRoutes(const vector<BookedRoute>& bookedRoutes,
                                  const Graph& g) {
    ships.clear();
    portQueues.clear();

    for (int i = 0; i < g.ports.size(); i++) {
      portQueues[i] = PortDockingQueue(g.ports[i].name, i);
    }

    for (const auto& booking : bookedRoutes) {
      Ship ship(booking.bookingID, booking.route, booking.originPort,
                booking.destPort);
      ship.currentLegIndex = 0;
      ship.isMoving = true;
      ships.push_back(ship);
    }

    isRunning = true;
    animationClock.restart();
  }

  void update(const vector<Location>& locations, const Graph& g) {
    if (!isRunning || isPaused) return;

    float deltaTime = animationClock.restart().asSeconds() * simulationSpeed;

    for (auto& pair : portQueues) {
      pair.second.processQueue(deltaTime);
    }

    for (auto& ship : ships) {
      if (ship.currentLegIndex >= ship.route.routeLegs.size()) {
        continue;
      }

      if (ship.isMoving) {
        ship.animationProgress += deltaTime * 0.1f;

        if (ship.animationProgress >= 1.0f) {
          ship.isMoving = false;
          ship.animationProgress = 1.0f;

          int nextPortIdx = ship.route.portPath[ship.currentLegIndex + 1];

          if (portQueues.find(nextPortIdx) != portQueues.end()) {
            portQueues[nextPortIdx].addShip(&ship);
          }
        }

        int fromIdx = ship.route.portPath[ship.currentLegIndex];
        int toIdx = ship.route.portPath[ship.currentLegIndex + 1];

        Vector2f fromPos = locations[fromIdx].position;
        Vector2f toPos = locations[toIdx].position;

        ship.position = fromPos + (toPos - fromPos) * ship.animationProgress;
      }
    }
  }

  void draw(RenderWindow& window, const vector<Location>& locations,
            const Graph& g) {
    if (!isRunning) return;
    for (const auto& pair : portQueues) {
      const PortDockingQueue& queue = pair.second;

      if (!queue.queue.empty()) {
        Vector2f portPos = locations[queue.portIndex].position;

        Text queueText;
        Font font;
        if (font.loadFromFile("arial.ttf")) {
          queueText.setFont(font);
          queueText.setString("Queue: " + to_string(queue.queue.size()));
          queueText.setCharacterSize(14);
          queueText.setFillColor(Color::Yellow);
          queueText.setPosition(portPos.x + 25, portPos.y - 25);
          window.draw(queueText);
        }

        float queueOffset = 30.0f;
        for (size_t i = 0; i < queue.queue.size(); i++) {
          CircleShape queueDot(3);
          queueDot.setFillColor(Color(255, 255, 0, 150));
          queueDot.setPosition(portPos.x + queueOffset + i * 10,
                               portPos.y + 30);
          window.draw(queueDot);
        }
      }
    }

    for (const auto& ship : ships) {
      if (ship.currentLegIndex >= ship.route.routeLegs.size()) continue;

      if (ship.isMoving) {
        int fromIdx = ship.route.portPath[ship.currentLegIndex];
        int toIdx = ship.route.portPath[ship.currentLegIndex + 1];

        Vector2f fromPos = locations[fromIdx].position;
        Vector2f toPos = locations[toIdx].position;

        float angle =
            atan2(toPos.y - fromPos.y, toPos.x - fromPos.x) * 180 / 3.14159f;

        shipRenderer.draw(window, ship.position, angle, ship.shipColor);

        Font font;
        if (font.loadFromFile("arial.ttf")) {
          Text shipInfo;
          shipInfo.setFont(font);
          shipInfo.setString(ship.shipID);
          shipInfo.setCharacterSize(10);
          shipInfo.setFillColor(Color::White);
          shipInfo.setPosition(ship.position.x + 15, ship.position.y - 5);
          window.draw(shipInfo);
        }
      } else if (ship.isDocked) {
        int portIdx = ship.route.portPath[ship.currentLegIndex + 1];
        Vector2f portPos = locations[portIdx].position;

        Vector2f dockedPos =
            portPos + Vector2f(20 + ship.queuePosition * 15, 40);

        shipRenderer.draw(
            window, dockedPos, 0,
            Color(ship.shipColor.r, ship.shipColor.g, ship.shipColor.b, 150));

        Font font;
        if (font.loadFromFile("arial.ttf")) {
          Text timerText;
          timerText.setFont(font);
          timerText.setString(to_string((int)ship.dockingTimeRemaining) + "s");
          timerText.setCharacterSize(10);
          timerText.setFillColor(Color::Cyan);
          timerText.setPosition(dockedPos.x, dockedPos.y + 15);
          window.draw(timerText);
        }
      }
    }
  }

  void togglePause() {
    isPaused = !isPaused;
    if (!isPaused) {
      animationClock.restart();
    }
  }

  void setSpeed(float speed) { simulationSpeed = speed; }

  bool isActive() const { return isRunning; }

  void stop() {
    isRunning = false;
    ships.clear();
    portQueues.clear();
  }

  string getStatistics() const {
    int movingShips = 0;
    int dockedShips = 0;
    int completedShips = 0;

    for (const auto& ship : ships) {
      if (ship.currentLegIndex >= ship.route.routeLegs.size()) {
        completedShips++;
      } else if (ship.isMoving) {
        movingShips++;
      } else if (ship.isDocked) {
        dockedShips++;
      }
    }

    return "Ships - Moving: " + to_string(movingShips) +
           " | Docked: " + to_string(dockedShips) +
           " | Completed: " + to_string(completedShips) +
           " | Total: " + to_string(ships.size());
  }
};

class LayoverControlPanel {
 private:
  Button* pauseButton;
  Button* speedUpButton;
  Button* slowDownButton;
  Button* stopButton;
  Text statsText;
  Font* font;
  float windowWidth;
  float windowHeight;

 public:
  LayoverControlPanel(Font& f, float winWidth, float winHeight)
      : font(&f), windowWidth(winWidth), windowHeight(winHeight) {
    float buttonWidth = 150;
    float buttonHeight = 40;
    float x = 20;
    float y = winHeight - 60;

    pauseButton = new Button(Vector2f(x, y),
                             Vector2f(buttonWidth, buttonHeight), "Pause", f);
    speedUpButton =
        new Button(Vector2f(x + 160, y), Vector2f(buttonWidth, buttonHeight),
                   "Speed Up", f);
    slowDownButton =
        new Button(Vector2f(x + 320, y), Vector2f(buttonWidth, buttonHeight),
                   "Slow Down", f);
    stopButton =
        new Button(Vector2f(x + 480, y), Vector2f(buttonWidth, buttonHeight),
                   "Stop & Exit", f);

    statsText.setFont(f);
    statsText.setCharacterSize(18);
    statsText.setFillColor(Color::White);
    statsText.setOutlineThickness(1);
    statsText.setOutlineColor(Color::Black);
    statsText.setPosition(20, 20);
  }

  ~LayoverControlPanel() {
    delete pauseButton;
    delete speedUpButton;
    delete slowDownButton;
    delete stopButton;
  }

  void updateStats(const string& stats) { statsText.setString(stats); }

  void update(RenderWindow& window, bool isPaused) {
    pauseButton->setText(isPaused ? "Resume" : "Pause");
    pauseButton->update(window);
    speedUpButton->update(window);
    slowDownButton->update(window);
    stopButton->update(window);
  }

  void draw(RenderWindow& window) {
    window.draw(statsText);
    pauseButton->draw(window);
    speedUpButton->draw(window);
    slowDownButton->draw(window);
    stopButton->draw(window);
  }

  enum ControlAction {
    NONE = 0,
    PAUSE = 1,
    SPEED_UP = 2,
    SLOW_DOWN = 3,
    STOP = 4
  };

  ControlAction checkClick(RenderWindow& window, Event& event) {
    if (pauseButton->isClicked(window, event)) return PAUSE;
    if (speedUpButton->isClicked(window, event)) return SPEED_UP;
    if (slowDownButton->isClicked(window, event)) return SLOW_DOWN;
    if (stopButton->isClicked(window, event)) return STOP;
    return NONE;
  }
};

class BookingConfirmationWindow {
 private:
  RectangleShape background;
  RectangleShape titleBar;
  Text titleText;
  vector<Text> confirmationDetails;
  Button* closeButton;
  bool isVisible;
  Font* font;
  float windowWidth;
  float windowHeight;

 public:
  BookingConfirmationWindow(Font& f, float winWidth, float winHeight)
      : font(&f),
        isVisible(false),
        closeButton(nullptr),
        windowWidth(winWidth),
        windowHeight(winHeight) {
    float width = windowWidth * 0.5f;
    float height = windowHeight * 0.6f;
    float x = (windowWidth - width) / 2;
    float y = (windowHeight - height) / 2;

    background.setSize(Vector2f(width, height));
    background.setPosition(x, y);
    background.setFillColor(Color(245, 255, 245));
    background.setOutlineThickness(3);
    background.setOutlineColor(Color(0, 150, 0));

    titleBar.setSize(Vector2f(width, 70));
    titleBar.setPosition(x, y);
    titleBar.setFillColor(Color(0, 150, 0));

    titleText.setFont(f);
    titleText.setString("Booking Confirmed!");
    titleText.setCharacterSize(32);
    titleText.setFillColor(Color::White);
    titleText.setStyle(Text::Bold);

    FloatRect bounds = titleText.getLocalBounds();
    titleText.setOrigin(bounds.left + bounds.width / 2,
                        bounds.top + bounds.height / 2);
    titleText.setPosition(x + width / 2, y + 35);

    closeButton = new Button(Vector2f(x + (width - 200) / 2, y + height - 80),
                             Vector2f(200, 50), "OK", f);
    closeButton->normalColor = Color(0, 150, 0);
    closeButton->hoverColor = Color(0, 200, 0);
    closeButton->shape.setFillColor(closeButton->normalColor);
  }

  ~BookingConfirmationWindow() { delete closeButton; }

  void show(const CompleteRoute& route, const string& from, const string& to,
            const string& date, const Graph& g) {
    isVisible = true;
    confirmationDetails.clear();

    float x = background.getPosition().x;
    float y = background.getPosition().y;
    float yPos = y + 100;

    string bookingID = "BK" + date.substr(0, 2) + date.substr(3, 2) +
                       date.substr(6, 4) + to_string(rand() % 10000);

    Text bookingIDText;
    bookingIDText.setFont(*font);
    bookingIDText.setString("Booking ID: " + bookingID);
    bookingIDText.setCharacterSize(24);
    bookingIDText.setFillColor(Color(0, 100, 0));
    bookingIDText.setStyle(Text::Bold);
    FloatRect bounds = bookingIDText.getLocalBounds();
    bookingIDText.setOrigin(bounds.left + bounds.width / 2, 0);
    bookingIDText.setPosition(x + background.getSize().x / 2, yPos);
    confirmationDetails.push_back(bookingIDText);
    yPos += 40;

    Text routeText;
    routeText.setFont(*font);
    routeText.setString("Route: " + from + " -> " + to);
    routeText.setCharacterSize(20);
    routeText.setFillColor(Color::Black);
    routeText.setPosition(x + 30, yPos);
    confirmationDetails.push_back(routeText);
    yPos += 35;

    Text dateText;
    dateText.setFont(*font);
    dateText.setString("Departure Date: " + date);
    dateText.setCharacterSize(18);
    dateText.setFillColor(Color::Black);
    dateText.setPosition(x + 30, yPos);
    confirmationDetails.push_back(dateText);
    yPos += 30;

    Text costText;
    costText.setFont(*font);
    costText.setString("Total Cost: $" + to_string(route.totalCost));
    costText.setCharacterSize(18);
    costText.setFillColor(Color::Black);
    costText.setPosition(x + 30, yPos);
    confirmationDetails.push_back(costText);
    yPos += 30;

    Text timeText;
    timeText.setFont(*font);
    timeText.setString("Travel Time: " + to_string(route.totalTime / 60) +
                       "h " + to_string(route.totalTime % 60) + "m");
    timeText.setCharacterSize(18);
    timeText.setFillColor(Color::Black);
    timeText.setPosition(x + 30, yPos);
    confirmationDetails.push_back(timeText);
    yPos += 30;

    Text stopsText;
    stopsText.setFont(*font);
    stopsText.setString("Number of Stops: " + to_string(route.layoverCount));
    stopsText.setCharacterSize(18);
    stopsText.setFillColor(Color::Black);
    stopsText.setPosition(x + 30, yPos);
    confirmationDetails.push_back(stopsText);
    yPos += 40;

    Text successText;
    successText.setFont(*font);
    successText.setString("Your cargo route has been successfully booked!");
    successText.setCharacterSize(16);
    successText.setFillColor(Color(0, 150, 0));
    successText.setStyle(Text::Italic);
    bounds = successText.getLocalBounds();
    successText.setOrigin(bounds.left + bounds.width / 2, 0);
    successText.setPosition(x + background.getSize().x / 2, yPos);
    confirmationDetails.push_back(successText);
  }

  void hide() { isVisible = false; }

  bool visible() const { return isVisible; }

  void update(RenderWindow& window) {
    if (isVisible) {
      closeButton->update(window);
    }
  }

  void draw(RenderWindow& window) {
    if (!isVisible) return;

    window.draw(background);
    window.draw(titleBar);
    window.draw(titleText);

    for (auto& text : confirmationDetails) {
      window.draw(text);
    }

    closeButton->draw(window);
  }

  bool checkClose(RenderWindow& window, Event& event) {
    if (isVisible && closeButton->isClicked(window, event)) {
      hide();
      return true;
    }
    return false;
  }
};
vector<string> extractAvailableDates(const vector<CompleteRoute>& routes) {
  set<string> uniqueDates;
  for (const auto& route : routes) {
    if (!route.routeLegs.empty()) {
      uniqueDates.insert(route.routeLegs[0].date);
    }
  }
  return vector<string>(uniqueDates.begin(), uniqueDates.end());
}
void showBookingRoute(const vector<Location>& locations, const Graph& g,
                      const CompleteRoute& route,
                      vector<RouteEdge>& bookingHighlightedRoutes) {
  bookingHighlightedRoutes.clear();

  Color routeColor;
  if (route.layoverCount == 0) {
    routeColor = Color::Magenta;
  } else if (route.layoverCount == 1) {
    routeColor = Color(255, 165, 0, 255);
  } else {
    routeColor = Color::Blue;
  }

  for (size_t i = 0; i < route.routeLegs.size(); i++) {
    int fromIdx = route.portPath[i];
    int toIdx = route.portPath[i + 1];

    RouteEdge edge(locations[fromIdx].position, locations[toIdx].position,
                   route.routeLegs[i], g.ports[fromIdx].name);
    edge.line.setFillColor(routeColor);
    bookingHighlightedRoutes.push_back(edge);
  }
}
enum GameState {
  MAIN_MENU,
  NAVIGATION_MENU,
  MAP_VIEW,
  SEARCH_ROUTES,
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
  TRACK_MULTI_LEG_BUILDING,
  TRACK_MULTI_LEG_ADD_PORT,
  FILTER_CUSTOM_COMPANIES,
  FILTER_CUSTOM_PORTS,
  FILTER_CUSTOM_TIME,
  GENERATE_SUBGRAPH,
  SUBGRAPH_COMPANY_SELECT,
  SUBGRAPH_WEATHER_SELECT,
  SUBGRAPH_VIEW,
  BOOK_CARGO,
  BOOK_CARGO_SELECT_PORTS,
  BOOK_CARGO_SELECT_DATE,
  BOOK_CARGO_VIEW_ROUTES,
  BOOK_CARGO_CONFIRM,
};

int main() {
  Graph g;
  g.parsePorts("PortCharges.txt");
  g.parseRoute("Routes.txt");
  g.parseWeatherData("WeatherData.txt");

  vector<string> weatherConditions = g.getAllWeatherConditions();
  vector<string> availableCompanies = g.getAllShippingCompanies();
  Font font;
  if (!font.loadFromFile("arial.ttf")) {
    cout << "Error: Could not load arial.ttf, trying system font..." << endl;
    if (!font.loadFromFile("C:/Windows/Fonts/arial.ttf")) {
      cout << "Error: Could not load any font!" << endl;
      return -1;
    }
  }

  RenderWindow window(VideoMode::getDesktopMode(), "Port Route Map",
                      Style::Fullscreen);
  Vector2u windowSize = window.getSize();
  float windowWidth = windowSize.x;
  float windowHeight = windowSize.y;

  Texture oceanTexture;
  Sprite oceanSprite;
  RectangleShape oceanBackground(Vector2f(windowWidth, windowHeight));
  oceanBackground.setFillColor(Color(20, 60, 100));

  bool oceanLoaded = false;
  if (oceanTexture.loadFromFile("ocean.png")) {
    oceanSprite.setTexture(oceanTexture);
    Vector2u oceanTextureSize = oceanTexture.getSize();
    float scaleX = (float)windowWidth / oceanTextureSize.x;
    float scaleY = (float)windowHeight / oceanTextureSize.y;
    oceanSprite.setScale(scaleX, scaleY);
    oceanLoaded = true;
  } else {
    cout << "Warning: Could not load ocean.png, using solid color background"
         << endl;
  }

  Texture mapTexture;
  Sprite mapSprite;
  vector<Location> locations;
  vector<RouteEdge> edges;
  RouteTooltip routeTooltip(font);
  InfoWindow infoWindow(font, windowWidth, windowHeight);
  RouteDisplayWindow routeDisplayWindow(font, windowWidth, windowHeight);
  int selectedEdgeIndex = -1;
  SubgraphMenu subgraphMenu(font, windowWidth, windowHeight, oceanTexture);
  vector<int> selectedPorts;
  ShortestRouteResult shortestRouteResult;
  vector<RouteEdge> shortestPathEdges;
  bool shortestRouteCalculated = false;
  vector<RouteEdge> cheapestPathEdges;
  bool cheapestRouteCalculated = false;
  UserPreferences userPreferences = {{}, {}, 999999, false, false, false};
  MultiLegJourney currentJourney;
  int selectedJourneyPortIdx = -1;
  bool journeyBuildingMode = false;
  vector<RouteEdge> availableRouteEdges;
  vector<RouteEdge> journeyPathEdges;
  PortAdditionWindow portAddWindow(font, windowWidth, windowHeight);
  JourneyControlPanel journeyControls(font, windowWidth, windowHeight);
  bool showingAvailableRoutes = false;
  DateSelectionWindow dateSelectionWindow(font, windowWidth, windowHeight);
  RouteBookingWindow routeBookingWindow(font, windowWidth, windowHeight);
  BookingConfirmationWindow bookingConfirmationWindow(font, windowWidth,
                                                      windowHeight);
  vector<BookedRoute> bookedRoutes;

  vector<int> bookingSelectedPorts;
  string selectedBookingDate = "";
  vector<CompleteRoute> availableBookingRoutes;
  int selectedBookingRouteIndex = -1;
  bool showingBookingRoutes = false;
  vector<RouteEdge> bookingHighlightedRoutes;
  Button* bookRouteButton = nullptr;
  LayoverSimulation layoverSim;
  LayoverControlPanel* layoverControls = nullptr;
  float currentSimSpeed = 1.0f;
  FilterSidebar filterSidebar(font, windowWidth, windowHeight);
  int filterMenuState = 0;
  vector<Button*> filterButtons;
  Text filterMenuTitle;

  FilterPopupWindow filterPopup(font, windowWidth, windowHeight);
  Button* applyFiltersButton =
      new Button(Vector2f(windowWidth - 240, windowHeight - 80),
                 Vector2f(220, 50), "Apply Filters", font);
  Button* removeFiltersButton =
      new Button(Vector2f(windowWidth - 240, windowHeight - 140),
                 Vector2f(220, 50), "Remove Filters", font);
  vector<RouteEdge> highlightedRoutes;
  bool routesDisplayed = false;
  Text instructionText;
  instructionText.setFont(font);
  instructionText.setCharacterSize(24);
  instructionText.setFillColor(Color::White);
  instructionText.setOutlineThickness(2);
  instructionText.setOutlineColor(Color::Black);
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
  float buttonWidth = 400;
  float buttonHeight = 70;
  float centerX = (windowWidth - buttonWidth) / 2;
  bool addingBefore = false;
  Button startButton(Vector2f(centerX, windowHeight * 0.35f),
                     Vector2f(buttonWidth, buttonHeight),
                     "Start Route Navigation", font);
  Button settingsButton(Vector2f(centerX, windowHeight * 0.50f),
                        Vector2f(buttonWidth, buttonHeight), "Settings", font);
  Button exitButton(Vector2f(centerX, windowHeight * 0.65f),
                    Vector2f(buttonWidth, buttonHeight), "Exit", font);
  NavigationMenu navMenu(font, windowWidth, windowHeight);
  FilterPreferencesMenu filterMenu(font, windowWidth, windowHeight,
                                   oceanTexture);
  GameState currentState = MAIN_MENU;
  CompanySelectionWindow companyWindow(font, windowWidth, windowHeight);
  PortAvoidanceWindow portWindow(font, windowWidth, windowHeight);
  VoyageTimeWindow timeWindow(font, windowWidth, windowHeight);
  while (window.isOpen()) {
    Event event;
    while (window.pollEvent(event)) {
      if (event.type == Event::Closed) window.close();
      if (event.type == Event::Resized) {
        windowWidth = event.size.width;
        windowHeight = event.size.height;

        View view(FloatRect(0, 0, event.size.width, event.size.height));
        window.setView(view);

        if (oceanLoaded) {
          Vector2u oceanTextureSize = oceanTexture.getSize();
          float scaleX = (float)windowWidth / oceanTextureSize.x;
          float scaleY = (float)windowHeight / oceanTextureSize.y;
          oceanSprite.setScale(scaleX, scaleY);
        }

        oceanBackground.setSize(Vector2f(windowWidth, windowHeight));

        infoWindow.handleResize(windowWidth, windowHeight);
        routeDisplayWindow.handleResize(windowWidth, windowHeight);
        filterPopup.handleResize(windowWidth, windowHeight);
        companyWindow.handleResize(windowWidth, windowHeight);
        portWindow.handleResize(windowWidth, windowHeight);
        timeWindow.handleResize(windowWidth, windowHeight);
        navMenu.handleResize(windowWidth, windowHeight);

        applyFiltersButton->setPosition(
            Vector2f(windowWidth - 240, windowHeight - 80));
        removeFiltersButton->setPosition(
            Vector2f(windowWidth - 240, windowHeight - 140));

        if (currentState == MAP_VIEW || currentState == SEARCH_ROUTES ||
            currentState == SHORTEST_ROUTE_SELECT ||
            currentState == CHEAPEST_ROUTE_SELECT) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);

          if (userPreferences.hasCompanyFilter ||
              userPreferences.hasPortFilter || userPreferences.hasTimeFilter) {
            customShipPreferences(userPreferences, locations, edges, g);
          }
        }
      }
      if (event.type == Event::MouseWheelScrolled) {
        if (infoWindow.isMouseOverWindow(window)) {
          infoWindow.scroll(event.mouseWheelScroll.delta);
        }
        if (routeDisplayWindow.isMouseOverWindow(window)) {
          routeDisplayWindow.scroll(event.mouseWheelScroll.delta);
        }
      }

      if (infoWindow.checkClose(window, event)) {
        if (selectedEdgeIndex >= 0 && selectedEdgeIndex < edges.size()) {
          edges[selectedEdgeIndex].setSelected(false);
          selectedEdgeIndex = -1;
        }
        continue;
      }

      if (routeDisplayWindow.checkClose(window, event)) {
        highlightedRoutes.clear();
        routesDisplayed = false;
        continue;
      }

      if (currentState == MAIN_MENU) {
        if (event.type == Event::KeyPressed &&
            event.key.code == Keyboard::Escape) {
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

      else if (currentState == NAVIGATION_MENU) {
        int clicked = navMenu.checkClick(window, event);
        if (clicked == 0) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
          selectedPorts.clear();
          highlightedRoutes.clear();
          routesDisplayed = false;
          currentState = SEARCH_ROUTES;
          navMenu.hide();
        } else if (clicked == 1) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
          bookingSelectedPorts.clear();
          selectedBookingDate = "";
          availableBookingRoutes.clear();
          bookingHighlightedRoutes.clear();
          showingBookingRoutes = false;
          selectedBookingRouteIndex = -1;

          for (auto& location : locations) {
            location.pin.setOutlineColor(Color::White);
          }

          currentState = BOOK_CARGO_SELECT_PORTS;
          navMenu.hide();
        } else if (clicked == 2) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
          selectedEdgeIndex = -1;
          currentState = MAP_VIEW;
          navMenu.hide();
        } else if (clicked == 3) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
          selectedEdgeIndex = -1;
          selectedPorts.clear();
          shortestPathEdges.clear();
          shortestRouteCalculated = false;
          currentState = SHORTEST_ROUTE_SELECT;
          navMenu.hide();
        } else if (clicked == 4) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
          selectedEdgeIndex = -1;
          selectedPorts.clear();
          cheapestPathEdges.clear();
          cheapestRouteCalculated = false;
          currentState = CHEAPEST_ROUTE_SELECT;
          navMenu.hide();
        } else if (clicked == 5) {
          currentState = GENERATE_SUBGRAPH;
          subgraphMenu.show();
          navMenu.hide();
        } else if (clicked == 6) {
          if (bookedRoutes.empty()) {
            Text noBookings;
            noBookings.setFont(font);
            noBookings.setString(
                "No cargo routes booked yet!\nPlease book routes first.");
            noBookings.setCharacterSize(30);
            noBookings.setFillColor(Color::Red);
          } else {
            loadMapView(mapTexture, mapSprite, locations, edges, window, font,
                        g);
            layoverSim.initializeFromBookedRoutes(bookedRoutes, g);
            currentState = PROCESS_LAYOVERS;
            navMenu.hide();
          }
        } else if (clicked == 7) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);

          currentJourney = MultiLegJourney();
          currentJourney.isComplete = false;
          currentJourney.totalCost = 0;
          currentJourney.totalTime = 0;
          selectedJourneyPortIdx = -1;
          journeyBuildingMode = true;
          showingAvailableRoutes = false;
          availableRouteEdges.clear();
          journeyPathEdges.clear();

          currentState = TRACK_MULTI_LEG;
          navMenu.hide();
          cout << "Starting multi-leg journey creation" << endl;
        } else if (clicked == 8) {
          currentState = MAIN_MENU;
          navMenu.hide();
        }
      } else if (currentState == MAP_VIEW && !infoWindow.visible()) {
        if (filterPopup.visible()) {
          int popupClick = filterPopup.checkClick(window, event);
          if (popupClick == 0) {
            filterPopup.hide();
            companyWindow.show(availableCompanies,
                               userPreferences.preferredCompanies);
          } else if (popupClick == 1) {
            filterPopup.hide();
            portWindow.show(g.ports, userPreferences.avoidPorts);
          } else if (popupClick == 2) {
            filterPopup.hide();
            timeWindow.show(userPreferences.maxVoyageTime);
          } else if (popupClick == 3) {
            loadMapView(mapTexture, mapSprite, locations, edges, window, font,
                        g);
            customShipPreferences(userPreferences, locations, edges, g);
            filterPopup.hide();
            cout << "Filters applied!" << endl;
          }
        } else if (companyWindow.visible()) {
          int companyClick =
              companyWindow.checkClick(window, event, availableCompanies);
          if (companyClick == -2) {
            companyWindow.hide();
            filterPopup.show();
          } else if (companyClick == -3) {
            userPreferences.preferredCompanies =
                companyWindow.getSelectedCompanies();
            userPreferences.hasCompanyFilter =
                !userPreferences.preferredCompanies.empty();
            companyWindow.hide();
            filterPopup.show();
            cout << "Company filter set: "
                 << userPreferences.preferredCompanies.size() << " companies"
                 << endl;
          }
        } else if (portWindow.visible()) {
          int portClick = portWindow.checkClick(window, event, g.ports);
          if (portClick == -2) {
            portWindow.hide();
            filterPopup.show();
          } else if (portClick == -3) {
            userPreferences.avoidPorts = portWindow.getAvoidedPorts();
            userPreferences.hasPortFilter = !userPreferences.avoidPorts.empty();
            portWindow.hide();
            filterPopup.show();
            cout << "Port avoidance set: " << userPreferences.avoidPorts.size()
                 << " ports" << endl;
          }
        } else if (timeWindow.visible()) {
          int timeClick = timeWindow.checkClick(window, event);
          if (timeClick == -2) {
            timeWindow.hide();
            filterPopup.show();
          } else if (timeClick >= 0) {
            if (timeClick == 0) {
              userPreferences.maxVoyageTime = 8 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 1) {
              userPreferences.maxVoyageTime = 16 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 2) {
              userPreferences.maxVoyageTime = 24 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 3) {
              userPreferences.maxVoyageTime = 48 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 4) {
              userPreferences.hasTimeFilter = false;
              userPreferences.maxVoyageTime = 999999;
            }
            timeWindow.hide();
            filterPopup.show();
            cout << "Voyage time filter set" << endl;
          }
        } else if (applyFiltersButton->isClicked(window, event)) {
          filterPopup.show();
        } else if (removeFiltersButton->isClicked(window, event)) {
          userPreferences.preferredCompanies.clear();
          userPreferences.avoidPorts.clear();
          userPreferences.hasCompanyFilter = false;
          userPreferences.hasPortFilter = false;
          userPreferences.hasTimeFilter = false;
          userPreferences.maxVoyageTime = 999999;
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
          cout << "All filters removed!" << endl;
        } else if (!filterPopup.isMouseOverWindow(window)) {
          bool locationClicked = false;

          for (auto& location : locations) {
            if (userPreferences.hasPortFilter &&
                g.portIsAvoid(location.name, userPreferences)) {
              continue;
            }

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
              bool sourceAvoid =
                  g.portIsAvoid(edges[i].sourceName, userPreferences);
              bool destAvoid = g.portIsAvoid(edges[i].routeInfo.destination,
                                             userPreferences);

              if (sourceAvoid || destAvoid ||
                  !g.routeMatchesPreferences(edges[i].routeInfo,
                                             userPreferences)) {
                continue;
              }

              if (edges[i].isClicked(window, event)) {
                if (selectedEdgeIndex >= 0 &&
                    selectedEdgeIndex < edges.size()) {
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
      } else if (currentState == SEARCH_ROUTES && !infoWindow.visible()) {
        if (filterPopup.visible()) {
          int popupClick = filterPopup.checkClick(window, event);
          if (popupClick == 0) {
            filterPopup.hide();
            companyWindow.show(availableCompanies,
                               userPreferences.preferredCompanies);
          } else if (popupClick == 1) {
            filterPopup.hide();
            portWindow.show(g.ports, userPreferences.avoidPorts);
          } else if (popupClick == 2) {
            filterPopup.hide();
            timeWindow.show(userPreferences.maxVoyageTime);
          } else if (popupClick == 3) {
            loadMapView(mapTexture, mapSprite, locations, edges, window, font,
                        g);
            customShipPreferences(userPreferences, locations, edges, g);
            filterPopup.hide();
            cout << "Filters applied!" << endl;
          }
        } else if (companyWindow.visible()) {
          int companyClick =
              companyWindow.checkClick(window, event, availableCompanies);
          if (companyClick == -2) {
            companyWindow.hide();
            filterPopup.show();
          } else if (companyClick == -3) {
            userPreferences.preferredCompanies =
                companyWindow.getSelectedCompanies();
            userPreferences.hasCompanyFilter =
                !userPreferences.preferredCompanies.empty();
            companyWindow.hide();
            filterPopup.show();
            cout << "Company filter set: "
                 << userPreferences.preferredCompanies.size() << " companies"
                 << endl;
          }
        } else if (portWindow.visible()) {
          int portClick = portWindow.checkClick(window, event, g.ports);
          if (portClick == -2) {
            portWindow.hide();
            filterPopup.show();
          } else if (portClick == -3) {
            userPreferences.avoidPorts = portWindow.getAvoidedPorts();
            userPreferences.hasPortFilter = !userPreferences.avoidPorts.empty();
            portWindow.hide();
            filterPopup.show();
            cout << "Port avoidance set: " << userPreferences.avoidPorts.size()
                 << " ports" << endl;
          }
        } else if (timeWindow.visible()) {
          int timeClick = timeWindow.checkClick(window, event);
          if (timeClick == -2) {
            timeWindow.hide();
            filterPopup.show();
          } else if (timeClick >= 0) {
            if (timeClick == 0) {
              userPreferences.maxVoyageTime = 8 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 1) {
              userPreferences.maxVoyageTime = 16 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 2) {
              userPreferences.maxVoyageTime = 24 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 3) {
              userPreferences.maxVoyageTime = 48 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 4) {
              userPreferences.hasTimeFilter = false;
              userPreferences.maxVoyageTime = 999999;
            }
            timeWindow.hide();
            filterPopup.show();
            cout << "Voyage time filter set" << endl;
          }
        } else if (applyFiltersButton->isClicked(window, event)) {
          filterPopup.show();
        } else if (removeFiltersButton->isClicked(window, event)) {
          userPreferences.preferredCompanies.clear();
          userPreferences.avoidPorts.clear();
          userPreferences.hasCompanyFilter = false;
          userPreferences.hasPortFilter = false;
          userPreferences.hasTimeFilter = false;
          userPreferences.maxVoyageTime = 999999;
          highlightedRoutes.clear();
          routesDisplayed = false;
          selectedPorts.clear();
          for (auto& loc : locations) {
            loc.pin.setOutlineColor(Color::White);
          }
          cout << "All filters removed!" << endl;
        } else if (!filterPopup.isMouseOverWindow(window)) {
          bool locationClicked = false;

          for (int i = 0; i < locations.size(); i++) {
            if (userPreferences.hasPortFilter &&
                g.portIsAvoid(locations[i].name, userPreferences)) {
              continue;
            }

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
                selectedPorts.erase(
                    remove(selectedPorts.begin(), selectedPorts.end(), i),
                    selectedPorts.end());
                locations[i].pin.setOutlineColor(Color::White);
                cout << "Deselected port: " << locations[i].name << endl;
                highlightedRoutes.clear();
                routesDisplayed = false;
              }

              if (selectedPorts.size() == 2) {
                int srcIdx = selectedPorts[0];
                int destIdx = selectedPorts[1];

                vector<CompleteRoute> allRoutes =
                    g.findAllPossibleRoutes(srcIdx, destIdx);
                vector<CompleteRoute> filteredRoutes;

                for (const auto& route : allRoutes) {
                  bool routeValid = true;

                  for (const Route& leg : route.routeLegs) {
                    if (!g.routeMatchesPreferences(leg, userPreferences)) {
                      routeValid = false;
                      break;
                    }
                  }

                  if (routeValid) {
                    for (int portIdx : route.portPath) {
                      if (g.portIsAvoid(g.ports[portIdx].name,
                                        userPreferences)) {
                        routeValid = false;
                        break;
                      }
                    }
                  }

                  if (routeValid) {
                    filteredRoutes.push_back(route);
                  }
                }

                highlightedRoutes.clear();

                if (filteredRoutes.empty()) {
                  vector<string> info;
                  info.push_back("No routes found matching your filters");
                  infoWindow.show("No Routes", info);
                } else {
                  for (const auto& completeRoute : filteredRoutes) {
                    Color routeColor;
                    if (completeRoute.layoverCount == 0) {
                      routeColor = Color::Magenta;
                    } else if (completeRoute.layoverCount == 1) {
                      routeColor = Color(255, 165, 0, 255);
                    } else {
                      routeColor = Color::Blue;
                    }

                    for (size_t i = 0; i < completeRoute.routeLegs.size();
                         i++) {
                      int fromIdx = completeRoute.portPath[i];
                      int toIdx = completeRoute.portPath[i + 1];

                      RouteEdge edge(locations[fromIdx].position,
                                     locations[toIdx].position,
                                     completeRoute.routeLegs[i],
                                     g.ports[fromIdx].name);
                      edge.line.setFillColor(routeColor);
                      highlightedRoutes.push_back(edge);
                    }
                  }

                  vector<string> routeDisplay =
                      g.formatAllRoutes(srcIdx, destIdx);
                  routeDisplayWindow.show(g.ports[srcIdx].name,
                                          g.ports[destIdx].name, routeDisplay);
                  routesDisplayed = true;
                }
              }

              locationClicked = true;
              break;
            }
          }

          if (!locationClicked && routesDisplayed) {
            for (int i = 0; i < highlightedRoutes.size(); i++) {
              if (highlightedRoutes[i].isClicked(window, event)) {
                vector<string> routeDetails =
                    highlightedRoutes[i].getRouteDetails();
                infoWindow.show("Route Information", routeDetails);
                break;
              }
            }
          }
        }
      } else if (currentState == SHORTEST_ROUTE_SELECT &&
                 !infoWindow.visible()) {
        if (filterPopup.visible()) {
          int popupClick = filterPopup.checkClick(window, event);
          if (popupClick == 0) {
            filterPopup.hide();
            companyWindow.show(availableCompanies,
                               userPreferences.preferredCompanies);
          } else if (popupClick == 1) {
            filterPopup.hide();
            portWindow.show(g.ports, userPreferences.avoidPorts);
          } else if (popupClick == 2) {
            filterPopup.hide();
            timeWindow.show(userPreferences.maxVoyageTime);
          } else if (popupClick == 3) {
            loadMapView(mapTexture, mapSprite, locations, edges, window, font,
                        g);
            customShipPreferences(userPreferences, locations, edges, g);
            filterPopup.hide();
            cout << "Filters applied!" << endl;
          }
        } else if (companyWindow.visible()) {
          int companyClick =
              companyWindow.checkClick(window, event, availableCompanies);
          if (companyClick == -2) {
            companyWindow.hide();
            filterPopup.show();
          } else if (companyClick == -3) {
            userPreferences.preferredCompanies =
                companyWindow.getSelectedCompanies();
            userPreferences.hasCompanyFilter =
                !userPreferences.preferredCompanies.empty();
            companyWindow.hide();
            filterPopup.show();
            cout << "Company filter set: "
                 << userPreferences.preferredCompanies.size() << " companies"
                 << endl;
          }
        } else if (portWindow.visible()) {
          int portClick = portWindow.checkClick(window, event, g.ports);
          if (portClick == -2) {
            portWindow.hide();
            filterPopup.show();
          } else if (portClick == -3) {
            userPreferences.avoidPorts = portWindow.getAvoidedPorts();
            userPreferences.hasPortFilter = !userPreferences.avoidPorts.empty();
            portWindow.hide();
            filterPopup.show();
            cout << "Port avoidance set: " << userPreferences.avoidPorts.size()
                 << " ports" << endl;
          }
        } else if (timeWindow.visible()) {
          int timeClick = timeWindow.checkClick(window, event);
          if (timeClick == -2) {
            timeWindow.hide();
            filterPopup.show();
          } else if (timeClick >= 0) {
            if (timeClick == 0) {
              userPreferences.maxVoyageTime = 8 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 1) {
              userPreferences.maxVoyageTime = 16 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 2) {
              userPreferences.maxVoyageTime = 24 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 3) {
              userPreferences.maxVoyageTime = 48 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 4) {
              userPreferences.hasTimeFilter = false;
              userPreferences.maxVoyageTime = 999999;
            }
            timeWindow.hide();
            filterPopup.show();
            cout << "Voyage time filter set" << endl;
          }
        } else if (applyFiltersButton->isClicked(window, event)) {
          filterPopup.show();
        } else if (removeFiltersButton->isClicked(window, event)) {
          userPreferences.preferredCompanies.clear();
          userPreferences.avoidPorts.clear();
          userPreferences.hasCompanyFilter = false;
          userPreferences.hasPortFilter = false;
          userPreferences.hasTimeFilter = false;
          userPreferences.maxVoyageTime = 999999;
          selectedPorts.clear();
          shortestPathEdges.clear();
          shortestRouteCalculated = false;
          for (auto& loc : locations) {
            loc.pin.setOutlineColor(Color::White);
          }
          cout << "All filters removed!" << endl;
        } else if (!filterPopup.isMouseOverWindow(window)) {
          bool locationClicked = false;
          for (int i = 0; i < locations.size(); i++) {
            if (userPreferences.hasPortFilter &&
                g.portIsAvoid(locations[i].name, userPreferences)) {
              continue;
            }

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
              } else if (alreadySelected) {
                selectedPorts.erase(
                    remove(selectedPorts.begin(), selectedPorts.end(), i),
                    selectedPorts.end());
                locations[i].pin.setOutlineColor(Color::White);
              }

              if (selectedPorts.size() == 2) {
                int srcIdx = selectedPorts[0];
                int destIdx = selectedPorts[1];
                if (userPreferences.hasCompanyFilter ||
                    userPreferences.hasPortFilter ||
                    userPreferences.hasTimeFilter) {
                  shortestRouteResult =
                      g.findShortestRoute(&g.ports[srcIdx], &userPreferences);
                  cout << "Computing shortest route WITH FILTERS applied"
                       << endl;
                } else {
                  shortestRouteResult =
                      g.findShortestRoute(&g.ports[srcIdx], &userPreferences);
                  cout << "Computing shortest route (no filters)" << endl;
                }

                if (shortestRouteResult.found &&
                    shortestRouteResult.dist[destIdx] != INF) {
                  vector<int> path;
                  int curr = destIdx;
                  while (curr != -1) {
                    path.push_back(curr);
                    curr = shortestRouteResult.parent[curr];
                  }
                  reverse(path.begin(), path.end());
                  vector<Route> routeLegs;
                  int totalTime = 0;
                  int totalCost = 0;

                  shortestPathEdges.clear();
                  for (int i = 0; i < path.size() - 1; i++) {
                    int from = path[i];
                    int to = path[i + 1];

                    bool legFound = false;
                    for (const Route& route : g.routes[from]) {
                      if (route.destination == g.ports[to].name) {
                        bool routeValid = true;
                        if (userPreferences.hasCompanyFilter) {
                          bool found = false;
                          for (const string& comp :
                               userPreferences.preferredCompanies) {
                            if (route.company == comp) {
                              found = true;
                              break;
                            }
                          }
                          routeValid = found;
                        }
                        if (routeValid && userPreferences.hasTimeFilter) {
                          routeValid =
                              route.travelTime <= userPreferences.maxVoyageTime;
                        }

                        if (routeValid) {
                          if (!routeLegs.empty()) {
                            if (!isValidLegTransition(routeLegs.back(),
                                                      route)) {
                              continue;
                            }
                          }

                          routeLegs.push_back(route);
                          totalTime += route.travelTime;
                          totalCost += route.cost;

                          RouteEdge edge(locations[from].position,
                                         locations[to].position, route,
                                         g.ports[from].name);
                          edge.line.setFillColor(Color::Green);
                          shortestPathEdges.push_back(edge);
                          legFound = true;
                          break;
                        }
                      }
                    }

                    if (!legFound) {
                      cout << "Warning: No valid route leg found from "
                           << g.ports[from].name << " to " << g.ports[to].name
                           << endl;
                    }
                  }

                  for (size_t i = 1; i < path.size(); i++) {
                    totalCost += g.ports[path[i]].cost;
                  }

                  shortestRouteCalculated = true;

                  string pathString =
                      g.buildPathString(shortestRouteResult.parent, destIdx);

                  vector<string> shortestRouteDisplay;
                  shortestRouteDisplay.push_back(
                      "=== SHORTEST ROUTE FOUND ===");
                  shortestRouteDisplay.push_back("");
                  shortestRouteDisplay.push_back("Path: " + pathString);
                  shortestRouteDisplay.push_back(
                      "Total Time: " + to_string(totalTime / 60) + "h " +
                      to_string(totalTime % 60) + "m");
                  shortestRouteDisplay.push_back("Total Cost: $" +
                                                 to_string(totalCost));
                  shortestRouteDisplay.push_back("Number of Legs: " +
                                                 to_string(routeLegs.size()));
                  shortestRouteDisplay.push_back(
                      "Layovers: " + to_string(max(0, (int)path.size() - 2)));
                  shortestRouteDisplay.push_back("");
                  for (size_t i = 0; i < routeLegs.size(); i++) {
                    const Route& leg = routeLegs[i];
                    shortestRouteDisplay.push_back("--- LEG " +
                                                   to_string(i + 1) + " ---");
                    shortestRouteDisplay.push_back(
                        "From: " + g.ports[path[i]].name +
                        " -> To: " + g.ports[path[i + 1]].name);
                    shortestRouteDisplay.push_back("Date: " + leg.date);
                    shortestRouteDisplay.push_back("Time: " + leg.depTime +
                                                   " - " + leg.arrTime);
                    shortestRouteDisplay.push_back(
                        "Duration: " + to_string(leg.travelTime / 60) + "h " +
                        to_string(leg.travelTime % 60) + "m");
                    shortestRouteDisplay.push_back("Cost: $" +
                                                   to_string(leg.cost));
                    shortestRouteDisplay.push_back("Company: " + leg.company);
                    shortestRouteDisplay.push_back("");
                  }
                  routeDisplayWindow.show(g.ports[srcIdx].name,
                                          g.ports[destIdx].name,
                                          shortestRouteDisplay);

                } else {
                  vector<string> info;
                  if (userPreferences.hasCompanyFilter ||
                      userPreferences.hasPortFilter ||
                      userPreferences.hasTimeFilter) {
                    info.push_back("No shortest route found!");
                    info.push_back("");
                    info.push_back("This may be because:");
                    info.push_back("- No routes match the selected companies");
                    info.push_back("- Route passes through avoided ports");
                    info.push_back("- No routes within max voyage time");
                  } else {
                    info.push_back("No route found between these ports");
                  }
                  infoWindow.show("Route Not Found", info);
                }
              }

              locationClicked = true;
              break;
            }
          }

          if (!locationClicked && shortestRouteCalculated) {
            for (int i = 0; i < shortestPathEdges.size(); i++) {
              if (shortestPathEdges[i].isClicked(window, event)) {
                vector<string> routeDetails =
                    shortestPathEdges[i].getRouteDetails();
                infoWindow.show("Route Information", routeDetails);
                break;
              }
            }
          }
        }
      }

      else if (currentState == CHEAPEST_ROUTE_SELECT && !infoWindow.visible()) {
        if (filterPopup.visible()) {
          int popupClick = filterPopup.checkClick(window, event);
          if (popupClick == 0) {
            filterPopup.hide();
            companyWindow.show(availableCompanies,
                               userPreferences.preferredCompanies);
          } else if (popupClick == 1) {
            filterPopup.hide();
            portWindow.show(g.ports, userPreferences.avoidPorts);
          } else if (popupClick == 2) {
            filterPopup.hide();
            timeWindow.show(userPreferences.maxVoyageTime);
          } else if (popupClick == 3) {
            loadMapView(mapTexture, mapSprite, locations, edges, window, font,
                        g);
            customShipPreferences(userPreferences, locations, edges, g);
            filterPopup.hide();
            cout << "Filters applied!" << endl;
          }
        } else if (companyWindow.visible()) {
          int companyClick =
              companyWindow.checkClick(window, event, availableCompanies);
          if (companyClick == -2) {
            companyWindow.hide();
            filterPopup.show();
          } else if (companyClick == -3) {
            userPreferences.preferredCompanies =
                companyWindow.getSelectedCompanies();
            userPreferences.hasCompanyFilter =
                !userPreferences.preferredCompanies.empty();
            companyWindow.hide();
            filterPopup.show();
            cout << "Company filter set: "
                 << userPreferences.preferredCompanies.size() << " companies"
                 << endl;
          }
        } else if (portWindow.visible()) {
          int portClick = portWindow.checkClick(window, event, g.ports);
          if (portClick == -2) {
            portWindow.hide();
            filterPopup.show();
          } else if (portClick == -3) {
            userPreferences.avoidPorts = portWindow.getAvoidedPorts();
            userPreferences.hasPortFilter = !userPreferences.avoidPorts.empty();
            portWindow.hide();
            filterPopup.show();
            cout << "Port avoidance set: " << userPreferences.avoidPorts.size()
                 << " ports" << endl;
          }
        } else if (timeWindow.visible()) {
          int timeClick = timeWindow.checkClick(window, event);
          if (timeClick == -2) {
            timeWindow.hide();
            filterPopup.show();
          } else if (timeClick >= 0) {
            if (timeClick == 0) {
              userPreferences.maxVoyageTime = 8 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 1) {
              userPreferences.maxVoyageTime = 16 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 2) {
              userPreferences.maxVoyageTime = 24 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 3) {
              userPreferences.maxVoyageTime = 48 * 60;
              userPreferences.hasTimeFilter = true;
            } else if (timeClick == 4) {
              userPreferences.hasTimeFilter = false;
              userPreferences.maxVoyageTime = 999999;
            }
            timeWindow.hide();
            filterPopup.show();
            cout << "Voyage time filter set" << endl;
          }
        } else if (applyFiltersButton->isClicked(window, event)) {
          filterPopup.show();
        } else if (removeFiltersButton->isClicked(window, event)) {
          userPreferences.preferredCompanies.clear();
          userPreferences.avoidPorts.clear();
          userPreferences.hasCompanyFilter = false;
          userPreferences.hasPortFilter = false;
          userPreferences.hasTimeFilter = false;
          userPreferences.maxVoyageTime = 999999;
          selectedPorts.clear();
          cheapestPathEdges.clear();
          cheapestRouteCalculated = false;
          for (auto& loc : locations) {
            loc.pin.setOutlineColor(Color::White);
          }
          cout << "All filters removed!" << endl;
        } else if (!filterPopup.isMouseOverWindow(window)) {
          bool locationClicked = false;
          for (int i = 0; i < locations.size(); i++) {
            if (userPreferences.hasPortFilter &&
                g.portIsAvoid(locations[i].name, userPreferences)) {
              continue;
            }

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
              } else if (alreadySelected) {
                selectedPorts.erase(
                    remove(selectedPorts.begin(), selectedPorts.end(), i),
                    selectedPorts.end());
                locations[i].pin.setOutlineColor(Color::White);
              }

              if (selectedPorts.size() == 2) {
                int srcIdx = selectedPorts[0];
                int destIdx = selectedPorts[1];

                shortestRouteResult =
                    g.findCheapestRoute(&g.ports[srcIdx], &userPreferences);
                if (shortestRouteResult.found &&
                    shortestRouteResult.dist[destIdx] != INF) {
                  vector<int> path;
                  int curr = destIdx;
                  while (curr != -1) {
                    path.push_back(curr);
                    curr = shortestRouteResult.parent[curr];
                  }
                  reverse(path.begin(), path.end());
                  vector<Route> routeLegs;
                  int totalTime = 0;
                  int totalCost = shortestRouteResult.dist[destIdx];

                  cheapestPathEdges.clear();
                  for (int i = 0; i < path.size() - 1; i++) {
                    int from = path[i];
                    int to = path[i + 1];

                    bool legFound = false;
                    for (const Route& route : g.routes[from]) {
                      if (route.destination == g.ports[to].name) {
                        bool routeValid =
                            g.routeMatchesPreferences(route, userPreferences);

                        if (routeValid) {
                          if (!routeLegs.empty()) {
                            if (!isValidLegTransition(routeLegs.back(),
                                                      route)) {
                              continue;
                            }
                          }

                          routeLegs.push_back(route);
                          totalTime += route.travelTime;

                          RouteEdge edge(locations[from].position,
                                         locations[to].position, route,
                                         g.ports[from].name);
                          edge.line.setFillColor(Color::Blue);
                          cheapestPathEdges.push_back(edge);
                          legFound = true;
                          break;
                        }
                      }
                    }

                    if (!legFound) {
                      cout << "Warning: No valid route leg found from "
                           << g.ports[from].name << " to " << g.ports[to].name
                           << endl;
                    }
                  }

                  cheapestRouteCalculated = true;

                  string pathString =
                      g.buildPathString(shortestRouteResult.parent, destIdx);

                  vector<string> cheapestRouteDisplay;
                  cheapestRouteDisplay.push_back(
                      "=== CHEAPEST ROUTE FOUND ===");
                  cheapestRouteDisplay.push_back("");
                  cheapestRouteDisplay.push_back("Path: " + pathString);
                  cheapestRouteDisplay.push_back("Total Cost: $" +
                                                 to_string(totalCost));
                  cheapestRouteDisplay.push_back(
                      "Total Time: " + to_string(totalTime / 60) + "h " +
                      to_string(totalTime % 60) + "m");
                  cheapestRouteDisplay.push_back("Number of Legs: " +
                                                 to_string(routeLegs.size()));
                  cheapestRouteDisplay.push_back(
                      "Layovers: " + to_string(max(0, (int)path.size() - 2)));
                  cheapestRouteDisplay.push_back("");

                  for (size_t i = 0; i < routeLegs.size(); i++) {
                    const Route& leg = routeLegs[i];
                    cheapestRouteDisplay.push_back("--- LEG " +
                                                   to_string(i + 1) + " ---");
                    cheapestRouteDisplay.push_back(
                        "From: " + g.ports[path[i]].name +
                        " -> To: " + g.ports[path[i + 1]].name);
                    cheapestRouteDisplay.push_back("Date: " + leg.date);
                    cheapestRouteDisplay.push_back("Time: " + leg.depTime +
                                                   " - " + leg.arrTime);
                    cheapestRouteDisplay.push_back(
                        "Duration: " + to_string(leg.travelTime / 60) + "h " +
                        to_string(leg.travelTime % 60) + "m");
                    cheapestRouteDisplay.push_back("Cost: $" +
                                                   to_string(leg.cost));
                    cheapestRouteDisplay.push_back(
                        "Port Fee: $" + to_string(g.ports[path[i + 1]].cost));
                    cheapestRouteDisplay.push_back("Company: " + leg.company);
                    cheapestRouteDisplay.push_back("");
                  }

                  cheapestRouteDisplay.push_back("=== COST BREAKDOWN ===");
                  int routeCostOnly = 0;
                  int portCostsOnly = 0;

                  for (const Route& leg : routeLegs) {
                    routeCostOnly += leg.cost;
                  }
                  for (size_t i = 1; i < path.size(); i++) {
                    portCostsOnly += g.ports[path[i]].cost;
                  }

                  cheapestRouteDisplay.push_back("Route Costs: $" +
                                                 to_string(routeCostOnly));
                  cheapestRouteDisplay.push_back("Port Fees: $" +
                                                 to_string(portCostsOnly));
                  cheapestRouteDisplay.push_back("Total: $" +
                                                 to_string(totalCost));
                  cheapestRouteDisplay.push_back("");

                  routeDisplayWindow.show(g.ports[srcIdx].name,
                                          g.ports[destIdx].name,
                                          cheapestRouteDisplay);

                } else {
                  vector<string> info;
                  if (userPreferences.hasCompanyFilter ||
                      userPreferences.hasPortFilter ||
                      userPreferences.hasTimeFilter) {
                    info.push_back(
                        "No shortest route found with current filters!");
                    info.push_back("");
                    info.push_back("This may be because:");
                    info.push_back("- No routes match the selected companies");
                    info.push_back("- Route passes through avoided ports");
                    info.push_back("- No routes within max voyage time");
                    info.push_back("- Timing constraints between legs");
                  } else {
                    info.push_back("No route found between these ports");
                    info.push_back("This could be due to:");
                    info.push_back(
                        "- No direct or connecting routes available");
                    info.push_back("- Timing constraints between route legs");
                    info.push_back(
                        "- Limited route availability on selected dates");
                  }
                  infoWindow.show("Route Not Found", info);
                }
              }

              locationClicked = true;
              break;
            }
          }

          if (!locationClicked && cheapestRouteCalculated) {
            for (int i = 0; i < cheapestPathEdges.size(); i++) {
              if (cheapestPathEdges[i].isClicked(window, event)) {
                vector<string> routeDetails =
                    cheapestPathEdges[i].getRouteDetails();
                infoWindow.show("Route Information", routeDetails);
                break;
              }
            }
          }
        }
      } else if (currentState == BOOK_CARGO_SELECT_PORTS &&
                 !infoWindow.visible()) {
        bool locationClicked = false;

        for (int i = 0; i < locations.size(); i++) {
          if (locations[i].isClicked(window, event)) {
            bool alreadySelected = false;
            for (int idx : bookingSelectedPorts) {
              if (idx == i) {
                alreadySelected = true;
                break;
              }
            }

            if (!alreadySelected && bookingSelectedPorts.size() < 2) {
              bookingSelectedPorts.push_back(i);
              locations[i].pin.setOutlineColor(Color::Magenta);

              if (bookingSelectedPorts.size() == 2) {
                int srcIdx = bookingSelectedPorts[0];
                int destIdx = bookingSelectedPorts[1];
                availableBookingRoutes =
                    g.findAllPossibleRoutes(srcIdx, destIdx);

                vector<string> availableDates =
                    extractAvailableDates(availableBookingRoutes);

                if (!availableDates.empty()) {
                  dateSelectionWindow.show(g.ports[srcIdx].name,
                                           g.ports[destIdx].name,
                                           availableDates);
                  currentState = BOOK_CARGO_SELECT_DATE;
                } else {
                  vector<string> info;
                  info.push_back("No routes available between these ports");
                  info.push_back("");
                  info.push_back(
                      "Try different ports or check route availability");
                  infoWindow.show("No Routes", info);
                  cout << "No routes found between selected ports" << endl;
                }
              }
            } else if (alreadySelected) {
              bookingSelectedPorts.erase(remove(bookingSelectedPorts.begin(),
                                                bookingSelectedPorts.end(), i),
                                         bookingSelectedPorts.end());
              locations[i].pin.setOutlineColor(Color::White);
              cout << "Deselected port: " << locations[i].name << endl;
            }

            locationClicked = true;
            break;
          }
        }

        if (event.type == Event::KeyPressed &&
            event.key.code == Keyboard::Escape) {
          bookingSelectedPorts.clear();
          for (auto& location : locations) {
            location.pin.setOutlineColor(Color::White);
          }
          currentState = NAVIGATION_MENU;
          navMenu.show();
        }
      }

      else if (currentState == BOOK_CARGO_SELECT_DATE) {
        int clicked = dateSelectionWindow.checkClick(window, event);

        if (clicked == -2) {
          dateSelectionWindow.hide();
          currentState = BOOK_CARGO_SELECT_PORTS;
          for (int idx : bookingSelectedPorts) {
            locations[idx].pin.setOutlineColor(Color::White);
          }
          bookingSelectedPorts.clear();
          cout << "Date selection cancelled" << endl;
        } else if (clicked == -3) {
          selectedBookingDate = dateSelectionWindow.getSelectedDate();
          if (!selectedBookingDate.empty()) {
            dateSelectionWindow.hide();
            vector<CompleteRoute> filteredRoutes;
            for (const auto& route : availableBookingRoutes) {
              if (!route.routeLegs.empty() &&
                  route.routeLegs[0].date == selectedBookingDate) {
                filteredRoutes.push_back(route);
              }
            }

            cout << "Found " << filteredRoutes.size() << " routes on "
                 << selectedBookingDate << endl;

            if (filteredRoutes.size() == 1) {
              showBookingRoute(locations, g, filteredRoutes[0],
                               bookingHighlightedRoutes);
              showingBookingRoutes = true;
              availableBookingRoutes = filteredRoutes;
              currentState = BOOK_CARGO_VIEW_ROUTES;
              cout << "Showing single route visualization" << endl;
            } else if (filteredRoutes.size() > 1) {
              routeBookingWindow.show(g.ports[bookingSelectedPorts[0]].name,
                                      g.ports[bookingSelectedPorts[1]].name,
                                      selectedBookingDate, filteredRoutes, g);
              availableBookingRoutes = filteredRoutes;
              currentState = BOOK_CARGO_VIEW_ROUTES;
              cout << "Showing route selection window with "
                   << filteredRoutes.size() << " options" << endl;
            } else {
              vector<string> info;
              info.push_back("No routes available on " + selectedBookingDate);
              info.push_back("");
              info.push_back("Please select a different date");
              infoWindow.show("No Routes", info);
              currentState = BOOK_CARGO_SELECT_PORTS;
              cout << "No routes on selected date" << endl;
            }
          }
        }
      }
else if (currentState == BOOK_CARGO_VIEW_ROUTES) {
    if (event.type == Event::MouseWheelScrolled) {
        if (routeBookingWindow.isMouseOverWindow(window)) {
            routeBookingWindow.scroll(event.mouseWheelScroll.delta);
        }
    }
    if (routeBookingWindow.visible()) {
        int clicked = routeBookingWindow.checkClick(window, event);
        
        if (clicked == RouteBookingWindow::CLOSE) {
            routeBookingWindow.hide();
            currentState = BOOK_CARGO_SELECT_DATE;
            dateSelectionWindow.show(
                g.ports[bookingSelectedPorts[0]].name,
                g.ports[bookingSelectedPorts[1]].name,
                extractAvailableDates(availableBookingRoutes)
            );
        }
        else if (clicked == RouteBookingWindow::BOOK) {
            CompleteRoute selectedRoute = routeBookingWindow.getSelectedRoute();
            
            availableBookingRoutes.clear();
            availableBookingRoutes.push_back(selectedRoute);
            
            routeBookingWindow.hide();
            
            bookingConfirmationWindow.show(
                selectedRoute,
                g.ports[bookingSelectedPorts[0]].name,
                g.ports[bookingSelectedPorts[1]].name,
                selectedBookingDate,
                g
            );
            currentState = BOOK_CARGO_CONFIRM;
            cout << "Booking confirmed for Route " << (routeBookingWindow.selectedRouteIndex + 1) << "!" << endl;
        }
        else if (clicked >= 0) {
            cout << "User selected route " << (clicked + 1) << " of " << availableBookingRoutes.size() << endl;
        }
    }
    
    if (event.type == Event::KeyPressed && event.key.code == Keyboard::Escape) {
        routeBookingWindow.hide();
        currentState = BOOK_CARGO_SELECT_DATE;
        dateSelectionWindow.show(
            g.ports[bookingSelectedPorts[0]].name,
            g.ports[bookingSelectedPorts[1]].name,
            extractAvailableDates(availableBookingRoutes)
        );
    }
}

      else if (currentState == BOOK_CARGO_CONFIRM) {
        if (bookingConfirmationWindow.checkClose(window, event)) {
          if (!availableBookingRoutes.empty()) {
            CompleteRoute bookedRoute = availableBookingRoutes[0];
            BookedRoute newBooking(bookedRoute,
                                   g.ports[bookingSelectedPorts[0]].name,
                                   g.ports[bookingSelectedPorts[1]].name,
                                   selectedBookingDate, "Guest");
            bookedRoutes.push_back(newBooking);
          }

          bookingConfirmationWindow.hide();

          bookingSelectedPorts.clear();
          selectedBookingDate = "";
          availableBookingRoutes.clear();
          bookingHighlightedRoutes.clear();
          showingBookingRoutes = false;
          if (bookRouteButton) {
            delete bookRouteButton;
            bookRouteButton = nullptr;
          }

          for (auto& location : locations) {
            location.pin.setOutlineColor(Color::White);
          }

          currentState = NAVIGATION_MENU;
          navMenu.show();
          cout << "Booking completed, returning to navigation menu" << endl;
        }
      } else if (currentState == TRACK_MULTI_LEG && !infoWindow.visible() &&
                 !portAddWindow.visible()) {
        auto action = journeyControls.checkClick(window, event);

        if (action == JourneyControlPanel::ADD_BEFORE) {
          if (selectedJourneyPortIdx != -1 &&
              currentJourney.portPath.size() > 0) {
            portAddWindow.show(g, currentJourney.portPath,
                               selectedJourneyPortIdx, true);
            currentState = TRACK_MULTI_LEG_ADD_PORT;
          }
        } else if (action == JourneyControlPanel::ADD_AFTER) {
          if (selectedJourneyPortIdx != -1 &&
              currentJourney.portPath.size() > 0) {
            portAddWindow.show(g, currentJourney.portPath,
                               selectedJourneyPortIdx, false);
            currentState = TRACK_MULTI_LEG_ADD_PORT;
          }
        } else if (action == JourneyControlPanel::REMOVE_PORT) {
          if (selectedJourneyPortIdx != -1 &&
              currentJourney.portPath.size() > 0) {
            currentJourney.portPath.erase(currentJourney.portPath.begin() +
                                          selectedJourneyPortIdx);
            rebuildJourneyVisualization(
                currentJourney, journeyPathEdges, availableRouteEdges,
                locations, g, selectedJourneyPortIdx, showingAvailableRoutes);
            journeyControls.updateStats(currentJourney, g);
          }
        } else if (action == JourneyControlPanel::CLEAR_JOURNEY) {
          currentJourney = MultiLegJourney();
          journeyPathEdges.clear();
          availableRouteEdges.clear();
          selectedJourneyPortIdx = -1;
          showingAvailableRoutes = false;

          for (auto& loc : locations) {
            loc.pin.setOutlineColor(Color::White);
          }

          cout << "Journey cleared" << endl;
        } else if (action == JourneyControlPanel::VIEW_SUMMARY) {
          if (currentJourney.portPath.size() >= 2) {
            vector<string> summary;
            summary.push_back("=== JOURNEY SUMMARY ===");
            summary.push_back("");
            summary.push_back("Total Ports: " +
                              to_string(currentJourney.portPath.size()));
            summary.push_back("Total Legs: " +
                              to_string(currentJourney.legs.size()));
            summary.push_back("Total Cost: $" +
                              to_string(currentJourney.totalCost));
            summary.push_back(
                "Total Time: " + to_string(currentJourney.totalTime / 60) +
                "h " + to_string(currentJourney.totalTime % 60) + "m");
            summary.push_back("");
            summary.push_back("=== ROUTE ===");

            for (size_t i = 0; i < currentJourney.legs.size(); i++) {
              const JourneyLeg& leg = currentJourney.legs[i];
              summary.push_back("");
              summary.push_back("--- LEG " + to_string(i + 1) + " ---");
              summary.push_back("From: " + g.ports[leg.fromPortIdx].name);
              summary.push_back("To: " + g.ports[leg.toPortIdx].name);
              summary.push_back("Date: " + leg.route.date);
              summary.push_back("Departure: " + leg.route.depTime);
              summary.push_back("Arrival: " + leg.route.arrTime);
              summary.push_back("Cost: $" + to_string(leg.route.cost));
              summary.push_back("Company: " + leg.route.company);
            }

            infoWindow.show("Journey Summary", summary);
          }
        } else if (!journeyControls.isMouseOver(window)) {
          bool locationClicked = false;

          for (int i = 0; i < locations.size(); i++) {
            if (locations[i].isClicked(window, event)) {
              bool isInJourney = false;
              int journeyIndex = -1;

              for (size_t j = 0; j < currentJourney.portPath.size(); j++) {
                if (currentJourney.portPath[j] == i) {
                  isInJourney = true;
                  journeyIndex = j;
                  break;
                }
              }

              if (currentJourney.portPath.empty()) {
                currentJourney.portPath.push_back(i);
                locations[i].pin.setOutlineColor(Color(255, 105, 180));
                selectedJourneyPortIdx = 0;
                showingAvailableRoutes = true;

                availableRouteEdges.clear();
                vector<Route> availableRoutes = g.getRoutesFromPort(i);

                for (const Route& route : availableRoutes) {
                  int destIdx = g.getPortIndex(route.destination);
                  if (destIdx != -1) {
                    RouteEdge edge(locations[i].position,
                                   locations[destIdx].position, route,
                                   g.ports[i].name);
                    edge.line.setFillColor(Color(255, 255, 0, 200));
                    availableRouteEdges.push_back(edge);
                  }
                }

                cout << "Started journey from: " << g.ports[i].name << endl;
              } else if (isInJourney) {
                selectedJourneyPortIdx = journeyIndex;

                for (auto& loc : locations) {
                  bool portInJourney = false;
                  for (int portIdx : currentJourney.portPath) {
                    for (size_t k = 0; k < locations.size(); k++) {
                      if (k == portIdx) {
                        locations[k].pin.setOutlineColor(Color(255, 105, 180));
                        portInJourney = true;
                        break;
                      }
                    }
                  }
                  bool isPortInJourney = false;
                  for (int portIdx : currentJourney.portPath) {
                    if (&loc == &locations[portIdx]) {
                      isPortInJourney = true;
                      break;
                    }
                  }
                  if (!isPortInJourney) {
                    loc.pin.setOutlineColor(Color::White);
                  }
                }

                locations[i].pin.setOutlineColor(Color::Yellow);
                showingAvailableRoutes = false;
                availableRouteEdges.clear();

                cout << "Selected port in journey: " << g.ports[i].name
                     << " (index " << journeyIndex << ")" << endl;
              } else if (showingAvailableRoutes &&
                         !currentJourney.portPath.empty()) {
                int lastPort = currentJourney.portPath.back();
                if (g.hasDirectRoute(lastPort, i)) {
                  Route route = g.getRouteBetween(lastPort, i);

                  JourneyLeg leg = {lastPort, i, route, true};
                  currentJourney.legs.push_back(leg);
                  currentJourney.portPath.push_back(i);

                  RouteEdge edge(locations[lastPort].position,
                                 locations[i].position, route,
                                 g.ports[lastPort].name);
                  edge.line.setFillColor(Color(255, 105, 180));
                  journeyPathEdges.push_back(edge);

                  locations[i].pin.setOutlineColor(Color(255, 105, 180));
                  selectedJourneyPortIdx = currentJourney.portPath.size() - 1;

                  availableRouteEdges.clear();
                  vector<Route> newRoutes = g.getRoutesFromPort(i);

                  for (const Route& newRoute : newRoutes) {
                    int destIdx = g.getPortIndex(newRoute.destination);
                    if (destIdx != -1) {
                      RouteEdge newEdge(locations[i].position,
                                        locations[destIdx].position, newRoute,
                                        g.ports[i].name);
                      newEdge.line.setFillColor(Color(255, 255, 0, 200));
                      availableRouteEdges.push_back(newEdge);
                    }
                  }

                  currentJourney.calculateTotals(g);

                  cout << "Added leg: " << g.ports[lastPort].name << " -> "
                       << g.ports[i].name << endl;
                }
              }

              locationClicked = true;
              break;
            }
          }

          if (!locationClicked && showingAvailableRoutes) {
            for (auto& edge : availableRouteEdges) {
              if (edge.isClicked(window, event)) {
                vector<string> routeInfo = edge.getRouteDetails();
                infoWindow.show("Available Route", routeInfo);
                break;
              }
            }
          }

          if (!locationClicked) {
            for (auto& edge : journeyPathEdges) {
              if (edge.isClicked(window, event)) {
                vector<string> routeInfo = edge.getRouteDetails();
                infoWindow.show("Journey Leg", routeInfo);
                break;
              }
            }
          }
        }
      }

      else if (currentState == TRACK_MULTI_LEG_ADD_PORT) {
        int clicked = portAddWindow.checkClick(window, event, g);

        if (clicked == -2) {
          portAddWindow.hide();
          currentState = TRACK_MULTI_LEG;
        } else if (clicked >= 0) {
          int newPortIdx = clicked;
          int selectedJourneyIdx = portAddWindow.getSelectedJourneyIndex();
          bool isAddingBefore = portAddWindow.getIsAddingBefore();

          if (selectedJourneyIdx != -1) {
            int insertPosition = selectedJourneyIdx;
            if (!isAddingBefore) {
              insertPosition = selectedJourneyIdx + 1;
            }

            if (insertPosition >= 0 &&
                insertPosition <= currentJourney.portPath.size()) {
              currentJourney.portPath.insert(
                  currentJourney.portPath.begin() + insertPosition, newPortIdx);

              rebuildJourneyVisualization(
                  currentJourney, journeyPathEdges, availableRouteEdges,
                  locations, g, selectedJourneyPortIdx, showingAvailableRoutes);

              selectedJourneyPortIdx = insertPosition;

              journeyControls.updateStats(currentJourney, g);
            }
          }

          portAddWindow.hide();
          currentState = TRACK_MULTI_LEG;
        }
      }

      else if (currentState == FILTER_PREFERENCES) {
        int clicked = filterMenu.checkClick(window, event);
        if (clicked == 0) {
          currentState = FILTER_COMPANIES;
          filterMenu.createCompanyMenu(availableCompanies);
          cout << "Showing shipping companies filter menu" << endl;
        } else if (clicked == 1) {
          currentState = FILTER_PORTS;
          filterMenu.createPortMenu(g.ports);
          cout << "Showing avoid ports filter menu" << endl;
        } else if (clicked == 2) {
          currentState = FILTER_TIME;
          filterMenu.createTimeMenu();
          cout << "Showing voyage time filter menu" << endl;
        } else if (clicked == 3) {
          currentState = FILTER_CUSTOM_COMPANIES;
          filterMenu.createCompanyMenu(availableCompanies);
          cout << "Showing custom preferences menu (step 1: companies)" << endl;
        } else if (clicked == 4) {
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
            userPreferences.hasCompanyFilter = true;
            loadMapView(mapTexture, mapSprite, locations, edges, window, font,
                        g);
            customShipPreferences(userPreferences, locations, edges, g);
            currentState = FILTER_RESULT_MAP;
            cout << "Company filter applied, showing filtered map" << endl;
          } else if (clicked < availableCompanies.size()) {
            string selectedCompany = availableCompanies[clicked];
            auto it =
                find(userPreferences.preferredCompanies.begin(),
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
            userPreferences.hasPortFilter = true;
            loadMapView(mapTexture, mapSprite, locations, edges, window, font,
                        g);
            customShipPreferences(userPreferences, locations, edges, g);
            currentState = FILTER_RESULT_MAP;
            cout << "Port filter applied, showing filtered map" << endl;
          } else if (clicked < g.ports.size()) {
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
            userPreferences.maxVoyageTime = 8 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 8 hours, showing filtered map"
                 << endl;
          } else if (clicked == 1) {
            userPreferences.maxVoyageTime = 16 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 16 hours, showing filtered map"
                 << endl;
          } else if (clicked == 2) {
            userPreferences.maxVoyageTime = 24 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 24 hours, showing filtered map"
                 << endl;
          } else if (clicked == 3) {
            userPreferences.maxVoyageTime = 48 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 48 hours, showing filtered map"
                 << endl;
          } else if (clicked == 4) {
            userPreferences.hasTimeFilter = false;
            userPreferences.maxVoyageTime = 999999;
            timeSelected = true;
            cout << "No voyage time limit" << endl;
          } else if (clicked == 5) {
            currentState = FILTER_PREFERENCES;
            filterMenu.createMainMenu();
          }

          if (timeSelected) {
            loadMapView(mapTexture, mapSprite, locations, edges, window, font,
                        g);
            customShipPreferences(userPreferences, locations, edges, g);
            currentState = FILTER_RESULT_MAP;
          }
        }

        if (event.type == Event::KeyPressed &&
            event.key.code == Keyboard::Escape) {
          currentState = FILTER_PREFERENCES;
          filterMenu.createMainMenu();
        }
      }

      else if (currentState == FILTER_CUSTOM_COMPANIES) {
        int clicked = filterMenu.checkClick(window, event);
        if (clicked != -1) {
          if (clicked == filterMenu.getButtonCount() - 1) {
            userPreferences.hasCompanyFilter = true;
            filterMenu.createPortMenu(g.ports);
            currentState = FILTER_CUSTOM_PORTS;
            cout << "Company preferences set, now showing ports" << endl;
          } else if (clicked < availableCompanies.size()) {
            string selectedCompany = availableCompanies[clicked];
            auto it =
                find(userPreferences.preferredCompanies.begin(),
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
            userPreferences.hasPortFilter = true;
            filterMenu.createTimeMenuForCustom();
            currentState = FILTER_CUSTOM_TIME;
            cout << "Port preferences set, now showing voyage time" << endl;
          } else if (clicked < g.ports.size()) {
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
          } else if (clicked == 1) {
            userPreferences.maxVoyageTime = 16 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 16 hours" << endl;
          } else if (clicked == 2) {
            userPreferences.maxVoyageTime = 24 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 24 hours" << endl;
          } else if (clicked == 3) {
            userPreferences.maxVoyageTime = 48 * 60;
            userPreferences.hasTimeFilter = true;
            timeSelected = true;
            cout << "Max voyage time set to 48 hours" << endl;
          } else if (clicked == 4) {
            userPreferences.hasTimeFilter = false;
            userPreferences.maxVoyageTime = 999999;
            timeSelected = true;
            cout << "No voyage time limit" << endl;
          } else if (clicked == 5) {
            timeSelected = true;
            cout << "All custom preferences applied, showing filtered map"
                 << endl;
          }

          if (timeSelected) {
            loadMapView(mapTexture, mapSprite, locations, edges, window, font,
                        g);
            customShipPreferences(userPreferences, locations, edges, g);
            currentState = FILTER_RESULT_MAP;
          }
        }

      } else if (currentState == GENERATE_SUBGRAPH) {
        int clicked = subgraphMenu.checkClick(window, event, vector<string>());

        if (clicked == 0) {
          currentState = SUBGRAPH_COMPANY_SELECT;
          subgraphMenu.createCompanyMenu(g.getAllShippingCompanies());
        } else if (clicked == 1) {
          currentState = SUBGRAPH_WEATHER_SELECT;
          subgraphMenu.createWeatherMenu(g.getAllWeatherConditions());
        } else if (clicked == 3) {
          currentState = NAVIGATION_MENU;
          navMenu.show();
          subgraphMenu.hide();
        } else if (clicked == -1) {
        }
      } else if (currentState == SUBGRAPH_COMPANY_SELECT) {
        vector<string> companies = g.getAllShippingCompanies();
        int clicked = subgraphMenu.checkClick(window, event, companies);

        if (clicked == -2) {
          subgraphMenu.selectAll(companies);
          for (size_t i = 0; i < companies.size(); i++) {
            subgraphMenu.buttons[i]->shape.setFillColor(Color(0, 150, 0));
          }
        } else if (clicked == -3) {
          subgraphMenu.clearAll();
          for (size_t i = 0; i < companies.size(); i++) {
            subgraphMenu.buttons[i]->shape.setFillColor(
                subgraphMenu.buttons[i]->normalColor);
          }
        } else if (clicked == -4) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);

          displaySubgraph(g, locations, edges, subgraphMenu);

          currentState = SUBGRAPH_VIEW;
        } else if (clicked >= 0 && clicked < (int)companies.size()) {
        }
      } else if (currentState == SUBGRAPH_WEATHER_SELECT) {
        vector<string> weatherConditions = g.getAllWeatherConditions();
        int clicked = subgraphMenu.checkClick(window, event, weatherConditions);

        if (clicked == -2) {
          subgraphMenu.selectAll(weatherConditions);
          for (int i = 0; i < weatherConditions.size(); i++) {
            subgraphMenu.buttons[i]->shape.setFillColor(Color(150, 0, 0));
          }
        } else if (clicked == -3) {
          subgraphMenu.clearAll();
          for (int i = 0; i < weatherConditions.size(); i++) {
            subgraphMenu.buttons[i]->shape.setFillColor(
                subgraphMenu.buttons[i]->normalColor);
          }
        } else if (clicked == -4) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);

          displaySubgraph(g, locations, edges, subgraphMenu);

          currentState = SUBGRAPH_VIEW;
        }
      } else if (currentState == SUBGRAPH_VIEW) {
        bool clickedOnSomething = false;

        for (auto& location : locations) {
          if (location.isClicked(window, event)) {
            Color portColor = location.pin.getFillColor();

            if (portColor.r > 200 && portColor.g > 200 && portColor.b < 100) {
              vector<string> portInfo = getPortInfo(location, g);
              infoWindow.show(location.name, portInfo);
              clickedOnSomething = true;
            } else {
              vector<string> blockedMsg;
              blockedMsg.push_back("Port is filtered out");
              blockedMsg.push_back("");
              blockedMsg.push_back("Port: " + location.name);
              blockedMsg.push_back("");
              blockedMsg.push_back(
                  "This port is not included in the current subgraph.");
              if (subgraphMenu.getCurrentMode() == SubgraphMenu::COMPANY_MODE) {
                blockedMsg.push_back(
                    "Reason: No routes from selected companies.");
              } else {
                blockedMsg.push_back(
                    "Reason: Has weather conditions to avoid.");
              }
              infoWindow.show("Filtered Port", blockedMsg);
              clickedOnSomething = true;
            }
            break;
          }
        }

        if (!clickedOnSomething) {
          for (int i = 0; i < edges.size(); i++) {
            if (edges[i].isClicked(window, event)) {
              Color routeColor = edges[i].line.getFillColor();

              if (routeColor.a > 150) {
                vector<string> routeDetails = edges[i].getRouteDetails();
                infoWindow.show("Route Information", routeDetails);
              } else {
                vector<string> blockedMsg;
                blockedMsg.push_back("Route is filtered out");
                blockedMsg.push_back("");
                blockedMsg.push_back("Route: " + edges[i].sourceName + " -> " +
                                     edges[i].routeInfo.destination);
                blockedMsg.push_back("");
                blockedMsg.push_back(
                    "This route is not included in the current subgraph.");
                if (subgraphMenu.getCurrentMode() ==
                    SubgraphMenu::COMPANY_MODE) {
                  blockedMsg.push_back(
                      "Reason: Not operated by selected companies.");
                } else {
                  blockedMsg.push_back(
                      "Reason: Source or destination port has weather to "
                      "avoid.");
                }
                blockedMsg.push_back("");
                blockedMsg.push_back("Company: " + edges[i].routeInfo.company);
                blockedMsg.push_back("Cost: $" +
                                     to_string(edges[i].routeInfo.cost));
                infoWindow.show("Filtered Route", blockedMsg);
              }
              clickedOnSomething = true;
              break;
            }
          }
        }

        if (event.type == Event::KeyPressed &&
            event.key.code == Keyboard::Escape) {
          currentState = GENERATE_SUBGRAPH;
          subgraphMenu.createMainMenu();
        }
      }

      if ((currentState == MAP_VIEW || currentState == SEARCH_ROUTES ||
           currentState == BOOK_CARGO || currentState == FILTER_PREFERENCES ||
           currentState == FILTER_COMPANIES || currentState == FILTER_PORTS ||
           currentState == FILTER_TIME || currentState == FILTER_CUSTOM ||
           currentState == PROCESS_LAYOVERS ||
           currentState == TRACK_MULTI_LEG ||
           currentState == TRACK_MULTI_LEG_ADD_PORT ||
           currentState == GENERATE_SUBGRAPH ||
           currentState == SHORTEST_ROUTE_SELECT ||
           currentState == CHEAPEST_ROUTE_SELECT ||
           currentState == FILTER_CUSTOM_COMPANIES ||
           currentState == FILTER_CUSTOM_PORTS ||
           currentState == FILTER_CUSTOM_TIME ||
           currentState == SUBGRAPH_COMPANY_SELECT ||
           currentState == SUBGRAPH_WEATHER_SELECT ||
           currentState == SUBGRAPH_VIEW) &&
          event.type == Event::KeyPressed &&
          event.key.code == Keyboard::Escape) {
        if (currentState == SUBGRAPH_VIEW) {
          loadMapView(mapTexture, mapSprite, locations, edges, window, font, g);
        }

        if (currentState == TRACK_MULTI_LEG ||
            currentState == TRACK_MULTI_LEG_ADD_PORT) {
          currentJourney = MultiLegJourney();
          journeyPathEdges.clear();
          availableRouteEdges.clear();
          selectedJourneyPortIdx = -1;
          showingAvailableRoutes = false;
          portAddWindow.hide();
        }

        filterPopup.hide();
        selectedPorts.clear();
        shortestPathEdges.clear();
        cheapestPathEdges.clear();
        highlightedRoutes.clear();
        shortestRouteCalculated = false;
        cheapestRouteCalculated = false;
        routesDisplayed = false;
        userPreferences.hasCompanyFilter = false;
        userPreferences.hasPortFilter = false;
        userPreferences.hasTimeFilter = false;
        userPreferences.preferredCompanies.clear();
        userPreferences.avoidPorts.clear();
        routeDisplayWindow.hide();
        infoWindow.hide();

        for (auto& loc : locations) {
          loc.pin.setOutlineColor(Color::White);
        }

        currentState = NAVIGATION_MENU;
        navMenu.show();
        subgraphMenu.hide();
      }
    }
    if (currentState == MAIN_MENU) {
      startButton.update(window);
      settingsButton.update(window);
      exitButton.update(window);
    } else if (currentState == NAVIGATION_MENU) {
      navMenu.update(window);
    } else if (currentState == MAP_VIEW) {
      for (auto& edge : edges) {
        edge.update(window);
      }
      for (auto& location : locations) {
        location.update(window);
      }
      infoWindow.update(window);
      companyWindow.update(window);
      portWindow.update(window);
      timeWindow.update(window);
    } else if (currentState == FILTER_RESULT_MAP) {
      for (auto& edge : edges) {
        edge.update(window);
      }
      for (auto& location : locations) {
        location.update(window);
      }
      infoWindow.update(window);
    } else if (currentState == SEARCH_ROUTES) {
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
      companyWindow.update(window);
      portWindow.update(window);
      timeWindow.update(window);
    } else if (currentState == PROCESS_LAYOVERS) {
      layoverSim.update(locations, g);
      if (layoverControls) {
        layoverControls->update(window, false);
      }
    } else if (currentState == SHORTEST_ROUTE_SELECT) {
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
      companyWindow.update(window);
      portWindow.update(window);
      timeWindow.update(window);
    } else if (currentState == CHEAPEST_ROUTE_SELECT) {
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
      companyWindow.update(window);
      portWindow.update(window);
      timeWindow.update(window);
    } else if (currentState == FILTER_PREFERENCES ||
               currentState == FILTER_COMPANIES ||
               currentState == FILTER_PORTS || currentState == FILTER_TIME ||
               currentState == FILTER_CUSTOM_COMPANIES ||
               currentState == FILTER_CUSTOM_PORTS ||
               currentState == FILTER_CUSTOM_TIME) {
      filterMenu.update(window);
    }

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
    } else if (currentState == NAVIGATION_MENU) {
      navMenu.draw(window);
    } else if (currentState == MAP_VIEW) {
      window.draw(mapSprite);

      for (auto& edge : edges) {
        if (userPreferences.hasCompanyFilter || userPreferences.hasPortFilter ||
            userPreferences.hasTimeFilter) {
          if (g.routeMatchesPreferences(edge.routeInfo, userPreferences) &&
              !g.portIsAvoid(edge.sourceName, userPreferences) &&
              !g.portIsAvoid(edge.routeInfo.destination, userPreferences)) {
            edge.line.setFillColor(Color(0, 255, 0, 150));
          } else {
            edge.line.setFillColor(Color(150, 150, 150, 50));
          }
        }
        edge.draw(window);
      }

      for (auto& location : locations) {
        if (userPreferences.hasPortFilter || userPreferences.hasCompanyFilter ||
            userPreferences.hasTimeFilter) {
          if (g.portIsAvoid(location.name, userPreferences)) {
            location.pin.setFillColor(Color(150, 150, 150));
            location.label.setFillColor(Color(150, 150, 150));
          } else {
            location.pin.setFillColor(Color(0, 255, 0));
            location.label.setFillColor(Color::White);
          }
        }
        location.draw(window);
      }

      infoWindow.draw(window);
      instructionText.setCharacterSize(20);
      instructionText.setFillColor(Color::White);
      instructionText.setOutlineThickness(2);
      instructionText.setOutlineColor(Color::Black);
      FloatRect bounds = instructionText.getLocalBounds();
      instructionText.setOrigin(bounds.left + bounds.width,
                                bounds.top + bounds.height);
      instructionText.setPosition(windowWidth - 20, windowHeight - 170);
      window.draw(instructionText);
      applyFiltersButton->draw(window);
      removeFiltersButton->draw(window);

      filterPopup.draw(window, userPreferences);
      companyWindow.draw(window, availableCompanies);
      portWindow.draw(window, g.ports);
      timeWindow.draw(window);

    } else if (currentState == BOOK_CARGO_SELECT_PORTS) {
      window.draw(mapSprite);
      for (auto& edge : edges) {
        edge.draw(window);
      }
      for (auto& location : locations) {
        location.draw(window);
      }
      Text instructionText;
      instructionText.setFont(font);
      instructionText.setCharacterSize(24);
      instructionText.setFillColor(Color::White);
      instructionText.setOutlineThickness(2);
      instructionText.setOutlineColor(Color::Black);

      instructionText.setPosition(20, 20);
      window.draw(instructionText);

      infoWindow.draw(window);
    }

    else if (currentState == BOOK_CARGO_SELECT_DATE) {
      window.draw(mapSprite);
      for (auto& edge : edges) {
        edge.draw(window);
      }
      for (int i = 0; i < locations.size(); i++) {
        bool isSelected = false;
        for (int idx : bookingSelectedPorts) {
          if (idx == i) {
            locations[i].pin.setOutlineColor(Color::Magenta);
            isSelected = true;
            break;
          }
        }
        if (!isSelected) {
          locations[i].pin.setOutlineColor(Color::White);
        }
        locations[i].draw(window);
      }

      dateSelectionWindow.draw(window);

      infoWindow.draw(window);
    }

    else if (currentState == BOOK_CARGO_VIEW_ROUTES) {
      window.draw(mapSprite);

      for (auto& edge : edges) {
        RectangleShape fadedEdge = edge.line;
        fadedEdge.setFillColor(Color(150, 150, 150, 50));
        window.draw(fadedEdge);
      }

      for (auto& edge : bookingHighlightedRoutes) {
        edge.draw(window);
      }

      for (auto& location : locations) {
        location.draw(window);
      }

      Text instructionText;
      instructionText.setFont(font);
      instructionText.setCharacterSize(24);
      instructionText.setFillColor(Color::White);
      instructionText.setOutlineThickness(2);
      instructionText.setOutlineColor(Color::Black);

      if (showingBookingRoutes && !routeBookingWindow.visible()) {
        instructionText.setPosition(20, 20);
        window.draw(instructionText);

        if (bookRouteButton) {
          bookRouteButton->draw(window);
        }
      }

      if (routeBookingWindow.visible()) {
        routeBookingWindow.draw(window);
      }

      infoWindow.draw(window);
    }

    else if (currentState == BOOK_CARGO_CONFIRM) {
      window.draw(mapSprite);

      for (auto& edge : edges) {
        RectangleShape fadedEdge = edge.line;
        fadedEdge.setFillColor(Color(150, 150, 150, 30));
        window.draw(fadedEdge);
      }

      for (auto& location : locations) {
        location.draw(window);
      }

      bookingConfirmationWindow.draw(window);
    } else if (currentState == TRACK_MULTI_LEG) {
      window.draw(mapSprite);

      for (auto& edge : edges) {
        RectangleShape fadedLine = edge.line;
        fadedLine.setFillColor(Color(150, 150, 150, 30));
        window.draw(fadedLine);
      }

      for (auto& edge : journeyPathEdges) {
        edge.line.setFillColor(Color(255, 105, 180, 255));
        window.draw(edge.line);
      }

      if (showingAvailableRoutes) {
        for (auto& edge : availableRouteEdges) {
          edge.line.setFillColor(Color(255, 255, 0, 200));
          window.draw(edge.line);
        }
      }

      for (auto& location : locations) {
        location.draw(window);
      }

      Text instructionText;
      instructionText.setFont(font);
      instructionText.setCharacterSize(20);
      instructionText.setFillColor(Color::White);
      instructionText.setOutlineThickness(2);
      instructionText.setOutlineColor(Color::Black);

      FloatRect bounds = instructionText.getLocalBounds();
      instructionText.setPosition(20, windowHeight - 100);
      window.draw(instructionText);

      journeyControls.draw(window);

      infoWindow.draw(window);
    } else if (currentState == TRACK_MULTI_LEG_ADD_PORT) {
      window.draw(mapSprite);
      for (auto& edge : journeyPathEdges) {
        window.draw(edge.line);
      }
      for (auto& location : locations) {
        location.draw(window);
      }

      portAddWindow.draw(window);
    } else if (currentState == SEARCH_ROUTES) {
      window.draw(mapSprite);

      bool filtersActive = userPreferences.hasCompanyFilter ||
                           userPreferences.hasPortFilter ||
                           userPreferences.hasTimeFilter;

      for (auto& edge : edges) {
        RectangleShape tempLine = edge.line;

        if (filtersActive) {
          bool allowed =
              g.routeMatchesPreferences(edge.routeInfo, userPreferences) &&
              !g.portIsAvoid(edge.sourceName, userPreferences) &&
              !g.portIsAvoid(edge.routeInfo.destination, userPreferences);

          if (allowed)
            tempLine.setFillColor(Color(0, 255, 0, 160));
          else
            tempLine.setFillColor(Color(150, 150, 150, 40));
        } else {
          tempLine.setFillColor(Color(150, 150, 150, 50));
        }

        window.draw(tempLine);
      }

      for (auto& hr : highlightedRoutes) {
        RectangleShape glow = hr.line;
        glow.setFillColor(Color(255, 215, 0, 230));
        window.draw(glow);
      }
      for (auto& location : locations) {
        if (filtersActive) {
          if (g.portIsAvoid(location.name, userPreferences)) {
            location.pin.setFillColor(Color(150, 150, 150));
            location.label.setFillColor(Color(150, 150, 150));
          } else {
            location.pin.setFillColor(Color(0, 255, 0));
            location.label.setFillColor(Color::White);
          }
        }

        location.draw(window);
      }

      infoWindow.draw(window);
      routeDisplayWindow.draw(window);

      FloatRect bounds = instructionText.getLocalBounds();
      instructionText.setOrigin(bounds.left + bounds.width,
                                bounds.top + bounds.height);
      instructionText.setPosition(windowWidth - 20, windowHeight - 170);

      window.draw(instructionText);

      applyFiltersButton->draw(window);
      removeFiltersButton->draw(window);

      filterPopup.draw(window, userPreferences);
      companyWindow.draw(window, availableCompanies);
      portWindow.draw(window, g.ports);
      timeWindow.draw(window);

    } else if (currentState == PROCESS_LAYOVERS) {
      window.draw(mapSprite);

      for (auto& edge : edges) {
        RectangleShape fadedLine = edge.line;
        fadedLine.setFillColor(Color(150, 150, 150, 30));
        window.draw(fadedLine);
      }

      for (auto& location : locations) {
        location.draw(window);
      }

      layoverSim.draw(window, locations, g);

      if (layoverControls) {
        layoverControls->updateStats(layoverSim.getStatistics());
        layoverControls->update(window, false);
        layoverControls->draw(window);
      }
    } else if (currentState == SHORTEST_ROUTE_SELECT ||
               currentState == CHEAPEST_ROUTE_SELECT) {
      window.draw(mapSprite);

      bool filtersActive = userPreferences.hasCompanyFilter ||
                           userPreferences.hasPortFilter ||
                           userPreferences.hasTimeFilter;

      for (auto& edge : edges) {
        if (filtersActive) {
          bool allowed =
              g.routeMatchesPreferences(edge.routeInfo, userPreferences) &&
              !g.portIsAvoid(edge.sourceName, userPreferences) &&
              !g.portIsAvoid(edge.routeInfo.destination, userPreferences);

          if (allowed) {
            if (currentState == SHORTEST_ROUTE_SELECT)
              edge.line.setFillColor(Color(255, 255, 0, 160));
            else
              edge.line.setFillColor(Color(0, 255, 255, 160));
          } else {
            edge.line.setFillColor(Color(150, 150, 150, 40));
          }
        }

        edge.draw(window);
      }

      for (auto& edge : shortestPathEdges) {
        edge.draw(window);
      }

      for (auto& location : locations) {
        if (filtersActive) {
          if (g.portIsAvoid(location.name, userPreferences)) {
            location.pin.setFillColor(Color(150, 150, 150));
            location.label.setFillColor(Color(150, 150, 150));
          } else {
            if (currentState == SHORTEST_ROUTE_SELECT)
              location.pin.setFillColor(Color(255, 255, 0));
            else
              location.pin.setFillColor(Color(0, 255, 255));
            location.label.setFillColor(Color::White);
          }
        }

        location.draw(window);
      }

      routeDisplayWindow.draw(window);

      infoWindow.draw(window);

      instructionText.setFont(font);
      instructionText.setCharacterSize(24);
      instructionText.setFillColor(Color::White);
      instructionText.setOutlineThickness(2);
      instructionText.setOutlineColor(Color::Black);

      FloatRect bounds = instructionText.getLocalBounds();
      instructionText.setOrigin(bounds.left + bounds.width,
                                bounds.top + bounds.height);
      instructionText.setPosition(windowWidth - 20, windowHeight - 170);

      window.draw(instructionText);

      applyFiltersButton->draw(window);
      removeFiltersButton->draw(window);
      filterPopup.draw(window, userPreferences);
      companyWindow.draw(window, availableCompanies);
      portWindow.draw(window, g.ports);
      timeWindow.draw(window);
    }

    else if (currentState == FILTER_PREFERENCES ||
             currentState == FILTER_COMPANIES || currentState == FILTER_PORTS ||
             currentState == FILTER_TIME ||
             currentState == FILTER_CUSTOM_COMPANIES ||
             currentState == FILTER_CUSTOM_PORTS ||
             currentState == FILTER_CUSTOM_TIME) {
      filterMenu.draw(window);
    } else if (currentState == BOOK_CARGO) {
      Text placeholder;
      placeholder.setFont(font);
      placeholder.setString("Book Cargo Routes\n\n(Press ESC to go back)");
      placeholder.setCharacterSize(40);
      placeholder.setFillColor(Color::White);
      FloatRect bounds = placeholder.getLocalBounds();
      placeholder.setOrigin(bounds.left + bounds.width / 2,
                            bounds.top + bounds.height / 2);
      placeholder.setPosition(windowWidth / 2, windowHeight / 2);
      window.draw(placeholder);
    } else if (currentState == PROCESS_LAYOVERS) {
      if (!layoverControls) {
        layoverControls =
            new LayoverControlPanel(font, windowWidth, windowHeight);
      }

      auto action = layoverControls->checkClick(window, event);

      if (action == LayoverControlPanel::PAUSE) {
        layoverSim.togglePause();
      } else if (action == LayoverControlPanel::SPEED_UP) {
        currentSimSpeed = min(5.0f, currentSimSpeed + 0.5f);
        layoverSim.setSpeed(currentSimSpeed);
      } else if (action == LayoverControlPanel::SLOW_DOWN) {
        currentSimSpeed = max(0.1f, currentSimSpeed - 0.5f);
        layoverSim.setSpeed(currentSimSpeed);
      } else if (action == LayoverControlPanel::STOP) {
        layoverSim.stop();
        delete layoverControls;
        layoverControls = nullptr;
        currentState = NAVIGATION_MENU;
        navMenu.show();
      }
    }
    if (currentState == BOOK_CARGO_SELECT_DATE) {
      dateSelectionWindow.update(window);
    }
    if (currentState == BOOK_CARGO_VIEW_ROUTES) {
      routeBookingWindow.update(window);
      if (bookRouteButton) {
        bookRouteButton->update(window);
      }
    }
    if (currentState == BOOK_CARGO_CONFIRM) {
      bookingConfirmationWindow.update(window);
    } else if (currentState == TRACK_MULTI_LEG) {
      for (auto& edge : edges) {
        edge.update(window);
      }
      for (auto& edge : availableRouteEdges) {
        edge.update(window);
      }
      for (auto& edge : journeyPathEdges) {
        edge.update(window);
      }
      for (auto& location : locations) {
        location.update(window);
      }
      journeyControls.updateStats(currentJourney, g);
      journeyControls.update(window);
      infoWindow.update(window);
    } else if (currentState == TRACK_MULTI_LEG_ADD_PORT) {
      portAddWindow.update(window);
    } else if (currentState == GENERATE_SUBGRAPH) {
      subgraphMenu.draw(window);
    } else if (currentState == SUBGRAPH_COMPANY_SELECT ||
               currentState == SUBGRAPH_WEATHER_SELECT) {
      subgraphMenu.draw(window);
    } else if (currentState == SUBGRAPH_VIEW) {
      window.draw(mapSprite);

      for (auto& edge : edges) {
        edge.draw(window);
      }

      for (auto& location : locations) {
        location.draw(window);
      }

      infoWindow.draw(window);
    } else if (currentState == FILTER_RESULT_MAP) {
      window.draw(mapSprite);

      for (auto& edge : edges) {
        edge.draw(window);
      }

      for (auto& location : locations) {
        location.draw(window);
      }

      infoWindow.draw(window);

      instructionText.setFont(font);
      instructionText.setCharacterSize(20);
      instructionText.setFillColor(Color::White);
      instructionText.setOutlineThickness(2);
      instructionText.setOutlineColor(Color::Black);
      FloatRect bounds = instructionText.getLocalBounds();
      instructionText.setOrigin(bounds.left + bounds.width,
                                bounds.top + bounds.height);
      instructionText.setPosition(windowWidth - 20, windowHeight - 20);
      window.draw(instructionText);
    }

    window.display();
  }
  delete applyFiltersButton;
  delete removeFiltersButton;
  if (bookRouteButton) {
    delete bookRouteButton;
    bookRouteButton = nullptr;
  }
  if (layoverControls) {
    delete layoverControls;
  }
  return 0;
}