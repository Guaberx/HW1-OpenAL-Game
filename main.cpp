//Juan Fernando Otoya
//Dani Julian Murcia
#include <iostream>
#include <fstream>
#include <cstring>
#include <AL/al.h>
#include <AL/alc.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <map>
#include <string>
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

void clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// Buffers are shared between rooms, keyed by file path: a file used by several
// rooms is decoded and uploaded once. Defined in getBuffer() below; freed here
// because buffers outlive every room.
extern map<string, ALuint> bufferCache;
void freeBufferCache();

// Owns the device/context so they are torn down by the destructor. Declare it
// before any room: rooms must still have a live context when they delete their
// sources, and the shared buffers must go after every source is gone.
struct ALContextGuard
{
    ALCdevice*  device;
    ALCcontext* context;
    ALContextGuard() : device(NULL), context(NULL) {}
    ~ALContextGuard()
    {
        freeBufferCache();
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


// WAV integers are ALWAYS little-endian, so byte i carries weight 2^(8*i).
// Building the value by shifting is correct on every host, which is why the old
// isBigEndian() byte-copy (it wrote a 2-byte value into the wrong half on
// big-endian machines) is gone.
int convertToInt(const char* buffer, int len)
{
    int a = 0;
    for(int i = 0; i < len; i++)
    {
        a |= (int)(unsigned char)buffer[i] << (8*i);
    }
    return a;
}

// Loads 8/16-bit PCM WAV. Walks the RIFF chunk list instead of assuming a
// fixed header layout, so files carrying LIST/fact/extensible chunks before
// "data" load correctly. On ANY failure it prints the reason, returns NULL and
// leaves chan/samplerate/bps/size at 0 -- the caller must not use them.
char * loadWAV(const char* fn, int& chan, int& samplerate, int& bps, int& size)
{
    chan = 0; samplerate = 0; bps = 0; size = 0;

    char id[4];
    char header[16];
    ifstream in(fn, ios::binary);
    if(!in)
    {
        cerr << "CANNOT OPEN FILE: " << fn << endl;
        return NULL;
    }
    if(!in.read(id,4) || strncmp(id,"RIFF",4)!=0)
    {
        cerr << "THIS IS NOT A WAV FILE: " << fn << endl;
        return NULL;
    }
    in.read(id,4);                                       //riff chunk size
    if(!in.read(id,4) || strncmp(id,"WAVE",4)!=0)
    {
        cerr << "MISSING WAVE HEADER: " << fn << endl;
        return NULL;
    }

    bool haveFmt = false;
    int audioFormat = 0;

    //Walk chunks until "data", skipping anything we do not understand.
    while(true)
    {
        char sizeField[4];
        if(!in.read(id,4) || !in.read(sizeField,4))
        {
            cerr << (haveFmt ? "MISSING data CHUNK: " : "MISSING fmt CHUNK: ") << fn << endl;
            chan = 0; samplerate = 0; bps = 0;
            return NULL;
        }
        int chunkSize = convertToInt(sizeField,4);
        if(chunkSize < 0)
        {
            cerr << "INVALID CHUNK SIZE: " << fn << endl;
            chan = 0; samplerate = 0; bps = 0;
            return NULL;
        }

        if(strncmp(id,"fmt ",4)==0)
        {
            if(chunkSize < 16 || !in.read(header,16))
            {
                cerr << "TRUNCATED fmt CHUNK: " << fn << endl;
                return NULL;
            }
            audioFormat = convertToInt(header,2);         //1 = uncompressed PCM
            chan        = convertToInt(header+2,2);
            samplerate  = convertToInt(header+4,4);
            //header+8 byte rate, header+12 block align
            bps         = convertToInt(header+14,2);
            haveFmt     = true;
            if(chunkSize > 16) in.seekg(chunkSize - 16, ios::cur);
        }
        else if(strncmp(id,"data",4)==0)
        {
            if(!haveFmt)
            {
                cerr << "data CHUNK BEFORE fmt CHUNK: " << fn << endl;
                return NULL;
            }
            size = chunkSize;
            break;
        }
        else
        {
            in.seekg(chunkSize, ios::cur);                //LIST, fact, ...
        }

        if(chunkSize & 1) in.seekg(1, ios::cur);          //chunks are word-aligned
        if(!in)
        {
            cerr << "TRUNCATED FILE WHILE SCANNING CHUNKS: " << fn << endl;
            chan = 0; samplerate = 0; bps = 0;
            return NULL;
        }
    }

    if(audioFormat != 1)
    {
        cerr << "NOT UNCOMPRESSED PCM (format " << audioFormat << "): " << fn << endl;
        chan = 0; samplerate = 0; bps = 0; size = 0;
        return NULL;
    }
    if(chan != 1 && chan != 2)
    {
        cerr << "UNSUPPORTED CHANNEL COUNT (" << chan << "): " << fn << endl;
        chan = 0; samplerate = 0; bps = 0; size = 0;
        return NULL;
    }
    if(bps != 8 && bps != 16)
    {
        cerr << "UNSUPPORTED BITS PER SAMPLE (" << bps << "): " << fn << endl;
        chan = 0; samplerate = 0; bps = 0; size = 0;
        return NULL;
    }
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

// ---------------------------------------------------------------------------
// Shared buffer cache. alBufferData() copies the samples into the AL buffer, so
// the CPU-side array is freed immediately instead of being kept alive for the
// whole run -- that alone was ~120 MB of dead weight.
// ---------------------------------------------------------------------------
map<string, ALuint> bufferCache;

ALuint getBuffer(const char* path)
{
    string key(path);
    map<string, ALuint>::iterator it = bufferCache.find(key);
    if(it != bufferCache.end()) return it->second;

    int channel = 0, sampleRate = 0, bps = 0, size = 0;
    char* data = loadWAV(path, channel, sampleRate, bps, size);
    if(data == NULL)
    {
        cerr << "FATAL: could not load \"" << path << "\". Aborting." << endl;
        exit(EXIT_FAILURE);
    }

    ALenum format;
    if(channel == 1) format = (bps == 8) ? AL_FORMAT_MONO8   : AL_FORMAT_MONO16;
    else             format = (bps == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;

    ALuint bufferid = 0;
    AL_CHECK(alGenBuffers(1, &bufferid));
    AL_CHECK(alBufferData(bufferid, format, data, size, sampleRate));
    delete [] data;

    bufferCache[key] = bufferid;
    return bufferid;
}

void freeBufferCache()
{
    for(map<string, ALuint>::iterator it = bufferCache.begin(); it != bufferCache.end(); ++it)
    {
        AL_CHECK(alDeleteBuffers(1, &it->second));
    }
    bufferCache.clear();
}

enum Sides {topRoom, botRoom, leftRoom, rightRoom};

class room
{
private:
    string description;
    string options;
    vector<room*> connectedRooms;
    vector<ALuint> sourceids;
    //Ambient playback runs on its own thread. It is joined -- never detached --
    //so it cannot outlive the room and touch sourceids after destruction.
    thread roomSounds;
    mutex soundMutex;
    condition_variable soundCv;
    bool soundStop;
public:
    room(string roomDescription, string roomOptions, vector<sound> sounds);
    void connectRoom(room* r, Sides roomPosition);
    room* gotoRoom(Sides nextRoom);
    void enterRoom();
    void showOptions();
    void getOptions();
    void startSoundThread();
    void stopSoundThread();
    void playSounds();
    void pauseSounds();
    void stopSounds();
    ~room();
};

bool gameRunning = true;
room* currentRoom = NULL;

void room::enterRoom()
{
    startSoundThread();
    while (currentRoom == this && gameRunning)
    {
        /* code */
        clearScreen();
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
    stopSoundThread();
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
        //Restarts the ambient thread: the sounds replay in the background
        //instead of freezing the prompt for five seconds.
        startSoundThread();
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
    ALuint sourceid = 0;
    description = roomDescription;
    options = roomOption;
    soundStop = false;
    int n = sounds.size();
    room* tmp = NULL;
    //Create 4 posible next rooms
    for (int i = 0; i < 4; i++)
    {
        connectedRooms.push_back(tmp);
    }
    
    //One source per sound; the buffer behind it is shared through the cache.
    for(int i = 0; i < n; i++) {
        ALuint bufferid = getBuffer(sounds.at(i).path);

        AL_CHECK(alGenSources(1,&sourceid));
        AL_CHECK(alSourcei(sourceid,AL_BUFFER,bufferid));
        AL_CHECK(alSourcei(sourceid,AL_LOOPING, AL_TRUE));

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

//Starts (or restarts) the ambient thread. Always joins the previous one first,
//so only one thread ever touches this room's sources.
void room::startSoundThread()
{
    stopSoundThread();
    {
        lock_guard<mutex> lock(soundMutex);
        soundStop = false;
    }
    roomSounds = thread(&room::playSounds, this);
}

//Signals the ambient thread to finish and waits for it.
void room::stopSoundThread()
{
    if(!roomSounds.joinable()) return;
    {
        lock_guard<mutex> lock(soundMutex);
        soundStop = true;
    }
    soundCv.notify_all();
    roomSounds.join();
}

void room::playSounds()
{
    for (size_t i = 0; i < sourceids.size(); i++)
    {
        AL_CHECK(alSourcePlay(sourceids.at(i)));
    }
    //Sleeps five seconds but wakes immediately if the room is being left, so
    //quitting the game never has to wait on a sleeping thread.
    {
        unique_lock<mutex> lock(soundMutex);
        soundCv.wait_for(lock, chrono::seconds(5), [this]{ return soundStop; });
    }
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
    //Join before anything else: the ambient thread reads sourceids.
    stopSoundThread();
    connectedRooms.clear();
    for (size_t i = 0; i < sourceids.size(); i++)
    {
        AL_CHECK(alDeleteSources(1,&sourceids.at(i)));
    }
    sourceids.clear();
    //Buffers are shared, so they are released once by freeBufferCache().
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
    "a: Ir a la izquierda\ns: Ir atras", room1Sounds);

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
        "Dejas atras al dragon y llegas a la orilla de un rio subterraneo.\n\
        El agua corre helada y se pierde en la oscuridad. Quiza lleve afuera.",
    "a: Volver hacia el dragon\nw: Meterse al agua", RioAfueraSounds);

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
    //La sala de los sabios (d) no esta implementada, asi que no se conecta ni se
    //ofrece: antes apuntaba a room1 misma y parecia que el juego se colgaba.
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
    RioNadando.connectRoom(&GameOver, botRoom); // s Ahogarse: rendirse es perder

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