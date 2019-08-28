//Juan Fernando Otoya
//Dani Julian Murcia
#include <iostream>
#include <fstream>
#include <cstring>
#include <AL/al.h>
#include <AL/alc.h>
#include <thread>
#include <chrono>
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

class room
{
private:
    /* data */
public:
    room(/* args */);
    ~room();
};

room::room(/* args */)
{
}

room::~room()
{
}



int main(int argc, char** argv)
{
    int channel, sampleRate, bps, size;
    char* data = loadWAV("./assets/sounds/Battle.wav",channel,sampleRate,bps,size);
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
    
    unsigned int bufferid;
    alGenBuffers(1, &bufferid);

    int format;
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
    unsigned int sourceid;
    alGenSources(1,&sourceid);
    alSourcei(sourceid,AL_BUFFER,bufferid);
    
    ALfloat listenerOri[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    alSourcePlay(sourceid);

    // char select = 0;
    // while(select != 'q'){
    //     cin >> select;
    // }
    this_thread::sleep_for(chrono::seconds(5));

    alDeleteSources(1,&sourceid);
    alDeleteBuffers(1,&bufferid);
    

    alcDestroyContext(context);
    alcCloseDevice(device);
    delete [] data;
    return 0;
}