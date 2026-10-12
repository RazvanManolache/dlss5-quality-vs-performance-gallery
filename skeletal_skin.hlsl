// Compact bind mesh + current/previous bone matrices. Raster and DXR share output.
cbuffer SkinParams : register(b0) {
 uint count,sourceOffset,targetOffset,boneCount;
 float blink,smile,jaw,previousBlink;
 float previousSmile,previousJaw;uint unused0,unused1;
};
ByteAddressBuffer source : register(t0);
ByteAddressBuffer bones : register(t1);
RWByteAddressBuffer currentVertices : register(u0);
RWByteAddressBuffer previousVertices : register(u1);
float3 transformBone(float3 p,uint bone,bool previous,float w){
 uint offset=(bone+(previous?boneCount:0))*64;
 float4 v=float4(p,w);
 return float3(dot(asfloat(bones.Load4(offset)),v),dot(asfloat(bones.Load4(offset+16)),v),dot(asfloat(bones.Load4(offset+32)),v));
}
[numthreads(64,1,1)]
void main(uint3 thread:SV_DispatchThreadID){
 if(thread.x>=count)return;
 uint src=(sourceOffset+thread.x)*152,dst=(targetOffset+thread.x)*48;
 uint4 ids=source.Load4(src+48);float4 weights=asfloat(source.Load4(src+64));
 float3 p=asfloat(source.Load3(src)),n=asfloat(source.Load3(src+12)),t=asfloat(source.Load3(src+32));
 float3 currentP=p+asfloat(source.Load3(src+80))*blink+asfloat(source.Load3(src+104))*smile+asfloat(source.Load3(src+128))*jaw;
 float3 oldP=p+asfloat(source.Load3(src+80))*previousBlink+asfloat(source.Load3(src+104))*previousSmile+asfloat(source.Load3(src+128))*previousJaw;
 float3 cp=0,op=0,cn=0,ct=0,on=0,ot=0;
 [unroll]for(uint i=0;i<4;++i){
  cp+=transformBone(currentP,ids[i],false,1)*weights[i];op+=transformBone(oldP,ids[i],true,1)*weights[i];
  cn+=transformBone(n,ids[i],false,0)*weights[i];ct+=transformBone(t,ids[i],false,0)*weights[i];
  on+=transformBone(n,ids[i],true,0)*weights[i];ot+=transformBone(t,ids[i],true,0)*weights[i];
 }
 currentVertices.Store3(dst,asuint(cp));currentVertices.Store3(dst+12,asuint(normalize(cn)));currentVertices.Store2(dst+24,source.Load2(src+24));currentVertices.Store3(dst+32,asuint(normalize(ct)));currentVertices.Store(dst+44,source.Load(src+44));
 previousVertices.Store3(dst,asuint(op));previousVertices.Store3(dst+12,asuint(normalize(on)));previousVertices.Store2(dst+24,source.Load2(src+24));previousVertices.Store3(dst+32,asuint(normalize(ot)));previousVertices.Store(dst+44,source.Load(src+44));
}
