// Save a small renderer-owned region; no desktop, window capture or presentation.
void saveRegion(Readback& rb,const fs::path& file,unsigned x,unsigned y,unsigned w,unsigned h){
 ensure(x+w<=W&&y+h<=H,"Readback crop extent");
 BYTE* mapped=nullptr;D3D12_RANGE range{0,SIZE_T(rb.size)},empty{};hr(rb.data->Map(0,&range,reinterpret_cast<void**>(&mapped)));
 std::ofstream stream(file,std::ios::binary);
 for(unsigned row=0;row<h;++row)stream.write(reinterpret_cast<char*>(mapped+rb.fp.Offset+size_t(y+row)*rb.fp.Footprint.RowPitch+x*rb.bpp),std::streamsize(w*rb.bpp));
 rb.data->Unmap(0,&empty);ensure(bool(stream),"Region export failed");
}
void detailReview(const fs::path& out){
 initPanel(out,false);panelTesting=true;uiAnimation=false;uiLights=false;
 clientSize(1920,1080);yaw=.38f;sceneTime=.9f;lightPhase=.63f;
 auto hidden=[&](){ensure(!IsWindowVisible(window)&&!IsWindowVisible(panel)&&!IsWindowVisible(peer->window),"Review must stay hidden");};hidden();
 auto select=[&](unsigned side,unsigned quality,unsigned phoenix){SendMessageW(qualityCombo[side],CB_SETCURSEL,quality,0);panelCommand(side?ID_RIGHT_QUALITY:ID_LEFT_QUALITY,CBN_SELCHANGE);SendMessageW(phoenixCombo[side],CB_SETCURSEL,phoenix,0);panelCommand(side?ID_RIGHT_PHOENIX:ID_LEFT_PHOENIX,CBN_SELCHANGE);};
 select(0,2,0);select(1,3,1);
 for(unsigned i=0;i<17;++i)renderComparison(i==16?out/"default_comparison":fs::path{},false);
 ensure(peer->shading==0&&peer->detailFlags==119,"Detail preset must retain light geometry/ray budgets");
 ensure(peer->network->program_steps()==154,"Full Phoenix network required");
 SetWindowTextW(panelStats,L"Offline capture  |  1920 x 1080");layoutPanel();saveHiddenControls(out);
 // Drive actual native dropdowns through independent presets, scopes and single views.
 for(unsigned quality=0;quality<5;++quality){select(1,quality,quality%3);renderComparison({},false);ensure(peer->detailFlags==(quality==3?119:quality==4?127:0),"Quality dropdown mapping");}
 select(1,3,1);SendMessageW(panelHeader,BM_CLICK,0,0);ensure(!panelExpanded,"Collapse control");renderComparison({},false);SendMessageW(panelHeader,BM_CLICK,0,0);ensure(panelExpanded,"Expand control");
 unsigned before[2]{serial,peer->serial};panelView=0;renderComparison({},false);ensure(serial==before[0]+1&&peer->serial==before[1],"Left single view must skip right work");
 before[0]=serial;before[1]=peer->serial;panelView=1;renderComparison({},false);ensure(serial==before[0]&&peer->serial==before[1]+1,"Right single view must skip left work");
 panelView=3;renderComparison({},false);panelView=2;
 clientSize(963,541);renderComparison({},false);ensure(W==963&&H==541&&peer->W==963&&peer->H==541,"Odd client resize");
 clientSize(729,973);renderComparison({},false);ensure(peer->W==729&&peer->H==973,"Portrait client resize");
 clientSize(1920,1080);select(0,2,0);select(1,3,1);renderComparison({},false);hidden();
 SendMessageW(window,WM_KEYDOWN,'G',0);ensure(viewQuality[0]==2&&viewQuality[1]==3,"Hotkeys must remain disabled");
 report["detail_ui_check"]={{"passed",true},{"preset",119},{"independent_controls",true},{"single_view_skips_inactive_side",true},{"collapse_expand",true},{"odd_and_portrait_resize",true},{"hotkeys_disabled",true},{"windows_hidden",true},{"presentation",false}};
 peer->moduleAudit();report["peer"]=peer->report;
 // A matched six-second sequence includes idle fabric movement, turning and walking.
 struct Clip{const char* name;unsigned flags;bool neural,scoped;};
 const Clip clips[]={{"beauty",0,false,true},{"light_full",0,true,false},{"light_protected",0,true,true},{"detail_full",119,true,false},{"detail_protected",119,true,true},{"soft_full",127,true,false},{"soft_protected",127,true,true}};
 focusPerson=0;focusFraming=1;focusAngle=.3f;focusZoom=1;
 report["detail_clip"]={{"frames",48},{"fps",8},{"duration_seconds",6},{"crop",{640,0,640,1080}},{"actual_person_camera",true},{"presentation",false},{"timing_excluded",true}};
 for(const auto& clip:clips){
  shading=clip.neural?0:2;detailFlags=clip.flags;enabled=clip.neural;materialScope=clip.scoped;reset=true;
  auto pose=[&](float t){yaw=.38f+.08f*sinf(t*XM_2PI);sceneTime=.9f+6*t;lightPhase=.63f+.3f*t;};pose(0);
  for(unsigned i=0;i<16;++i)render(cases[0],{},false,false);
  const fs::path folder=out/"clips"/clip.name;fs::create_directories(folder);
  for(unsigned i=0;i<48;++i){
   pose(float(i)/47);auto row=render(cases[0],{},false,false);begin();
   capture(cmd.Get(),enabled?enhanced.Get():scene.Get(),enabled?rbOutput:rbInput,enabled?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   capture(cmd.Get(),motion.Get(),rbMotion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),gPosition.Get(),rbPosition,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);submit();
   const auto frame=folder/("frame_"+std::to_string(i));fs::create_directory(frame);
   saveRegion(enabled?rbOutput:rbInput,frame/"color.rgba16f",640,0,640,1080);saveRegion(rbMotion,frame/"motion.rg16f",640,0,640,1080);saveRegion(rbPosition,frame/"position.rgba32f",640,0,640,1080);
   std::ofstream(frame/"frame.json")<<row.dump(2);hidden();
  }
  std::cout<<"Captured motion "<<clip.name<<std::endl;
 }
}
