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

struct sound
{
    char* path;
    int x,y,z;
}typedef sound;


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
    room(string roomDescription, string roomOptions, vector<sound> sounds);
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
    cout << "                               .___ .____          ___.                 .__        __  .__     " << endl;
    cout << "  __________  __ __  ____    __| _/ |    |   _____ \\_ |__ ___.__._______|__| _____/  |_|  |__  " << endl;
    cout << " /  ___/  _ \\|  |  \\/    \\  / __ |  |    |   \\__  \\ | __ <   |  |\\_  __ \\  |/    \\   __\\  |  \\ " << endl;
    cout << " \\___ (  <_> )  |  /   |  \\/ /_/ |  |    |___ / __ \\| \\_\\ \\___  | |  | \\/  |   |  \\  | |   Y  \\" << endl;
    cout << "/____  >____/|____/|___|  /\\____ |  |_______ (____  /___  / ____| |__|  |__|___|  /__| |___|  /" << endl;
    cout << "     \\/                 \\/      \\/          \\/    \\/    \\/\\/                    \\/          \\/ " << endl;
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

room::room(string roomDescription, string roomOption, vector<sound> sounds)
{
    unsigned int bufferid, sourceid;
    description = roomDescription;
    options = roomOption;
    int n = sounds.size();
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
        data = loadWAV(sounds.at(i).path,channel,sampleRate,bps,size);
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
        alSource3f(sourceid,AL_POSITION,sounds.at(i).x,sounds.at(i).y,sounds.at(i).z);
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

    vector<sound> EntranceSounds;
    vector<sound> HuesosSounds;
    //Cuando se agrega un sonido, 
    EntranceSounds.push_back({"./assets/sounds/Battle.wav",10,0,-1});
    EntranceSounds.push_back({"./assets/sounds/Gun1.wav",-10,0,-1});

    HuesosSounds.push_back({"./assets/sounds/Gun2.wav",10,0,-1});

    room Entrance(
        "Te acabas de despertar...\nNotas que estas en un lugar obscuro. Parece una cueva y solo entran unos pocos rayos de luz.\n\
        Notas que la entrada ha quedado obstruida por una gran piedra y parece que tu unica opcion es seguir adelante",
    "w: Adentrarse en la cueva", EntranceSounds);

    room room1(
        "Aqui el personaje escucha una brisa por a alguno de los lados. Asi se guia y se da cuenta de que hay una salida de la cueva. Porque hay brisa",
    "a: Ir a la izquierda\nd: Ir a la derecha\ns: Ir atras", HuesosSounds);

    room Huesos(
        "Te resbalas hacia un hueco y caes en unos huesos",
    "w: Seguir hacia adelante\n", HuesosSounds);

    room Dragon(
        "Tienes al frente tuyo un dragon dormido",
    "a: Ir hacia la izquierda\nd: Ir hacia la derecha\ns: Volver a los huesos", HuesosSounds);

    room TesoroDragon(
        "",
    "w: Coger Parte del Tesoro\nd: Ir a la derecha (Devolverse)", HuesosSounds);

    room CogerParteTesoro(
        "Hoyes que el dragon se ha despertado -> dice quien ha cogido mi tesoro o algo asi",
    "w: Intentar huir del dragon", HuesosSounds);

    room RioAfuera(
        "",
    "a: Ir a la derecha\nd: Meterse al agua", HuesosSounds);

    room RioNadando(
        "Llevas mucho tiempo nadando en aquel rio obscuro. No sabes si saldras con vida",
    "w: Seguir nadando\n:s Ahogarse!", HuesosSounds);

    room Win(
        "Ganaste el juego",
    "w: Volver a jugar", HuesosSounds);

    room GameOver(
        "\n\nHas Perdido!!!\n\n",
    "w: Volver a jugar", HuesosSounds);


    //enum Sides {topRoom, botRoom, leftRoom, rightRoom};


    Entrance.connectRoom(&room1,topRoom);

    room1.connectRoom(&Entrance,botRoom);
    room1.connectRoom(&Huesos,leftRoom); // a para ir a los huesos
    room1.connectRoom(&room1,rightRoom); // d para ir donde los sabios. Esta parte no esta hecha
    //Aqui faltaria agregar las otras opciones desde esta habitacion

    Huesos.connectRoom(&Dragon,topRoom);

    Dragon.connectRoom(&TesoroDragon, leftRoom);// seleccionando a
    Dragon.connectRoom(&RioAfuera, rightRoom);// seleccionando d
    Dragon.connectRoom(&Huesos, botRoom);// seleccionando s

    TesoroDragon.connectRoom(&CogerParteTesoro, topRoom);// seleccionando w
    TesoroDragon.connectRoom(&Dragon, rightRoom);// seleccionando d

    CogerParteTesoro.connectRoom(&GameOver, topRoom);// Aqui el dragon se despierta, esta rugiendo y haciendo sonidos miedosos.

    RioAfuera.connectRoom(&Dragon, leftRoom); // a Devolverse a donde el dragon
    RioAfuera.connectRoom(&RioNadando, topRoom); // w Entrar al rio y seguir su curso

    RioNadando.connectRoom(&Win, topRoom); // w Seguir nadando
    RioNadando.connectRoom(&Win, botRoom); // s Ahogarse

    Win.connectRoom(&Entrance, topRoom);
    GameOver.connectRoom(&Entrance, topRoom);


    
    
    currentRoom = &Entrance;
    //GAME LOOP
    while (gameRunning)
    {
        currentRoom->enterRoom();
    }

    alcDestroyContext(context);
    alcCloseDevice(device);
    
    return 0;
}