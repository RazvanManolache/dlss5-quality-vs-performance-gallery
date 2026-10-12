// Feature ablations and low-cost scene variants. Phoenix stays at 154 launches.
void researchCheck(const fs::path& out){
 ensure(!IsWindowVisible(window),"Research capture must remain hidden");
 struct Condition{const char* name;unsigned quality,flags;bool neural;int direct=-1,gi=-1,reflection=-1,geometry=-1;};
 const Condition conditions[]={
  {"beauty_off",2,0,false},{"beauty_nr",2,0,true},
  {"beauty_direct1",2,0,false,1},{"beauty_gi0",2,0,false,-1,0},
  {"beauty_no_second",2,128,false},{"beauty_reflect1",2,0,false,-1,-1,1},
  {"beauty_sss_off",2,8192,false},{"beauty_geometry_low",2,0,false,-1,-1,-1,0},
  {"middle_off",1,0,false},{"detail_off",0,119,false},
  {"light_nr",0,0,true},{"texture_nr",0,16,true},{"normal_nr",0,32,true},
  {"filter_nr",0,68,true},{"detail_nr",0,119,true},{"soft_nr",0,127,true},
  {"gi1_nr",0,119|128,true,-1,1},{"reflect_nr",0,119|256,true},
  {"proxy_nr",0,119|512,true},{"contact_nr",0,119|1024,true},
  {"proxy_contact_nr",0,127|512|1024,true},{"proxy_contact_off",0,127|512|1024,false},
  {"sparse_nr",0,127|128|256|1024|16384,true,-1,1,1},
  {"sparse_off",0,127|128|256|1024|16384,false,-1,1,1},
  {"sparse_shadowed_nr",0,127|128|256|1024,true,-1,1,1}
 };
 const unsigned count=unsigned(std::size(conditions));focusPerson=-1;materialScope=false;
 auto select=[&](const Condition& c){shading=c.quality;detailFlags=c.flags;enabled=c.neural;directOverride=c.direct;giOverride=c.gi;reflectionOverride=c.reflection;geometryOverride=c.geometry;reset=true;};
 auto pose=[&](float t){yaw=.38f+.12f*sinf(t*XM_2PI);cameraRadius=4.8f;sceneTime=.9f+2*t;lightPhase=.63f+.15f*t;};
 report["detail_protocol"]={{"warmup_frames",16},{"measured_frames",64},{"repeats",3},{"width",W},{"height",H},{"presentation",false},{"desktop_capture",false},{"windows_hidden",true},{"network_blocks",8},{"refinement_passes",1},{"camera","Matched two-second moving room camera, visitors and lighting"}};
 report["research_protocol"]={{"conditions",count},{"full_frame_neural",true},{"timing_capture",false},{"conditions_order","forward, reverse, rotated"},{"still_views",{"room","person0","person1","person2"}},{"motion_fps",24},{"motion_frames",48},{"motion_crop",{640,0,640,1080}}};
 select(conditions[14]);pose(0);for(unsigned i=0;i<64;++i){render(cases[0],{},false,false);report["frames"].back()["phase"]="device_warmup";}
 for(unsigned repeat=0;repeat<3;++repeat)for(unsigned k=0;k<count;++k){
  unsigned index=repeat==1?count-1-k:repeat==2?(k+9)%count:k;const auto& c=conditions[index];select(c);
  for(unsigned i=0;i<80;++i){pose(i<16?0:float(i-16)/63);auto start=std::chrono::steady_clock::now();render(cases[0],{},false,false);auto& row=report["frames"].back();row["condition"]=c.name;row["phase"]="timing";row["repeat"]=repeat;row["measured"]=i>=16;row["sequence_frame"]=i<16?-1:int(i-16);row["frame_wall_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
  ensure(!IsWindowVisible(window),"Research check revealed a window");std::cout<<"Research timing "<<repeat<<" "<<c.name<<std::endl;
 }
 auto still=[&](const Condition& c,const fs::path& folder){
  select(c);pose(0);for(unsigned i=0;i<17;++i)render(cases[0],{},false,false);
  fs::create_directories(folder);begin();capture(cmd.Get(),scene.Get(),rbInput,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  if(enabled)capture(cmd.Get(),enhanced.Get(),rbOutput,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  capture(cmd.Get(),gPosition.Get(),rbPosition,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);submit();
  save(rbInput,folder/"input.rgba16f");if(enabled)save(rbOutput,folder/"output.rgba16f");save(rbPosition,folder/"position.rgba32f");
  report["frames"].back()["phase"]="still";report["frames"].back()["condition"]=c.name;std::ofstream(folder/"frame.json")<<report["frames"].back().dump(2);
 };
 for(const auto& c:conditions){focusPerson=-1;still(c,out/"stills"/c.name);focusPerson=0;focusFraming=1;still(c,out/"people"/"person0"/c.name);}
 const unsigned displayed[]={0,14,15,20,22,24};
 for(unsigned person=1;person<3;++person)for(unsigned index:displayed){focusPerson=int(person);focusFraming=1;still(conditions[index],out/"people"/("person"+std::to_string(person))/conditions[index].name);}
 for(unsigned index:displayed){const auto& c=conditions[index];select(c);focusPerson=0;focusFraming=1;
  pose(0);for(unsigned i=0;i<16;++i)render(cases[0],{},false,false);
  for(unsigned i=0;i<48;++i){float t=float(i)/24;sceneTime=.9f+t;lightPhase=.63f+.075f*t;render(cases[0],{},false,false);
   auto folder=out/"clips"/c.name/("frame_"+std::to_string(i));fs::create_directories(folder);begin();
   capture(cmd.Get(),scene.Get(),rbInput,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   if(enabled)capture(cmd.Get(),enhanced.Get(),rbOutput,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   capture(cmd.Get(),motion.Get(),rbMotion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),gPosition.Get(),rbPosition,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),gPrevious.Get(),rbPrevious,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);submit();
   saveRegion(rbInput,folder/"input.rgba16f",640,0,640,1080);if(enabled)saveRegion(rbOutput,folder/"output.rgba16f",640,0,640,1080);
   saveRegion(rbMotion,folder/"motion.rg16f",640,0,640,1080);saveRegion(rbPosition,folder/"position.rgba32f",640,0,640,1080);saveRegion(rbPrevious,folder/"previous.rgba32f",640,0,640,1080);
   auto& row=report["frames"].back();row["phase"]="motion";row["condition"]=c.name;row["sequence_frame"]=i;std::ofstream(folder/"frame.json")<<row.dump(2);
  }std::cout<<"Research motion "<<c.name<<std::endl;
 }
 // Exercise both new choices through the real hidden panel, preserving timing rows.
 auto timingFrames=std::move(report["frames"]);report["frames"]=Json::array();
 initPanel(out,false);panelTesting=true;uiAnimation=false;uiLights=false;clientSize(1920,1080);
 auto selectControl=[&](HWND control,unsigned value,unsigned id){SendMessageW(control,CB_SETCURSEL,value,0);panelCommand(id,CBN_SELCHANGE);};
 selectControl(viewCombo,2,ID_VIEW);selectControl(qualityCombo[0],2,ID_LEFT_QUALITY);selectControl(phoenixCombo[0],0,ID_LEFT_PHOENIX);selectControl(phoenixCombo[1],2,ID_RIGHT_PHOENIX);
 for(unsigned q:{5u,6u}){selectControl(qualityCombo[1],q,ID_RIGHT_QUALITY);for(unsigned i=0;i<17;++i)renderComparison({},false);
  ensure(peer->detailFlags==(q==5?(127|512|1024):(127|128|256|1024|16384)),"Research panel flags");ensure(peer->giCount()==(q==6?1u:0u),"Research panel indirect samples");
  ensure(!IsWindowVisible(window)&&!IsWindowVisible(panel)&&!IsWindowVisible(peer->window),"Research panel revealed a window");
 }
 report["research_ui"]={{"passed",true},{"quality_choices",{5,6}},{"windows_hidden",true},{"presentation",false},{"frames",std::move(report["frames"])}};
 report["frames"]=std::move(timingFrames);
 report["detail_protocol"]["passed"]=true;report["research_protocol"]["passed"]=true;
}

