// Deformed triangles feed rasterization, previous-frame motion and DXR.
struct VisitorState {float x,z,yaw,walk,phase;unsigned exhibit;};
VisitorState visitorState(unsigned person,float time){
 struct Stop {float x,z,lookX,lookZ,wait;};
 const Stop routes[][3]={
  {{-1.35f,.65f,.85f,0,4},{-2.15f,-3.75f,-3.3f,-5.8f,4},{-2.6f,-.4f,.85f,0,3}},
  {{2.15f,-.8f,.85f,0,5},{2.9f,-3.5f,3.4f,-5.7f,5},{2.75f,.8f,.85f,0,3}},
  {{-.9f,-2.5f,.85f,0,2.5f},{.4f,-3.4f,1.7f,-9.3f,3.5f},{-.6f,-4.5f,-3.3f,-5.8f,4}},
  {{-4.7f,-4.1f,-6.5f,-4.7f,7},{-4.7f,-2.7f,-3.3f,-5.8f,5},{-5.1f,-1.7f,-7.5f,1.7f,5}},
  {{-4.7f,.1f,-4.7f,1.6f,12},{-4.2f,-1.8f,-3.3f,-5.8f,5},{-3.5f,.2f,.85f,0,5}},
  {{-4.7f,1.6f,-4.7f,.1f,13},{-5.4f,3.1f,-7.5f,1.7f,6},{-4.8f,3.9f,-6.9f,6.7f,5}},
  {{-.8f,-6.3f,-3.3f,-5.8f,7},{.7f,-7.5f,1.7f,-9.3f,6},{1.4f,-5.5f,3.4f,-5.7f,5}},
  {{5,-3,6.6f,-4.6f,6},{5,-.5f,8.1f,.5f,6},{5,1.9f,6.6f,5.6f,5}},
  {{4.2f,-2.2f,4.2f,-.9f,13},{3.6f,-4.5f,3.4f,-5.7f,6},{3.8f,-1.9f,.85f,0,4}},
  {{4.2f,-.9f,4.2f,-2.2f,13},{6,-1.3f,8.1f,.5f,6},{5.6f,2.7f,6.6f,5.6f,5}},
  {{-1.5f,3.3f,.85f,0,5},{-3.3f,5,-6.9f,6.7f,5},{-3.9f,3.1f,-7.5f,1.7f,5}},
  {{2.3f,3.8f,6.6f,5.6f,6},{.5f,5.6f,-2.3f,9.1f,5},{1.2f,3.8f,.85f,0,5}}};
 ensure(person<ActorCount,"Actor route range");
 if(person==3)return {-8.9f,3.6f,XM_PIDIV2,0,0,0};
 if(person==6)return {8.9f,8.f,-XM_PIDIV2,0,0,0};
 if(person==12)return {-4.6f,-6.1f,1.2f,0,0,0};
 if(person==13)return {7.4f,3.1f,-1.7f,0,0,0};
 if(person==14||person==15){auto family=visitorState(person==14?10:11,time);family.x+=person==14?-.9f:.9f;family.z+=.25f;return family;}
 if(person>=VisitorCount)return {person==VisitorCount?-5.3f:5.3f,8.8f,person==VisitorCount?-.5f:2.5f,0,0,0};
 static_assert(std::size(routes)==12,"Twelve circulating adult routes");
 auto smooth=[](float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);};
 auto turn=[](float a,float b,float t){return a+remainderf(b-a,XM_2PI)*t;};
 auto& r=routes[person];float durations[3],cycle=0,totalDistance=0;
 for(unsigned i=0;i<3;++i){auto& a=r[i];auto& b=r[(i+1)%3];durations[i]=hypotf(b.x-a.x,b.z-a.z)/.65f+1;cycle+=a.wait+durations[i];totalDistance+=hypotf(b.x-a.x,b.z-a.z);}
 float clock=fmodf(std::max(time,0.f),cycle),distanceWalked=floorf(std::max(time,0.f)/cycle)*totalDistance;VisitorState out{};
 for(unsigned i=0;i<3;++i){auto& a=r[i];auto& b=r[(i+1)%3];float dx=b.x-a.x,dz=b.z-a.z,dist=hypotf(dx,dz),looking=atan2f(a.lookX-a.x,a.lookZ-a.z),heading=atan2f(dx,dz);
  if(clock<a.wait){float incoming=atan2f(a.x-r[(i+2)%3].x,a.z-r[(i+2)%3].z);out={a.x,a.z,turn(incoming,looking,smooth(clock/.7f)),0,distanceWalked,i};break;}
  clock-=a.wait;
  if(clock<durations[i]){float t=clock/durations[i],s=smooth(t),speed=dist*6*t*(1-t)/durations[i];out={a.x+dx*s,a.z+dz*s,turn(looking,heading,smooth(t/.18f)),smooth(speed/.4f),distanceWalked+dist*s,i};break;}
  clock-=durations[i];distanceWalked+=dist;
 }return out;
}
void sampleBones(unsigned person,float time,std::vector<float>& output){
 auto state=visitorState(person,time);const auto& p=people[person];const auto& walk=p.rig.at("clips")[0];const auto& idle=p.rig.at("clips")[1];
 unsigned wc=walk.at("frames"),ic=idle.at("frames");float duration=idle.at("seconds");
 float wp=fmodf(state.phase/std::max(.5f,p.stride),1.f)*wc;
 float clock=fmodf(std::max(0.f,time+person*.37f),duration*2)/duration;
 float ip=(clock<=1?clock:2-clock)*(ic-1);
 unsigned wa=unsigned(wp)%wc,wb=(wa+1)%wc,ia=unsigned(ip),ib=std::min(ia+1,ic-1),offset=idle.at("offset");
 float wf=wp-floorf(wp),f=ip-floorf(ip);const unsigned count=p.boneCount*16;
 for(unsigned i=0;i<count;++i){float a=p.matrices[size_t(wa)*count+i],b=p.matrices[size_t(wb)*count+i],c=p.matrices[size_t(offset+ia)*count+i],d=p.matrices[size_t(offset+ib)*count+i];float walking=a+(b-a)*wf,standing=c+(d-c)*f;output[i]=standing+(walking-standing)*state.walk;}
}
XMFLOAT3 headAnchor(unsigned person,float time){
 auto& p=people[person];sampleBones(person,time,p.currentBones);unsigned bone=p.rig.at("head_bone");const auto& anchor=p.rig.at("head_anchor");float v[4]={anchor[0],anchor[1],anchor[2],1};XMFLOAT3 result{};float* output=&result.x;
 for(unsigned row=0;row<3;++row)for(unsigned c=0;c<4;++c)output[row]+=p.currentBones[bone*16+row*4+c]*v[c];return result;
}
XMFLOAT3 faceWeights(unsigned person,float time){
 if(!facialAnimation||person>=VisitorCount)return {};
 const float cycle=fmodf(std::max(0.f,time)+person*.47f,3.7f+.09f*person);
 float blink=std::max(0.f,1-fabsf(cycle-.32f)/.105f);
 float smile=.12f+.08f*sinf(time*.6f+person);
 float jaw=(person==12||person==4||person==8)?.08f*(.5f+.5f*sinf(time*6.1f+person)):0;
 return {blink,smile,jaw};
}
void animateGallery(bool cold){
 activeActorTriangles=0;auto camera=cameraPose();
 for(unsigned i=0;i<ActorCount;++i){auto& p=people[i];auto state=visitorState(i,sceneTime);float distance=hypotf(state.x-camera.eye.x,state.z-camera.eye.z);
  p.lod=characterDetail==0?0:characterDetail==2?2:shading==2||focusPerson==int(i)?0:distance<3?0:distance<6?1:distance<9?2:3;
  activeActorTriangles+=variety[4+i][p.lod].count/3;
  sampleBones(i,sceneTime,p.currentBones);sampleBones(i,cold?sceneTime:previousSceneTime,p.previousBones);
  void* mapped;D3D12_RANGE empty{};hr(p.boneUpload->Map(0,&empty,&mapped));memcpy(mapped,p.currentBones.data(),p.currentBones.size()*4);memcpy(static_cast<char*>(mapped)+p.currentBones.size()*4,p.previousBones.data(),p.previousBones.size()*4);p.boneUpload->Unmap(0,nullptr);
 }
 for(unsigned i=0;i<gallery.size();++i){auto& o=gallery[i];XMMATRIX base=XMLoadFloat4x4(&o.baseWorld);
  if(actorObject(o)){auto state=visitorState(o.animationKind-10,sceneTime);XMStoreFloat4x4(&o.world,XMMatrixRotationY(state.yaw)*XMMatrixTranslation(state.x,o.animationKind-10>=VisitorCount?.26f:-.025f,state.z));}
  else if(o.animationKind>=100){unsigned person=o.animationKind%100;auto state=visitorState(person,sceneTime);const auto& p=people[person];const auto& hand=p.rig.at("hands").at("Bip01 R Hand");unsigned bone=hand.at("bone");XMFLOAT4X4 rest{};for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)rest.m[row][col]=hand.at("rest_matrix")[row][col];XMFLOAT4X4 skin{};memcpy(&skin,p.currentBones.data()+bone*16,64);
   float visibility=std::max(.001f,1-state.walk);XMMATRIX attachment=XMMatrixScaling(visibility,visibility,visibility)*XMMatrixTranslation(0,.075f,o.animationKind>=200?.045f:0.f)*XMMatrixTranspose(XMLoadFloat4x4(&rest))*XMMatrixTranspose(XMLoadFloat4x4(&skin))*XMMatrixRotationY(state.yaw)*XMMatrixTranslation(state.x,-.025f,state.z);
   // Tiny scale, rather than a singular transform, keeps DXR instances valid.
   XMVECTOR scale,rot,pos;ensure(XMMatrixDecompose(&scale,&rot,&pos,base),"Prop scale");XMStoreFloat4x4(&o.world,XMMatrixScalingFromVector(scale)*attachment);
  }
  else if(o.animationKind){XMVECTOR scale,rotation,position;ensure(XMMatrixDecompose(&scale,&rotation,&position,base),"Animated object transform");XMMATRIX transform=o.animationKind==1?XMMatrixRotationY(sceneTime*.18f):XMMatrixRotationY(sceneTime*.22f+i*.35f)*XMMatrixRotationZ(.22f*sinf(sceneTime*.8f+i));XMStoreFloat4x4(&o.world,XMMatrixScalingFromVector(scale)*XMMatrixRotationQuaternion(rotation)*transform*XMMatrixTranslationFromVector(position));}
  if(cold)o.previousWorld=o.world;
 }
 constexpr auto vertexState=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
 for(auto* resource:{rayVertices.Get(),previousVertices.Get()})phoenix_native::transition(cmd.Get(),resource,vertexState,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 cmd->SetComputeRootSignature(skeletalRoot.Get());cmd->SetPipelineState(skeletalPSO.Get());cmd->SetComputeRootUnorderedAccessView(3,rayVertices->GetGPUVirtualAddress());cmd->SetComputeRootUnorderedAccessView(4,previousVertices->GetGPUVirtualAddress());
 for(unsigned i=0;i<ActorCount;++i){const auto& p=people[i];auto f=faceWeights(i,sceneTime),old=faceWeights(i,cold?sceneTime:previousSceneTime);unsigned m=assetMesh(4+i,p.lod);
  struct Parameters{unsigned count,sourceOffset,targetOffset,boneCount;float blink,smile,jaw,previousBlink,previousSmile,previousJaw;unsigned pad0=0,pad1=0;} params{meshList[m]->count,p.rig.at("lod_offsets")[p.lod],vertexOffsets[m],p.boneCount,f.x,f.y,f.z,old.x,old.y,old.z};
  cmd->SetComputeRoot32BitConstants(0,12,&params,0);cmd->SetComputeRootShaderResourceView(1,p.source->GetGPUVirtualAddress());cmd->SetComputeRootShaderResourceView(2,p.boneUpload->GetGPUVirtualAddress());cmd->Dispatch((params.count+63)/64,1,1);
 }
 for(auto* resource:{rayVertices.Get(),previousVertices.Get()}){D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;barrier.UAV.pResource=resource;cmd->ResourceBarrier(1,&barrier);phoenix_native::transition(cmd.Get(),resource,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,vertexState);}
 D3D12_RANGE empty{};void* mapped;hr(rayObjects->Map(0,&empty,&mapped));auto* objects=static_cast<RayObject*>(mapped);for(unsigned q=0;q<3;++q)for(unsigned i=0;i<gallery.size();++i){objects[q*gallery.size()+i].world=gallery[i].world;objects[q*gallery.size()+i].vertexOffset=vertexOffsets[qualityMesh(gallery[i],q)];}rayObjects->Unmap(0,nullptr);
 hr(tlasInstances[geometryQuality()]->Map(0,&empty,&mapped));auto* instances=static_cast<D3D12_RAYTRACING_INSTANCE_DESC*>(mapped);for(unsigned i=0;i<gallery.size();++i){XMFLOAT4X4 transform;XMStoreFloat4x4(&transform,XMMatrixTranspose(XMLoadFloat4x4(&gallery[i].world)));memcpy(instances[i].Transform,&transform,48);instances[i].AccelerationStructure=blas[qualityMesh(gallery[i],geometryQuality())]->GetGPUVirtualAddress();}tlasInstances[geometryQuality()]->Unmap(0,nullptr);
}
void refitAcceleration(){
 ComPtr<ID3D12GraphicsCommandList4> list;hr(cmd.As(&list));
 auto refit=[&](D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input,ID3D12Resource* output,ID3D12Resource* scratch){D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc{};desc.Inputs=input;desc.SourceAccelerationStructureData=desc.DestAccelerationStructureData=output->GetGPUVirtualAddress();desc.ScratchAccelerationStructureData=scratch->GetGPUVirtualAddress();list->BuildRaytracingAccelerationStructure(&desc,0,nullptr);D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;barrier.UAV.pResource=output;cmd->ResourceBarrier(1,&barrier);};
 D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input{};input.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;input.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE|D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE|D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;input.NumDescs=1;input.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;
 for(unsigned i=0;i<ActorCount;++i){unsigned m=assetMesh(4+i,people[i].lod);D3D12_RAYTRACING_GEOMETRY_DESC geometry{};geometry.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;geometry.Flags=D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;geometry.Triangles.VertexBuffer={rayVertices->GetGPUVirtualAddress()+vertexOffsets[m]*sizeof(Vertex),sizeof(Vertex)};geometry.Triangles.VertexCount=meshList[m]->count;geometry.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;input.pGeometryDescs=&geometry;refit(input,blas[m].Get(),peopleScratch[m].Get());}
 input.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;input.NumDescs=UINT(gallery.size());input.InstanceDescs=tlasInstances[geometryQuality()]->GetGPUVirtualAddress();refit(input,tlas[geometryQuality()].Get(),tlasScratch[geometryQuality()].Get());
}
