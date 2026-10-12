// Native GPU readbacks validate skinning, morphs, LODs, motion and real UI.
void validateSkin(const fs::path& out){
 const UINT64 size=rayVertices->GetDesc().Width;auto rb=phoenix_native::buffer(device.Get(),size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
 constexpr auto state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
 begin();phoenix_native::transition(cmd.Get(),rayVertices.Get(),state,D3D12_RESOURCE_STATE_COPY_SOURCE);cmd->CopyResource(rb.Get(),rayVertices.Get());phoenix_native::transition(cmd.Get(),rayVertices.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,state);submit();
 void* mapped;D3D12_RANGE range{0,SIZE_T(size)},empty{};hr(rb->Map(0,&range,&mapped));auto* vertices=static_cast<Vertex*>(mapped);Json checks=Json::array();
 for(unsigned actor=0;actor<ActorCount;++actor){const auto& p=people[actor];auto data=bytes(root/"assets/living_people"/(std::string(AssetNames[4+actor])+".skin"));unsigned offset=p.rig.at("lod_offsets")[p.lod],m=assetMesh(4+actor,p.lod);auto f=faceWeights(actor,sceneTime);float maximum=0;unsigned samples=0;
  for(unsigned i=0;i<meshList[m]->count;i+=137){const uint8_t* record=data.data()+size_t(offset+i)*152;float bind[3],weights[4],delta[18];unsigned ids[4];memcpy(bind,record,12);memcpy(ids,record+48,16);memcpy(weights,record+64,16);memcpy(delta,record+80,72);float position[4]={bind[0]+delta[0]*f.x+delta[6]*f.y+delta[12]*f.z,bind[1]+delta[1]*f.x+delta[7]*f.y+delta[13]*f.z,bind[2]+delta[2]*f.x+delta[8]*f.y+delta[14]*f.z,1},expected[3]{};
   for(unsigned b=0;b<4;++b)for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)expected[row]+=p.currentBones[ids[b]*16+row*4+col]*position[col]*weights[b];const auto& v=vertices[vertexOffsets[m]+i];
   for(unsigned k=0;k<3;++k){ensure(std::isfinite(v.p[k])&&std::isfinite(v.n[k]),"Finite GPU skin output");maximum=std::max(maximum,fabsf(expected[k]-v.p[k]));}++samples;
  }ensure(maximum<.00002f,"GPU/CPU skeletal and morph agreement");checks.push_back({{"actor",actor},{"lod",p.lod},{"samples",samples},{"maximum_position_error_m",maximum}});
 }rb->Unmap(0,&empty);report["skin_validation"]=checks;std::ofstream(out/"skin-validation.json")<<checks.dump(2);
}
void livingCheck(const fs::path& out){
 clientSize(1920,1080);ensure(!IsWindowVisible(window),"Living check must stay hidden");
 struct Condition{const char* name;unsigned quality,detail;bool neural;};
 const Condition conditions[]={{"beauty_off",2,0,false},{"detail_off",3,0,false},{"detail_nr_full",3,0,true},{"detail_nr_distance",3,1,true},{"detail_nr_reduced",3,2,true}};
 auto select=[&](const Condition& c){qualityPreset(c.quality);characterDetail=c.detail;enabled=c.neural;materialScope=false;displayMode=enabled?1:0;reset=true;};
 auto pose=[&](float t){sceneTime=8.f+t*2;yaw=.18f;lightPhase=.63f;cameraRadius=9.5f;};
 report["detail_protocol"]={{"warmup_frames",12},{"measured_frames",48},{"repeats",3},{"width",W},{"height",H},{"presentation",false},{"desktop_capture",false},{"windows_hidden",true}};
 report["living_protocol"]={{"visitors",VisitorCount},{"animals",AnimalCount},{"objects",gallery.size()},{"windows_hidden",true},{"presentation",false},{"desktop_capture",false}};
 select(conditions[3]);pose(0);for(unsigned i=0;i<32;++i)render(cases[0],{},false,false);
 for(unsigned repeat=0;repeat<3;++repeat)for(unsigned k=0;k<5;++k){const auto& c=conditions[repeat==1?4-k:repeat==2?(k+2)%5:k];select(c);focusPerson=-1;
  for(unsigned i=0;i<60;++i){pose(i<12?0:float(i-12)/47);auto start=std::chrono::steady_clock::now();render(cases[0],{},false,false);auto& row=report["frames"].back();row["condition"]=c.name;row["phase"]="timing";row["repeat"]=repeat;row["measured"]=i>=12;row["frame_wall_ms"]=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
  std::cout<<"Living timing "<<repeat<<" "<<c.name<<std::endl;
 }
 auto still=[&](const Condition& c,const fs::path& folder){select(c);for(unsigned i=0;i<12;++i)render(cases[0],{},false,false);fs::create_directories(folder);begin();capture(cmd.Get(),scene.Get(),rbInput,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);if(enabled)capture(cmd.Get(),enhanced.Get(),rbOutput,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),gPosition.Get(),rbPosition,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),motion.Get(),rbMotion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);submit();save(rbInput,folder/"input.rgba16f");if(enabled)save(rbOutput,folder/"output.rgba16f");save(rbPosition,folder/"position.rgba32f");save(rbMotion,folder/"motion.rg16f");std::ofstream(folder/"frame.json")<<report["frames"].back().dump(2);};
 pose(0);focusPerson=-1;for(const auto& c:conditions)still(c,out/"room"/"angle0"/c.name);
 yaw=-.55f;cameraRadius=8.5f;for(unsigned c:{0u,3u})still(conditions[c],out/"room"/"angle1"/conditions[c].name);
 for(unsigned animal=0;animal<AnimalCount;++animal){focusAnimal=int(animal);for(unsigned c:{0u,3u})still(conditions[c],out/"animals"/("animal"+std::to_string(animal))/conditions[c].name);}focusAnimal=-1;
 validateSkin(out);focusFraming=1;
 for(unsigned person=0;person<VisitorCount;++person){pose(0);focusPerson=int(person);for(unsigned c:{0u,3u,4u})still(conditions[c],out/"people"/("person"+std::to_string(person))/conditions[c].name);std::cout<<"Living person "<<person<<std::endl;}
 // Isolated authored activities, including the photo and phone gestures.
 for(unsigned person:{0u,2u,3u,6u,7u,12u}){focusPerson=int(person);focusFraming=0;sceneTime=person==0?2.9f:1.8f;still(conditions[3],out/"activities"/("person"+std::to_string(person)));}
 // Exact matched closed/open-eye samples, with independent facial control.
 focusPerson=0;focusFraming=2;sceneTime=.32f;facialAnimation=false;still(conditions[3],out/"faces"/"off");facialAnimation=true;still(conditions[3],out/"faces"/"blink");validateSkin(out);
 for(unsigned clip=0;clip<3;++clip){select(conditions[3]);focusPerson=clip==0?-1:clip==1?12:0;focusFraming=clip==2?2:1;float start=clip==2?0.f:8.f;sceneTime=start;for(unsigned i=0;i<12;++i)render(cases[0],{},false,false);
  for(unsigned i=0;i<48;++i){sceneTime=start+float(i)/24;render(cases[0],{},false,false);auto folder=out/"clips"/(clip==0?"room":clip==1?"guide":"face")/("frame"+std::to_string(i));fs::create_directories(folder);begin();capture(cmd.Get(),scene.Get(),rbInput,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),enhanced.Get(),rbOutput,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),motion.Get(),rbMotion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);submit();save(rbInput,folder/"input.rgba16f");save(rbOutput,folder/"output.rgba16f");save(rbMotion,folder/"motion.rg16f");std::ofstream(folder/"frame.json")<<report["frames"].back().dump(2);}
 }
 // Exercise generated native launch plans at exact odd, portrait and wide sizes.
 focusPerson=focusAnimal=-1;select(conditions[3]);sceneTime=8;report["exact_resolution_checks"]=Json::array();
 const unsigned extents[][2]={{397,257},{963,541},{729,973},{1537,863},{2560,1080},{1919,1079},{1920,1080}};
 for(const auto& extent:extents){clientSize(extent[0],extent[1]);for(unsigned i=0;i<3;++i){render(cases[0],{},false,false);ensure(NW==W&&NH==H&&activeNW==W&&activeNH==H&&offsetX==0&&offsetY==0,"Exact native Phoenix dimensions");}
  auto folder=out/"resolutions"/(std::to_string(W)+"x"+std::to_string(H));fs::create_directories(folder);begin();capture(cmd.Get(),scene.Get(),rbInput,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),enhanced.Get(),rbOutput,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);capture(cmd.Get(),motion.Get(),rbMotion,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);submit();save(rbInput,folder/"input.rgba16f");save(rbOutput,folder/"output.rgba16f");save(rbMotion,folder/"motion.rg16f");std::ofstream(folder/"frame.json")<<report["frames"].back().dump(2);report["exact_resolution_checks"].push_back({{"width",W},{"height",H},{"neural_width",NW},{"neural_height",NH},{"passed",true}});
 }
 auto timingFrames=std::move(report["frames"]);report["frames"]=Json::array();initPanel(out,false);panelTesting=true;uiAnimation=false;uiLights=false;
 ensure(peer->sharedTextures==textures.size(),"Reuse all immutable textures");ensure(SendMessageW(personCombo,CB_GETCOUNT,0,0)==VisitorCount+1,"Every human selectable");
 selectPerson(-1);panelCommand(ID_PREVIOUS_PERSON,BN_CLICKED);ensure(focusPerson==int(VisitorCount)-1,"Previous wraps");panelCommand(ID_NEXT_PERSON,BN_CLICKED);ensure(focusPerson==0,"Next wraps");
 for(unsigned i=0;i<VisitorCount;++i){selectPerson(int(i));focusFraming=i%3;sceneTime=8;renderComparison({},false);const auto& sides=report["comparison_frames"].back()["sides"];ensure(sides[0]["camera_eye"]==sides[1]["camera_eye"]&&sides[0]["camera_target"]==sides[1]["camera_target"],"Matched focus cameras");}
 SendMessageW(characterCombo[1],CB_SETCURSEL,2,0);panelCommand(ID_RIGHT_DETAIL,CBN_SELCHANGE);ensure(viewCharacterDetail[1]==2,"Body detail control");
 SendMessageW(faceCheck,BM_SETCHECK,BST_UNCHECKED,0);panelCommand(ID_FACES,BN_CLICKED);renderComparison({},false);ensure(!peer->facialAnimation,"Facial toggle applies both sides");
 panelView=0;unsigned before=peer->serial;renderComparison({},false);ensure(peer->serial==before,"Single view skips inactive peer");panelView=2;
 clientSize(729,973);renderComparison({},false);clientSize(963,541);renderComparison({},false);clientSize(1920,1080);renderComparison({},false);
 ensure(!IsWindowVisible(window)&&!IsWindowVisible(panel)&&!IsWindowVisible(peer->window),"All windows hidden");peer->moduleAudit();report["living_ui"]={{"passed",true},{"people",VisitorCount},{"shared_textures",peer->sharedTextures},{"camera_matches",true},{"single_view",true},{"resize",true},{"frames",std::move(report["frames"])}};report["frames"]=std::move(timingFrames);report["living_protocol"]["passed"]=true;
}
