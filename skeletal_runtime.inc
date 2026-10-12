struct Person {
 Json rig;std::vector<float> matrices,currentBones,previousBones;
 ComPtr<ID3D12Resource> source,boneUpload;
 unsigned boneCount=0,lod=0;float stride=1;
};
Person people[ActorCount];
ComPtr<ID3D12Resource> previousVertices;
ComPtr<ID3D12RootSignature> skeletalRoot;
ComPtr<ID3D12PipelineState> skeletalPSO;
unsigned characterDetail=1,activeActorTriangles=0;
bool facialAnimation=true;
unsigned assetMesh(unsigned asset,unsigned lod){return asset<4?6+asset*2+std::min(lod,1u):14+(asset-4)*4+lod;}
bool actorObject(const GalleryObject& o)const{return o.animationKind>=10&&o.animationKind<10+ActorCount;}
void loadRigs(){
 for(unsigned i=0;i<ActorCount;++i){auto& p=people[i];const std::string name=AssetNames[4+i];const auto folder=root/"assets/living_people";
  p.rig=phoenix_bundle::json_file(folder/(name+"_rig.json"));p.boneCount=p.rig.at("bone_count");p.stride=p.rig.at("stride");ensure(p.boneCount>0&&p.boneCount<=128,"Bone count");
  auto b=bytes(folder/(name+".bones"));ensure(b.size()%64==0,"Bone matrix extent");p.matrices.resize(b.size()/4);memcpy(p.matrices.data(),b.data(),b.size());p.currentBones.resize(p.boneCount*16);p.previousBones.resize(p.boneCount*16);
  auto src=bytes(folder/(name+".skin"));unsigned vertexCount=0;for(unsigned lod=0;lod<4;++lod){ensure(p.rig.at("lod_counts")[lod]==variety[4+i][lod].count,"Skin/mesh vertex count");vertexCount+=variety[4+i][lod].count;}ensure(src.size()==size_t(vertexCount)*152,"Skin record extent");
  auto staging=phoenix_native::buffer(device.Get(),src.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* mapped;D3D12_RANGE empty{};hr(staging->Map(0,&empty,&mapped));memcpy(mapped,src.data(),src.size());staging->Unmap(0,nullptr);
  p.source=phoenix_native::buffer(device.Get(),src.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);cmd->CopyBufferRegion(p.source.Get(),0,staging.Get(),0,src.size());phoenix_native::transition(cmd.Get(),p.source.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);uploads.push_back(staging);
  p.boneUpload=phoenix_native::buffer(device.Get(),p.boneCount*128,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
 }
 D3D12_ROOT_PARAMETER rp[5]{};rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;rp[0].Constants={0,0,12};
 for(unsigned i=1;i<5;++i){rp[i].ParameterType=i<3?D3D12_ROOT_PARAMETER_TYPE_SRV:D3D12_ROOT_PARAMETER_TYPE_UAV;rp[i].Descriptor.ShaderRegister=(i-1)%2;}
 D3D12_ROOT_SIGNATURE_DESC rd{5,rp,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> rs,error;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&rs,&error));hr(device->CreateRootSignature(0,rs->GetBufferPointer(),rs->GetBufferSize(),IID_PPV_ARGS(&skeletalRoot)));
 auto source=bytes(root/"skeletal_skin.hlsl");auto cs=shader(std::string(source.begin(),source.end()),"main","cs_5_1");D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=skeletalRoot.Get();pd.CS={cs->GetBufferPointer(),cs->GetBufferSize()};hr(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&skeletalPSO)));
}
