// Reproducible full-resolution screenshots without showing a window or presenting.
void screenshotMatrix(const fs::path& out){
 clientSize(1920,1080);ensure(!IsWindowVisible(window),"Screenshot matrix must stay hidden");
 deterministicAnimation=true;animation=false;movingLight=false;livingAnimation=false;
 facialAnimation=true;characterDetail=0;sceneTime=8;lightPhase=.63f;focusAngle=.3f;focusZoom=1;focusAnimal=-1;
 const char* qualities[]={"very_light","middle","beauty","light_detail","soft_shadows","room_bounce_contact","sparse_gi_reflections"};
 const char* modes[]={"off","protected","full_frame"};
 report["screenshot_matrix"]={{"width",W},{"height",H},{"qualities",qualities},{"phoenix_modes",modes},{"warmup_frames",12},{"desktop_capture",false},{"presentation",false},{"windows_hidden",true},{"body_detail","full, held constant"},{"captures",Json::array()}};
 // Same display shader and sRGB conversion as the interactive application.
 auto screenshot=[&](const fs::path& path){
  begin();unsigned index=swap->GetCurrentBackBufferIndex();
  phoenix_native::transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  phoenix_native::transition(cmd.Get(),back[index].Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
  auto target=rt(index);cmd->OMSetRenderTargets(1,&target,FALSE,nullptr);cmd->SetPipelineState(displayPSO.Get());cmd->SetGraphicsRootSignature(displayRoot.Get());
  ID3D12DescriptorHeap* heaps[]{srv.Get()};cmd->SetDescriptorHeaps(1,heaps);cmd->SetGraphicsRootDescriptorTable(0,view(32));
  D3D12_VIEWPORT viewport{0,0,float(W),float(H),0,1};D3D12_RECT scissor{0,0,LONG(W),LONG(H)};cmd->RSSetViewports(1,&viewport);cmd->RSSetScissorRects(1,&scissor);
  struct Display{unsigned mode;float divider;unsigned width,height;} dc{enabled?1u:0u,.5f,W,H};cmd->SetGraphicsRoot32BitConstants(1,4,&dc,0);
  cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);cmd->DrawInstanced(3,1,0,0);
  capture(cmd.Get(),back[index].Get(),rbDisplay,D3D12_RESOURCE_STATE_RENDER_TARGET);
  phoenix_native::transition(cmd.Get(),back[index].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
  phoenix_native::transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  submit();savePNG(rbDisplay,path);ensure(errors()==0,"Screenshot display pass D3D12 errors");
 };
 for(unsigned v=0;v<2+VisitorCount*2;++v){
  std::string camera;
  if(v<2){focusPerson=-1;yaw=v==0?.18f:-.55f;cameraRadius=v==0?9.5f:8.5f;camera="room"+std::to_string(v);}
  else{unsigned p=(v-2)/2;focusPerson=int(p);focusFraming=(v-2)%2==0?1:2;camera="person"+std::to_string(p)+(focusFraming==1?"_body":"_face");}
  auto folder=out/"images"/camera;fs::create_directories(folder);Json referenceEye,referenceTarget;
  for(unsigned q=0;q<7;++q)for(unsigned mode=0;mode<3;++mode){
   qualityPreset(q);enabled=mode!=0;materialScope=mode==1;displayMode=enabled?1:0;reset=true;
   for(unsigned frame=0;frame<12;++frame)render(cases[0],{},false,false);
   auto row=report["frames"].back();
   if(q==0&&mode==0){referenceEye=row["camera_eye"];referenceTarget=row["camera_target"];}
   ensure(row["camera_eye"]==referenceEye&&row["camera_target"]==referenceTarget,"Matrix camera must match");
   ensure(row["own_launches"]==(enabled?154:0)&&row["exact_neural_resolution"]==true,"Full native Phoenix matrix frame");
   std::string file=std::string(qualities[q])+"__"+modes[mode]+".png";
   screenshot(folder/file);row["view"]=camera;row["quality_index"]=q;row["quality"]=qualities[q];row["phoenix_mode"]=modes[mode];row["image"]=(fs::path("images")/camera/file).generic_string();
   report["screenshot_matrix"]["captures"].push_back(row);
  }
  // Flush a small checkpoint; final run.json is only marked passed on completion.
  std::ofstream(out/"matrix-progress.json")<<report["screenshot_matrix"].dump(2);
  std::cout<<"Screenshot matrix "<<v+1<<"/"<<2+VisitorCount*2<<" "<<camera<<std::endl;
 }
 ensure(report["screenshot_matrix"]["captures"].size()==(2+VisitorCount*2)*21,"Complete screenshot matrix");
 report["screenshot_matrix"]["passed"]=true;
}
