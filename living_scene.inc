// CPU pose interpolation and native DXR refits. GPU refit work is inside the
// measured renderer interval. Uploads occur only after the previous fence.
void sampleFox(float time,std::vector<Vertex>& output){
 unsigned count=variety[4][0].count;
 float pose=fmodf(std::max(time,0.f),foxDuration)/foxDuration*foxPoseCount;
 unsigned a=unsigned(pose)%foxPoseCount,b=(a+1)%foxPoseCount;float blend=pose-floorf(pose);
 for(unsigned i=0;i<count;++i){auto* dst=reinterpret_cast<float*>(&output[i]);auto* x=reinterpret_cast<const float*>(&foxPoses[size_t(a)*count+i]);auto* y=reinterpret_cast<const float*>(&foxPoses[size_t(b)*count+i]);for(unsigned k=0;k<12;++k)dst[k]=x[k]+(y[k]-x[k])*blend;}
}
void animateGallery(bool cold){
 for(unsigned i=0;i<gallery.size();++i){auto& o=gallery[i];XMMATRIX base=XMLoadFloat4x4(&o.baseWorld);
  if(o.animationKind){XMVECTOR scale,rotation,position;ensure(XMMatrixDecompose(&scale,&rotation,&position,base),"Animated object transform");
   XMMATRIX poseTransform=o.animationKind==1?XMMatrixRotationY(sceneTime*.18f):XMMatrixRotationY(sceneTime*.22f+i*.35f)*XMMatrixRotationZ(.22f*sinf(sceneTime*.8f+i));
   XMStoreFloat4x4(&o.world,XMMatrixScalingFromVector(scale)*XMMatrixRotationQuaternion(rotation)*poseTransform*XMMatrixTranslationFromVector(position));
  }
  if(cold)o.previousWorld=o.world;
 }
 sampleFox(sceneTime,foxCurrentVertices);sampleFox(cold?sceneTime:previousSceneTime,foxPreviousVertices);
 D3D12_RANGE empty{};void* mapped;
 auto upload=[&](ID3D12Resource* target,const std::vector<Vertex>& v,size_t offset){hr(target->Map(0,&empty,&mapped));memcpy(static_cast<char*>(mapped)+offset,v.data(),v.size()*sizeof(Vertex));target->Unmap(0,nullptr);};
 upload(variety[4][0].data.Get(),foxCurrentVertices,0);upload(foxPrevious.data.Get(),foxPreviousVertices,0);upload(rayVertices.Get(),foxCurrentVertices,size_t(vertexOffsets[14])*sizeof(Vertex));
 hr(rayObjects->Map(0,&empty,&mapped));auto* objects=static_cast<RayObject*>(mapped);
 for(unsigned q=0;q<3;++q)for(unsigned i=0;i<gallery.size();++i)objects[q*gallery.size()+i].world=gallery[i].world;
 rayObjects->Unmap(0,nullptr);
 hr(tlasInstances[shading]->Map(0,&empty,&mapped));auto* instances=static_cast<D3D12_RAYTRACING_INSTANCE_DESC*>(mapped);
 for(unsigned i=0;i<gallery.size();++i){XMFLOAT4X4 transform;XMStoreFloat4x4(&transform,XMMatrixTranspose(XMLoadFloat4x4(&gallery[i].world)));memcpy(instances[i].Transform,&transform,48);}
 tlasInstances[shading]->Unmap(0,nullptr);
}
void refitAcceleration(){
 ComPtr<ID3D12GraphicsCommandList4> list;hr(cmd.As(&list));
 auto refit=[&](D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input,ID3D12Resource* output,ID3D12Resource* scratch){
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc{};desc.Inputs=input;desc.SourceAccelerationStructureData=desc.DestAccelerationStructureData=output->GetGPUVirtualAddress();desc.ScratchAccelerationStructureData=scratch->GetGPUVirtualAddress();list->BuildRaytracingAccelerationStructure(&desc,0,nullptr);D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;barrier.UAV.pResource=output;cmd->ResourceBarrier(1,&barrier);
 };
 D3D12_RAYTRACING_GEOMETRY_DESC geometry{};geometry.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;geometry.Flags=D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;geometry.Triangles.VertexBuffer={rayVertices->GetGPUVirtualAddress()+vertexOffsets[14]*sizeof(Vertex),sizeof(Vertex)};geometry.Triangles.VertexCount=variety[4][0].count;geometry.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;
 D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input{};input.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;input.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE|D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE|D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;input.NumDescs=1;input.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;input.pGeometryDescs=&geometry;
 refit(input,blas[14].Get(),foxScratch.Get());
 input.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;input.NumDescs=UINT(gallery.size());input.InstanceDescs=tlasInstances[shading]->GetGPUVirtualAddress();refit(input,tlas[shading].Get(),tlasScratch[shading].Get());
}
