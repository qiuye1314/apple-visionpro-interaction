#include "SpatialQuotes.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Check(bool condition,const char* message){
    if(!condition){std::fprintf(stderr,"CHECK FAILED: %s\n",message);std::exit(2);}
}
int main(){
    using spatial_quotes::Rotation;
    Check(spatial_quotes::Count==80,"eighty quote pairs");
    for(std::size_t i=0;i<spatial_quotes::Count;++i){
        const auto& quote=spatial_quotes::Quotes[i];
        Check(quote.title&&*quote.title&&quote.subtitle&&*quote.subtitle,"nonempty copy");
        for(std::size_t j=0;j<i;++j)
            Check(std::strcmp(quote.title,spatial_quotes::Quotes[j].title)!=0,"distinct titles");
    }
    std::puts("PASS all eighty quote pairs are complete and titles are distinct");

    for(std::uint64_t seed:{UINT64_C(0),UINT64_C(1),UINT64_C(2),UINT64_C(0xffffffffffffffff),UINT64_C(0x621479e53648f0ab)}){
        Rotation rotation(seed);std::size_t previous=spatial_quotes::Count;
        for(int cycle=0;cycle<24;++cycle){
            std::array<bool,spatial_quotes::Count> seen{};
            for(std::size_t i=0;i<spatial_quotes::Count;++i){
                const auto current=rotation.Index();
                Check(current<spatial_quotes::Count&&!seen[current],"no repeat inside a shuffle cycle");
                Check(current!=previous,"no immediate repeat across shuffle boundaries");
                seen[current]=true;previous=current;
                rotation.Refresh(cycle*100.0+i);
            }
        }
    }
    std::puts("PASS five seeds each traverse twenty-four complete cycles without repeats or repeated boundaries");

    Rotation first(77),same(77),different(78);bool diverged=false;
    for(int i=0;i<800;++i){
        Check(first.Index()==same.Index(),"deterministic seeded rotation");
        diverged|=first.Index()!=different.Index();
        first.Refresh(i);same.Refresh(i);different.Refresh(i);
    }
    Check(diverged,"different seeds produce different decks");
    std::puts("PASS identical seeds reproduce the sequence and independent session seeds differ");

    Rotation automatic(90);automatic.Update(0,true);auto before=automatic.Index();
    automatic.Update(14.999,true);Check(automatic.Index()==before,"wait the full interval");
    automatic.Update(15,true);Check(automatic.Index()!=before,"refresh at fifteen active seconds");
    Check(automatic.SecondsUntilRefresh()==15,"automatic refresh resets interval");
    Rotation expected=automatic;expected.Refresh(100000);
    automatic.Update(100000,true);Check(automatic.Index()==expected.Index(),"long active gaps advance at most once");
    std::puts("PASS the fifteen-second boundary and long active gaps advance one quote and reset the interval");

    Rotation paused(91);paused.Update(0,true);paused.Update(6,true);before=paused.Index();
    paused.Update(7,false);paused.Update(1000,false);paused.Update(1001,true);
    Check(paused.Index()==before&&paused.SecondsUntilRefresh()==9,"inactive time is excluded");
    paused.Update(1009.999,true);Check(paused.Index()==before,"remaining active time is preserved");
    paused.Update(1010,true);Check(paused.Index()!=before,"resume after the remaining interval");
    Rotation initiallyHidden(92);initiallyHidden.Update(0,false);before=initiallyHidden.Index();
    initiallyHidden.Update(100,false);initiallyHidden.Update(101,true);initiallyHidden.Update(115.999,true);
    Check(initiallyHidden.Index()==before,"initially hidden time does not count");
    initiallyHidden.Update(116,true);Check(initiallyHidden.Index()!=before,"first visible interval lasts fifteen seconds");
    std::puts("PASS hidden time never accumulates and returning resumes only the remaining visible interval");

    Rotation manual(93);manual.Update(0,true);manual.Update(14,true);manual.Refresh(14.5);before=manual.Index();
    manual.Update(15,true);manual.Update(29.499,true);Check(manual.Index()==before,"manual refresh resets the countdown");
    manual.Update(29.5,true);Check(manual.Index()!=before,"manual refresh starts a full new interval");
    Rotation hiddenManual(94);hiddenManual.Update(0,false);hiddenManual.Refresh(100);before=hiddenManual.Index();
    hiddenManual.Update(500,true);hiddenManual.Update(514.999,true);
    Check(hiddenManual.Index()==before,"manual refresh while paused preserves inactive ownership");
    hiddenManual.Update(515,true);Check(hiddenManual.Index()!=before,"paused manual refresh resumes normally");
    std::puts("PASS manual refresh restarts a full fifteen seconds during both active and paused operation");

    Rotation clock(95);clock.Update(100,true);clock.Update(105,true);before=clock.Index();
    const double remaining=clock.SecondsUntilRefresh();
    clock.Update(std::numeric_limits<double>::quiet_NaN(),true);
    clock.Update(std::numeric_limits<double>::infinity(),true);
    clock.Refresh(-std::numeric_limits<double>::infinity());
    Check(clock.Index()==before&&clock.SecondsUntilRefresh()==remaining,"nonfinite clocks leave state intact");
    clock.Update(99,true);Check(clock.Index()==before&&clock.SecondsUntilRefresh()==remaining,"backward clocks add no elapsed time");
    clock.Update(108.999,true);Check(clock.Index()==before,"backward-clock recovery retains remaining interval");
    clock.Update(109,true);Check(clock.Index()!=before,"backward-clock recovery reaches the normal boundary");
    std::puts("PASS invalid and backward clocks preserve valid copy and countdown state");
    return 0;
}
