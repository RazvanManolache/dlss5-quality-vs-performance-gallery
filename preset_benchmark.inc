// Sustained cost of all 21 quality/Phoenix choices in the current living scene.
void presetBenchmark(const fs::path& out){
 clientSize(1920,1080);ensure(!IsWindowVisible(window),"Preset timing must stay hidden");
 deterministicAnimation=true;animation=false;movingLight=false;livingAnimation=true;
 facialAnimation=true;characterDetail=0;focusPerson=focusAnimal=-1;yaw=.18f;cameraRadius=9.5f;lightPhase=.63f;
 const char* qualities[]={"very_light","middle","beauty","light_detail","soft_shadows","room_bounce_contact","sparse_gi_reflections"};
 const char* modes[]={"off","protected","full_frame"};
 report["preset_benchmark"]={{"qualities",7},{"phoenix_modes",3},{"repeats",3},{"warmup_frames",12},{"measured_frames",48},
  {"width",W},{"height",H},{"body_detail","full"},{"camera","room0"},{"scene_time_start",8},{"scene_time_end",10},
  {"moving_people",true},{"presentation",false},{"capture_during_timing",false},{"windows_hidden",true},{"order","forward, reverse, rotated"}};
 qualityPreset(3);enabled=true;materialScope=false;sceneTime=8;reset=true;
 for(unsigned i=0;i<64;++i)render(cases[0],{},false,false);
 for(unsigned repeat=0;repeat<3;++repeat)for(unsigned k=0;k<21;++k){
  unsigned index=repeat==1?20-k:repeat==2?(k+8)%21:k,q=index/3,mode=index%3;
  qualityPreset(q);enabled=mode!=0;materialScope=mode==1;displayMode=enabled?1:0;reset=true;
  for(unsigned i=0;i<60;++i){
   sceneTime=8+(i<12?0:2.f*float(i-12)/47);
   auto start=std::chrono::steady_clock::now();render(cases[0],{},false,false);
   double wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
   auto& row=report["frames"].back();row["phase"]="preset_benchmark";row["quality_index"]=q;row["quality"]=qualities[q];row["phoenix_mode"]=modes[mode];row["repeat"]=repeat;row["measured"]=i>=12;row["frame_wall_ms"]=wall;
  }
  std::cout<<"Preset timing "<<repeat+1<<"/3 "<<qualities[q]<<" "<<modes[mode]<<std::endl;
 }
 ensure(!IsWindowVisible(window),"Preset timing revealed a window");report["preset_benchmark"]["passed"]=true;
}
