static double gyroTestClock=1;
static float gyroTestPitch=0,gyroTestYaw=0;
static int gyroActiveCalls=0,gyroStoppedCalls=0;
static bool gyroUnavailable=false;
static spatial_motion::Status GyroTestPoll(bool active,spatial_motion::Sample& sample){
    if(!active){++gyroStoppedCalls;return spatial_motion::Status::Idle;}
    ++gyroActiveCalls;if(gyroUnavailable)return spatial_motion::Status::Unavailable;
    gyroTestClock+=1./60.;
    sample={spatial_motion::Multiply(spatial_motion::Axis(1,0,0,gyroTestPitch*.01745329252f),
                                    spatial_motion::Axis(0,1,0,gyroTestYaw*.01745329252f)),gyroTestClock,0};
    return spatial_motion::Status::Ready;
}
static void GyroTests(){
    auto& s=spatial::Get();std::remove(s.path);s.reduceMotion=true;Frame();
    while(spatial::IntroActive())Frame();Settle();
    spatial_motion::Install(GyroTestPoll);const auto settings=绘制.按钮;
    const int commands=commandCalls,exits=exitCalls;
    assert(!s.gyroEnabled);s.threeD=true;s.exploded=false;spatial::Pose(7,-4,2);Settle();
    gyroTestPitch=20;gyroTestYaw=10;Settle();assert(gyroActiveCalls==0);
    assert(std::fabs(s.pitch-7)<.001f&&std::fabs(s.yaw+4)<.001f);
    std::puts("PASS motion is off by default and physical movement cannot alter ordinary 3D interaction");
    ClickRect(spatial::GyroButtonBounds());Frame();assert(s.gyroEnabled&&s.threeD&&!s.exploded);
    const float basePitch=s.targetPitch,baseYaw=s.targetYaw;gyroTestPitch=30;gyroTestYaw=15;Settle();
    assert(std::fabs(s.gyroOffset.pitch)>3&&std::fabs(s.pitch-basePitch)>3);
    assert(s.targetPitch==basePitch&&s.targetYaw==baseYaw);
    std::puts("PASS the separate gyro button activates calibrated 3D motion without rewriting the manual pose");
    s.messageUntil=0;Capture("draw-only-gyro-3d.ppm");
    ClickAt(913,58);Settle();assert(s.exploded&&s.gyroEnabled&&s.threeD);
    gyroTestPitch=38;gyroTestYaw=-7;Settle();assert(std::fabs(s.gyroOffset.yaw)>2);
    s.messageUntil=0;Capture("draw-only-gyro-exploded.ppm");
    ClickAt(823,58);Settle();assert(!s.threeD&&s.exploded&&s.gyroEnabled);
    gyroTestPitch=34;Settle();assert(std::fabs(s.gyroOffset.pitch)>1);
    std::puts("PASS motion follows both combined and standalone exploded views while their switches remain independent");
    const ImVec2 before=spatial_transform::ProjectLocal(spatial::HomeQuoteBounds().GetCenter());
    const auto quote=spatial::HomeQuotes().Index();ClickRect(spatial::HomeQuoteBounds());
    assert(spatial::HomeQuotes().Index()!=quote);
    const auto pose=s.gyroOffset;const auto center=spatial_transform::ProjectLocal(spatial::HomeQuoteBounds().GetCenter());
    Mouse(center.x,center.y,true);gyroTestPitch=60;gyroTestYaw=30;Settle();
    assert(std::fabs(s.gyroOffset.pitch-pose.pitch)<.001f&&std::fabs(s.gyroOffset.yaw-pose.yaw)<.001f);
    Mouse(center.x+35,center.y,true);Mouse(center.x+35,center.y,false);Settle();
    assert(std::fabs(s.gyroOffset.pitch-pose.pitch)<.001f);
    std::puts("PASS projected controls stay clickable during motion and a held touch freezes the sensor view");
    const int activeBefore=gyroActiveCalls;opened=false;Frame();Settle();assert(gyroActiveCalls==activeBefore);
    opened=true;Frame();Settle();assert(s.gyroEnabled&&std::fabs(s.gyroOffset.pitch)<.001f);
    s.threeD=false;s.exploded=false;Frame();const int paused=gyroActiveCalls;Settle();assert(gyroActiveCalls==paused);
    assert(s.gyroEnabled&&std::fabs(s.pitch)<.001f&&std::fabs(s.yaw)<.001f);
    std::puts("PASS hiding the panel or disabling both spatial modes stops sampling and returning recalibrates");
    s.threeD=true;Frame();gyroTestPitch+=12;Settle();ClickRect(spatial::GyroButtonBounds());Settle();
    assert(!s.gyroEnabled&&std::fabs(s.pitch-s.targetPitch)<.001f&&std::fabs(s.yaw-s.targetYaw)<.001f);
    assert(spatial::SavePreferences());s.gyroEnabled=false;assert(spatial::LoadPreferences());assert(!s.gyroEnabled);
    std::puts("PASS disabling motion restores the original manual view and saved preferences keep motion off at startup");
    gyroUnavailable=true;ClickRect(spatial::GyroButtonBounds());Frame();assert(!s.gyroEnabled);
    assert(std::strstr(s.message,"暂不可用")!=nullptr);gyroUnavailable=false;
    std::puts("PASS unavailable sensors fall back to ordinary interaction with a visible status message");
    for(const auto display:{ImVec2(1600,1000),ImVec2(720,1280)}){
        width=(int)display.x;height=(int)display.y;Frame();spatial::SetGyroEnabled(true);s.threeD=true;s.exploded=true;
        spatial::Pose(48,-36,12);Settle();gyroTestPitch+=70;gyroTestYaw-=70;Settle();
        const auto bounds=spatial::WindowBounds();
        assert(bounds.Min.x>=7.5f&&bounds.Min.y>=7.5f&&bounds.Max.x<=width-7.5f&&bounds.Max.y<=height-7.5f);
        ClickRect(spatial::GyroButtonBounds());Settle();assert(!s.gyroEnabled);
    }
    width=1600;height=1000;Frame();
    std::puts("PASS sensor tilt keeps the visible exploded shell and gyro button within landscape and portrait bounds");
    assert(commandCalls==commands&&exitCalls==exits&&std::memcmp(&settings,&绘制.按钮,sizeof(settings))==0);
    spatial_motion::Install(nullptr);s.threeD=false;s.exploded=false;spatial::ResetPose();Settle();s.messageUntil=0;
    Capture("draw-only-gyro-off.ppm");std::remove(s.path);
    std::puts("PASS the gyro UI changes no drawing settings, application commands or driver state");
}
