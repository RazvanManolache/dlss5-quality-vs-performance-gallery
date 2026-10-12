unsigned shading=1,fullSamples=20,detailFlags=0;
int directOverride=-1,giOverride=-1,reflectionOverride=-1,geometryOverride=-1;
unsigned geometryQuality()const{return geometryOverride>=0?unsigned(geometryOverride):shading;}
unsigned directCount()const{return directOverride>=0?unsigned(directOverride):shading==2?fullSamples:shading==1?4:1;}
unsigned giCount()const{return giOverride>=0?unsigned(giOverride):shading==2?16:shading==1?4:0;}
unsigned reflectionCount()const{return reflectionOverride>=0?unsigned(reflectionOverride):shading==2?24:shading==1?8:1;}
void qualityPreset(unsigned q){
 shading=q>=3?0:q;detailFlags=q==3?119:q==4?127:q==5?(127|512|1024):q==6?(127|128|256|1024|16384):0;
 directOverride=giOverride=reflectionOverride=geometryOverride=-1;
 if(q==6){giOverride=1;reflectionOverride=1;}
}
float cameraRadius=7.6f,lightPhase=0;
bool movingLight=true,displayCaptureOnly=false,materialScope=true;float neuralBlend=.85f;
ComPtr<ID3D12Resource> diffuseScene,skinGuide,skinSharp,skinScratch,gAlbedo,gPosition,gPrevious;
ComPtr<ID3D12PipelineState> gbufferPSO,rayPSO;
ComPtr<ID3D12RootSignature> rayRoot;
ComPtr<ID3D12DescriptorHeap> rayHeap;
ComPtr<ID3D12Resource> rayVertices,rayObjects,rayConstants,blas[MeshCount],tlas[3],tlasInstances[3],tlasScratch[3],peopleScratch[MeshCount];
std::vector<ComPtr<ID3D12Resource>> asScratch;
std::unique_ptr<pg::phoenix::direct::Pass> bloomHorizontal,filmVertical;
ComPtr<ID3D12Resource> bloomScratch,filmScene;
std::unique_ptr<pg::phoenix::direct::Pass> skinHorizontal,skinVertical,environmentHorizontal,environmentVertical;
Readback rbDiffuse,rbGuide,rbSharp,rbPosition,rbPrevious,rbLinear,rbSkinResult;
bool captureLighting=false;
struct GalleryObject {XMFLOAT4X4 world,previousWorld,baseWorld;XMFLOAT4 material;unsigned mesh,lowMesh;unsigned animationKind=0;};
float sceneTime=0,previousSceneTime=0;
bool livingAnimation=true,deterministicAnimation=false;
std::chrono::steady_clock::time_point animationClock=std::chrono::steady_clock::now();
#include "skeletal_runtime.inc"
unsigned qualityMesh(const GalleryObject& o,unsigned q){return actorObject(o)?assetMesh(4+o.animationKind-10,people[o.animationKind-10].lod):q==2?o.mesh:q==1&&o.lowMesh==3?5:o.lowMesh;}

struct RayObject {XMFLOAT4X4 world;XMFLOAT4 material;unsigned vertexOffset,kind,pad0=0,pad1=0;};
struct RayParams {XMFLOAT4 eye;unsigned width,height,quality,objectBase;unsigned directSamples,giSamples,reflectionSamples,pad=0;XMFLOAT4 positions[8],colors[8];};
std::vector<GalleryObject> gallery;
Mesh* meshList[MeshCount]{};
unsigned vertexOffsets[MeshCount]{};
const char* shadingName()const{return shading==2?"beauty":shading==1?"middle":"light";}
void add(unsigned m,unsigned low,XMMATRIX world,XMFLOAT4 material){GalleryObject o{};XMStoreFloat4x4(&o.world,world);o.previousWorld=o.baseWorld=o.world;o.material=material;o.mesh=m;o.lowMesh=low;gallery.push_back(o);}
void block(float x,float y,float z,float sx,float sy,float sz,XMFLOAT4 color){add(4,4,XMMatrixScaling(sx,sy,sz)*XMMatrixTranslation(x,y,z),color);}
void exhibit(unsigned asset,float x,float z,float scale=1,float angle=0,bool turn=false){
 XMFLOAT4 dark{.035f,.046f,.058f,2},brass{.42f,.24f,.075f,4};
 float top=asset==2?.12f:.9f;
 block(x,top*.5f,z,1.2f,top,1.1f,dark);block(x,top+.025f,z,1.24f,.05f,1.14f,brass);
 unsigned m=6+asset*2;
 add(m,m+1,XMMatrixScaling(scale,scale,scale)*XMMatrixRotationY(angle)*XMMatrixTranslation(x,top+.05f+.775f*scale,z),{0,0,0,float(6+asset)});
 if(turn)gallery.back().animationKind=1;
}
void makeGallery(){
 meshList[0]=&head[0];meshList[1]=&head[1];meshList[2]=&marble[0];meshList[3]=&marble[1];meshList[4]=&box;meshList[5]=&marbleMedium;
 for(unsigned i=0;i<VarietyCount;++i)for(unsigned q=0;q<(i<4?2u:4u);++q)meshList[assetMesh(i,q)]=&variety[i][q];
 loadRigs();
 XMFLOAT4 stone{.22f,.25f,.29f,2},dark{.024f,.031f,.044f,2},gold{.42f,.24f,.075f,4};
 // All actors share native skeletal, facial and motion-vector animation.
 for(unsigned i=0;i<ActorCount;++i){unsigned m=assetMesh(4+i,0);add(m,m,XMMatrixIdentity(),{0,0,0,float(10+i)});gallery.back().animationKind=10+i;}
 exhibit(0,.85f,0,.95f,-.3f,true);gallery.back().lowMesh=gallery.back().mesh;
 block(0,-.12f,0,24,.18f,24,{.18f,.21f,.26f,3});
 block(0,3,-12,24,6,.3f,stone);block(-12,3,0,.3f,6,24,stone);block(12,3,0,.3f,6,24,stone);block(0,3,12,24,6,.3f,stone);
 // Asymmetric exhibit islands, with a clear 4.1 m camera aisle.
 const float layouts[14][5]={
  {1,-3.3f,-5.8f,.9f,.2f},{3,3.4f,-5.7f,.75f,-.45f},{2,-6.5f,-4.7f,1.2f,.5f},
  {0,6.6f,-4.6f,1.2f,-.7f},{1,-8.4f,-8.7f,1.1f,.4f},{2,-3.5f,-9.5f,1.35f,0},
  {3,1.7f,-9.3f,.8f,-.2f},{0,7.3f,-9.4f,.85f,.4f},{1,8.1f,.5f,.75f,-.6f},
  {2,-7.5f,1.7f,1.1f,-1},{3,-6.9f,6.7f,.9f,.6f},{0,6.6f,5.6f,1.15f,-1.4f},
  {1,-2.3f,9.1f,.8f,.1f},{2,3.5f,9.5f,1.2f,2.8f}};
 for(unsigned i=0;i<14;++i){const auto& p=layouts[i];exhibit(unsigned(p[0]),p[1],p[2],p[3],p[4],i==0||i==3||i==8);}
 // A small retained sculpture collection, rather than a wall of identical busts.
 for(int side:{-1,1})for(int k=0;k<3;++k){float x=side*(5.2f+1.5f*k),z=-7.5f;block(x,.45f,z,.9f,.9f,.9f,dark);add(2,3,XMMatrixScaling(.8f,.8f,.8f)*XMMatrixRotationY(-side*.4f)*XMMatrixTranslation(x,1.55f,z),{0,0,0,1});}
 for(int k=-5;k<=5;++k){float x=k*2.f;block(x,2.6f,-10.8f,.3f,5.3f,.3f,gold);block(x,5.5f,0,.16f,.18f,24,dark);}
 for(int side:{-1,1})for(int k=-5;k<=5;++k){float z=k*2.f;block(side*10.5f,2.6f,z,.4f,5.3f,.4f,stone);block(side*10.5f,.25f,z,.75f,.5f,.75f,dark);}
 for(int k=-4;k<=4;++k){float x=k*2.5f;block(x,3.2f,-11.8f,1.9f,2.7f,.12f,gold);block(x,3.2f,-11.7f,1.78f,2.58f,.14f,k%2?XMFLOAT4{.055f,.018f,.013f,2}:XMFLOAT4{.009f,.035f,.065f,2});}
 // Suspended kinetic bars behind the aisle, with independent slow sway.
 for(unsigned k=0;k<5;++k){block((int(k)-2)*.65f,3.05f+k*.3f,-6.2f,1.4f,.09f,.15f,k%2?gold:XMFLOAT4{.05f,.23f,.25f,4});gallery.back().animationKind=2;}
 // Brass floor inlays, display labels and seating make the wider room legible.
 for(int k=-4;k<=4;++k){float v=k*2.4f;block(v,-.019f,0,.018f,.008f,23.4f,gold);block(0,-.018f,v,23.4f,.008f,.018f,gold);}
 for(const auto& p:layouts){float x=p[1],z=p[2];block(x,.62f,z+.563f,.56f,.19f,.018f,{.34f,.39f,.43f,4});block(x,.62f,z+.575f,.43f,.018f,.006f,dark);}
 for(int side:{-1,1})for(float z:{3.6f,8.f}){
  float x=side*9.f;block(x,.5f,z,.85f,.16f,2.3f,{.095f,.043f,.019f,2});
  block(x+side*.32f,.87f,z,.13f,.65f,2.3f,dark);
  for(float end:{-.85f,.85f})block(x,.24f,z+end,.65f,.48f,.13f,gold);
 }
 // Abstract wall reliefs give the side aisles different silhouettes and materials.
 for(int side:{-1,1})for(unsigned k=0;k<7;++k){
  float z=-8.5f+k*2.8f;block(side*11.72f,2.8f,z,.13f,2.f,1.7f,dark);
  for(unsigned j=0;j<3;++j){block(side*11.61f,2.2f+j*.5f,z+(int(j)-1)*.32f,.12f,.18f,.95f,k%2?gold:XMFLOAT4{.035f,.18f,.22f,4});}
 }
 // A second suspended mobile, above the crowd rather than across the focus aisle.
 for(unsigned k=0;k<7;++k){block(4.8f+(int(k)-3)*.45f,3.5f+k*.15f,2.8f,.8f,.065f,.12f,k%2?gold:XMFLOAT4{.13f,.035f,.018f,4});gallery.back().animationKind=2;}
 // Recessed visible luminaires; shadow rays ignore their emitter surfaces.
 const XMFLOAT4 lamps[8]={{-2.7f,4.2f,3.5f,.7f},{3.2f,3.6f,2,.8f},{-5,4.8f,-4,.5f},{5,4.8f,-4,.65f},{-4.5f,5,-8,.8f},{4.5f,5,-8,.8f},{-4.6f,3.5f,9.5f,.6f},{6,3.8f,9.5f,.6f}};
 for(unsigned k=0;k<8;++k){auto p=lamps[k];block(p.x,p.y+.04f,p.z,2*p.w,.08f,2*p.w,dark);block(p.x,p.y-.008f,p.z,1.8f*p.w,.015f,1.8f*p.w,k%2?XMFLOAT4{.7f,1.25f,2.3f,5}:XMFLOAT4{2.1f,1.25f,.5f,5});}
 // Low natural-history plinths for the animated Rocketbox animal exhibits.
 for(float x:{-5.3f,5.3f})block(x,.12f,8.8f,2.2f,.24f,1.5f,dark);
 // Small props follow the authored hand bones, not a fixed world-space offset.
 for(unsigned person:{0u,2u,7u}){block(0,0,0,person==0?.12f:.065f,person==0?.07f:.12f,person==0?.045f:.012f,dark);gallery.back().animationKind=100+person;if(person==0){block(0,0,0,.045f,.045f,.06f,gold);gallery.back().animationKind=200+person;}}
 ensure(gallery.size()<512,"Gallery constant slots");
}
void buildAcceleration(){
 ComPtr<ID3D12Device5> d;ComPtr<ID3D12GraphicsCommandList4> list;hr(device.As(&d));hr(cmd.As(&list));D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};hr(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&options,sizeof(options)));ensure(options.RaytracingTier>=D3D12_RAYTRACING_TIER_1_1,"DXR 1.1 required");
 std::vector<Vertex> pool;
 for(unsigned i=0;i<MeshCount;++i){vertexOffsets[i]=unsigned(pool.size());void* data;D3D12_RANGE range{0,meshList[i]->count*sizeof(Vertex)};hr(meshList[i]->data->Map(0,&range,&data));auto* p=(Vertex*)data;pool.insert(pool.end(),p,p+meshList[i]->count);D3D12_RANGE empty{};meshList[i]->data->Unmap(0,&empty);}
 const auto vertexBytes=pool.size()*sizeof(Vertex);auto initial=phoenix_native::buffer(device.Get(),vertexBytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* mapped;D3D12_RANGE empty{};hr(initial->Map(0,&empty,&mapped));memcpy(mapped,pool.data(),vertexBytes);initial->Unmap(0,nullptr);uploads.push_back(initial);
 rayVertices=phoenix_native::buffer(device.Get(),vertexBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);previousVertices=phoenix_native::buffer(device.Get(),vertexBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
 constexpr auto vertexState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
 for(auto* target:{rayVertices.Get(),previousVertices.Get()}){cmd->CopyBufferRegion(target,0,initial.Get(),0,vertexBytes);phoenix_native::transition(cmd.Get(),target,D3D12_RESOURCE_STATE_COPY_DEST,vertexState);}
 for(unsigned i=0;i<MeshCount;++i)meshList[i]->view.BufferLocation=rayVertices->GetGPUVirtualAddress()+vertexOffsets[i]*sizeof(Vertex);
 auto build=[&](D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input,ComPtr<ID3D12Resource>& output){D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};d->GetRaytracingAccelerationStructurePrebuildInfo(&input,&info);ensure(info.ResultDataMaxSizeInBytes>0,"Raytracing prebuild");output=phoenix_native::buffer(device.Get(),info.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);auto scratch=phoenix_native::buffer(device.Get(),info.ScratchDataSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);asScratch.push_back(scratch);D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc{};desc.Inputs=input;desc.DestAccelerationStructureData=output->GetGPUVirtualAddress();desc.ScratchAccelerationStructureData=scratch->GetGPUVirtualAddress();list->BuildRaytracingAccelerationStructure(&desc,0,nullptr);D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;barrier.UAV.pResource=output.Get();cmd->ResourceBarrier(1,&barrier);};
 for(unsigned i=0;i<MeshCount;++i){D3D12_RAYTRACING_GEOMETRY_DESC g{};g.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;g.Flags=i>=14?D3D12_RAYTRACING_GEOMETRY_FLAG_NONE:D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;g.Triangles.VertexBuffer={rayVertices->GetGPUVirtualAddress()+vertexOffsets[i]*sizeof(Vertex),sizeof(Vertex)};g.Triangles.VertexCount=meshList[i]->count;g.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input{};input.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;input.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE|(i>=14?D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE:D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE);input.NumDescs=1;input.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;input.pGeometryDescs=&g;build(input,blas[i]);if(i>=14){D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};d->GetRaytracingAccelerationStructurePrebuildInfo(&input,&info);peopleScratch[i]=phoenix_native::buffer(device.Get(),info.UpdateScratchDataSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);}}
 std::vector<RayObject> objects;
 for(unsigned quality=0;quality<3;++quality){std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;for(unsigned i=0;i<gallery.size();++i){const auto& o=gallery[i];unsigned m=qualityMesh(o,quality);D3D12_RAYTRACING_INSTANCE_DESC instance{};XMFLOAT4X4 trans;XMStoreFloat4x4(&trans,XMMatrixTranspose(XMLoadFloat4x4(&o.world)));memcpy(instance.Transform,&trans,48);instance.InstanceID=i;instance.InstanceMask=o.material.w==5?2:1;instance.Flags=D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;instance.AccelerationStructure=blas[m]->GetGPUVirtualAddress();instances.push_back(instance);objects.push_back({o.world,o.material,vertexOffsets[m],unsigned(o.material.w),0,0});}
  auto upload=phoenix_native::buffer(device.Get(),instances.size()*sizeof(instances[0]),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);hr(upload->Map(0,&empty,&mapped));memcpy(mapped,instances.data(),instances.size()*sizeof(instances[0]));upload->Unmap(0,nullptr);tlasInstances[quality]=upload;D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input{};input.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;input.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE|D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;input.NumDescs=UINT(instances.size());input.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;input.InstanceDescs=upload->GetGPUVirtualAddress();build(input,tlas[quality]);D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};d->GetRaytracingAccelerationStructurePrebuildInfo(&input,&info);tlasScratch[quality]=phoenix_native::buffer(device.Get(),info.UpdateScratchDataSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);}
 rayObjects=phoenix_native::buffer(device.Get(),objects.size()*sizeof(RayObject),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);hr(rayObjects->Map(0,&empty,&mapped));memcpy(mapped,objects.data(),objects.size()*sizeof(RayObject));rayObjects->Unmap(0,nullptr);submit();asScratch.clear();begin();
}
#include "visitor_scene.inc"
void initLighting(const std::string& source,D3D12_GRAPHICS_PIPELINE_STATE_DESC pd){
 makeGallery();ensure(gallery.size()<512,"Gallery constant buffer capacity");buildAcceleration();auto extra=bytes(root/"gbuffer.hlsl");auto vs=shader(source,"vs","vs_5_1"),ps=shader(source+std::string(extra.begin(),extra.end()),"gbuffer","ps_5_1");pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pd.NumRenderTargets=5;pd.RTVFormats[4]=DXGI_FORMAT_R32G32B32A32_FLOAT;pd.BlendState.RenderTarget[4].RenderTargetWriteMask=15;pd.RTVFormats[2]=DXGI_FORMAT_R16G16B16A16_FLOAT;pd.RTVFormats[3]=DXGI_FORMAT_R32G32B32A32_FLOAT;pd.BlendState.RenderTarget[2].RenderTargetWriteMask=pd.BlendState.RenderTarget[3].RenderTargetWriteMask=15;hr(device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&gbufferPSO)));
 lightingTargets();
 D3D12_DESCRIPTOR_RANGE sr{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,RaySRVCount,1,0,0},ur{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,2,0,0,0};D3D12_ROOT_PARAMETER rp[4]{};rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;rp[0].Descriptor.ShaderRegister=0;rp[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;rp[1].Descriptor.ShaderRegister=0;rp[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rp[2].DescriptorTable={1,&sr};rp[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rp[3].DescriptorTable={1,&ur};D3D12_STATIC_SAMPLER_DESC samp{};samp.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;samp.AddressU=samp.AddressV=samp.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;samp.MaxLOD=D3D12_FLOAT32_MAX;samp.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;D3D12_ROOT_SIGNATURE_DESC rd{4,rp,1,&samp,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> rs,err;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&rs,&err));hr(device->CreateRootSignature(0,rs->GetBufferPointer(),rs->GetBufferSize(),IID_PPV_ARGS(&rayRoot)));
 wchar_t exe[MAX_PATH];GetModuleFileNameW(nullptr,exe,MAX_PATH);auto dxil=bytes(fs::path(exe).parent_path()/"ray_lighting.dxil");D3D12_COMPUTE_PIPELINE_STATE_DESC cd{};cd.pRootSignature=rayRoot.Get();cd.CS={dxil.data(),dxil.size()};hr(device->CreateComputePipelineState(&cd,IID_PPV_ARGS(&rayPSO)));D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,RaySRVCount+4,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rayHeap)));auto cpu=rayHeap->GetCPUDescriptorHandleForHeapStart();
 for(auto* resource:{gAlbedo.Get(),skinGuide.Get(),gPosition.Get()}){D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=resource->GetDesc().Format;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=resource->GetDesc().MipLevels;device->CreateShaderResourceView(resource,&sd,cpu);cpu.ptr+=srvStride;}
 D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=DXGI_FORMAT_R32_TYPELESS;sd.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Buffer.NumElements=UINT(rayVertices->GetDesc().Width/4);sd.Buffer.Flags=D3D12_BUFFER_SRV_FLAG_RAW;device->CreateShaderResourceView(rayVertices.Get(),&sd,cpu);cpu.ptr+=srvStride;sd.Format=DXGI_FORMAT_UNKNOWN;sd.Buffer.NumElements=UINT(gallery.size()*3);sd.Buffer.Flags=D3D12_BUFFER_SRV_FLAG_NONE;sd.Buffer.StructureByteStride=sizeof(RayObject);device->CreateShaderResourceView(rayObjects.Get(),&sd,cpu);cpu.ptr+=srvStride;
 for(unsigned kind=0;kind<2;++kind){auto* resource=textures[colorHigh[kind]].Get();sd={};sd.Format=resource->GetDesc().Format;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=resource->GetDesc().MipLevels;device->CreateShaderResourceView(resource,&sd,cpu);cpu.ptr+=srvStride;}
 for(unsigned asset=0;asset<VarietyCount;++asset){auto* resource=textures[varietyTexture[asset][0]].Get();sd={};sd.Format=resource->GetDesc().Format;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=resource->GetDesc().MipLevels;device->CreateShaderResourceView(resource,&sd,cpu);cpu.ptr+=srvStride;}
 for(auto* resource:{diffuseScene.Get(),skinSharp.Get(),scene.Get(),skinSharp.Get()}){D3D12_UNORDERED_ACCESS_VIEW_DESC ud{};ud.Format=resource->GetDesc().Format;ud.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;device->CreateUnorderedAccessView(resource,nullptr,&ud,cpu);cpu.ptr+=srvStride;}rayConstants=phoenix_native::buffer(device.Get(),512,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);submit();begin();
}
void skinDiffusion(){
 using phoenix_native::transition;pg::phoenix::direct::Constants c{W,H,0,0,0,0,.5f,1,0};transition(cmd.Get(),skinScratch.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);skinHorizontal->record(cmd.Get(),{diffuseScene.Get(),skinGuide.Get(),skinSharp.Get()},skinScratch.Get(),c);transition(cmd.Get(),skinScratch.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);skinVertical->record(cmd.Get(),{skinScratch.Get(),skinGuide.Get(),skinSharp.Get()},scene.Get(),c);transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
void environmentDenoise(){
 using phoenix_native::transition;pg::phoenix::direct::Constants c{W,H,detailFlags,shading,0,0,.5f,1,0};transition(cmd.Get(),skinScratch.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);environmentHorizontal->record(cmd.Get(),{scene.Get(),gPosition.Get(),skinGuide.Get()},skinScratch.Get(),c);transition(cmd.Get(),skinScratch.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);environmentVertical->record(cmd.Get(),{skinScratch.Get(),gPosition.Get(),skinGuide.Get()},scene.Get(),c);transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
void finishLighting(){
 using phoenix_native::transition;pg::phoenix::direct::Constants c{W,H,0,0,0,0,.5f,1,0};
 transition(cmd.Get(),bloomScratch.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);bloomHorizontal->record(cmd.Get(),{scene.Get(),nullptr,nullptr},bloomScratch.Get(),c);transition(cmd.Get(),bloomScratch.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 transition(cmd.Get(),filmScene.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);filmVertical->record(cmd.Get(),{scene.Get(),bloomScratch.Get(),nullptr},filmScene.Get(),c);transition(cmd.Get(),filmScene.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 // Keep the scene resource stable for its raster, ray and display descriptors.
 transition(cmd.Get(),filmScene.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);cmd->CopyResource(scene.Get(),filmScene.Get());transition(cmd.Get(),filmScene.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}
void renderGallery(XMMATRIX vp,XMVECTOR eye,bool cold){
 animateGallery(cold);refitAcceleration();
 using phoenix_native::transition;ID3D12DescriptorHeap* heaps[]{srv.Get()};cmd->SetDescriptorHeaps(1,heaps);cmd->SetGraphicsRootSignature(sceneRoot.Get());cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);cmd->SetPipelineState(gbufferPSO.Get());for(auto* r:{gAlbedo.Get(),motion.Get(),skinGuide.Get(),gPosition.Get(),gPrevious.Get()})transition(cmd.Get(),r,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);D3D12_CPU_DESCRIPTOR_HANDLE targets[]{rt(7),rt(3),rt(5),rt(8),rt(9)};float zero[4]{};for(auto t:targets)cmd->ClearRenderTargetView(t,zero,0,nullptr);auto dh=dsv->GetCPUDescriptorHandleForHeapStart();cmd->ClearDepthStencilView(dh,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);cmd->OMSetRenderTargets(5,targets,FALSE,&dh);D3D12_VIEWPORT viewport{0,0,float(W),float(H),0,1};D3D12_RECT scissor{0,0,LONG(W),LONG(H)};cmd->RSSetViewports(1,&viewport);cmd->RSSetScissorRects(1,&scissor);
 void* mapped;D3D12_RANGE empty{};hr(constants->Map(0,&empty,&mapped));unsigned drawCount=0;for(const auto& o:gallery){CBuffer cb{};cb.world=o.world;XMStoreFloat4x4(&cb.vp,vp);XMStoreFloat4x4(&cb.prev,XMLoadFloat4x4(&o.previousWorld)*prevVP);cb.lightProjection=o.previousWorld;XMStoreFloat4(&cb.eye,eye);cb.material=o.material;cb.options={1,0,1,1};cb.experiment={float(shading),float(W),float(H),float(detailFlags)};memcpy((char*)mapped+drawCount*512,&cb,sizeof(cb));cmd->SetGraphicsRootConstantBufferView(0,constants->GetGPUVirtualAddress()+drawCount++*512);cmd->SetGraphicsRootDescriptorTable(1,view(o.material.w>=6?64+(unsigned(o.material.w)-6)*3:o.material.w==1?18:12));auto& m=*meshList[qualityMesh(o,geometryQuality())];D3D12_VERTEX_BUFFER_VIEW previousView=m.view;if(actorObject(o))previousView.BufferLocation=previousVertices->GetGPUVirtualAddress()+vertexOffsets[qualityMesh(o,geometryQuality())]*sizeof(Vertex);D3D12_VERTEX_BUFFER_VIEW views[2]={m.view,previousView};cmd->IASetVertexBuffers(0,2,views);cmd->DrawInstanced(m.count,1,0,0);}constants->Unmap(0,nullptr);for(auto* r:{gAlbedo.Get(),motion.Get(),skinGuide.Get(),gPosition.Get(),gPrevious.Get()})transition(cmd.Get(),r,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cmd->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,6);
 RayParams params{};XMStoreFloat4(&params.eye,eye);params.width=W;params.height=H;params.quality=shading;params.objectBase=UINT(gallery.size())*geometryQuality();params.directSamples=directCount();params.giSamples=giCount();params.reflectionSamples=reflectionCount();params.pad=detailFlags;
 const XMFLOAT4 pos[8]={{-2.7f,4.2f,3.5f,.7f},{3.2f,3.6f,2,.8f},{-5,4.8f,-4,.5f},{5,4.8f,-4,.65f},{-4.5f,5,-8,.8f},{4.5f,5,-8,.8f},{-4.6f,3.5f,9.5f,.6f},{6,3.8f,9.5f,.6f}};
 const XMFLOAT4 color[8]={{23,17,11,0},{9,13,19,0},{18,10,5,0},{7,12,15,0},{13,9,6,0},{8,8,13,0},{22,27,32,0},{30,22,15,0}};
 for(unsigned i=0;i<8;++i){params.positions[i]=pos[i];params.colors[i]=color[i];float pulse=1+.07f*sinf(lightPhase*1.3f+i*.9f);params.colors[i].x*=pulse;params.colors[i].y*=pulse;params.colors[i].z*=pulse;}
 hr(rayConstants->Map(0,&empty,&mapped));memcpy(mapped,&params,sizeof(params));rayConstants->Unmap(0,nullptr);
 auto* target=shading==2?diffuseScene.Get():scene.Get();transition(cmd.Get(),target,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);transition(cmd.Get(),skinSharp.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);ID3D12DescriptorHeap* rayHeaps[]{rayHeap.Get()};cmd->SetDescriptorHeaps(1,rayHeaps);cmd->SetComputeRootSignature(rayRoot.Get());cmd->SetPipelineState(rayPSO.Get());cmd->SetComputeRootConstantBufferView(0,rayConstants->GetGPUVirtualAddress());cmd->SetComputeRootShaderResourceView(1,tlas[geometryQuality()]->GetGPUVirtualAddress());auto gpu=rayHeap->GetGPUDescriptorHandleForHeapStart();cmd->SetComputeRootDescriptorTable(2,gpu);gpu.ptr+=(RaySRVCount+(shading==2?0:2))*srvStride;cmd->SetComputeRootDescriptorTable(3,gpu);cmd->Dispatch((W+7)/8,(H+7)/8,1);transition(cmd.Get(),target,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(cmd.Get(),skinSharp.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cmd->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,7);if(shading==2){if(detailFlags&8192){transition(cmd.Get(),diffuseScene.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);cmd->CopyResource(scene.Get(),diffuseScene.Get());transition(cmd.Get(),diffuseScene.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);transition(cmd.Get(),scene.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}else skinDiffusion();if(captureLighting)capture(cmd.Get(),scene.Get(),rbSkinResult,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}environmentDenoise();if(captureLighting)capture(cmd.Get(),scene.Get(),rbLinear,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);finishLighting();cmd->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,4);
}
void tradeoff(const fs::path& out){
 struct Condition{const char* name;unsigned quality;bool neural;};const Condition conditions[]={{"reference",2,false},{"balanced",1,false},{"balanced_neural",1,true},{"cheap",0,false},{"cheap_neural",0,true}};auto pose=[&](unsigned frame){float t=float(frame)/95;yaw=-.65f+1.35f*sinf(t*XM_PI);cameraRadius=7.6f-.45f*sinf(t*XM_PI);lightPhase=t*2.8f;sceneTime=float(frame)/24;};
 shading=2;enabled=true;reset=true;pose(0);for(unsigned i=0;i<128;++i){render(cases[0],{},false,false);report["frames"].back()["phase"]="device_warmup";}report["tradeoff_protocol"]={{"device_warmup_frames",128},{"warmup_frames",16},{"measured_frames",96},{"repeats",2},{"timing_presentation",false},{"timing_capture",false},{"clip_frames",96},{"geometry","Full foreground horse and twelve animated human visitors; varied distant props use reduced geometry in cheap/balanced"},{"full_direct_samples",fullSamples}};
 for(unsigned repeat=0;repeat<2;++repeat)for(unsigned k=0;k<5;++k){const auto& condition=conditions[repeat?4-k:k];shading=condition.quality;enabled=condition.neural;reset=true;for(unsigned i=0;i<112;++i){pose(i<16?0:i-16);render(cases[0],{},false,false);auto& row=report["frames"].back();row["condition"]=condition.name;row["phase"]="timing";row["repeat"]=repeat;row["measured"]=i>=16;row["sequence_frame"]=i<16?-1:int(i-16);}}
 for(const auto& condition:conditions){shading=condition.quality;enabled=condition.neural;reset=true;displayMode=enabled?1:0;displayCaptureOnly=true;for(unsigned i=0;i<16;++i){pose(0);render(cases[0],{},false,false);report["frames"].back()["phase"]="clip_warmup";}for(unsigned i=0;i<96;++i){pose(i);displayCaptureOnly=!(i==0||i==31||i==63||i==95);render(cases[0],out/"clips"/condition.name/("frame_"+std::to_string(i)),false,true);auto& row=report["frames"].back();row["condition"]=condition.name;row["phase"]="clip";row["sequence_frame"]=i;}}displayCaptureOnly=false;
}

#include "beauty_compare.inc"
