// Window events only queue a new extent. GPU ownership changes on the render thread.
unsigned W=BenchmarkWidth,H=BenchmarkHeight,NW=BenchmarkWidth,NH=BenchmarkHeight;
unsigned activeNW=BenchmarkWidth,activeNH=BenchmarkHeight,offsetX=0,offsetY=0,pendingWidth=0,pendingHeight=0,resolutionGeneration=0,networkGeneration=0;
bool fixedResolution=true,minimized=false;
std::unique_ptr<phoenix_bundle::Bundle> model;
std::vector<phoenix_kernel::Binary> modelKernels;
std::vector<uint8_t> modelWeights;

void screenPasses(){
 auto load=[&](const char* name){auto b=bytes(root/name);return std::string(b.begin(),b.end());};
 const auto skin=load("skin.hlsl"),filter=load("environment_filter.hlsl"),finish=load("finish_lighting.hlsl");
 auto pass=[&](const std::string& source){return std::make_unique<pg::phoenix::direct::Pass>(device.Get(),source.c_str());};
 skinHorizontal=pass("#define HORIZONTAL 1\n"+skin);skinVertical=pass("#define HORIZONTAL 0\n"+skin);
 environmentHorizontal=pass("#define HORIZONTAL 1\n"+filter);environmentVertical=pass("#define HORIZONTAL 0\n"+filter);
 bloomHorizontal=pass("#define HORIZONTAL 1\n"+finish);filmVertical=pass("#define HORIZONTAL 0\n"+finish);
 encode=pass(pg::phoenix::direct::bridge_encode_source());
 decode=pass(std::string(pg::phoenix::direct::bridge_common)+load("window_decode.hlsl"));
}
void sceneTargets(){
 scene=image(device.Get(),W,H,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET|D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 motion=image(device.Get(),W,H,DXGI_FORMAT_R16G16_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 depth=image(device.Get(),W,H,DXGI_FORMAT_D32_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,D3D12_RESOURCE_STATE_DEPTH_WRITE);
 enhanced=phoenix_native::image(device.Get(),W,H,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,true);
 device->CreateRenderTargetView(scene.Get(),nullptr,rt(2));device->CreateRenderTargetView(motion.Get(),nullptr,rt(3));device->CreateDepthStencilView(depth.Get(),nullptr,dsv->GetCPUDescriptorHandleForHeapStart());srvView(scene.Get(),32);srvView(enhanced.Get(),33);
 rbInput=readback(device.Get(),scene.Get(),8);rbOutput=readback(device.Get(),enhanced.Get(),8);rbMotion=readback(device.Get(),motion.Get(),4);rbDisplay=readback(device.Get(),back[0].Get(),4);
}
void lightingTargets(){
 auto floating=[&](){return phoenix_native::image(device.Get(),W,H,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);};
 auto renderTarget=[&](DXGI_FORMAT format){return image(device.Get(),W,H,format,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);};
 gAlbedo=renderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT);gPosition=renderTarget(DXGI_FORMAT_R32G32B32A32_FLOAT);gPrevious=renderTarget(DXGI_FORMAT_R32G32B32A32_FLOAT);skinGuide=renderTarget(DXGI_FORMAT_R16G16B16A16_FLOAT);
 diffuseScene=floating();skinSharp=floating();skinScratch=floating();bloomScratch=floating();filmScene=floating();
 device->CreateRenderTargetView(gAlbedo.Get(),nullptr,rt(7));device->CreateRenderTargetView(gPosition.Get(),nullptr,rt(8));device->CreateRenderTargetView(gPrevious.Get(),nullptr,rt(9));device->CreateRenderTargetView(skinGuide.Get(),nullptr,rt(5));
 rbPrevious=readback(device.Get(),gPrevious.Get(),16);rbPosition=readback(device.Get(),gPosition.Get(),16);rbDiffuse=readback(device.Get(),diffuseScene.Get(),8);rbGuide=readback(device.Get(),skinGuide.Get(),8);rbSharp=readback(device.Get(),skinSharp.Get(),8);rbLinear=readback(device.Get(),scene.Get(),8);rbSkinResult=readback(device.Get(),scene.Get(),8);
}
void refreshRayTargets(){
 auto cpu=rayHeap->GetCPUDescriptorHandleForHeapStart();
 for(auto* resource:{gAlbedo.Get(),skinGuide.Get(),gPosition.Get()}){D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=resource->GetDesc().Format;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;device->CreateShaderResourceView(resource,&sd,cpu);cpu.ptr+=srvStride;}
 cpu=rayHeap->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=RaySRVCount*srvStride;
 for(auto* resource:{diffuseScene.Get(),skinSharp.Get(),scene.Get(),skinSharp.Get()}){D3D12_UNORDERED_ACCESS_VIEW_DESC ud{};ud.Format=resource->GetDesc().Format;ud.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;device->CreateUnorderedAccessView(resource,nullptr,&ud,cpu);cpu.ptr+=srvStride;}
}
void selectNetwork(){
 const auto& plan=model->select_frame(W,H,[&](const Json& candidate){phoenix_native::preflight(candidate,modelKernels);});
 if(network&&W==NW&&H==NH)return;
 network.reset();NW=W;NH=H;activeNW=W;activeNH=H;offsetX=offsetY=0;
 normalized=phoenix_native::image(device.Get(),W,H,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true);
 network=std::make_unique<phoenix_native::Network>(device.Get(),dispatch.api(),plan,modelWeights,modelKernels,normalized.Get(),motion.Get());++networkGeneration;
 rbRaw=readback(device.Get(),normalized.Get(),8);
}
void resizeTargets(unsigned width,unsigned height){
 ensure(width&&height,"Nonzero render extent required");if(width==W&&height==H)return;
 hr(queue->Signal(fence.Get(),++fenceValue));hr(fence->SetEventOnCompletion(fenceValue,event));ensure(WaitForSingleObject(event,30000)==WAIT_OBJECT_0,"Resize fence timeout");
 begin();hr(cmd->Close());for(auto& buffer:back)buffer.Reset();
 W=width;H=height;hr(swap->ResizeBuffers(2,W,H,DXGI_FORMAT_UNKNOWN,0));for(unsigned i=0;i<2;++i){hr(swap->GetBuffer(i,IID_PPV_ARGS(&back[i])));device->CreateRenderTargetView(back[i].Get(),nullptr,rt(i));}
 // Release screen-sized memory before replacements; meshes, acceleration structures,
 // weights and the network are retained when their processing extent is unchanged.
 scene.Reset();motion.Reset();depth.Reset();enhanced.Reset();gAlbedo.Reset();gPosition.Reset();gPrevious.Reset();skinGuide.Reset();diffuseScene.Reset();skinSharp.Reset();skinScratch.Reset();bloomScratch.Reset();filmScene.Reset();
 rbInput={};rbOutput={};rbMotion={};rbDisplay={};rbPrevious={};rbPosition={};rbDiffuse={};rbGuide={};rbSharp={};rbLinear={};rbSkinResult={};
 sceneTargets();lightingTargets();refreshRayTargets();selectNetwork();screenPasses();reset=true;animationClock=std::chrono::steady_clock::now();++resolutionGeneration;
 report["resize_events"].push_back({{"width",W},{"height",H},{"neural_width",NW},{"neural_height",NH},{"generation",resolutionGeneration},{"network_generation",networkGeneration}});
}
bool updateWindow(bool wait=false){
 MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
 if(!running)return false;
 if(minimized){animationClock=std::chrono::steady_clock::now();if(wait)WaitMessage();return false;}
 if(!fixedResolution&&pendingWidth&&pendingHeight){const unsigned w=pendingWidth,h=pendingHeight;pendingWidth=pendingHeight=0;resizeTargets(w,h);}
 return true;
}
void clientSize(unsigned width,unsigned height){
 RECT rect{0,0,LONG(width),LONG(height)};ensure(AdjustWindowRectExForDpi(&rect,DWORD(GetWindowLongPtrW(window,GWL_STYLE)),FALSE,DWORD(GetWindowLongPtrW(window,GWL_EXSTYLE)),GetDpiForWindow(window)),"Window extent adjustment");
 ensure(SetWindowPos(window,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE),"Window resize");ensure(updateWindow(),"Window must be drawable");RECT actual{};GetClientRect(window,&actual);if(unsigned(actual.right)!=width||unsigned(actual.bottom)!=height||W!=width||H!=height){std::cerr<<"Requested "<<width<<"x"<<height<<", client "<<actual.right<<"x"<<actual.bottom<<", render "<<W<<"x"<<H<<std::endl;throw std::runtime_error("Window/render sizes must match");}
}
