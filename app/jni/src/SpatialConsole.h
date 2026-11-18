#pragma once
// Startup presentation only. Number mappings and connection behavior belong to
// their original callers; an exhausted input stream never selects an option.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cctype>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace spatial_console {
struct Terminal {
    FILE* input;
    FILE* output;
    bool color;

    static bool SupportsColor(FILE* output) {
        if(std::getenv("NO_COLOR"))return false;
        const char* term=std::getenv("TERM");
        if(term&&std::strcmp(term,"dumb")==0)return false;
#if defined(_WIN32)
        return _isatty(_fileno(output))!=0;
#else
        return isatty(fileno(output))!=0;
#endif
    }
    explicit Terminal(FILE* input=stdin,FILE* output=stdout,int useColor=-1)
        :input(input),output(output),color(useColor<0?SupportsColor(output):useColor!=0){}
    const char* Accent() const{return color?"\033[1;36m":"";}
    const char* Bright() const{return color?"\033[1;37m":"";}
    const char* Muted() const{return color?"\033[2m":"";}
    const char* Reset() const{return color?"\033[0m":"";}
};

inline void Rule(const Terminal& t){
    std::fprintf(t.output,"%s  --------------------------------------------------------%s\n",t.Muted(),t.Reset());
}
inline void Brand(const Terminal& t){
    std::fprintf(t.output,"\n%s  LSRAEL%s  /  DRAW 1.39\n",t.Accent(),t.Reset());
    std::fprintf(t.output,"%s  简洁绘制，专注画面。%s\n",t.Muted(),t.Reset());
}
inline void Heading(const Terminal& t,int step,const char* title,const char* description){
    Brand(t);Rule(t);
    std::fprintf(t.output,"\n%s  %02d / 02%s  %s%s%s\n",t.Accent(),step,t.Reset(),t.Bright(),title,t.Reset());
    std::fprintf(t.output,"%s  %s%s\n\n",t.Muted(),description,t.Reset());
}
inline void Card(const Terminal& t,int number,const char* title,const char* tag,const char* description){
    std::fprintf(t.output,"%s  [%02d]%s  %s%s%s",t.Accent(),number,t.Reset(),t.Bright(),title,t.Reset());
    if(tag&&*tag)std::fprintf(t.output,"  %s%s%s",t.Accent(),tag,t.Reset());
    std::fprintf(t.output,"\n%s        %s%s\n\n",t.Muted(),description,t.Reset());
}
inline void Message(const Terminal& t,const char* title,const char* detail){
    std::fprintf(t.output,"\n%s  %s%s  %s\n",t.Accent(),title,t.Reset(),detail);
    std::fflush(t.output);
}

// Read an entire choice line. Invalid or long lines are consumed completely,
// preventing their tail from becoming the next startup question's answer.
inline int ReadChoice(const Terminal& t,int minimum,int maximum){
    for(;;){
        std::fprintf(t.output,"%s  选择 %d-%d%s  > ",t.Accent(),minimum,maximum,t.Reset());
        std::fflush(t.output);
        char line[128]{};
        if(!std::fgets(line,sizeof(line),t.input)){
            Message(t,"已取消","输入已结束，启动流程关闭。");
            return 0;
        }
        bool complete=std::strchr(line,'\n')!=nullptr||std::feof(t.input);
        if(!complete){
            int c=std::fgetc(t.input);
            if(c!=EOF){
                while(c!='\n'&&c!=EOF)c=std::fgetc(t.input);
                std::fprintf(t.output,"%s  输入过长，请仅输入选项编号。%s\n\n",t.Muted(),t.Reset());
                continue;
            }
        }
        char* start=line;
        while(*start&&std::isspace(static_cast<unsigned char>(*start)))++start;
        char* end=start+std::strlen(start);
        while(end>start&&std::isspace(static_cast<unsigned char>(end[-1])))--end;
        *end=0;
        bool digits=start<end;
        for(const char* p=start;p<end;++p)if(*p<'0'||*p>'9'){digits=false;break;}
        char* parsedEnd=nullptr;errno=0;
        long value=digits?std::strtol(start,&parsedEnd,10):0;
        if(digits&&errno==0&&parsedEnd==end&&value>=minimum&&value<=maximum)
            return static_cast<int>(value);
        std::fprintf(t.output,"%s  请输入 %d-%d 之间的编号，按回车继续。%s\n\n",t.Muted(),minimum,maximum,t.Reset());
    }
}

inline const char* DriverName(int choice){
    static const char* const names[]={"未选择","TWT 驱动","RT 驱动"};
    return names[choice>=1&&choice<=2?choice:0];
}
inline const char* DisplayName(int choice){
    return choice==1?"录屏保护":choice==2?"普通显示":"未选择";
}
inline int ChooseDriver(const Terminal& t){
    Heading(t,1,"连接已安装驱动","选择与你已刷入驱动对应的连接方案。");
    Card(t,1,"TWT 驱动","","使用已安装的 TWT 读取通道。");
    Card(t,2,"RT 驱动","","使用已安装的 RT 读取通道。");
    Card(t,0,"退出","","关闭本次启动。");
    Rule(t);
    int choice=ReadChoice(t,0,2);
    if(choice)Message(t,"已选择",DriverName(choice));
    return choice;
}
inline int ChooseDriver(){return ChooseDriver(Terminal());}

inline int ChooseDisplayMode(unsigned width,unsigned height,const Terminal& t){
    Heading(t,2,"选择显示模式","确定悬浮界面的显示方式，随后进入你的空间。");
    std::fprintf(t.output,"%s  当前画布%s  %u x %u\n\n",t.Muted(),t.Reset(),width,height);
    Card(t,1,"录屏保护","","使用原有录屏保护窗口。");
    Card(t,2,"普通显示","","使用原有普通显示窗口。");
    Rule(t);
    int choice=ReadChoice(t,1,2);
    if(choice)Message(t,"已选择",DisplayName(choice));
    return choice;
}
inline int ChooseDisplayMode(unsigned width,unsigned height){
    return ChooseDisplayMode(width,height,Terminal());
}
inline void SessionIntro(){
    const Terminal t;
    std::fprintf(t.output,"\n%s  驱动连接完成%s\n%s  下一步配置显示方式。%s\n",t.Accent(),t.Reset(),t.Muted(),t.Reset());
    std::fflush(t.output);
}
inline void BeginGraphics(unsigned width,unsigned height,bool protectedMode){
    const Terminal t;Rule(t);
    std::fprintf(t.output,"\n%s  正在准备画面%s\n",t.Bright(),t.Reset());
    std::fprintf(t.output,"%s  模式%s  %s  %s|%s  %u x %u\n\n",t.Muted(),t.Reset(),
        DisplayName(protectedMode?1:2),t.Muted(),t.Reset(),width,height);
    std::fflush(t.output);
}
inline void GraphicsStep(const char* label,bool success=true){
    const Terminal t;
    std::fprintf(t.output,"%s  %s%s  %s\n",t.Accent(),success?"+":"!",t.Reset(),label);
    std::fflush(t.output);
}
inline void Ready(){
    const Terminal t;
    Message(t,"空间已就绪","正在打开悬浮界面。");
    std::fprintf(t.output,"%s  游戏由你手动启动。%s\n\n",t.Muted(),t.Reset());
    std::fflush(t.output);
}
} // namespace spatial_console
