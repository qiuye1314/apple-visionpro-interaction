#pragma once
// Include after the platform's OpenGL/GLES declarations. stb_image is already
// supplied by the Android application; desktop previews link one implementation.
#include "SpatialVideo.h"
#include "SpatialVideoData.h"
#include "stb_image.h"
#include <cstdint>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace spatial_video_texture {
static_assert(spatial_video_data::Count==spatial_video::FrameCount,"Video frame count mismatch");
static_assert(spatial_video_data::Width==spatial_video::Width&&spatial_video_data::Height==spatial_video::Height,"Video dimensions mismatch");
struct Cache {GLuint texture=0;int frame=-1,width=0,height=0;bool failed=false;};
inline Cache& Get(){static Cache cache;return cache;}
inline ImTextureID Texture(int frame){
    auto& cache=Get();
    if(cache.texture&&cache.frame==frame)return (ImTextureID)(intptr_t)cache.texture;
    const unsigned char* encoded=nullptr;int bytes=0;
    if(!spatial_video_data::Frame(frame,encoded,bytes))return nullptr;
    int width=0,height=0,components=0;
    unsigned char* pixels=stbi_load_from_memory(encoded,bytes,&width,&height,&components,4);
    if(!pixels){cache.failed=true;return nullptr;}
    GLint previousTexture=0,previousAlignment=4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D,&previousTexture);glGetIntegerv(GL_UNPACK_ALIGNMENT,&previousAlignment);
    if(!cache.texture)glGenTextures(1,&cache.texture);
    glBindTexture(GL_TEXTURE_2D,cache.texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    if(cache.width!=width||cache.height!=height){
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        cache.width=width;cache.height=height;
    }else glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT,previousAlignment);glBindTexture(GL_TEXTURE_2D,(GLuint)previousTexture);
    stbi_image_free(pixels);cache.frame=frame;cache.failed=false;
    return (ImTextureID)(intptr_t)cache.texture;
}
inline void Release(){
    auto& cache=Get();if(cache.texture)glDeleteTextures(1,&cache.texture);cache=Cache{};
}
inline void Install(){spatial_video::SetProvider(Texture,Release);}
} // namespace spatial_video_texture
