// Scene-lab rendering only. The learned network and its assets are unchanged.
unsigned shading=0;
float cameraRadius=3.5f,lightPhase=0;
bool movingLight=false,displayCaptureOnly=false;
ComPtr<ID3D12Resource> shadowMaps[3],diffuseScene,skinGuide,skinSharp,skinScratch;
ComPtr<ID3D12PipelineState> lightingPSO[3],shadowPSO;
std::unique_ptr<pg::phoenix::direct::Pass> skinHorizontal,skinVertical;
Readback rbDiffuse,rbGuide,rbSharp;
const unsigned shadowSize[3]={1024,2048,4096};
const char* shadingName()const{return shading==2?"reference":shading==1?"balanced":"cheap";}
D3D12_CPU_DESCRIPTOR_HANDLE shadowDSV(unsigned i){auto h=dsv->GetCPUDescriptorHandleForHeapStart();h.ptr+=(i+1)*device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);return h;}
void initLighting(const std::string& source,D3D12_GRAPHICS_PIPELINE_STATE_DESC pd){
 for(unsigned i=0;i<3;++i){
  D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=desc.Height=shadowSize[i];desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=DXGI_FORMAT_R32_TYPELESS;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_CLEAR_VALUE clear{};clear.Format=DXGI_FORMAT_D32_FLOAT;clear.DepthStencil.Depth=1;
  hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&clear,IID_PPV_ARGS(&shadowMaps[i])));
  D3D12_DEPTH_STENCIL_VIEW_DESC dd{};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;device->CreateDepthStencilView(shadowMaps[i].Get(),&dd,shadowDSV(i));
  D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=DXGI_FORMAT_R32_FLOAT;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;
  auto handle=srv->GetCPUDescriptorHandleForHeapStart();handle.ptr+=(36+i)*srvStride;device->CreateShaderResourceView(shadowMaps[i].Get(),&sd,handle);
 }
 auto extra=bytes(root/"lighting.hlsl");std::string lit=source+std::string(extra.begin(),extra.end());
 auto vertex=shader("#define QUALITY 0\n"+lit,"vs","vs_5_1");pd.VS={vertex->GetBufferPointer(),vertex->GetBufferSize()};
 for(unsigned i=0;i<3;++i){
  auto pixel=shader("#define QUALITY "+std::to_string(i)+"\n"+lit,"lighting","ps_5_1");pd.PS={pixel->GetBufferPointer(),pixel->GetBufferSize()};pd.NumRenderTargets=i==2?4:2;
  pd.RTVFormats[2]=pd.RTVFormats[3]=i==2?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_UNKNOWN;
  pd.BlendState.RenderTarget[2].RenderTargetWriteMask=pd.BlendState.RenderTarget[3].RenderTargetWriteMask=15;
  hr(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&lightingPSO[i])));
 }
 vertex=shader("#define QUALITY 0\n"+lit,"shadowVertex","vs_5_1");pd.VS={vertex->GetBufferPointer(),vertex->GetBufferSize()};pd.PS={};pd.NumRenderTargets=0;
 for(auto& format:pd.RTVFormats)format=DXGI_FORMAT_UNKNOWN;
 pd.RasterizerState.DepthBias=64;pd.RasterizerState.SlopeScaledDepthBias=1.5f;hr(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&shadowPSO)));
 diffuseScene=image(device.Get(),W,H,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 skinGuide=image(device.Get(),W,H,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 skinSharp=image(device.Get(),W,H,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 skinScratch=phoenix_native::image(device.Get(),W,H,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);
 device->CreateRenderTargetView(diffuseScene.Get(),nullptr,rt(4));device->CreateRenderTargetView(skinGuide.Get(),nullptr,rt(5));device->CreateRenderTargetView(skinSharp.Get(),nullptr,rt(6));
 auto skin=bytes(root/"skin.hlsl");std::string skinSource(skin.begin(),skin.end());
 skinHorizontal=std::make_unique<pg::phoenix::direct::Pass>(device.Get(),("#define HORIZONTAL 1\n"+skinSource).c_str());
 skinVertical=std::make_unique<pg::phoenix::direct::Pass>(device.Get(),("#define HORIZONTAL 0\n"+skinSource).c_str());
 rbDiffuse=readback(device.Get(),diffuseScene.Get(),8);rbGuide=readback(device.Get(),skinGuide.Get(),8);rbSharp=readback(device.Get(),skinSharp.Get(),8);
}
template<class Draw> void drawSubjects(Draw& draw,const Case& c){
 draw(box,XMMatrixScaling(40,.12f,40)*XMMatrixTranslation(0,-.09f,0),{.16f,.19f,.22f,2},0);
 draw(box,XMMatrixScaling(.73f,.6f,.7f)*XMMatrixTranslation(-.67f,.25f,0),{.06f,.085f,.11f,2},0);
 draw(box,XMMatrixScaling(.8f,.6f,.7f)*XMMatrixTranslation(.75f,.25f,0),{.09f,.12f,.15f,2},0);
 draw(head[c.low?1:0],XMMatrixTranslation(-.67f,1.33f,0),{0,0,0,0},0);
 draw(marble[c.low?1:0],XMMatrixTranslation(.75f,1.33f,0),{0,0,0,1},1);
}
void skinDiffusion(){
 using phoenix_native::transition;
 for(auto* r:{diffuseScene.Get(),skinGuide.Get(),skinSharp.Get()})transition(cmd.Get(),r,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 pg::phoenix::direct::Constants c{W,H,0,0,0,0,.5f,1,0};
 transition(cmd.Get(),skinScratch.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 skinHorizontal->record(cmd.Get(),{diffuseScene.Get(),skinGuide.Get(),skinSharp.Get()},skinScratch.Get(),c);
 transition(cmd.Get(),skinScratch.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 skinVertical->record(cmd.Get(),{skinScratch.Get(),skinGuide.Get(),skinSharp.Get()},scene.Get(),c);
 transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
void tradeoff(const fs::path& out){
 struct Condition{const char* name;unsigned quality;bool neural;};
 const Condition conditions[]={{"reference",2,false},{"balanced",1,false},{"balanced_neural",1,true},{"cheap",0,false},{"cheap_neural",0,true}};
 auto pose=[&](unsigned frame){float t=float(frame)/95; // All paths see identical poses and moving light.
  yaw=-.9f+2.0f*sinf(t*XM_PI);cameraRadius=3.5f-.8f*sinf(t*XM_PI);lightPhase=t*2.8f;
 };
 // Warm the complete rendering/network workload before either timing repeat.
 // A light raster-only startup otherwise leaves this GPU in a slow idle state.
 shading=2;enabled=true;reset=true;pose(0);
 for(unsigned i=0;i<128;++i){render(cases[0],{},false,false);report["frames"].back()["phase"]="device_warmup";}
 report["tradeoff_protocol"]={{"device_warmup_frames",128},{"warmup_frames",16},{"measured_frames",96},{"repeats",2},{"timing_presentation",false},{"timing_capture",false},{"clip_frames",96},{"geometry","same original meshes in all five conditions"}};
 for(unsigned repeat=0;repeat<2;++repeat)for(unsigned k=0;k<5;++k){const auto& condition=conditions[repeat?4-k:k];shading=condition.quality;enabled=condition.neural;reset=true;
  for(unsigned i=0;i<112;++i){pose(i<16?0:i-16);render(cases[0],{},false,false);auto& row=report["frames"].back();row["condition"]=condition.name;row["phase"]="timing";row["repeat"]=repeat;row["measured"]=i>=16;row["sequence_frame"]=i<16?-1:int(i-16);}
 }
 for(const auto& condition:conditions){shading=condition.quality;enabled=condition.neural;reset=true;displayMode=enabled?1:0;displayCaptureOnly=true;
  for(unsigned i=0;i<16;++i){pose(0);render(cases[0],{},false,false);report["frames"].back()["phase"]="clip_warmup";}
  for(unsigned i=0;i<96;++i){pose(i);auto folder=out/"clips"/condition.name/("frame_"+std::to_string(i));
   // Four anchors retain all raw surfaces for independent image/motion checks.
   displayCaptureOnly=!(i==0||i==31||i==63||i==95);render(cases[0],folder,false,true);auto& row=report["frames"].back();row["condition"]=condition.name;row["phase"]="clip";row["sequence_frame"]=i;
  }
 }
 displayCaptureOnly=false;
}
