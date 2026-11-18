#include "DrawOnlyVisuals.h"
#include "DrawOnlyProjection.h"
#include <cassert>
#include <limits>
#include <cstdio>

static void Pass(const char* message){std::printf("PASS %s\n",message);}
static bool ContainsColor(const ImDrawList* list,int first,ImU32 color){
    for(int i=first;i<list->VtxBuffer.Size;++i)if(list->VtxBuffer[i].col==color)return true;
    return false;
}
int main(){
    using namespace draw_only_visuals;
    ImGui::CreateContext();auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;io.DisplaySize={1600,1000};io.DeltaTime=1.f/60.f;
    io.Fonts->AddFontDefault();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    ImGui::NewFrame();auto* list=ImGui::GetForegroundDrawList();
    const float nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
    assert(!Finite(nan)&&!Finite(infinity)&&Finite(0));
    Pass("finite checks remain effective under fast-math");
    assert(BoxValid({100,100,180,340},io.DisplaySize));
    assert(!BoxValid({nan,100,180,340},io.DisplaySize));
    assert(!BoxValid({100,100,99,340},io.DisplaySize));
    assert(!BoxValid({-1000,100,-900,340},io.DisplaySize));
    Pass("invalid, inverted and fully offscreen boxes are rejected");
    ImVec2 a(-100,80),b(1800,80);assert(ClipLine(a,b,io.DisplaySize));
    assert(a.x==0&&a.y==80&&b.x==1600&&b.y==80);
    a={-50,-5};b={-1,80};assert(!ClipLine(a,b,io.DisplaySize));
    a={nan,80};b={80,80};assert(!ClipLine(a,b,io.DisplaySize));
    Pass("rays and limbs clip to the viewport without invalid vertices");
    const int count=list->VtxBuffer.Size;
    Corners(list,{nan,50,100,100},true,false);
    Health(list,{100,100,180,340},90,0,true,false);
    Health(list,{100,100,180,340},nan,100,true,false);
    HeadMarker(list,{0,0},false);
    assert(list->VtxBuffer.Size==count);
    Pass("bad boxes, zero maximum health and invalid joints draw nothing");
    for(ImVec2 view: {ImVec2(1600,1000),ImVec2(720,1280),ImVec2(540,320)}){
        for(float top: {0.f,12.f,100.f,view.y-12.f}){
            const auto layout=LayoutFor({100,top,180,top+120},view);
            assert(layout.healthY>layout.nameY+layout.nameSize);
            assert(layout.distanceY>layout.healthY+3*Scale(view));
            assert(layout.weaponY>layout.distanceY+layout.metaSize);
            assert(layout.weaponY+layout.metaSize<view.y-3);
        }
    }
    Pass("name, health, distance and weapon occupy separate rows at screen edges");
    const auto white=PaletteColors();SetTheme(3);const auto blue=PaletteColors();
    assert(white.visible!=blue.visible&&Color(true,false)!=Color(false,false)&&Color(true,false)!=Color(true,true));
    SetTheme(999);assert(ThemeIndex()==spatial_theme::Count-1);SetTheme(-1);assert(ThemeIndex()==0);
    Pass("global theme and visibility produce distinct restrained colors");
    const int hiddenHead=list->VtxBuffer.Size;
    HeadMarker(list,{220,160},false,false);
    assert(ContainsColor(list,hiddenHead,Color(false,false)));
    assert(!ContainsColor(list,hiddenHead,Color(true,false)));
    const int visibleHead=list->VtxBuffer.Size;
    HeadMarker(list,{250,160},false);
    assert(ContainsColor(list,visibleHead,Color(true,false)));
    assert(!ContainsColor(list,visibleHead,Color(false,false)));
    Pass("head markers preserve actual cover visibility and the compatible default");
    std::array<ImVec2,15> partialPoints{};std::array<bool,15> partialVisible{};
    partialPoints[1]={150,150};partialPoints[2]={150,230};
    partialPoints[3]={130,150};partialPoints[5]={110,190};partialPoints[7]={115,230};
    partialVisible[1]=partialVisible[3]=partialVisible[5]=true;
    const int partialSkeleton=list->VtxBuffer.Size;
    Skeleton(list,{100,100,200,400},partialPoints,partialVisible,false);
    assert(ContainsColor(list,partialSkeleton,Color(true,false)));
    assert(ContainsColor(list,partialSkeleton,Color(false,false)));
    Pass("partial cover produces both visible and occluded limb colors");
    Entity partialEntity;partialEntity.box={300,160,380,400};partialEntity.visible[1]=true;
    const int exposedTorso=list->VtxBuffer.Size;
    EntityOverlay(list,partialEntity);
    assert(ContainsColor(list,exposedTorso,Color(true,false)));
    assert(!ContainsColor(list,exposedTorso,Color(false,false)));
    partialEntity.visible.fill(false);
    const int hiddenTorso=list->VtxBuffer.Size;
    EntityOverlay(list,partialEntity);
    assert(ContainsColor(list,hiddenTorso,Color(false,false)));
    assert(!ContainsColor(list,hiddenTorso,Color(true,false)));
    Pass("whole-character markers follow exposed torso while a hidden head stays independent");
    std::array<ImVec2,15> points{};std::array<bool,15> visible{};visible.fill(true);
    points[1]={150,150};points[2]={150,270};
    const int beforeSkeleton=list->VtxBuffer.Size;
    Skeleton(list,{100,100,200,400},points,visible,false);
    assert(list->VtxBuffer.Size>beforeSkeleton);
    points[1]={nan,150};points[2]={150,infinity};
    const int invalidSkeleton=list->VtxBuffer.Size;
    Skeleton(list,{100,100,200,400},points,visible,false);
    assert(list->VtxBuffer.Size==invalidSkeleton);
    Pass("nearby 120-pixel limbs survive while corrupt joints are isolated");
    const Box root{100,100,200,400};
    const std::array<ImVec2,15> standing={{{150,112},{150,155},{150,250},{127,160},{173,160},
        {112,205},{188,205},{118,245},{182,245},{135,260},{165,260},{133,320},{167,320},{130,390},{170,390}}};
    auto missing=standing;missing[0]={};
    const auto headless=BoxFromJoints(missing,root,io.DisplaySize);
    assert(BoxValid(headless,io.DisplaySize)&&headless.top<=root.top&&headless.Width()<200);
    missing[13]={};missing[14]={};
    const auto incomplete=BoxFromJoints(missing,root,io.DisplaySize);
    assert(BoxValid(incomplete,io.DisplaySize)&&incomplete.bottom>=root.bottom);
    const int incompleteMarkers=list->VtxBuffer.Size;
    Corners(list,incomplete,true,false);Name(list,incomplete,"Partial",3,true,false);
    assert(list->VtxBuffer.Size>incompleteMarkers);
    std::array<ImVec2,15> empty{};
    const auto fallback=BoxFromJoints(empty,root,io.DisplaySize);
    assert(fallback.left==root.left&&fallback.top==root.top&&fallback.right==root.right&&fallback.bottom==root.bottom);
    assert(!BoxValid(BoxFromJoints(empty,{},io.DisplaySize),io.DisplaySize));
    Pass("missing head or ankles retain marker bounds and empty joints use the root box");
    auto outlier=standing;outlier[0]={80000,-80000};
    for(Box reference:{root,Box{}}){
        const auto bounded=BoxFromJoints(outlier,reference,io.DisplaySize);
        assert(BoxValid(bounded,io.DisplaySize)&&bounded.Width()<200&&bounded.Height()<400);
        const auto accepted=ReliableJoints(outlier,reference,io.DisplaySize);
        assert(!accepted[0]&&accepted[1]&&accepted[13]&&accepted[14]);
    }
    Pass("one absurd joint cannot stretch the box or suppress reliable body joints");
    outlier=standing;outlier[0]={900,112};
    const auto filtered=FilterJoints(outlier,root,io.DisplaySize);
    assert(!JointValid(filtered[0])&&filtered[1].x==standing[1].x&&filtered[1].y==standing[1].y);
    const int isolatedHead=list->VtxBuffer.Size;
    HeadMarker(list,filtered[0],false,true);
    assert(list->VtxBuffer.Size==isolatedHead);
    HeadMarker(list,FilterJoints(standing,root,io.DisplaySize)[0],false,true);
    assert(list->VtxBuffer.Size>isolatedHead);
    Pass("independent head markers share outlier filtering without hiding a reliable head");
    points={};points[1]={280,260};points[2]={400,260};
    const int proneSkeleton=list->VtxBuffer.Size;
    Skeleton(list,{250,230,570,280},points,visible,false);
    assert(list->VtxBuffer.Size>proneSkeleton);
    const int noBoxSkeleton=list->VtxBuffer.Size;
    Skeleton(list,{nan,0,0,0},points,visible,false);
    assert(list->VtxBuffer.Size>noBoxSkeleton);
    Pass("horizontal prone limbs survive and valid skeletons do not require a valid box");
    const Box overheadRoot{380,480,400,520};
    std::array<ImVec2,15> prone={{{300,500},{340,500},{400,500},{335,490},{335,510},
        {365,480},{365,520},{400,480},{400,520},{415,490},{415,510},{490,490},{490,510},{580,490},{580,510}}};
    const auto proneAccepted=ReliableJoints(prone,overheadRoot,io.DisplaySize);
    for(bool accepted:proneAccepted)assert(accepted);
    const auto proneBounds=BoxFromJoints(prone,overheadRoot,io.DisplaySize);
    assert(BoxValid(proneBounds,io.DisplaySize)&&proneBounds.left<300&&proneBounds.right>580);
    const auto proneFiltered=FilterJoints(prone,overheadRoot,io.DisplaySize);
    assert(proneFiltered[0].x==300&&proneFiltered[13].x==580&&proneFiltered[14].x==580);
    const int overheadSkeleton=list->VtxBuffer.Size;
    Skeleton(list,proneBounds,proneFiltered,visible,false);
    assert(list->VtxBuffer.Size>overheadSkeleton);
    bool drewFeet=false;
    for(int i=overheadSkeleton;i<list->VtxBuffer.Size;++i)if(list->VtxBuffer[i].pos.x>575)drewFeet=true;
    assert(drewFeet);
    prone[0]={1500,500};
    const auto proneOutlier=FilterJoints(prone,overheadRoot,io.DisplaySize);
    assert(!JointValid(proneOutlier[0])&&proneOutlier[13].x==580&&proneOutlier[14].x==580);
    Pass("overhead prone bodies use robust joint scale despite a short root box and still reject outliers");
    points={};points[1]={-40,140};points[2]={30,200};
    const int edgeSkeleton=list->VtxBuffer.Size;
    Skeleton(list,{},points,visible,false);
    assert(list->VtxBuffer.Size>edgeSkeleton);
    for(int i=edgeSkeleton;i<list->VtxBuffer.Size;++i)
        assert(list->VtxBuffer[i].pos.x>=-4.f&&list->VtxBuffer[i].pos.x<=io.DisplaySize.x+4.f);
    Pass("partially offscreen joints draw clipped limbs at the viewport edge");
    points={};points[0]={900,100};points[1]={100,100};
    const int longSkeleton=list->VtxBuffer.Size;
    Skeleton(list,{},points,visible,false);
    assert(list->VtxBuffer.Size==longSkeleton);
    Skeleton(list,{80,80,160,220},points,visible,false);
    assert(list->VtxBuffer.Size==longSkeleton);
    Pass("excessively long corrupt connections and their head circles are rejected");
    struct ProjectionWorld { float X,Y,Z; };
    struct ProjectionBounds { float X,Y,Z,W; };
    for(int orientation=0;orientation<2;++orientation){
        float matrix[16]{};ProjectionWorld world{};ProjectionBounds projected{};
        if(orientation==0){matrix[0]=matrix[5]=1;matrix[11]=-1;matrix[15]=1000;}
        else{matrix[0]=matrix[9]=-1;matrix[7]=1;world.Y=1000;}
        assert(draw_only_projection::ProjectActorBounds(matrix,world,io.DisplaySize.x*.5f,io.DisplaySize.y*.5f,projected));
        const Box projectedRoot{projected.X,projected.Y-projected.W,projected.X+projected.Z,projected.Y};
        const auto recovered=BoxFromJoints(empty,projectedRoot,io.DisplaySize);
        assert(BoxValid(recovered,io.DisplaySize));
        const int beforeFallbackName=list->VtxBuffer.Size;
        Name(list,recovered,"Unknown health",4,true,false);
        assert(list->VtxBuffer.Size>beforeFallbackName);
    }
    Pass("missing bones retain name markers from actual overhead and rolled-camera volume projections");
    const auto fit=Fit("A long player label with much more than one line",16,100);
    assert(!fit.empty()&&Measure(fit,16).x<=100);
    const auto sanitized=Fit("One\nTwo\tThree",16,500);
    assert(sanitized.find('\n')==std::string::npos&&sanitized.find('\t')==std::string::npos);
    Pass("long names fit their label and control characters cannot add rows");
    Health(list,{100,100,180,340},1000,100,true,false);
    Health(list,{100,100,180,340},0,100,true,false);
    Name(list,{100,100,180,340},"",1,true,false);
    Distance(list,{100,100,180,340},123);
    Weapon(list,{100,100,180,340},"M416",30,40);
    for(const auto& vertex:list->VtxBuffer)assert(Finite(vertex.pos.x)&&Finite(vertex.pos.y));
    Pass("health clamps and fallback names keep all generated vertices finite");
    ImGui::Render();ImGui::DestroyContext();return 0;
}
