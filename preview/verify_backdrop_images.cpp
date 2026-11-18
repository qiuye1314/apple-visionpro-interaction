// Standalone image/browser regression checks. No GL context or ImGui link is needed.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

using GLuint=unsigned int;
using GLenum=unsigned int;
using GLint=int;
constexpr GLenum GL_TEXTURE_BINDING_2D=0x8069,GL_UNPACK_ALIGNMENT=0x0cf5,GL_NO_ERROR=0;
constexpr GLenum GL_TEXTURE_2D=0x0de1,GL_TEXTURE_MIN_FILTER=0x2801,GL_TEXTURE_MAG_FILTER=0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S=0x2802,GL_TEXTURE_WRAP_T=0x2803,GL_LINEAR=0x2601;
constexpr GLenum GL_RGBA=0x1908,GL_UNSIGNED_BYTE=0x1401;

static GLuint bound=601,nextTexture=1;
static GLint alignment=8,uploadedWidth=0,uploadedHeight=0;
static GLenum pendingError=GL_NO_ERROR;
static bool failUpload=false,failGenerate=false;
static std::unordered_set<GLuint> alive;
static std::vector<GLuint> deleted;
static unsigned char firstPixel[4]{};
static int uploads=0;
void glGetIntegerv(GLenum name,GLint* value){*value=name==GL_TEXTURE_BINDING_2D?static_cast<GLint>(bound):alignment;}
void glDeleteTextures(int count,const GLuint* values){for(int i=0;i<count;++i){alive.erase(values[i]);deleted.push_back(values[i]);if(bound==values[i])bound=0;}}
GLenum glGetError(){const auto error=pendingError;pendingError=GL_NO_ERROR;return error;}
void glGenTextures(int count,GLuint* values){for(int i=0;i<count;++i){values[i]=failGenerate?0:nextTexture++;if(values[i])alive.insert(values[i]);}}
void glBindTexture(GLenum,GLuint value){bound=value;}
void glPixelStorei(GLenum,GLint value){alignment=value;}
void glTexParameteri(GLenum,GLenum,GLint){}
void glTexImage2D(GLenum,int,int,int width,int height,int,GLenum,GLenum,const void* pixels){
    ++uploads;uploadedWidth=width;uploadedHeight=height;std::memcpy(firstPixel,pixels,4);
    if(failUpload)pendingError=0x0505;
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "Main/Images/Files/stb_image.h"
#include "SpatialBackdropImage.h"
#include "SpatialBackdropFiles.h"

namespace fs=std::filesystem;
static void Check(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL %s\n",message);std::exit(2);}}
static bool Deleted(GLuint value){return std::find(deleted.begin(),deleted.end(),value)!=deleted.end();}
static fs::path fixtureRoot;
static std::string Path(const char* name){return (fixtureRoot/fs::u8path(name)).u8string();}
static void Append32(std::vector<unsigned char>& data,std::uint32_t value){
    data.push_back(static_cast<unsigned char>(value>>24));data.push_back(static_cast<unsigned char>(value>>16));
    data.push_back(static_cast<unsigned char>(value>>8));data.push_back(static_cast<unsigned char>(value));
}
static std::uint32_t Crc32(const unsigned char* data,std::size_t count){
    static const auto table=[](){
        std::vector<std::uint32_t> result(256);
        for(std::uint32_t i=0;i<256;++i){std::uint32_t value=i;for(int bit=0;bit<8;++bit)value=(value>>1)^((value&1)?0xedb88320u:0);result[i]=value;}
        return result;
    }();
    std::uint32_t value=0xffffffffu;
    for(std::size_t i=0;i<count;++i)value=table[(value^data[i])&255]^(value>>8);
    return value^0xffffffffu;
}
static void Chunk(std::vector<unsigned char>& png,const char* type,const std::vector<unsigned char>& payload){
    Append32(png,static_cast<std::uint32_t>(payload.size()));const auto start=png.size();
    png.insert(png.end(),type,type+4);png.insert(png.end(),payload.begin(),payload.end());
    Append32(png,Crc32(png.data()+start,png.size()-start));
}
static std::vector<unsigned char> PngHeader(int width,int height){
    std::vector<unsigned char> png{137,80,78,71,13,10,26,10},header;
    Append32(header,width);Append32(header,height);header.insert(header.end(),{8,6,0,0,0});
    Chunk(png,"IHDR",header);return png;
}
static void WriteFile(const fs::path& path,const std::vector<unsigned char>& bytes){
    std::ofstream stream(path,std::ios::binary);Check(static_cast<bool>(stream),"open fixture output");
    stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    stream.close();Check(static_cast<bool>(stream),"write fixture output");
}
static void Png(const fs::path& path,int width,int height,unsigned char red,unsigned char green,unsigned char blue,unsigned char alpha){
    const std::size_t row=static_cast<std::size_t>(width)*4+1;
    std::vector<unsigned char> raw(row*height);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        auto* pixel=raw.data()+y*row+1+x*4;pixel[0]=red;pixel[1]=green;pixel[2]=blue;pixel[3]=alpha;
    }
    // zlib stream containing uncompressed DEFLATE blocks avoids a test dependency on an encoder library.
    std::vector<unsigned char> encoded{0x78,0x01};encoded.reserve(raw.size()+raw.size()/65535*5+16);
    std::size_t offset=0;
    while(offset<raw.size()){
        const auto size=static_cast<unsigned>(std::min<std::size_t>(65535,raw.size()-offset));
        encoded.push_back(offset+size==raw.size()?1:0);
        encoded.push_back(size&255);encoded.push_back(size>>8);encoded.push_back((~size)&255);encoded.push_back(((~size)>>8)&255);
        encoded.insert(encoded.end(),raw.begin()+offset,raw.begin()+offset+size);offset+=size;
    }
    std::uint32_t a=1,b=0;
    for(unsigned char value:raw){a=(a+value)%65521;b=(b+a)%65521;}
    Append32(encoded,(b<<16)|a);
    auto png=PngHeader(width,height);Chunk(png,"IDAT",encoded);Chunk(png,"IEND",{});WriteFile(path,png);
}
static void MakeFixtures(){
    Png(fs::u8path(Path("雪 夜.png")),32,16,42,75,120,170);
    Png(fs::u8path(Path("large.png")),3000,1500,73,111,159,210);
    WriteFile(fs::u8path(Path("invalid.jpg")),{'n','o','t',' ','a','n',' ','i','m','a','g','e'});
    WriteFile(fs::u8path(Path("broken.png")),{137,80,78,71,13,10,26,10,'B','R','O','K','E','N'});
    WriteFile(fs::u8path(Path("wide.png")),PngHeader(9000,1));
    WriteFile(fs::u8path(Path("many_pixels.png")),PngHeader(8192,8192));
    {
        std::ofstream file(fs::u8path(Path("oversize.jpg")),std::ios::binary);
        Check(static_cast<bool>(file),"open oversize fixture");file.write("\xff\xd8\xff",3);
        file.seekp(32*1024*1024);file.put('!');file.close();Check(static_cast<bool>(file),"write oversize fixture");
    }
    WriteFile(fs::u8path(Path("small.jpg")),{
        0xff,0xd8,0xff,0xe0,0x00,0x10,0x4a,0x46,0x49,0x46,0x00,0x01,0x01,0x00,0x00,0x01,0x00,0x01,0x00,0x00,
        0xff,0xdb,0x00,0x43,0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,0x07,0x07,0x07,0x09,0x09,0x08,0x0a,0x0c,
        0x14,0x0d,0x0c,0x0b,0x0b,0x0c,0x19,0x12,0x13,0x0f,0x14,0x1d,0x1a,0x1f,0x1e,0x1d,0x1a,0x1c,0x1c,0x20,
        0x24,0x2e,0x27,0x20,0x22,0x2c,0x23,0x1c,0x1c,0x28,0x37,0x29,0x2c,0x30,0x31,0x34,0x34,0x34,0x1f,0x27,
        0x39,0x3d,0x38,0x32,0x3c,0x2e,0x33,0x34,0x32,0xff,0xdb,0x00,0x43,0x01,0x09,0x09,0x09,0x0c,0x0b,0x0c,
        0x18,0x0d,0x0d,0x18,0x32,0x21,0x1c,0x21,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,
        0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,
        0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0xff,0xc0,
        0x00,0x11,0x08,0x00,0x0c,0x00,0x18,0x03,0x01,0x22,0x00,0x02,0x11,0x01,0x03,0x11,0x01,0xff,0xc4,0x00,
        0x1f,0x00,0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
        0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0xff,0xc4,0x00,0xb5,0x10,0x00,0x02,0x01,0x03,0x03,
        0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7d,0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,
        0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,
        0x52,0xd1,0xf0,0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,0x29,
        0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,
        0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,
        0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,
        0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,
        0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
        0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xff,0xc4,0x00,0x1f,0x01,0x00,0x03,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,
        0x06,0x07,0x08,0x09,0x0a,0x0b,0xff,0xc4,0x00,0xb5,0x11,0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,
        0x05,0x04,0x04,0x00,0x01,0x02,0x77,0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,
        0x07,0x61,0x71,0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,0x15,
        0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,0x27,0x28,0x29,0x2a,0x35,
        0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,
        0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,
        0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,
        0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,
        0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,
        0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xff,0xda,0x00,0x0c,0x03,0x01,0x00,0x02,0x11,0x03,0x11,
        0x00,0x3f,0x00,0xf3,0xea,0x28,0xa2,0xbe,0x84,0xf2,0xc2,0x8a,0x28,0xa0,0x0f,0xff,0xd9
    });
    for(const char* directory:{"filter/Alpha","filter/zeta","bounded/Alpha","bounded/zeta"})fs::create_directories(fs::u8path(Path(directory)));
    for(const char* name:{"雪 夜.png","B.JPG","a.JpEg","ignored.txt",".hidden.png","hidden.png"})WriteFile(fs::u8path(Path("filter"))/fs::u8path(name),{'f'});
    for(int index=0;index<600;++index){char name[32];std::snprintf(name,sizeof(name),"image-%04d.png",index);WriteFile(fs::u8path(Path("bounded"))/name,{'f'});}
}
struct Fixtures {
    Fixtures(){
        const auto nonce=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        fixtureRoot=fs::temp_directory_path()/("spatial-backdrop-images-"+std::to_string(nonce));
        Check(fs::create_directories(fixtureRoot),"create unique temporary fixture directory");MakeFixtures();
    }
    ~Fixtures(){std::error_code error;fs::remove_all(fixtureRoot,error);}
};
static bool Load(const char* name){const auto path=Path(name);return spatial_backdrop_image::Load(path.c_str());}
int main(){
    Fixtures fixtures;
    using spatial_backdrop_image::Get;
    using spatial_backdrop_image::Release;
    Check(Load("雪 夜.png"),"UTF8 PNG loads");
    Check(Get().width==32&&Get().height==16&&Get().sourceWidth==32&&Get().sourceHeight==16,"PNG dimensions");
    Check(firstPixel[0]==42&&firstPixel[1]==75&&firstPixel[2]==120&&firstPixel[3]==170,"PNG RGBA preserved");
    Check(bound==601&&alignment==8&&alive.size()==1,"successful load preserves GL binding and unpack alignment");
    const auto first=Get().textureName;
    Check(spatial_backdrop_image::Load(Get().loadedPath),"reload aliased current path");
    Check(Get().loadedPath==Path("雪 夜.png")&&Deleted(first),"reload preserves path and deletes replaced unbound texture");
    std::puts("PASS UTF8 PNG, RGBA, dimensions, path-alias reload and successful GL state restoration");

    auto previous=Get();const auto priorAlive=alive.size();
    for(const char* name:{"invalid.jpg","broken.png","oversize.jpg","wide.png","many_pixels.png","missing.png"}){
        Check(!Load(name),"reject bad/oversize/missing fixture");
        Check(Get().textureName==previous.textureName&&Get().width==previous.width&&std::strcmp(Get().loadedPath,previous.loadedPath)==0,"failed decode preserves previous image");
        Check(Get().error[0]&&alive.size()==priorAlive,"failure has a message and no GPU texture leak");
    }
    Check(!spatial_backdrop_image::Load(nullptr)&&!spatial_backdrop_image::Load(""),"reject empty paths");
    const std::string tooLong(768,'a');Check(!spatial_backdrop_image::Load(tooLong.c_str()),"reject long path");
    std::puts("PASS missing, corrupt, unsupported, oversize, pixel-budget and empty inputs preserve the current background");

    failUpload=true;
    Check(!Load("small.jpg"),"simulate GL upload failure");
    failUpload=false;
    Check(Get().textureName==previous.textureName&&alive.size()==priorAlive&&bound==601&&alignment==8,"upload failure restores GL state and keeps old image");
    failGenerate=true;Check(!Load("small.jpg"),"simulate GL texture allocation failure");failGenerate=false;
    Check(Get().textureName==previous.textureName&&alive.size()==priorAlive&&bound==601&&alignment==8,"allocation failure keeps state");
    std::puts("PASS failed GPU allocation/upload cleans temporary textures and keeps the previous background and caller GL state");

    Check(Load("small.jpg")&&Get().width==24&&Get().height==12&&firstPixel[3]==255,"JPEG loads opaque RGBA");
    bound=Get().textureName;const auto oldBound=bound;
    Check(Load("large.png"),"large PNG downsample");
    Check(Get().width==2048&&Get().height==1024&&uploadedWidth==2048&&uploadedHeight==1024&&Get().sourceWidth==3000&&Get().sourceHeight==1500,"downsample max dimension and preserve aspect ratio");
    Check(firstPixel[0]==73&&firstPixel[1]==111&&firstPixel[2]==159&&firstPixel[3]==210,"downsample RGBA preserved");
    Check(bound==oldBound&&alive.count(oldBound)&&Get().retiredTexture==oldBound&&alive.size()==2,"bound old texture retired until safe release");
    bound=999;Check(Load("small.jpg")&&Deleted(oldBound)&&Get().retiredTexture==0&&alive.size()==1,"retired texture reclaimed once unbound");
    Release();Check(Get().texture==nullptr&&alive.empty()&&Get().loadedPath[0]==0,"release clears all texture and image state");
    std::puts("PASS JPEG, bounded bilinear downsampling, bound-texture retirement and deterministic release");

    using spatial_backdrop_image::GetPreset;
    using spatial_backdrop_image::ReleaseCustom;
    Check(Load("雪 夜.png"),"custom background before embedded presets");
    const auto custom=Get();
    bound=custom.textureName;const auto customBinding=bound;
    const auto beforePresetUploads=uploads;
    const auto& star=GetPreset(0);
    Check(star.texture&&star.sourceWidth==1920&&star.sourceHeight==1080&&star.width==1920&&star.height==1080,"first original embedded JPEG decodes at original dimensions");
    Check(bound==customBinding&&alignment==8,"preset success preserves custom binding and unpack alignment");
    failUpload=true;const auto& failedPreset=GetPreset(1);failUpload=false;
    Check(!failedPreset.texture&&failedPreset.error[0]&&alive.size()==2,"failed preset does not retain a temporary GPU texture");
    const auto afterFailedUpload=uploads;const auto afterFailedGeneration=nextTexture;
    for(int frame=0;frame<120;++frame)Check(&GetPreset(1)==&failedPreset,"failed preset keeps stable state");
    Check(uploads==afterFailedUpload&&nextTexture==afterFailedGeneration,"failed preset never reattempts decoding/upload per frame");
    const auto& butterfly=GetPreset(2);
    Check(butterfly.texture&&butterfly.sourceWidth==1920&&butterfly.sourceHeight==1311&&butterfly.width==1920&&butterfly.height==1311,"third original embedded JPEG decodes at original dimensions");
    Check(star.textureName!=butterfly.textureName&&star.textureName!=custom.textureName&&butterfly.textureName!=custom.textureName,"embedded images and custom image have distinct textures");
    Check(Get().textureName==custom.textureName&&Get().width==custom.width&&std::strcmp(Get().loadedPath,custom.loadedPath)==0&&Get().error[0]==0,"preset successes and failure preserve custom cache and error state");
    Check(uploads==beforePresetUploads+3&&alive.size()==3&&bound==customBinding&&alignment==8,"three lazy preset attempts preserve caller GL state without leaking");
    const auto cachedUploads=uploads;const auto cachedGeneration=nextTexture;
    for(int frame=0;frame<120;++frame){
        Check(&GetPreset(0)==&star&&&GetPreset(2)==&butterfly,"preset successes keep stable cached state");
        Check(!GetPreset(-1).texture&&!GetPreset(3).texture,"invalid preset indices return empty state");
    }
    Check(uploads==cachedUploads&&nextTexture==cachedGeneration&&alive.size()==3,"repeated presets and invalid indices do not decode or allocate");
    ReleaseCustom();
    Check(!Get().texture&&alive.size()==2&&alive.count(star.textureName)&&alive.count(butterfly.textureName),"custom release keeps all built-in caches alive");
    Check(&GetPreset(0)==&star&&&GetPreset(1)==&failedPreset&&&GetPreset(2)==&butterfly&&uploads==cachedUploads,"custom release does not reset successful or failed preset attempts");
    Release();
    Check(alive.empty()&&!star.texture&&!failedPreset.texture&&!butterfly.texture&&failedPreset.error[0]==0,"full release resets every preset state and frees all textures");
    std::puts("PASS independent embedded/custom caches, lazy success/failure reuse, invalid indices and custom-only/full release");

    const int expectedWidth[]={1920,1280,1920},expectedHeight[]={1080,720,1311};
    const std::size_t expectedSize[]={190314,212520,412654};
    const char* expectedName[]={"星眸","泡影","蓝蝶"};
    const auto reloadUploads=uploads;bound=777;
    for(int index=0;index<spatial_backdrop_presets::Count;++index){
        const auto encoded=spatial_backdrop_presets::Get(index);
        Check(encoded.bytes&&encoded.size==expectedSize[index]&&encoded.bytes[0]==0xff&&encoded.bytes[1]==0xd8,"embedded original JPEG byte lengths and signatures");
        Check(std::strcmp(spatial_backdrop_presets::Name(index),expectedName[index])==0,"preset names match original order");
        const auto& preset=GetPreset(index);
        Check(preset.texture&&preset.error[0]==0&&preset.width==expectedWidth[index]&&preset.height==expectedHeight[index],"all original presets decode after full release, including prior failure");
        Check(preset.sourceWidth==expectedWidth[index]&&preset.sourceHeight==expectedHeight[index]&&std::strcmp(preset.loadedPath,spatial_backdrop_presets::Key(index))==0,"preset metadata and source dimensions preserved");
    }
    Check(uploads==reloadUploads+3&&alive.size()==3&&bound==777&&alignment==8,"one fresh upload per preset after release restores caller GL state");
    Check(!spatial_backdrop_presets::Get(-1).bytes&&!spatial_backdrop_presets::Get(3).bytes,"invalid byte access is empty");
    Release();Release();Check(alive.empty()&&!Get().texture,"full release is idempotent");
    std::puts("PASS all three original JPEGs, dimensions, names, embedded resource sizes and retry after context release");

    using namespace spatial_backdrop_files;
    const auto filterPath=Path("filter");
    const auto hidden=fs::u8path(filterPath)/L"hidden.png";
    Check(SetFileAttributesW(hidden.c_str(),FILE_ATTRIBUTE_HIDDEN)!=0,"mark Windows hidden fixture");
    auto listing=List(filterPath);
    Check(listing.error.empty()&&listing.entries.size()==5,"filter image extensions, hidden files and text");
    Check(listing.entries[0].directory&&listing.entries[0].name=="Alpha"&&listing.entries[1].directory&&listing.entries[1].name=="zeta","directories sorted before files");
    Check(listing.entries[2].name=="a.JpEg"&&listing.entries[3].name=="B.JPG"&&listing.entries[4].name=="雪 夜.png","case insensitive image extensions and UTF8 names");
    Check(Parent(filterPath+"/")==Parent(filterPath)&&fs::u8path(Parent(filterPath))==fs::u8path(filterPath).parent_path(),"parent handles trailing slash");
    Check(fs::u8path(Parent(Path("中文目录/子目录")))==fs::u8path(Path("中文目录")),"parent handles Chinese paths");
    Check(Parent("D:/")=="D:\\","root parent stays at root");
    Check(!List(Path("no_such_directory")).error.empty(),"missing directory returns actionable error");
    std::puts("PASS UTF8 file browsing, directory ordering, hidden filtering, mixed-case extensions and parent/root navigation");

    auto bounded=List(Path("bounded"));
    Check(bounded.entries.size()==512&&!bounded.error.empty(),"bounded listing reports truncation");
    Check(bounded.entries.front().name=="Alpha"&&bounded.entries[1].name=="zeta"&&bounded.entries.back().name=="image-0509.png","heap retains the first 512 sorted items independent of iteration order");
    Check(std::is_sorted(bounded.entries.begin(),bounded.entries.end(),Before),"bounded list sorted");
    std::puts("PASS directory browsing retains at most 512 predictable sorted entries");
    return 0;
}
