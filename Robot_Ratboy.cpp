#include "RobotBase.h"
#include <vector>
#include <cmath>
#include <limits>
#include <cstdlib>

class Robot_Ratboy : public RobotBase
{
private:
    int targetRow = -1;
    int targetCol = -1;

    int distance(int r1, int c1, int r2, int c2)
    {
        return std::abs(r1 - r2) + std::abs(c1 - c2);
    }

public:
    Robot_Ratboy() : RobotBase(3, 4, railgun)
    {
        m_name = "Ratboy";
        m_character = '@';
    }

    void get_radar_direction(int& dir) override
    {
        if (targetRow == -1) {
            dir = std::rand() % 9;
        } else {
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
        }
    }

    void process_radar_results(const std::vector<RadarObj>& radar) override
    {
        int myRow, myCol;
        get_current_location(myRow, myCol);

        int best = std::numeric_limits<int>::max();

        for (const auto& obj : radar) {
            if (obj.m_type == 'R') {
                int d = distance(myRow, myCol, obj.m_row, obj.m_col);

                if (d < best) {
                    best = d;
                    targetRow = obj.m_row;
                    targetCol = obj.m_col;
                }
            }
        }
    }

    bool get_shot_location(int& shotRow, int& shotCol) override
    {
        if (targetRow == -1) {
            return false;
        }

        shotRow = targetRow;
        shotCol = targetCol;
        return true;
    }

    void get_move_direction(int& dir, int& dist) override
    {
        if (targetRow == -1) {
            dir = (std::rand() % 8) + 1;
            dist = 1;
            return;
        }

        int r, c;
        get_current_location(r, c);

        int dr = targetRow - r;
        int dc = targetCol - c;

        if (std::rand() % 4 == 0) {
            dir = (std::rand() % 8) + 1;
        }
        else if (std::abs(dr) > std::abs(dc)) {
            dir = (dr > 0) ? 5 : 1;
        }
        else {
            dir = (dc > 0) ? 3 : 7;
        }

        dist = 1;
    }
};

extern "C" RobotBase* create_robot()
{
    return new Robot_Ratboy();
}

extern "C" const char* robot_summary()
{
    return "Tracks nearest target, railguns on sight.";
}