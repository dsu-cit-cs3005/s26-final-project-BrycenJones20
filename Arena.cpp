#include "Arena.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <dlfcn.h>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

namespace fs = std::filesystem;

using RobotSummaryFn = const char* (*)();

Arena::Arena()
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    setupBoard();
}

Arena::Arena(int w, int h) : width(w), height(h)
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    setupBoard();
}

Arena::~Arena()
{
    for (auto& r : robots) {
        delete r.robot;
        r.robot = nullptr;

        if (r.handle) {
            dlclose(r.handle);
            r.handle = nullptr;
        }
    }
}

bool Arena::loadConfig(const std::string& filename)
{
    std::ifstream fin(filename);
    if (!fin) {
        std::cerr << "Could not open config file: " << filename << "\n";
        return false;
    }

    std::string line;

    while (std::getline(fin, line)) {
        if (line.empty()) continue;

        std::replace(line.begin(), line.end(), ':', ' ');
        std::stringstream ss(line);

        std::string key;
        ss >> key;

        if (key == "Arena_Size") {
            ss >> height >> width;
        }
        else if (key == "Max_Rounds") {
            ss >> maxRounds;
        }
        else if (key == "Sleep_interval") {
            ss >> sleepInterval;
        }
        else if (key == "Game_State_Live") {
            std::string value;
            ss >> value;
            liveMode = (value == "true" || value == "True" || value == "1");
        }
        else if (key == "Flamethrowers") {
            ss >> flamethrowerCount;
        }
        else if (key == "Pits") {
            ss >> pitCount;
        }
        else if (key == "Mounds") {
            ss >> moundCount;
        }
    }

    setupBoard();
    return true;
}

void Arena::setupBoard()
{
    board.assign(height, std::vector<char>(width, '.'));
}

bool Arena::inBounds(int row, int col) const
{
    return row >= 0 && row < height && col >= 0 && col < width;
}

int Arena::randomBetween(int low, int high)
{
    return low + (std::rand() % (high - low + 1));
}

void Arena::placeObstacles()
{
    auto placeOne = [&](char type) {
        while (true) {
            int r = randomBetween(0, height - 1);
            int c = randomBetween(0, width - 1);

            if (board[r][c] == '.' && !cellHasRobot(r, c)) {
                board[r][c] = type;
                break;
            }
        }
    };

    for (int i = 0; i < flamethrowerCount; i++) placeOne('F');
    for (int i = 0; i < pitCount; i++) placeOne('P');
    for (int i = 0; i < moundCount; i++) placeOne('M');
}

bool Arena::cellHasRobot(int row, int col) const
{
    for (const auto& r : robots) {
        if (r.row == row && r.col == col) {
            return true;
        }
    }
    return false;
}

int Arena::robotAt(int row, int col) const
{
    for (size_t i = 0; i < robots.size(); i++) {
        if (robots[i].row == row && robots[i].col == col) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool Arena::cellBlocksMovement(int row, int col) const
{
    if (!inBounds(row, col)) return true;
    if (board[row][col] == 'M') return true;

    int index = robotAt(row, col);
    if (index != -1) return true;

    return false;
}

void Arena::placeRobotRandomly(RobotInstance& r)
{
    while (true) {
        int row = randomBetween(0, height - 1);
        int col = randomBetween(0, width - 1);

        if (board[row][col] == '.' && !cellHasRobot(row, col)) {
            r.row = row;
            r.col = col;
            r.robot->move_to(row, col);
            return;
        }
    }
}

void Arena::loadRobots()
{
    robots.clear();

    std::vector<fs::path> robotFiles;

    for (const auto& entry : fs::directory_iterator(".")) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().filename().string();

            if (name.rfind("Robot_", 0) == 0 && entry.path().extension() == ".cpp") {
                robotFiles.push_back(entry.path());
            }
        }
    }

    if (fs::exists("robots") && fs::is_directory("robots")) {
        for (const auto& entry : fs::directory_iterator("robots")) {
            if (entry.is_regular_file()) {
                std::string name = entry.path().filename().string();

                if (name.rfind("Robot_", 0) == 0 && entry.path().extension() == ".cpp") {
                    robotFiles.push_back(entry.path());
                }
            }
        }
    }

    const std::string symbols = "@#$%!&ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (const auto& path : robotFiles) {
        std::string source = path.string();
        std::string stem = path.stem().string();
        std::string sharedLib = "lib" + stem + ".so";

        std::string compileCmd =
            "g++ -shared -fPIC -o " + sharedLib + " " + source +
            " RobotBase.o -I. -std=c++20";

        std::cout << "Compiling " << source << "...\n";

        if (std::system(compileCmd.c_str()) != 0) {
            std::cerr << "Failed to compile " << source << "\n";
            continue;
        }

        std::string loadPath = "./" + sharedLib;
        void* handle = dlopen(loadPath.c_str(), RTLD_LAZY);

        if (!handle) {
            std::cerr << "Failed to load " << loadPath << ": " << dlerror() << "\n";
            continue;
        }

        RobotFactory createRobot = reinterpret_cast<RobotFactory>(dlsym(handle, "create_robot"));

        if (!createRobot) {
            std::cerr << "Missing create_robot in " << loadPath << "\n";
            dlclose(handle);
            continue;
        }

        RobotSummaryFn summaryFn = reinterpret_cast<RobotSummaryFn>(dlsym(handle, "robot_summary"));

        if (!summaryFn) {
            std::cerr << "Missing robot_summary in " << loadPath << "\n";
            dlclose(handle);
            continue;
        }

        const char* summaryText = summaryFn();

        if (!summaryText || std::string(summaryText).empty() || std::string(summaryText).size() > 50) {
            std::cerr << "Invalid robot_summary in " << loadPath << "\n";
            dlclose(handle);
            continue;
        }

        RobotBase* bot = createRobot();

        if (!bot) {
            std::cerr << "create_robot returned null for " << loadPath << "\n";
            dlclose(handle);
            continue;
        }

        bot->set_boundaries(height, width);

        RobotInstance instance;
        instance.robot = bot;
        instance.handle = handle;
        instance.alive = true;
        instance.summary = summaryText;
        instance.symbol = symbols[robots.size() % symbols.size()];

        bot->m_character = instance.symbol;

        robots.push_back(instance);
        placeRobotRandomly(robots.back());

        std::cout << "Loaded " << bot->m_name << " " << instance.symbol
                  << ": " << instance.summary << "\n";
    }

    if (robots.size() < 2) {
        std::cerr << "Warning: fewer than 2 robots loaded.\n";
    }
}

void Arena::run()
{
    setupBoard();
    placeObstacles();
    loadRobots();

    std::cout << "\nGame starting with " << robots.size() << " robots.\n";

    for (int round = 1; round <= maxRounds; round++) {
        std::cout << "\n=========== ROUND " << round << " ===========\n";

        printArena();

        if (checkWin()) {
            printWinner();
            return;
        }

        takeTurn(round);

        if (liveMode) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(sleepInterval * 1000))
            );
        }
    }

    std::cout << "\nMax rounds reached.\n";
    printWinner();
}

void Arena::takeTurn(int round)
{
    (void)round;

    for (auto& r : robots) {
        if (!r.alive) {
            std::cout << r.robot->m_name << " " << r.symbol << " is out.\n";
            continue;
        }

        int radarDir = 0;
        r.robot->get_radar_direction(radarDir);

        if (radarDir < 0 || radarDir > 8) {
            radarDir = 0;
        }

        std::vector<RadarObj> radar = scanDirection(r, radarDir);
        r.robot->process_radar_results(radar);

        std::cout << "\n" << r.robot->print_stats() << "\n";

        if (radar.empty()) {
            std::cout << "  radar scan returned nothing\n";
        } else {
            std::cout << "  radar scan returned ";
            for (const auto& obj : radar) {
                std::cout << obj.m_type << " at (" << obj.m_row << "," << obj.m_col << ") ";
            }
            std::cout << "\n";
        }

        int shotRow = 0;
        int shotCol = 0;

        if (r.robot->get_shot_location(shotRow, shotCol)) {
            std::cout << "  shooting at (" << shotRow << "," << shotCol << ")\n";
            handleShot(r, shotRow, shotCol);
        } else {
            int dir = 0;
            int dist = 0;

            r.robot->get_move_direction(dir, dist);
            std::cout << "  moving direction " << dir << " distance " << dist << "\n";
            moveRobot(r, dir, dist);
        }

        if (checkWin()) {
            return;
        }
    }
}

char Arena::getCellTypeForRadar(int row, int col) const
{
    int idx = robotAt(row, col);

    if (idx != -1) {
        return robots[idx].alive ? 'R' : 'X';
    }

    return board[row][col];
}

std::vector<RadarObj> Arena::scanDirection(RobotInstance& r, int direction)
{
    std::vector<RadarObj> results;

    int startRow = r.row;
    int startCol = r.col;

    auto addCell = [&](int row, int col) {
        if (!inBounds(row, col)) return;
        if (row == startRow && col == startCol) return;

        char type = getCellTypeForRadar(row, col);

        if (type != '.') {
            results.emplace_back(type, row, col);
        }
    };

    if (direction == 0) {
        for (int d = 1; d <= 8; d++) {
            int row = startRow + directions[d].first;
            int col = startCol + directions[d].second;
            addCell(row, col);
        }

        return results;
    }

    int dr = directions[direction].first;
    int dc = directions[direction].second;

    int sideR = 0;
    int sideC = 0;

    if (dr == 0) {
        sideR = 1;
        sideC = 0;
    } else if (dc == 0) {
        sideR = 0;
        sideC = 1;
    } else {
        sideR = -dr;
        sideC = dc;
    }

    for (int step = 1; step <= std::max(height, width); step++) {
        int centerR = startRow + dr * step;
        int centerC = startCol + dc * step;

        bool anyInBounds = false;

        for (int offset = -1; offset <= 1; offset++) {
            int row = centerR + sideR * offset;
            int col = centerC + sideC * offset;

            if (inBounds(row, col)) {
                anyInBounds = true;
                addCell(row, col);
            }
        }

        if (!anyInBounds) break;
    }

    return results;
}

void Arena::moveRobot(RobotInstance& r, int direction, int distance)
{
    if (direction < 1 || direction > 8 || distance <= 0) {
        std::cout << "  robot did not move\n";
        return;
    }

    int maxMove = r.robot->get_move_speed();

    if (maxMove <= 0) {
        std::cout << "  robot is trapped and cannot move\n";
        return;
    }

    distance = std::min(distance, maxMove);

    int dr = directions[direction].first;
    int dc = directions[direction].second;

    for (int step = 0; step < distance; step++) {
        int nextRow = r.row + dr;
        int nextCol = r.col + dc;

        if (!inBounds(nextRow, nextCol)) {
            break;
        }

        int other = robotAt(nextRow, nextCol);

        if (other != -1 || board[nextRow][nextCol] == 'M') {
            break;
        }

        r.row = nextRow;
        r.col = nextCol;
        r.robot->move_to(r.row, r.col);

        if (board[nextRow][nextCol] == 'P') {
            r.robot->disable_movement();
            std::cout << "  fell into a pit and can no longer move\n";
            break;
        }

        if (board[nextRow][nextCol] == 'F') {
            int damage = randomBetween(30, 50);
            std::cout << "  moved through flamethrower obstacle for " << damage << " damage\n";

            int index = robotAt(r.row, r.col);
            if (index != -1) {
                damageRobot(index, damage);
            }

            if (!r.alive) {
                board[nextRow][nextCol] = '.';
                break;
            }
        }
    }

    std::cout << "  now at (" << r.row << "," << r.col << ")\n";
}

void Arena::handleShot(RobotInstance& shooter, int shotRow, int shotCol)
{
    if (!inBounds(shotRow, shotCol)) {
        std::cout << "  shot was out of bounds\n";
        return;
    }

    WeaponType weapon = shooter.robot->get_weapon();

    int sr = shooter.row;
    int sc = shooter.col;

    int dr = (shotRow > sr) ? 1 : (shotRow < sr ? -1 : 0);
    int dc = (shotCol > sc) ? 1 : (shotCol < sc ? -1 : 0);

    if (dr == 0 && dc == 0) {
        std::cout << "  robot cannot shoot itself\n";
        return;
    }

    if (weapon == railgun) {
        int damage = randomBetween(10, 20);

        int row = sr + dr;
        int col = sc + dc;

        while (inBounds(row, col)) {
            damageCell(row, col, damage, &shooter);
            row += dr;
            col += dc;
        }
    }
    else if (weapon == flamethrower) {
        int damage = randomBetween(30, 50);

        int sideR = 0;
        int sideC = 0;

        if (dr == 0) {
            sideR = 1;
            sideC = 0;
        } else if (dc == 0) {
            sideR = 0;
            sideC = 1;
        } else {
            sideR = -dr;
            sideC = dc;
        }

        for (int step = 1; step <= 4; step++) {
            int centerR = sr + dr * step;
            int centerC = sc + dc * step;

            for (int offset = -1; offset <= 1; offset++) {
                int row = centerR + sideR * offset;
                int col = centerC + sideC * offset;

                if (inBounds(row, col)) {
                    damageCell(row, col, damage, &shooter);
                }
            }
        }
    }
    else if (weapon == grenade) {
        if (shooter.robot->get_grenades() <= 0) {
            std::cout << "  no grenades left\n";
            return;
        }

        shooter.robot->decrement_grenades();

        int damage = randomBetween(10, 40);

        for (int r = shotRow - 1; r <= shotRow + 1; r++) {
            for (int c = shotCol - 1; c <= shotCol + 1; c++) {
                if (inBounds(r, c)) {
                    damageCell(r, c, damage, &shooter);
                }
            }
        }
    }
    else if (weapon == hammer) {
        if (std::abs(shotRow - sr) <= 1 && std::abs(shotCol - sc) <= 1) {
            int damage = randomBetween(50, 60);
            damageCell(shotRow, shotCol, damage, &shooter);
        } else {
            std::cout << "  hammer target too far away\n";
        }
    }
}

void Arena::damageCell(int row, int col, int damage, RobotInstance* shooter)
{
    int idx = robotAt(row, col);

    if (idx == -1) return;

    if (shooter != nullptr && robots[idx].robot == shooter->robot) {
        return;
    }

    damageRobot(idx, damage);
}

void Arena::damageRobot(int index, int damage)
{
    if (index < 0 || index >= static_cast<int>(robots.size())) return;
    if (!robots[index].alive) return;

    RobotInstance& target = robots[index];

    int armor = target.robot->get_armor();
    int reducedDamage = static_cast<int>(damage * (1.0 - armor * 0.10));

    if (reducedDamage < 0) {
        reducedDamage = 0;
    }

    std::cout << "  hit " << target.robot->m_name << " " << target.symbol
              << " for " << reducedDamage << " damage\n";

    target.robot->take_damage(reducedDamage);
    target.robot->reduce_armor(1);

    if (target.robot->get_health() <= 0) {
        target.alive = false;
        std::cout << "  " << target.robot->m_name << " destroyed\n";
    }
}

bool Arena::checkWin()
{
    int aliveCount = 0;

    for (const auto& r : robots) {
        if (r.alive) aliveCount++;
    }

    return aliveCount <= 1;
}

void Arena::printWinner()
{
    for (const auto& r : robots) {
        if (r.alive) {
            std::cout << "\nGAME OVER: Winner is " << r.robot->m_name
                      << " " << r.symbol << "\n";
            return;
        }
    }

    std::cout << "\nGAME OVER: No winner.\n";
}

void Arena::printArena()
{
    std::cout << "   ";
    for (int c = 0; c < width; c++) {
        std::cout << c % 10 << "  ";
    }
    std::cout << "\n";

    for (int r = 0; r < height; r++) {
        std::cout << r % 10 << "  ";

        for (int c = 0; c < width; c++) {
            int idx = robotAt(r, c);

            if (idx != -1) {
                if (robots[idx].alive) {
                    std::cout << "R" << robots[idx].symbol << " ";
                } else {
                    std::cout << "X" << robots[idx].symbol << " ";
                }
            } else {
                std::cout << board[r][c] << "  ";
            }
        }

        std::cout << "\n";
    }
}