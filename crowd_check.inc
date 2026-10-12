// Native offscreen evidence for the expanded gallery, including real controls.
void crowdCheck(const fs::path& out){
 clientSize(1920,1080);
 ensure(!IsWindowVisible(window),"Crowd capture must stay hidden");
 struct Condition{const char* name;unsigned quality;bool neural;};
 const Condition conditions[]={{"beauty_off",2,false},{"middle_off",1,false},{"detail_off",3,false},{"detail_nr",3,true},{"proxy_contact_nr",5,true}};
 materialScope=false;focusPerson=-1;
 auto select=[&](const Condition& c){qualityPreset(c.quality);enabled=c.neural;reset=true;};
 auto pose=[&](float t){yaw=.18f+.12f*sinf(t*XM_2PI);cameraRadius=7.6f;sceneTime=.9f+2*t;lightPhase=.63f+.15f*t;};
 report["detail_protocol"]={{"warmup_frames",16},{"measured_frames",64},{"repeats",3},{"width",W},{"height",H},{"presentation",false},{"desktop_capture",false},{"windows_hidden",true}};
 report["crowd_protocol"]={{"visitors",VisitorCount},{"objects",gallery.size()},{"windows_hidden",true},{"presentation",false},{"desktop_capture",false},{"network_blocks",8},{"learned_launches_per_frame",154}};
 select(conditions[3]);pose(0);for(unsigned i=0;i<32;++i)render(cases[0],{},false,false);
 for(unsigned repeat=0;repeat<3;++repeat)for(unsigned k=0;k<5;++k){
  const auto& c=conditions[repeat==1?4-k:repeat==2?(k+2)%5:k];select(c);
  for(unsigned i=0;i<80;++i){pose(i<16?0:float(i-16)/63);auto start=std::chrono::steady_clock::now();render(cases[0],{},false,false);auto& row=report["frames"].back();row["condition"]=c.name;row["phase"]="timing";row["repeat"]=repeat;row["measured"]=i>=16;row["frame_wall_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
  std::cout<<"Crowd timing "<<repeat<<" "<<c.name<<std::endl;
 }
 auto still=[&](const Condition& c,const fs::path& folder){
  select(c);for(unsigned i=0;i<17;++i)render(cases[0],{},false,false);
  fs::create_directories(folder);begin();capture(cmd.Get(),scene.Get(),rbInput,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  if(enabled)capture(cmd.Get(),enhanced.Get(),rbOutput,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  capture(cmd.Get(),gPosition.Get(),rbPosition,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);submit();
  save(rbInput,folder/"input.rgba16f");if(enabled)save(rbOutput,folder/"output.rgba16f");save(rbPosition,folder/"position.rgba32f");
  std::ofstream(folder/"frame.json")<<report["frames"].back().dump(2);
 };
 for(unsigned angle=0;angle<3;++angle){pose(0);yaw=angle==0?.18f:angle==1?-.55f:XM_PI;cameraRadius=angle==0?7.6f:10.2f;focusPerson=-1;
  for(unsigned index:{0u,1u,3u,4u})still(conditions[index],out/"room"/("angle"+std::to_string(angle))/conditions[index].name);
 }
 pose(0);focusFraming=1;
 for(unsigned person=0;person<VisitorCount;++person){focusPerson=int(person);for(unsigned index:{0u,4u})still(conditions[index],out/"people"/("person"+std::to_string(person))/conditions[index].name);std::cout<<"Crowd person "<<person<<std::endl;}
 // Keep moving overview and a close talking visitor for visual inspection.
 for(unsigned clip=0;clip<2;++clip){select(conditions[4]);focusPerson=clip?4:-1;focusFraming=1;pose(0);sceneTime=clip?12.f:8.f;
  for(unsigned i=0;i<16;++i)render(cases[0],{},false,false);
  for(unsigned i=0;i<48;++i){sceneTime=(clip?12.f:8.f)+float(i)/24;render(cases[0],{},false,false);
   auto folder=out/"clips"/(clip?"person":"room")/("frame"+std::to_string(i));fs::create_directories(folder);begin();
   capture(cmd.Get(),scene.Get(),rbInput,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),enhanced.Get(),rbOutput,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),motion.Get(),rbMotion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);submit();
   save(rbInput,folder/"input.rgba16f");save(rbOutput,folder/"output.rgba16f");save(rbMotion,folder/"motion.rg16f");std::ofstream(folder/"frame.json")<<report["frames"].back().dump(2);
  }
 }
 auto timingFrames=std::move(report["frames"]);report["frames"]=Json::array();
 initPanel(out,false);panelTesting=true;uiAnimation=false;uiLights=false;clientSize(1920,1080);
 ensure(peer->sharedTextures==textures.size(),"Comparison must reuse every immutable source texture");
 ensure(SendMessageW(personCombo,CB_GETCOUNT,0,0)==VisitorCount+1,"Every visitor in focus selector");
 auto command=[&](int id){SendMessageW(GetDlgItem(panelBody,id),BM_CLICK,0,0);};
 selectPerson(-1);command(ID_PREVIOUS_PERSON);ensure(focusPerson==int(VisitorCount)-1,"Previous wraps all visitors");command(ID_NEXT_PERSON);ensure(focusPerson==0,"Next wraps all visitors");
 viewQuality[0]=2;viewPhoenix[0]=0;viewQuality[1]=5;viewPhoenix[1]=2;
 for(unsigned i=0;i<VisitorCount;++i){SendMessageW(personCombo,CB_SETCURSEL,i+1,0);panelCommand(ID_PERSON,CBN_SELCHANGE);focusFraming=i%3;sceneTime=.9f;renderComparison({},false);const auto& sides=report["comparison_frames"].back()["sides"];ensure(focusPerson==int(i)&&sides[0]["camera_eye"]==sides[1]["camera_eye"]&&sides[0]["camera_target"]==sides[1]["camera_target"],"Crowd focus cameras must match");}
 panelView=0;unsigned before=peer->serial;renderComparison({},false);ensure(peer->serial==before,"Single view skips inactive renderer");panelView=2;
 clientSize(729,973);renderComparison({},false);clientSize(963,541);renderComparison({},false);clientSize(1920,1080);renderComparison({},false);
 ensure(!IsWindowVisible(window)&&!IsWindowVisible(panel)&&!IsWindowVisible(peer->window),"Crowd controls must stay hidden");
 peer->moduleAudit();report["crowd_ui"]={{"passed",true},{"people",VisitorCount},{"shared_textures",peer->sharedTextures},{"camera_matches",true},{"single_view",true},{"resize",true},{"frames",std::move(report["frames"])}};
 report["frames"]=std::move(timingFrames);report["crowd_protocol"]["passed"]=true;
}
