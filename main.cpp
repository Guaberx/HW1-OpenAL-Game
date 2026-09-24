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

// ---------------------------------------------------------------------------
// OpenAL error checking. AL reports failures through a queue you must drain
// yourself; wrap every AL call in AL_CHECK(...) so nothing fails silently.
// ---------------------------------------------------------------------------
const char* alErrorString(ALenum err)
{
    switch(err)
    {
    case AL_NO_ERROR:          return "AL_NO_ERROR";
    case AL_INVALID_NAME:      return "AL_INVALID_NAME";
    case AL_INVALID_ENUM:      return "AL_INVALID_ENUM";
    case AL_INVALID_VALUE:     return "AL_INVALID_VALUE";
    case AL_INVALID_OPERATION: return "AL_INVALID_OPERATION";
    case AL_OUT_OF_MEMORY:     return "AL_OUT_OF_MEMORY";
    default:                   return "UNKNOWN_AL_ERROR";
    }
}

bool alCheckError(const char* what, const char* file, int line)
{
    bool failed = false;
    for(ALenum err = alGetError(); err != AL_NO_ERROR; err = alGetError())
    {
        failed = true;
        cerr << "[OpenAL] " << alErrorString(err) << " after " << what
             << " (" << file << ":" << line << ")" << endl;
    }
    return failed;
}

#define AL_CHECK(expr) do { (expr); alCheckError(#expr, __FILE__, __LINE__); } while(0)

// Owns the device/context so they are torn down by the destructor. Declare it
// before any room: rooms must still have a live context when they delete their
// sources and buffers.
struct ALContextGuard
{
    ALCdevice*  device;
    ALCcontext* context;
    ALContextGuard() : device(NULL), context(NULL) {}
    ~ALContextGuard()
    {
        alcMakeContextCurrent(NULL);
        if(context) alcDestroyContext(context);
        if(device)  alcCloseDevice(device);
    }
};

struct sound
{
    const char* path;
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

// Loads 8/16-bit PCM WAV. On ANY failure it prints the reason, returns NULL and
// leaves chan/samplerate/bps/size at 0 -- the caller must not use them.
char * loadWAV(const char* fn, int& chan, int& samplerate, int& bps, int& size)
{
    chan = 0; samplerate = 0; bps = 0; size = 0;

    char buffer[4];
    ifstream in(fn, ios::binary);
    if(!in)
    {
        cerr << "CANNOT OPEN FILE: " << fn << endl;
        return NULL;
    }
    if(!in.read(buffer,4) || strncmp(buffer,"RIFF",4)!=0)
    {
        cerr << "THIS IS NOT A WAV FILE: " << fn << endl;
        return NULL;
    }
    in.read(buffer,4);                                  //riff chunk size
    if(!in.read(buffer,4) || strncmp(buffer,"WAVE",4)!=0)
    {
        cerr << "MISSING WAVE HEADER: " << fn << endl;
        return NULL;
    }
    if(!in.read(buffer,4) || strncmp(buffer,"fmt ",4)!=0)
    {
        cerr << "MISSING fmt CHUNK: " << fn << endl;
        return NULL;
    }
    if(!in.read(buffer,4))
    {
        cerr << "TRUNCATED fmt CHUNK: " << fn << endl;
        return NULL;
    }
    int fmtSize = convertToInt(buffer,4);
    in.read(buffer,2);
    int audioFormat = convertToInt(buffer,2);           //1 = uncompressed PCM
    in.read(buffer,2);
    chan = convertToInt(buffer,2);
    in.read(buffer,4);
    samplerate = convertToInt(buffer,4);
    in.read(buffer,4);                                  //byte rate
    in.read(buffer,2);                                  //block align
    if(!in.read(buffer,2))
    {
        cerr << "TRUNCATED fmt CHUNK: " << fn << endl;
        chan = 0; samplerate = 0;
        return NULL;
    }
    bps = convertToInt(buffer,2);
    if(fmtSize > 16) in.seekg(fmtSize - 16, ios::cur);  //skip WAVE_FORMAT_EXTENSIBLE tail

    if(audioFormat != 1)
    {
        cerr << "NOT UNCOMPRESSED PCM (format " << audioFormat << "): " << fn << endl;
        chan = 0; samplerate = 0; bps = 0;
        return NULL;
    }
    if(chan != 1 && chan != 2)
    {
        cerr << "UNSUPPORTED CHANNEL COUNT (" << chan << "): " << fn << endl;
        chan = 0; samplerate = 0; bps = 0;
        return NULL;
    }
    if(bps != 8 && bps != 16)
    {
        cerr << "UNSUPPORTED BITS PER SAMPLE (" << bps << "): " << fn << endl;
        chan = 0; samplerate = 0; bps = 0;
        return NULL;
    }

    if(!in.read(buffer,4) || strncmp(buffer,"data",4)!=0)
    {
        cerr << "MISSING data CHUNK (unsupported WAV layout): " << fn << endl;
        chan = 0; samplerate = 0; bps = 0;
        return NULL;
    }
    if(!in.read(buffer,4))
    {
        cerr << "TRUNCATED data CHUNK: " << fn << endl;
        chan = 0; samplerate = 0; bps = 0;
        return NULL;
    }
    size = convertToInt(buffer,4);
    if(size <= 0)
    {
        cerr << "EMPTY OR INVALID data CHUNK (" << size << " bytes): " << fn << endl;
        chan = 0; samplerate = 0; bps = 0; size = 0;
        return NULL;
    }

    char * data = new char[size];
    if(!in.read(data,size))
    {
        cerr << "TRUNCATED AUDIO DATA: " << fn << endl;
        delete [] data;
        chan = 0; samplerate = 0; bps = 0; size = 0;
        return NULL;
    }
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
    thread roomSounds;
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
    roomSounds = thread(&room::playSounds,this);
    while (currentRoom == this && gameRunning)
    {
        /* code */
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
    roomSounds.detach();
    room::stopSounds();
    
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
        room::playSounds();
        //room::enterRoom();
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
    int channel = 0, sampleRate = 0, bps = 0, size = 0;
    char* data = NULL;
    int format = 0;
    room* tmp = NULL;
    //Create 4 posible next rooms
    for (int i = 0; i < 4; i++)
    {
        connectedRooms.push_back(tmp);
    }
    
    //Loads all wav data for the room and creates its buffers and sources
    for(int i = 0; i < n; i++) {
        data = loadWAV(sounds.at(i).path,channel,sampleRate,bps,size);
        if(data == NULL)
        {
            cerr << "FATAL: could not load \"" << sounds.at(i).path
                 << "\". Aborting." << endl;
            exit(EXIT_FAILURE);
        }
        wavData.push_back(data);
        AL_CHECK(alGenBuffers(1, &bufferid));
        
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
        AL_CHECK(alBufferData(bufferid,format,data,size,sampleRate));
        AL_CHECK(alGenSources(1,&sourceid));
        AL_CHECK(alSourcei(sourceid,AL_BUFFER,bufferid));
        AL_CHECK(alSourcei(sourceid,AL_LOOPING, AL_TRUE));

        bufferids.push_back(bufferid);
        sourceids.push_back(sourceid);
        AL_CHECK(alSource3f(sourceid,AL_POSITION,sounds.at(i).x,sounds.at(i).y,sounds.at(i).z));
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
    for (size_t i = 0; i < sourceids.size(); i++)
    {
        AL_CHECK(alSourcePlay(sourceids.at(i)));
    }
    this_thread::sleep_for(chrono::seconds(5));
    room::stopSounds();

}

void room::pauseSounds()
{
    for (size_t i = 0; i < sourceids.size(); i++)
    {
        AL_CHECK(alSourcePause(sourceids.at(i)));
    }
}

void room::stopSounds()
{
    for (size_t i = 0; i < sourceids.size(); i++)
    {
        AL_CHECK(alSourceStop(sourceids.at(i)));
    }
}

room::~room()
{
    connectedRooms.clear();
    //Sources first: a buffer still attached to a live source cannot be deleted.
    for (size_t i = 0; i < sourceids.size(); i++)
    {
        AL_CHECK(alDeleteSources(1,&sourceids.at(i)));
    }
    sourceids.clear();
    for (size_t i = 0; i < bufferids.size(); i++)
    {
        AL_CHECK(alDeleteBuffers(1,&bufferids.at(i)));
    }
    bufferids.clear();
    for (size_t i = 0; i < wavData.size(); i++)
    {
        delete [] wavData.at(i);
    }
    wavData.clear();
    
}

int main()
{
    //INITIALIZATION
    //Declared before any room so it is destroyed LAST: ~room() still needs a
    //live context to delete its sources and buffers.
    ALContextGuard al;

    al.device = alcOpenDevice(NULL);
    if(al.device == NULL)
    {
        cerr << "Cannot Open Sound card" << endl;
        return EXIT_FAILURE;
    }
    al.context = alcCreateContext(al.device,NULL);
    if(al.context == NULL)
    {
        cerr << "Cannot Open Context" << endl;
        return EXIT_FAILURE;
    }
    alcMakeContextCurrent(al.context);

    ALfloat listenerOri[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    AL_CHECK(alListener3f(AL_POSITION, 0.0f, 0.0f, 1.0f));
    AL_CHECK(alListener3f(AL_VELOCITY, 0, 0, 0));
    AL_CHECK(alListenerfv(AL_ORIENTATION, listenerOri));

    vector<sound> EntranceSounds;
    vector<sound> room1Sounds;
    vector<sound> HuesosSounds;
    vector<sound> DragonSounds;
    vector<sound> TesoroDragonSounds;
    vector<sound> CogerParteTesoroSounds;
    vector<sound> RioAfueraSounds;
    vector<sound> RioNadandoSounds;
    vector<sound> WinSounds;
    vector<sound> GameOverSounds;
    //Cuando se agrega un sonido, 
    EntranceSounds.push_back({"./assets/sounds/Sonido de la cueva.wav",0,0,0});
    EntranceSounds.push_back({"./assets/sounds/algo.wav",0,0,0});

    room1Sounds.push_back({"./assets/sounds/Sonido de la cueva.wav",0,0,0});
    room1Sounds.push_back({"./assets/sounds/Pasos Caminando.wav",0,0,0});

    HuesosSounds.push_back({"./assets/sounds/Sonido de la cueva.wav",0,0,0});
    HuesosSounds.push_back({"./assets/sounds/mono/Sonido Fractura de hueso.wav",2,0,3});
    HuesosSounds.push_back({"./assets/sounds/mono/Sonido Fractura de hueso.wav",-2,0,-1});
    HuesosSounds.push_back({"./assets/sounds/mono/Sonido Dragon Durmiendo.wav",0,0,15});

    DragonSounds.push_back({"./assets/sounds/Sonido Dragon Durmiendo.wav",0,0,0});

    TesoroDragonSounds.push_back({"./assets/sounds/Sonido Tesoro.wav",0,0,0});
    
    CogerParteTesoroSounds.push_back({"./assets/sounds/test.wav",0,0,0});

    RioAfueraSounds.push_back({"./assets/sounds/Sonido agua.wav",0,0,0});
    
    RioNadandoSounds.push_back({"./assets/sounds/mono/Sonido agua.wav",0,0,0});
    
    WinSounds.push_back({"./assets/sounds/mono/algo.wav",0,0,0});

    GameOverSounds.push_back({"./assets/sounds/mono/Sonido Fuego Dragon.wav",0,0,0});
    GameOverSounds.push_back({"./assets/sounds/mono/Sonido Entorno en Llamas.wav",0,0,0});
    GameOverSounds.push_back({"./assets/sounds/mono/Sonido Fuego Dragon.wav",0,0,0});
    GameOverSounds.push_back({"./assets/sounds/mono/Sonido Cuerpo Quemandose.wav",0,0,0});



    

    

    room Entrance(
        "Te acabas de despertar...\nNotas que estas en un lugar obscuro. Parece una cueva y solo entran unos pocos rayos de luz.\n\
        Notas que la entrada ha quedado obstruida por una gran piedra y parece que tu unica opcion es seguir adelante",
    "w: Adentrarse en la cueva", EntranceSounds);

    room room1(
        "Aqui el personaje escucha una brisa por a alguno de los lados. Asi se guia y se da cuenta de que hay una salida de la cueva. Porque hay brisa",
    "a: Ir a la izquierda\nd: Ir a la derecha\ns: Ir atras", room1Sounds);

    room Huesos(
        "Te resbalas hacia un hueco y caes en unos huesos",
    "w: Seguir hacia adelante\n", HuesosSounds);

    room Dragon(
        "Tienes al frente tuyo un dragon dormido",
    "a: Ir hacia la izquierda\nd: Ir hacia la derecha\ns: Volver a los huesos", DragonSounds);

    room TesoroDragon(
        "Ves el tesoro y quedas asombrado de las riquezas del dragon",
    "w: Coger Parte del Tesoro\nd: Ir a la derecha (Devolverse)", TesoroDragonSounds);

    room CogerParteTesoro(
        "El dragon se ha despertado mientras coges parte de su tesoro",
    "w: Intentar huir del dragon", CogerParteTesoroSounds);

    room RioAfuera(
        "",
    "a: Ir a la derecha\nw: Meterse al agua", RioAfueraSounds);

    room RioNadando(
        "Llevas mucho tiempo nadando en aquel rio obscuro. No sabes si saldras con vida",
    "w: Seguir nadando\n:s Ahogarse!", RioNadandoSounds);

    room Win(
        "Ganaste el juego",
    "w: Volver a jugar", WinSounds);

    room GameOver(
        "\n\nHas Perdido!!!\n\n",
    "w: Volver a jugar", GameOverSounds);


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

    //Device and context are released by ~ALContextGuard, after every room.
    return 0;
}