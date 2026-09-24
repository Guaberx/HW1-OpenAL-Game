# HW1-OpenAL-Game
In this repository i'll be developing a simple command line game to test OpenAL

# Requirements:
openal
thread

# To install openal in your sistem run next line:
sudo apt-get install libopenal-dev

# To build and run (note you must have openal installed):
make
make run

# Or compile by hand:
g++ -std=c++11 -o main main.cpp -lopenal -lpthread

# Run from the repository root: the sound paths are relative to it.

# Notes
All sounds must be in WAV format.
If needed to position a sound in the space, then the sound must be Mono.
