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
    //Tittle
    cout << "  _________                        .___ .____          ___.         .__        __"   << endl;
    cout << " /   _____/ ____  __ __  ____    __| _/ |    |   _____ \\_ |_________|__| _____/  |_ "<< endl;
    cout << " \\_____  \\ /  _ \\|  |  \\/    \\  / __ |  |    |   \\__  \\ | __ \\_  __ \\  |/    \\   __\\"<< endl;
    cout << " /        (  <_> )  |  /   |  \\/ /_/ |  |    |___ / __ \\| \\_\\ \\  | \\/  |   |  \\  |  "<< endl;
    cout << "/_______  /\\____/|____/|___|  /\\____ |  |_______ (____  /___  /__|  |__|___|  /__|  "<< endl;
    cout << "        \\/                  \\/      \\/          \\/    \\/    \\/              \\/      "<< endl;
    //Description
    cout << endl << description << endl << endl;
    //Options
    room::showOptions();
    room::getOptions();
}

void room::showOptions()
{
    cout << "Select one of the next options and press ENTER" << endl << endl;
    cout << "q: Quit Game" << endl;
    cout << "l: Listen Sounds" << endl << endl;
    cout << options << endl << endl;
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
        
        cout << "Listening the Room... ONLY FOR FIVE SECONDS!!!" << endl;
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

    vector<char*> soundsSalon;
    soundsSalon.push_back("./assets/sounds/class.wav");

    vector<char*> soundsCampamento;
    soundsCampamento.push_back("./assets/sounds/night.wav");

    vector<char*> soundsAeropuerto;
    soundsAeropuerto.push_back("./assets/sounds/airplaneatgate.wav");

    vector<char*> soundsPiscina;
    soundsPiscina.push_back("./assets/sounds/nightwater.wav");

    vector<char*> soundsBatalla;
    soundsBatalla.push_back("./assets/sounds/Battle.wav");

    room salon("Este es el salon de Clases","w: ir al campamento\na: Ir al aereoupuerto\nd: Ir a la piscina", soundsSalon);
    room campamento("Este es el campamento","s: ir a clase\n", soundsCampamento);
    room aeropuerto("Este es el aereoupuerto","d: ir a clase\n", soundsAeropuerto);
    room piscina("Este es la piscina","a: ir a clase\ns: Ir a la BATALLA!!!!", soundsPiscina);
    room batalla("Bienvenido a la Batalla PUTOS!!!!!\nDe ahora en adelante deberan cuidar su propia vida y nadie, absolutamente nadie los endra en cuenta HIJOS DE SU PUTA MADRE.\nAhora vayan a la batalla y luchen con mi pais","w: Volver a la piscina\na: ir a clase\n", soundsBatalla);

    //enum Sides {topRoom, botRoom, leftRoom, rightRoom};

    salon.connectRoom(&campamento, topRoom);
    salon.connectRoom(&aeropuerto, leftRoom);
    salon.connectRoom(&piscina, rightRoom);

    campamento.connectRoom(&salon, botRoom);

    aeropuerto.connectRoom(&salon, rightRoom);

    piscina.connectRoom(&salon, leftRoom);
    piscina.connectRoom(&batalla, botRoom);

    batalla.connectRoom(&piscina, topRoom);

    currentRoom = &salon;
    //GAME LOOP
    while (gameRunning)
    {
        currentRoom->enterRoom();
    }
    

    alcDestroyContext(context);
    alcCloseDevice(device);
    
    return 0;
}