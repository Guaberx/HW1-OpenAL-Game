# HW1-OpenAL-Game
In this repository i'll be developing a simple command line game to test OpenAL

# Requirements:
openal
thread

# To install openal in your sistem run next line:
sudo apt-get install libopenal-dev

# In order to Compile with g++ run the next line (note you must have openal installed):
g++ -std=c++11 -o main main.cpp -lopenal -lpthread
