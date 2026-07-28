#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>
static uint32_t be32(const uint8_t*p){return((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint32_t readVlq(const uint8_t*d,size_t sz,size_t&pos){uint32_t v=0;for(int i=0;i<4&&pos<sz;++i){uint8_t b=d[pos++];v=(v<<7)|(b&0x7F);if(!(b&0x80))break;}return v;}
int main(int argc,char**argv){
    if(argc<2)return 1;
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
    size_t sz=f.tellg();f.seekg(0);std::vector<uint8_t>data(sz);f.read((char*)data.data(),sz);
    if(sz<14||memcmp(data.data(),"MThd",4))return 1;
    uint32_t hLen=be32(data.data()+4);size_t tOff=8+hLen;
    if(tOff+8>sz||memcmp(data.data()+tOff,"MTrk",4))return 1;
    uint32_t tLen=be32(data.data()+tOff+4);
    const uint8_t*trk=data.data()+tOff+8;size_t tSz=tLen,pos=0;
    uint32_t tick=0;uint8_t last=0;
    const char*nn[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    bool inMain=false;int noteCount=0;
    while(pos<tSz&&noteCount<200){
        uint32_t d=readVlq(trk,tSz,pos);tick+=d;if(pos>=tSz)break;
        uint8_t st=trk[pos];if(st<0x80){st=last;}else{last=st;pos++;}
        if(st==0xFF){uint8_t t=trk[pos++];uint32_t l=readVlq(trk,tSz,pos);
            if(t==0x06){std::string txt((char*)trk+pos,l);
                if(txt=="Main A")inMain=true;
                else if(inMain){break;}
            }pos+=l;
        }else if(st==0xF0){uint32_t l=readVlq(trk,tSz,pos);pos+=l;}
        else{uint8_t type=st&0xF0;uint8_t ch=st&0x0F;
            if(type==0x90&&inMain){uint8_t note=trk[pos],vel=trk[pos+1];
                if(vel>0&&ch==10){printf("tick=%u ch=%d note=%d (%s%d)\n",tick,ch+1,note,nn[note%12],note/12-1);noteCount++;}
                pos+=2;
            }else if(type==0x80){pos+=2;}
            else if(type==0xC0||type==0xD0){pos++;}
            else{pos+=2;}
        }
    }return 0;
}
