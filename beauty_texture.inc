unsigned sharedTextures=0;
unsigned texture(const fs::path& p){
 // Source maps are immutable and can be shared by the two device contexts.
 static std::map<std::pair<ID3D12Device*,std::wstring>,ComPtr<ID3D12Resource>> cache;
 auto key=std::make_pair(device.Get(),fs::absolute(p).wstring());auto found=cache.find(key);
 if(found!=cache.end()){unsigned index=unsigned(textures.size());textures.push_back(found->second);++sharedTextures;return index;}
 auto b=bytes(p);ensure(b.size()>=8,"Texture header");unsigned tw,th;memcpy(&tw,b.data(),4);memcpy(&th,b.data()+4,4);ensure(b.size()==8+size_t(tw)*th*4,"Texture extent");
 unsigned levels=1;size_t offset=8;auto mipFile=root/"assets/beauty_maps002"/(p.filename().string()+".mips");
 if(fs::exists(mipFile)){b=bytes(mipFile);ensure(b.size()>=12,"Mip header");unsigned mw,mh;memcpy(&mw,b.data(),4);memcpy(&mh,b.data()+4,4);memcpy(&levels,b.data()+8,4);ensure(mw==tw&&mh==th&&levels>0&&levels<=16,"Mip dimensions");offset=12;}
 D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=tw;desc.Height=th;desc.DepthOrArraySize=1;desc.MipLevels=UINT16(levels);desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
 D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> t;hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&t)));
 std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(levels);UINT64 total;device->GetCopyableFootprints(&desc,0,levels,0,footprints.data(),nullptr,nullptr,&total);
 auto upload=phoenix_native::buffer(device.Get(),total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* dst;D3D12_RANGE empty{};hr(upload->Map(0,&empty,&dst));
 for(unsigned level=0;level<levels;++level){unsigned w=std::max(1u,tw>>level),h=std::max(1u,th>>level);ensure(offset+size_t(w)*h*4<=b.size(),"Mip extent");auto& fp=footprints[level];for(unsigned y=0;y<h;++y)memcpy(static_cast<char*>(dst)+fp.Offset+size_t(y)*fp.Footprint.RowPitch,b.data()+offset+size_t(y)*w*4,size_t(w)*4);offset+=size_t(w)*h*4;}
 ensure(offset==b.size(),"Complete mip payload");upload->Unmap(0,nullptr);
 for(unsigned level=0;level<levels;++level){D3D12_TEXTURE_COPY_LOCATION from{upload.Get(),D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,footprints[level]},to{t.Get(),D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,level};cmd->CopyTextureRegion(&to,0,0,0,&from,nullptr);}
 phoenix_native::transition(cmd.Get(),t.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);unsigned index=unsigned(textures.size());textures.push_back(t);uploads.push_back(upload);cache.emplace(key,t);return index;
}
