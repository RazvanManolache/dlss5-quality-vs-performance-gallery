// Counterbalanced, offscreen experiment. Full network stays fixed at eight blocks.
void detailCheck(const fs::path& out){
 ensure(!IsWindowVisible(window),"Detail experiment must stay hidden");
 struct Condition{const char* name;unsigned quality,flags;bool neural,scoped;};
 const Condition conditions[]={
  {"beauty",2,0,false,true},{"light_protected",0,0,true,true},{"light_full",0,0,true,false},
  {"fabric_full",0,7,true,false},{"fabric_protected",0,7,true,true},
  {"all_maps_full",0,55,true,false},{"all_detail_full",0,119,true,false},
  {"all_detail_protected",0,119,true,true},{"all_detail_shadows_full",0,127,true,false},
  {"all_detail_shadows_protected",0,127,true,true}
 };
 const unsigned count=unsigned(std::size(conditions));
 auto pose=[&](float t){yaw=.38f+.12f*sinf(t*XM_2PI);cameraRadius=4.8f;sceneTime=.9f+2*t;lightPhase=.63f+.15f*t;};
 auto select=[&](const Condition& c){shading=c.quality;detailFlags=c.flags;enabled=c.neural;materialScope=c.scoped;displayMode=enabled?1:0;reset=true;};
 report["detail_protocol"]={{"warmup_frames",16},{"measured_frames",64},{"repeats",3},{"width",W},{"height",H},{"presentation",false},{"desktop_capture",false},{"windows_hidden",true},{"network_blocks",8},{"refinement_passes",1},{"budget_max_extra_gpu_ms",.5},{"camera","Same two-second moving camera, visitors and lighting in each condition"}};
 select(conditions[3]);pose(0);for(unsigned i=0;i<48;++i){render(cases[0],{},false,false);report["frames"].back()["phase"]="device_warmup";}
 for(unsigned repeat=0;repeat<3;++repeat)for(unsigned k=0;k<count;++k){
  unsigned index=repeat==1?count-1-k:repeat==2?(k+4)%count:k;const auto& c=conditions[index];select(c);
  for(unsigned i=0;i<80;++i){pose(i<16?0:float(i-16)/63);auto start=std::chrono::steady_clock::now();render(cases[0],{},false,false);auto& row=report["frames"].back();row["condition"]=c.name;row["phase"]="timing";row["repeat"]=repeat;row["measured"]=i>=16;row["sequence_frame"]=i<16?-1:int(i-16);row["frame_wall_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
  ensure(!IsWindowVisible(window),"Detail experiment revealed a window");
  std::cout<<"Timed "<<repeat<<" "<<c.name<<std::endl;
 }
 for(const auto& c:conditions){select(c);pose(0);for(unsigned i=0;i<17;++i){render(cases[0],i==16?out/"stills"/c.name:fs::path{},false,false);auto& row=report["frames"].back();row["condition"]=c.name;row["phase"]="still";}}
 report["detail_protocol"]["passed"]=true;
}
