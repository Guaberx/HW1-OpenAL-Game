#Juan Fernando Otoya
from openal import *
import time
import threading
# oal.alSourcePause()

class room:
    def __init__(self):
        self.sources = [] #OpenAL sources and its coordinates
        self.pos = [0,0,0] #x,y,z coordinates of the center of the room
        self.threads = []
    def addsource(self,path):
        self.sources.append(oalOpen(path))
    def playSource(self,index):
        if index <= len(self.sources) - 1:
            self.sources[index].oal.plays()
            while self.sources[index].get_state() == AL_PLAYING:
                time.sleep(1)
    def playAll(self):
        '''Creates a thread per source and plays the sources's sounds'''
        for i in range(len(self.sources)):
            self.threads.append(threading.Thread(self.playSource(i)))
# room1 = room()
# room1.addsource("./assets/sounds/Battle.wav")

source = oalOpen("./assets/sounds/Battle.wav")

# and start playback
source.play()
source.looping = False
if source.get_state() == AL_PLAYING:
    time.sleep(12)
x = 1
while True:
    # room1.playAll()
    #x = input()
    if x == 'q':
        break
    source.play()
oalQuit()