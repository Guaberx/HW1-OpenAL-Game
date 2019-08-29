//Juan Fernando Otoya
//Dani Julian Murcia
#include <iostream>
#include <fstream>
#include <cstring>
#include <AL/al.h>
#include <AL/alc.h>
#include <thread>
#include <chrono>
#include <vector>
#include <stdlib.h>

using namespace std;

bool isBigEndian(){
    int a = 1;
    return !((char*)&a)[0];
}

int convertToInt(char* buffer, int len)
{
    int a = 0;
    if(isBigEndian())
    {
        for(int i = 0; i < len; i++)
        {
            ((char*)&a)[3-i] = buffer[i];
        }
    }else
    {
        for(int i = 0; i < len; i++)
        {
            ((char*)&a)[i] = buffer[i];
        }
    }
    return a;
}

char * loadWAV(const char* fn, int& chan, int& samplerate, int& bps, int& size)
{
    // TODO: Check for errors
    char buffer[4];
    ifstream in(fn,ios::binary);
    in.read(buffer,4);
    if(strncmp(buffer,"RIFF",4)!=0){
        cout << "THIS IS NOT A WAV FILE" << endl;
        return NULL;
    }
    in.read(buffer,4);
    in.read(buffer,4);//WAVE
    in.read(buffer,4);//fmt 
    in.read(buffer,4);//16
    in.read(buffer,2);//1
    in.read(buffer,2);
    chan = convertToInt(buffer,2);
    in.read(buffer,4);
    samplerate = convertToInt(buffer,4);
    in.read(buffer,4);
    in.read(buffer,2);
    in.read(buffer,2);
    bps = convertToInt(buffer,2);
    in.read(buffer,4);//data
    in.read(buffer,4);
    size = convertToInt(buffer,4);
    char * data = new char[size];
    in.read(data,size);
    return data;
}

enum Sides {topRoom, botRoom, leftRoom, rightRoom};

class room
{
private:
    string description;
    string options;
    vector<room*> connectedRooms;
    vector<unsigned int> bufferids;
    vector<unsigned int> sourceids;
    vector<char*> wavData;
public:
    room(string roomDescription, string roomOptions, vector<char*> soundsPath);
    void connectRoom(room* r, Sides roomPosition);
    room* gotoRoom(Sides nextRoom);
    void enterRoom();
    void showOptions();
    void getOptions();
    void playSounds();
    void pauseSounds();
    void stopSounds();
    ~room();
};

bool gameRunning = true;
room* currentRoom = NULL;


void room::enterRoom()
{
    system("clear");
    room::showOptions();
    room::getOptions();
}

void room::showOptions()
{
    cout << "Select one of the next options and press ENTER" << endl;
    cout << "q: Quit Game" << endl;
    cout << "l: Listen Sounds" << endl;
    cout << options << endl;
}

void room::getOptions()
{
    char option = 0;
    room* tmp = NULL;
    cout << "-->> ";
    cin >> option;
    switch (option)
    {
    case 'w':
        tmp = gotoRoom(topRoom);
        break;
    case 's':
        tmp = gotoRoom(botRoom);
        break;
    case 'a':
        tmp = gotoRoom(leftRoom);
        break;
    case 'd':
        tmp = gotoRoom(rightRoom);
        break;
    case 'l':
        cout << "Listening the Room... ONLY FOR FIVE SECONDS!!!";
        this_thread::sleep_for(chrono::seconds(1));
        room:playSounds();
        room::enterRoom();
        break;
    case 'q':
        tmp = NULL;
        gameRunning = false;
        break;
    default:
        cout << "Not a valid Option... Duh!";
        room::getOptions();
        break;
    }
    if (tmp != NULL)
    {
        currentRoom = tmp;
    }
    
}

room::room(string roomDescription, string roomOption, vector<char*> soundsPath)
{
    unsigned int bufferid, sourceid;
    description = roomDescription;
    options = roomOption;
    int n = soundsPath.size();
    int channel, sampleRate, bps, size;
    char* data;
    int format;
    room* tmp = NULL;

    //Create 4 posible next rooms
    for (int i = 0; i < 4; i++)
    {
        connectedRooms.push_back(tmp);
    }
    
    //Loads all wav data for the room and creates its buffers and sources
    for(int i = 0; i < n; i++) {
        data = loadWAV(soundsPath.at(i),channel,sampleRate,bps,size);
        wavData.push_back(data);
        alGenBuffers(1, &bufferid);
        
        if(channel==1)
        {
            if(bps==8)
            {
                format=AL_FORMAT_MONO8;
            }else{
                format=AL_FORMAT_MONO16;
            }
        }else{
            if(bps == 8)
            {
                format=AL_FORMAT_STEREO8;
            }else{
                format=AL_FORMAT_STEREO16;
            }
        }
        alBufferData(bufferid,format,data,size,sampleRate);
        alGenSources(1,&sourceid);
        alSourcei(sourceid,AL_BUFFER,bufferid);
        
        bufferids.push_back(bufferid);
        sourceids.push_back(sourceid);
    }
}

void room::connectRoom(room* r, Sides roomPosition)
{
    connectedRooms.at(roomPosition) = r;
}

room* room::gotoRoom(Sides nextRoom)
{
    return connectedRooms.at(nextRoom);
}

void room::playSounds()
{
    for (int i = 0; i < sourceids.size(); i++)
    {
        alSourcePlay(sourceids.at(i));
    }
    this_thread::sleep_for(chrono::seconds(5));
    room::stopSounds();

}

void room::pauseSounds()
{
    for (int i = 0; i < sourceids.size(); i++)
    {
        alSourcePause(sourceids.at(i));
    }
}

void room::stopSounds()
{
    for (int i = 0; i < sourceids.size(); i++)
    {
        alSourceStop(sourceids.at(i));
    }
}

room::~room()
{
    connectedRooms.clear();
    for (int i = 0; i < bufferids.size(); i++)
    {     
        alDeleteSources(1,&bufferids.at(i));
        alDeleteBuffers(1,&bufferids.at(i));
    }
    for (int i = 0; i < wavData.size(); i++)
    {     
        delete [] wavData.at(i);
    }
    wavData.clear();
    
}

int main(int argc, char** argv)
{
    //INITIALIZATION   
    ALCdevice* device = alcOpenDevice(NULL);
    if(device == NULL)
    {
        cout << "Cannot Open Sound card" << endl;
        return 0;
    }
    ALCcontext* context = alcCreateContext(device,NULL);
    if(context == NULL)
    {
        cout << "Cannot Open Context" << endl;
        return 0;
    }
    alcMakeContextCurrent(context);
    
    ALfloat listenerOri[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    alListener3f(AL_POSITION, 0.0f, 0.0f, 1.0f);
    // check for errors
    alListener3f(AL_VELOCITY, 0, 0, 0);
    // check for errors
    alListenerfv(AL_ORIENTATION, listenerOri);
    // check for errors

    vector<char*> salon1;
    salon1.push_back("./assets/sounds/class.wav");

    vector<char*> campamento1;
    campamento1.push_back("./assets/sounds/night.wav");

    vector<char*> aereoupuerto1;
    aereoupuerto1.push_back("./assets/sounds/airplaneatgate.wav");

    vector<char*> piscina1;
    piscina1.push_back("./assets/sounds/nightwater.wav");

    room testRoom1("Este es el salon de Clases","w: ir al campamento\na: Ir al aereoupuerto\nd: Ir a la piscina", salon1);
    room testRoom2("Este es el campamento","w: ir a clase\n", campamento1);
    room testRoom3("Este es el aereoupuerto","d: ir a clase\n", aereoupuerto1);
    room testRoom4("Este es la piscina","a: ir a clase\n", piscina1);

    //enum Sides {topRoom, botRoom, leftRoom, rightRoom};

    testRoom1.connectRoom(&testRoom2, topRoom);
    testRoom1.connectRoom(&testRoom3, leftRoom);
    testRoom1.connectRoom(&testRoom4, rightRoom);

    testRoom2.connectRoom(&testRoom1, botRoom);

    testRoom3.connectRoom(&testRoom1, rightRoom);

    testRoom4.connectRoom(&testRoom1, leftRoom);
    


    currentRoom = &testRoom1;
    //GAME LOOP
    while (gameRunning)
    {
        currentRoom->enterRoom();
    }
    
    

    alcDestroyContext(context);
    alcCloseDevice(device);
    
    return 0;
}