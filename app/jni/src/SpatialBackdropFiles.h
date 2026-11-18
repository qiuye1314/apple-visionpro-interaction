#pragma once
// A bounded local image browser. Paths and filenames at the public boundary
// are UTF-8; filesystem failures are returned in Listing::error.
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace spatial_backdrop_files {
struct Entry {std::string name,path;bool directory=false;};
struct Listing {std::vector<Entry> entries;std::string directory,error;};
inline std::string DefaultDirectory(){
#if defined(__ANDROID__)
    return "/sdcard/Pictures";
#else
    try{
        std::error_code error;const auto path=std::filesystem::current_path(error);
        return error?std::string("."):path.u8string();
    }catch(...){return ".";}
#endif
}
inline std::string Parent(const std::string& utf8Path){
    try{
        std::error_code error;
        auto path=std::filesystem::absolute(std::filesystem::u8path(utf8Path.empty()?DefaultDirectory():utf8Path),error);
        if(error)return DefaultDirectory();
        path=path.lexically_normal();
        if(path.filename().empty()&&path!=path.root_path())path=path.parent_path();
        auto parent=path.parent_path();
        return (parent.empty()?path:parent).u8string();
    }catch(...){return DefaultDirectory();}
}
inline std::string Fold(std::string value){
    for(char& c:value)if(static_cast<unsigned char>(c)<128)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
inline bool Before(const Entry& left,const Entry& right){
    if(left.directory!=right.directory)return left.directory;
    const auto a=Fold(left.name),b=Fold(right.name);
    return a==b?left.name<right.name:a<b;
}
inline bool Hidden(const std::filesystem::path& path,const std::string& name){
    if(name.empty()||name.front()=='.')return true;
#if defined(_WIN32)
    const DWORD attributes=GetFileAttributesW(path.c_str());
    if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)))return true;
#endif
    return false;
}
inline Listing List(const std::string& utf8Path){
    Listing result;
    try{
        std::error_code error;
        auto directory=std::filesystem::absolute(std::filesystem::u8path(utf8Path.empty()?DefaultDirectory():utf8Path),error);
        if(error){result.error="无法识别这个目录";return result;}
        directory=directory.lexically_normal();result.directory=directory.u8string();
        if(!std::filesystem::is_directory(directory,error)||error){
            result.error="目录不存在或无法访问，请返回上一级";return result;
        }
        std::filesystem::directory_iterator iterator(directory,error),end;
        if(error){result.error="无法读取目录，请选择可访问的图片目录";return result;}
        bool truncated=false;
        for(;iterator!=end;iterator.increment(error)){
            if(error)break;
            const auto path=iterator->path();const auto name=path.filename().u8string();
            if(Hidden(path,name))continue;
            std::error_code statusError;const bool isDirectory=iterator->is_directory(statusError);
            if(statusError)continue;
            if(!isDirectory){
                if(!iterator->is_regular_file(statusError)||statusError)continue;
                const auto extension=Fold(path.extension().u8string());
                if(extension!=".png"&&extension!=".jpg"&&extension!=".jpeg")continue;
            }
            Entry entry{name,path.u8string(),isDirectory};
            if(result.entries.size()<512){
                result.entries.push_back(std::move(entry));
                std::push_heap(result.entries.begin(),result.entries.end(),Before);
            }else{
                truncated=true;
                if(Before(entry,result.entries.front())){
                    std::pop_heap(result.entries.begin(),result.entries.end(),Before);
                    result.entries.back()=std::move(entry);
                    std::push_heap(result.entries.begin(),result.entries.end(),Before);
                }
            }
        }
        std::sort(result.entries.begin(),result.entries.end(),Before);
        if(error)result.error="部分项目无法读取，已显示可访问的图片";
        else if(truncated)result.error="项目较多，已显示前 512 项";
    }catch(...){result.error="无法读取这个目录，请选择其他图片目录";}
    return result;
}
} // namespace spatial_backdrop_files
