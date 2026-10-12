void personCheck(const fs::path& out){
 initPanel(out,false);panelTesting=true;uiAnimation=false;uiLights=false;clientSize(1920,1080);
 sceneTime=.9f;lightPhase=.63f;displayCaptureOnly=true;peer->displayCaptureOnly=true;
 auto hidden=[&](){ensure(!IsWindowVisible(window)&&!IsWindowVisible(panel)&&!IsWindowVisible(peer->window),"Person check must stay hidden");};
 auto command=[&](int id){SendMessageW(GetDlgItem(panelBody,id),BM_CLICK,0,0);};
 auto select=[&](HWND control,unsigned value,int id){SendMessageW(control,CB_SETCURSEL,value,0);panelCommand(id,CBN_SELCHANGE);};
 hidden();command(ID_NEXT_PERSON);ensure(focusPerson==0,"Next from overview");command(ID_PREVIOUS_PERSON);ensure(focusPerson==int(VisitorCount)-1,"Previous wraps");command(ID_NEXT_PERSON);ensure(focusPerson==0,"Next wraps");
 float overviewYaw=yaw,overviewRadius=cameraRadius;command(ID_ROTATE_LEFT);ensure(focusAngle<.3f&&yaw==overviewYaw,"Focused rotation stays local");command(ID_ZOOM_IN);ensure(focusZoom<1&&cameraRadius==overviewRadius,"Focused zoom stays local");command(ID_RESET_CAMERA);ensure(focusAngle==.3f&&focusZoom==1,"Focused reset");
 report["person_captures"]=Json::array();
 for(unsigned person=0;person<VisitorCount;++person)for(unsigned framing=0;framing<3;++framing){
  select(personCombo,person+1,ID_PERSON);select(framingCombo,framing,ID_FRAMING);
  for(unsigned pair=0;pair<2;++pair){
   select(qualityCombo[0],pair?0:2,ID_LEFT_QUALITY);select(phoenixCombo[0],pair?2:0,ID_LEFT_PHOENIX);
   select(qualityCombo[1],3,ID_RIGHT_QUALITY);select(phoenixCombo[1],2,ID_RIGHT_PHOENIX);sideDirty[0]=sideDirty[1]=true;
   const auto name="person_"+std::to_string(person)+"_framing_"+std::to_string(framing)+(pair?"_light":"_beauty");
   for(unsigned i=0;i<17;++i){renderComparison(i==16?out/name:fs::path{},false);hidden();}
   const auto& sides=report["comparison_frames"].back()["sides"];
   ensure(sides[0]["camera_eye"]==sides[1]["camera_eye"]&&sides[0]["camera_target"]==sides[1]["camera_target"],"Comparison camera mismatch");
   ensure(sides[0]["focus_person"]==person&&sides[1]["focus_person"]==person,"Selected person camera");
   begin();capture(cmd.Get(),peer->gPosition.Get(),peer->rbPosition,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);submit();save(peer->rbPosition,out/name/"position.rgba32f");
   report["person_captures"].push_back({{"path",name},{"person",person},{"framing",framing},{"pair",pair},{"camera_eye",sides[0]["camera_eye"]},{"camera_target",sides[0]["camera_target"]}});
   std::cout<<"Captured "<<name<<std::endl;
  }
 }
 // The camera follows the moving visitor without repeated neural history resets.
 select(personCombo,1,ID_PERSON);select(framingCombo,1,ID_FRAMING);select(qualityCombo[0],3,ID_LEFT_QUALITY);select(phoenixCombo[0],2,ID_LEFT_PHOENIX);
 XMFLOAT3 firstTarget{};unsigned histories=0;
 for(unsigned i=0;i<32;++i){sceneTime=.9f+7*float(i)/31;renderComparison({},false);auto camera=cameraPose();auto state=visitorState(0,sceneTime);ensure(fabsf(camera.target.x-state.x)<1e-5f&&fabsf(camera.target.z-state.z)<1e-5f,"Follow target must match visitor route");if(i==0)firstTarget=camera.target;else {const auto& sides=report["comparison_frames"].back()["sides"];ensure(!sides[0]["reset"].get<bool>()&&!sides[1]["reset"].get<bool>(),"Following must preserve neural history");++histories;}hidden();}
 ensure(fabsf(cameraPose().target.z-firstTarget.z)>.1f,"Visitor must actually move");
 uiAnimation=true;sceneTime=5;float priorTime=sceneTime;renderComparison({},false);ensure(sceneTime>priorTime,"Animation continues while focused");uiAnimation=false;
 select(phoenixCombo[1],1,ID_RIGHT_PHOENIX);renderComparison({},false);ensure(peer->materialScope,"Protected scope still selectable");
 panelView=0;unsigned peerBefore=peer->serial;renderComparison({},false);ensure(peer->serial==peerBefore,"Focused single view skips inactive renderer");panelView=2;
 clientSize(729,973);renderComparison({},false);clientSize(963,541);renderComparison({},false);clientSize(1920,1080);renderComparison({},false);
 command(ID_OVERVIEW);ensure(focusPerson==-1&&yaw==overviewYaw&&cameraRadius==overviewRadius,"Overview restores original gallery camera");renderComparison({},false);
 select(personCombo,1,ID_PERSON);select(framingCombo,1,ID_FRAMING);sceneTime=.9f;renderComparison({},false);
 scrollOffset=280;layoutPanel();SetWindowTextW(panelStats,L"Offline focus camera check");saveHiddenControls(out);hidden();
 report["person_check"]={{"passed",true},{"actual_world_camera",true},{"people",VisitorCount},{"framings",3},{"snapshots",VisitorCount*6},{"follows_walking",true},{"history_preserved_frames",histories},{"same_camera_both_halves",true},{"single_view",true},{"resize",true},{"windows_hidden",true},{"presentation",false},{"desktop_capture",false}};
 peer->moduleAudit();report["peer"]=peer->report;
}
