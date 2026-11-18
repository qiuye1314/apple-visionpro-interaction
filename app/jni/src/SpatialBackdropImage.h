#pragma once
// Include after the platform's GL/GLES declarations. Loading is an explicit
// user action; built-in presets decode once on first use and retain their cache.
#include "imgui.h"
#include "stb_image.h"
#include "SpatialBackdropPresetData.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <vector>
#if defined(_WIN32)
#include <filesystem>
#endif

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace spatial_backdrop_image {
struct State {
    ImTextureID texture=nullptr;
    int width=0,height=0,sourceWidth=0,sourceHeight=0;
    char error[160]{};
    char loadedPath[768]{};
    GLuint textureName=0,retiredTexture=0;
};
inline State& Get(){static State value;return value;}
inline bool Fail(State& cache,const char* message){
    std::snprintf(cache.error,sizeof(cache.error),"%s",message);return false;
}
inline bool Fail(const char* message){
    return Fail(Get(),message);
}
struct FileCloser {void operator()(FILE* file)const{if(file)std::fclose(file);}};
struct PixelCloser {void operator()(unsigned char* pixels)const{if(pixels)stbi_image_free(pixels);}};
inline FILE* OpenUtf8(const char* path){
#if defined(_WIN32)
    const auto native=std::filesystem::u8path(path);
    return _wfopen(native.c_str(),L"rb");
#else
    return std::fopen(path,"rb");
#endif
}
inline bool Supported(const unsigned char* bytes,std::size_t count){
    static const unsigned char png[]={137,80,78,71,13,10,26,10};
    return (count>=sizeof(png)&&std::memcmp(bytes,png,sizeof(png))==0)||
           (count>=3&&bytes[0]==0xff&&bytes[1]==0xd8&&bytes[2]==0xff);
}
inline void Downsample(const unsigned char* source,int width,int height,
                       unsigned char* target,int outWidth,int outHeight){
    const float sx=static_cast<float>(width)/outWidth,sy=static_cast<float>(height)/outHeight;
    for(int y=0;y<outHeight;++y){
        const float fy=(std::max)(0.f,(y+.5f)*sy-.5f);
        const int y0=(std::min)(height-1,static_cast<int>(fy)),y1=(std::min)(height-1,y0+1);
        const float wy=fy-y0;
        for(int x=0;x<outWidth;++x){
            const float fx=(std::max)(0.f,(x+.5f)*sx-.5f);
            const int x0=(std::min)(width-1,static_cast<int>(fx)),x1=(std::min)(width-1,x0+1);
            const float wx=fx-x0;
            const auto* a=source+(static_cast<std::size_t>(y0)*width+x0)*4;
            const auto* b=source+(static_cast<std::size_t>(y0)*width+x1)*4;
            const auto* c=source+(static_cast<std::size_t>(y1)*width+x0)*4;
            const auto* d=source+(static_cast<std::size_t>(y1)*width+x1)*4;
            auto* pixel=target+(static_cast<std::size_t>(y)*outWidth+x)*4;
            for(int channel=0;channel<4;++channel){
                const float upper=a[channel]+(b[channel]-a[channel])*wx;
                const float lower=c[channel]+(d[channel]-c[channel])*wx;
                pixel[channel]=static_cast<unsigned char>(upper+(lower-upper)*wy+.5f);
            }
        }
    }
}
inline bool LoadMemory(State& cache,const unsigned char* encoded,std::size_t length,const char* source){
    if(!encoded||!length)return Fail(cache,"图片数据为空或无法读取");
    constexpr std::size_t maximumFile=32u*1024u*1024u;
    if(length>maximumFile)return Fail(cache,"图片过大，请选择不超过 32 MB 的文件");
    if(!source||std::strlen(source)>=sizeof(cache.loadedPath))return Fail(cache,"图片标识过长或无效");
    char sourceCopy[sizeof(cache.loadedPath)];
    std::snprintf(sourceCopy,sizeof(sourceCopy),"%s",source);
    try{
        if(!Supported(encoded,length))return Fail(cache,"仅支持 PNG 和 JPEG 图片");
        int width=0,height=0,components=0;
        if(!stbi_info_from_memory(encoded,static_cast<int>(length),&width,&height,&components))
            return Fail(cache,"图片格式损坏或暂不支持，请换一张图片");
        if(width<=0||height<=0||width>8192||height>8192||
           static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)>32u*1024u*1024u)
            return Fail(cache,"图片尺寸过大，请使用 8192 像素以内的图片");
        int decodedWidth=0,decodedHeight=0;
        std::unique_ptr<unsigned char,PixelCloser> pixels(stbi_load_from_memory(
            encoded,static_cast<int>(length),&decodedWidth,&decodedHeight,&components,4));
        if(!pixels||decodedWidth!=width||decodedHeight!=height)
            return Fail(cache,"图片解码失败，请换一张 PNG 或 JPEG 图片");
        int targetWidth=width,targetHeight=height;
        const int longest=(std::max)(width,height);
        std::vector<unsigned char> resized;
        const unsigned char* upload=pixels.get();
        if(longest>2048){
            targetWidth=(std::max)(1,width*2048/longest);
            targetHeight=(std::max)(1,height*2048/longest);
            resized.resize(static_cast<std::size_t>(targetWidth)*targetHeight*4);
            Downsample(pixels.get(),width,height,resized.data(),targetWidth,targetHeight);
            upload=resized.data();
        }

        GLint previousTexture=0,previousAlignment=4;
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&previousTexture);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&previousAlignment);
        // A previously bound image remains alive until a later load or release,
        // allowing the caller's exact binding to be restored after replacement.
        if(cache.retiredTexture&&cache.retiredTexture!=static_cast<GLuint>(previousTexture)){
            glDeleteTextures(1,&cache.retiredTexture);cache.retiredTexture=0;
        }
        for(int i=0;i<16&&glGetError()!=GL_NO_ERROR;++i){}
        GLuint fresh=0;glGenTextures(1,&fresh);
        if(!fresh)return Fail(cache,"当前绘制环境无法创建图片，请稍后重试");
        glBindTexture(GL_TEXTURE_2D,fresh);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,targetWidth,targetHeight,0,GL_RGBA,GL_UNSIGNED_BYTE,upload);
        const GLenum uploadError=glGetError();
        glPixelStorei(GL_UNPACK_ALIGNMENT,previousAlignment);
        glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(previousTexture));
        if(uploadError!=GL_NO_ERROR){
            glDeleteTextures(1,&fresh);return Fail(cache,"图片上传失败，已保留原来的背景");
        }
        if(cache.textureName){
            if(cache.textureName==static_cast<GLuint>(previousTexture))cache.retiredTexture=cache.textureName;
            else glDeleteTextures(1,&cache.textureName);
        }
        cache.textureName=fresh;cache.texture=(ImTextureID)(intptr_t)fresh;
        cache.width=targetWidth;cache.height=targetHeight;
        cache.sourceWidth=width;cache.sourceHeight=height;
        std::snprintf(cache.loadedPath,sizeof(cache.loadedPath),"%s",sourceCopy);
        cache.error[0]=0;return true;
    }catch(const std::bad_alloc&){return Fail(cache,"图片占用内存过多，请选择更小的图片");}
    catch(...){return Fail(cache,"图片无法解码，请重新选择图片");}
}
inline bool Load(const char* utf8Path){
    auto& cache=Get();
    if(!utf8Path||!*utf8Path)return Fail("请先选择一张 PNG 或 JPEG 图片");
    const auto pathLength=std::strlen(utf8Path);
    if(pathLength>=sizeof(cache.loadedPath))return Fail("图片路径过长，请换到较短的目录");
    char pathCopy[sizeof(cache.loadedPath)];
    std::memcpy(pathCopy,utf8Path,pathLength+1);utf8Path=pathCopy;
    try{
        std::unique_ptr<FILE,FileCloser> file(OpenUtf8(utf8Path));
        if(!file)return Fail("无法打开图片，请检查文件是否存在或可读取");
        if(std::fseek(file.get(),0,SEEK_END)!=0)return Fail("无法读取图片文件");
        const long length=std::ftell(file.get());
        constexpr long maximumFile=32L*1024L*1024L;
        if(length<=0)return Fail("图片文件为空或无法读取");
        if(length>maximumFile)return Fail("图片过大，请选择不超过 32 MB 的文件");
        if(std::fseek(file.get(),0,SEEK_SET)!=0)return Fail("无法读取图片文件");
        std::vector<unsigned char> encoded(static_cast<std::size_t>(length));
        if(std::fread(encoded.data(),1,encoded.size(),file.get())!=encoded.size()||
           std::fgetc(file.get())!=EOF||std::ferror(file.get()))return Fail("图片读取不完整，请重新选择");
        file.reset();
        return LoadMemory(cache,encoded.data(),encoded.size(),utf8Path);
    }catch(const std::bad_alloc&){return Fail("图片占用内存过多，请选择更小的图片");}
    catch(...){return Fail("图片路径无法读取，请重新选择文件");}
}
struct PresetCache {State state;bool attempted=false;};
inline std::array<PresetCache,spatial_backdrop_presets::Count>& Presets(){
    static std::array<PresetCache,spatial_backdrop_presets::Count> caches{};return caches;
}
inline const State& GetPreset(int index){
    if(index<0||index>=spatial_backdrop_presets::Count){
        static const State invalid=[](){State state;Fail(state,"内置背景不存在");return state;}();
        return invalid;
    }
    auto& preset=Presets()[static_cast<std::size_t>(index)];
    if(!preset.attempted){
        // Cache failures too: a failed GPU context must not cause a decode/upload
        // attempt on every frame. Release() starts a new context/cache lifetime.
        preset.attempted=true;
        const auto encoded=spatial_backdrop_presets::Get(index);
        LoadMemory(preset.state,encoded.bytes,encoded.size,spatial_backdrop_presets::Key(index));
    }
    return preset.state;
}
inline void ReleaseState(State& cache){
    if(cache.textureName)glDeleteTextures(1,&cache.textureName);
    if(cache.retiredTexture&&cache.retiredTexture!=cache.textureName)glDeleteTextures(1,&cache.retiredTexture);
    cache=State{};
}
inline void ReleaseCustom(){ReleaseState(Get());}
inline void Release(){
    ReleaseCustom();
    for(auto& preset:Presets()){ReleaseState(preset.state);preset.attempted=false;}
}
} // namespace spatial_backdrop_image
