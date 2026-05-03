CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -fPIC

all: RobotWarz test_robot

RobotBase.o: RobotBase.cpp RobotBase.h RadarObj.h
	$(CXX) $(CXXFLAGS) -c RobotBase.cpp

Arena.o: Arena.cpp Arena.h RobotBase.h RadarObj.h
	$(CXX) $(CXXFLAGS) -c Arena.cpp

main.o: main.cpp Arena.h
	$(CXX) $(CXXFLAGS) -c main.cpp

RobotWarz: main.o Arena.o RobotBase.o
	$(CXX) $(CXXFLAGS) main.o Arena.o RobotBase.o -ldl -o RobotWarz

test_robot: test_robot.cpp RobotBase.o
	$(CXX) $(CXXFLAGS) test_robot.cpp RobotBase.o -ldl -o test_robot

clean:
	rm -f *.o *.so RobotWarz test_robot libRobot_*.so