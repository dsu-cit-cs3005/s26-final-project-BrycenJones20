#pragma once

#include <vector>
#include <string>
#include "RobotBase.h"

class Arena {
public:
    struct RobotInstance {
        RobotBase* robot = nullptr;
        void* handle = nullptr;
        int row = 0;
        int col = 0;
        bool alive = true;
        char symbol = '?';
        std::string summary;
    };

    Arena();
    Arena(int w, int h);
    ~Arena();

    bool loadConfig(const std::string& filename);
    void run();
    void loadRobots();
    void takeTurn(int round);
    void printArena();

private:
    int width = 20;
    int height = 20;
    int maxRounds = 100;
    double sleepInterval = 0.5;
    bool liveMode = true;

    int flamethrowerCount = 5;
    int pitCount = 5;
    int moundCount = 5;

    std::vector<std::vector<char>> board;
    std::vector<RobotInstance> robots;

    void setupBoard();
    void placeObstacles();
    void placeRobotRandomly(RobotInstance& r);

    bool inBounds(int row, int col) const;
    bool cellHasRobot(int row, int col) const;
    int robotAt(int row, int col) const;
    bool cellBlocksMovement(int row, int col) const;

    std::vector<RadarObj> scanDirection(RobotInstance& r, int direction);
    char getCellTypeForRadar(int row, int col) const;

    void moveRobot(RobotInstance& r, int direction, int distance);
    void handleShot(RobotInstance& shooter, int shotRow, int shotCol);
    void damageRobot(int index, int damage);
    void damageCell(int row, int col, int damage, RobotInstance* shooter = nullptr);

    int randomBetween(int low, int high);
    bool checkWin();
    void printWinner();
};