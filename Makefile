CXX = g++ -std=c++98
CPPFLAGS = -DDEBUG
SRC = ./src/mysh.cpp
ARGS = -Wall -Wtype-limits -Wextra
BUILD_FOLDER = ./out/
EXE_NAME = mysh

all: $(SRC)
	@ mkdir -p $(BUILD_FOLDER)
	@ $(CXX) $(SRC) -o $(BUILD_FOLDER)$(EXE_NAME) $(CPPFLAGS) $(ARGS)

version-check:
	@ $(CXX) ./src/version_check.cpp -o version_check
	@ ./version_check && rm ./version_check

run:
	@ make all && cd $(BUILD_FOLDER) && ./$(EXE_NAME)

clean:
	@ rm -rf $(BUILD_FOLDER)
