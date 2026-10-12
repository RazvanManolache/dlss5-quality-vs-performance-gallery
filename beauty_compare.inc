void beautyCompare(const fs::path& out){
 struct Condition{const char* name;unsigned quality;bool neural,scoped;};
 const Condition conditions[]={{"beauty",2,false,true},{"beauty_neural",2,true,true},{"beauty_all",2,true,false},{"middle",1,false,true},{"middle_neural",1,true,true},{"middle_all",1,true,false},{"light",0,false,true},{"light_neural",0,true,true},{"light_all",0,true,false}};
 auto pose=[&](float t){yaw=.38f+.25f*sinf(t*XM_2PI);cameraRadius=4.8f-.3f*sinf(t*XM_PI);sceneTime=.9f+6*t;lightPhase=sceneTime*.7f;};
 auto select=[&](const Condition& c){shading=c.quality;enabled=c.neural;materialScope=c.scoped;displayMode=enabled?1:0;reset=true;};
 select(conditions[1]);pose(0);for(unsigned i=0;i<64;++i){render(cases[0],{},false,false);report["frames"].back()["phase"]="device_warmup";}
 report["beauty_protocol"]={{"warmup_frames",16},{"measured_frames",96},{"repeats",2},{"clip_frames",144},{"clip_fps",24},{"timing_capture",false},{"timing_presentation",false},{"scope","Same 1080p gallery, camera, visitors, lighting art direction and shared finish. True rendering samples, bounce depth, reflections, texture filtering and distant geometry vary."}};
 for(unsigned repeat=0;repeat<2;++repeat)for(unsigned k=0;k<9;++k){const auto& condition=conditions[repeat?8-k:k];select(condition);for(unsigned i=0;i<112;++i){pose(i<16?0:float(i-16)/95);render(cases[0],{},false,false);auto& row=report["frames"].back();row["condition"]=condition.name;row["phase"]="timing";row["repeat"]=repeat;row["measured"]=i>=16;row["sequence_frame"]=i<16?-1:int(i-16);}}
 for(const auto& condition:conditions){select(condition);displayCaptureOnly=true;pose(0);for(unsigned i=0;i<16;++i){render(cases[0],{},false,false);report["frames"].back()["phase"]="clip_warmup";}
  for(unsigned i=0;i<144;++i){pose(float(i)/143);displayCaptureOnly=!(i==0||i==47||i==95||i==143);render(cases[0],out/"clips"/condition.name/("frame_"+std::to_string(i)),false,true);auto& row=report["frames"].back();row["condition"]=condition.name;row["phase"]="clip";row["sequence_frame"]=i;}
 }
 displayCaptureOnly=false;materialScope=true;
}
