#include "RobotBase.h"
#include <vector>
#include <cstdlib>
#include <cmath>
#include <limits>

class Robot_BrycenBot : public RobotBase
{
private:
    int targetRow = -1;
    int targetCol = -1;
    int radarDir = 0;
    int turnsSinceSeen = 100;

    int distance(int r1, int c1, int r2, int c2)
    {
        return std::abs(r1 - r2) + std::abs(c1 - c2);
    }

public:
    Robot_BrycenBot() : RobotBase(4, 3, railgun)
    {
        m_name = "BrycenBot";
        m_character = 'B';
    }

    void get_radar_direction(int& dir) override
    {
        if (targetRow != -1 && turnsSinceSeen < 4) {
            int r, c;
            get_current_location(r, c);

            int dr = targetRow - r;
            int dc = targetCol - c;

            if (dr < 0 && dc == 0) dir = 1;
            else if (dr < 0 && dc > 0) dir = 2;
            else if (dr == 0 && dc > 0) dir = 3;
            else if (dr > 0 && dc > 0) dir = 4;
            else if (dr > 0 && dc == 0) dir = 5;
            else if (dr > 0 && dc < 0) dir = 6;
            else if (dr == 0 && dc < 0) dir = 7;
            else if (dr < 0 && dc < 0) dir = 8;
            else dir = 0;
            return;
        }

        radarDir++;
        if (radarDir > 8) radarDir = 0;
        dir = radarDir;
    }

    void process_radar_results(const std::vector<RadarObj>& radar) override
    {
        turnsSinceSeen++;

        int myRow, myCol;
        get_current_location(myRow, myCol);

        int bestDist = std::numeric_limits<int>::max();
        bool found = false;

        for (const auto& obj : radar) {
            if (obj.m_type == 'R') {
                int d = distance(myRow, myCol, obj.m_row, obj.m_col);

                if (d < bestDist) {
                    bestDist = d;
                    targetRow = obj.m_row;
                    targetCol = obj.m_col;
                    found = true;
                }
            }
        }

        if (found) {
            turnsSinceSeen = 0;
        }
    }

    bool get_shot_location(int& shotRow, int& shotCol) override
    {
        if (targetRow == -1 || turnsSinceSeen > 3) {
            return false;
        }

        shotRow = targetRow;
        shotCol = targetCol;
        return true;
    }

    void get_move_direction(int& dir, int& dist) override
    {
        int r, c;
        get_current_location(r, c);

        if (targetRow != -1 && turnsSinceSeen <= 3) {
            int dr = targetRow - r;
            int dc = targetCol - c;

            if (std::abs(dr) > std::abs(dc)) {
                dir = (dr > 0) ? 5 : 1;
            } else {
                dir = (dc > 0) ? 3 : 7;
            }

            dist = 1;
            return;
        }

        if (r <= 2) {
            dir = 5;
        }
        else if (r >= m_board_row_max - 3) {
            dir = 1;
        }
        else if (c <= 2) {
            dir = 3;
        }
        else if (c >= m_board_col_max - 3) {
            dir = 7;
        }
        else {
            int choices[] = {1, 3, 5, 7};
            dir = choices[std::rand() % 4];
        }

        dist = 1;
    }
};

extern "C" RobotBase* create_robot()
{
    return new Robot_BrycenBot();
}

extern "C" const char* robot_summary()
{
    return "Patrols center, scans, railguns fresh targets.";
}