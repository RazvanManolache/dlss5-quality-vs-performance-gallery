#include "gallery_assets.hlsli"
// DXR 1.1 inline queries. Every ray contributes to area-light visibility,
// one-bounce diffuse transport, or glossy reflected scene radiance.
RaytracingAccelerationStructure sceneAS:register(t0);
Texture2D<float4> albedoBuffer:register(t1);
Texture2D<float4> normalBuffer:register(t2);
Texture2D<float4> positionBuffer:register(t3);
ByteAddressBuffer vertices:register(t4);
struct Object {row_major float4x4 world;float4 material;uint vertexOffset,kind,pad0,pad1;};
StructuredBuffer<Object> objects:register(t5);
Texture2D<float4> skinColor:register(t6);
Texture2D<float4> marbleColor:register(t7);
Texture2D<float4> assetColor[GALLERY_ASSET_COUNT]:register(t8);
SamplerState linearWrap:register(s0);
RWTexture2D<float4> result:register(u0);
RWTexture2D<float4> sharpResult:register(u1);
cbuffer Lighting:register(b0){
 float4 camera;uint width,height,quality,objectBase;
 uint directSamples,giSamples,reflectionSamples,pad;
 float4 lightPosition[8];float4 lightColor[8];
};
float3 srgbDecode(float3 c){return lerp(c/12.92,pow(max((c+.055)/1.055,0),2.4),step(.04045,c));}
float2 disk(uint k,uint count){float a=(k+.5)*2.39996323,r=sqrt((k+.5)/count);return float2(cos(a),sin(a))*r;}
float2 rotateSample(float2 p,float angle){float s,c;sincos(angle,s,c);return float2(p.x*c-p.y*s,p.x*s+p.y*c);}
float sampleRotation(uint2 p){uint h=p.x*1973+p.y*9277+89173;h=(h^(h>>16))*2246822519u;h=(h^(h>>13))*3266489917u;return float(h&65535)*(6.2831853/65536);}
float3 tangent(float3 n){return normalize(cross(abs(n.y)<.95?float3(0,1,0):float3(1,0,0),n));}
struct Hit{float3 p,n,albedo;uint kind;};
float3 positionAt(uint v){return asfloat(vertices.Load3(v*48));}
float3 normalAt(uint v){return asfloat(vertices.Load3(v*48+12));}
float2 uvAt(uint v){return asfloat(vertices.Load2(v*48+24));}
bool visibleCandidate(uint instance,uint primitive,float2 b){
 Object o=objects[objectBase+instance];if(o.kind<10)return true;
 uint v=o.vertexOffset+primitive*3;float2 uv=uvAt(v)*(1-b.x-b.y)+uvAt(v+1)*b.x+uvAt(v+2)*b.y;
 return assetColor[NonUniformResourceIndex(o.kind-6)].SampleLevel(linearWrap,uv,0).a>=.5;
}
bool occluded(float3 p,float3 direction,float distance){
 RayDesc ray;ray.Origin=p;ray.Direction=direction;ray.TMin=.003;ray.TMax=max(.004,distance-.008);
 RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH|RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
 q.TraceRayInline(sceneAS,0,1,ray);
 while(q.Proceed()){if(q.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE&&visibleCandidate(q.CandidateInstanceID(),q.CandidatePrimitiveIndex(),q.CandidateTriangleBarycentrics()))q.CommitNonOpaqueTriangleHit();}
 return q.CommittedStatus()==COMMITTED_TRIANGLE_HIT;
}
bool trace(float3 p,float3 direction,out Hit hit){
 hit=(Hit)0;RayDesc ray;ray.Origin=p;ray.Direction=direction;ray.TMin=.004;ray.TMax=80;
 RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> q;
 q.TraceRayInline(sceneAS,0,255,ray);while(q.Proceed()){if(q.CandidateType()==CANDIDATE_NON_OPAQUE_TRIANGLE&&visibleCandidate(q.CandidateInstanceID(),q.CandidatePrimitiveIndex(),q.CandidateTriangleBarycentrics()))q.CommitNonOpaqueTriangleHit();}
 if(q.CommittedStatus()!=COMMITTED_TRIANGLE_HIT)return false;
 Object o=objects[objectBase+q.CommittedInstanceID()];uint v=o.vertexOffset+q.CommittedPrimitiveIndex()*3;
 float2 b=q.CommittedTriangleBarycentrics();float3 bary=float3(1-b.x-b.y,b.x,b.y);
 hit.p=p+direction*q.CommittedRayT();
 hit.n=normalize(mul(float4(normalAt(v)*bary.x+normalAt(v+1)*bary.y+normalAt(v+2)*bary.z,0),o.world).xyz);
 if(dot(hit.n,direction)>0)hit.n=-hit.n;
 float2 uv=uvAt(v)*bary.x+uvAt(v+1)*bary.y+uvAt(v+2)*bary.z;
 hit.kind=o.kind;hit.albedo=o.material.xyz;
 if(o.kind==0)hit.albedo=srgbDecode(skinColor.SampleLevel(linearWrap,uv,0).rgb);
 if(o.kind==1)hit.albedo=srgbDecode(marbleColor.SampleLevel(linearWrap,uv,0).rgb);
 if(o.kind>=6)hit.albedo=srgbDecode(assetColor[NonUniformResourceIndex(o.kind-6)].SampleLevel(linearWrap,uv,0).rgb);
 // The floor has actual world-space tile joints, including in reflections.
 if(o.kind==3){float2 tile=abs(frac(hit.p.xz*.7)-.5);hit.albedo*=any(tile>.493)?.65:1;}
 return true;
}
 float3 sky(float3 n){return float3(.05,.062,.082)*(.4+.6*saturate(n.y));}
float3 secondary(Hit h){
 // Secondary hits have one sample at each area-light centre and their own
 // visibility rays. This is an explicit one-bounce approximation.
 if(h.kind==5)return h.albedo;
 float3 c=sky(h.n);
 [loop]for(uint j=0;j<8;++j){float3 delta=lightPosition[j].xyz-h.p;float d2=dot(delta,delta),d=sqrt(d2);float3 l=delta/d;float nl=saturate(dot(h.n,l));
  if(nl>0&&((pad&16384)||!occluded(h.p+h.n*.004,l,d)))c+=lightColor[j].rgb*nl/(1+d2);
 }
 return c*h.albedo;
}
// Research flags: 128 no second bounce, 256 traced reflections, 512 room-bounce
// approximation, 1024 screen contact AO, 8192 no skin diffusion, 16384 unshadowed
// secondary irradiance. The learned model and its workload are unchanged.
float contactVisibility(uint2 p,float3 pos,float3 n){
 float occ=0;float radius=clamp(28/max(length(camera.xyz-pos),.7),3,28);
 [unroll]for(uint k=0;k<8;++k){float2 d=disk(k,8)*radius;int2 q=clamp(int2(p)+int2(round(d)),int2(0,0),int2(width-1,height-1));
  float4 v=positionBuffer.Load(int3(q,0));float3 delta=v.xyz-pos;float dist=length(delta);
  if(v.w>0&&dist>.008&&dist<.55)occ+=max(dot(n,delta/max(dist,.001))-.08,0)*saturate(1-dist/.55);
 }
 return saturate(1-occ*.55);
}
float3 roomBounce(float3 pos,float3 n){
 // Stable analytic irradiance approximation for this room, not traced transport.
 float floorWeight=max(-n.y,0)*saturate(1-(pos.y+.03)/4);
 float wallWeight=(abs(n.x)+abs(n.z))*.25;
 return float3(.12,.105,.09)*floorWeight+float3(.055,.065,.08)*wallWeight;
}
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){
 if(id.x>=width||id.y>=height)return;uint2 p=id.xy;
 float4 pos=positionBuffer.Load(int3(p,0));float4 base=albedoBuffer.Load(int3(p,0));float4 guide=normalBuffer.Load(int3(p,0));
 sharpResult[p]=float4(0,0,0,1);
 if(pos.w==0){float2 uv=(p+.5)/float2(width,height);result[p]=float4(sky(float3(0,1,0))*(1-.25*dot(uv-.5,uv-.5)),1);return;}
 uint kind=(uint)pos.w-1;if(kind==5){result[p]=float4(base.rgb,1);return;}float3 n=normalize(guide.xyz),v=normalize(camera.xyz-pos.xyz),origin=pos.xyz+n*.004;
 float rough=base.a,ndv=max(.025,saturate(dot(n,v))),rotation=sampleRotation(p);float visibility=(pad&1024)?contactVisibility(p,pos.xyz,n):1;float3 diffuse=base.rgb*(sky(n)+((pad&512)?roomBounce(pos.xyz,n):0))*visibility,spec=0;
 [loop]for(uint j=0;j<8;++j){
  uint samples=((pad&8)&&pos.w>=11&&frac(pos.w)<.1)?4:directSamples;
  [loop]for(uint k=0;k<samples;++k){float2 sample=samples==1?float2(0,0):rotateSample(disk(k,samples),rotation+j*.83)*lightPosition[j].w;
   float3 light=lightPosition[j].xyz+float3(sample.x,0,sample.y);float3 delta=light-pos.xyz;float d2=dot(delta,delta),d=sqrt(d2);float3 l=delta/d;float nl=saturate(dot(n,l));
   if(nl<=0||occluded(origin,l,d))continue;
   float3 energy=lightColor[j].rgb/(1+d2)/samples;diffuse+=base.rgb*energy*nl;
   float3 h=normalize(v+l);float nh=saturate(dot(n,h)),vh=saturate(dot(v,h));float a=rough*rough,a2=a*a;
   float D=a2/(3.14159265*pow(nh*nh*(a2-1)+1,2)+1e-5),gk=pow(rough+1,2)/8;
   float G=(nl/(nl*(1-gk)+gk))*(ndv/(ndv*(1-gk)+gk));float3 f0=kind==4?base.rgb*.75:.035;
   float3 F=f0+(1-f0)*pow(1-vh,5);spec+=energy*D*G*F/(4*ndv);
  }
 }
 float3 t=tangent(n),b=cross(n,t);
 [loop]for(uint k=0;k<giSamples;++k){float2 s=rotateSample(disk(k,giSamples),rotation);float3 d=normalize(t*s.x+b*s.y+n*sqrt(saturate(1-dot(s,s))));Hit h;
  // Cosine-weighted diffuse estimator; every hit affects the visible radiance.
  bool found=trace(origin,d,h);// Direct emitters are already integrated by the explicit light sampler.
  float3 incoming=found?(h.kind==5?float3(0,0,0):secondary(h)):sky(d);
  // Beauty includes a sparse second diffuse bounce with its own visibility.
  // Fourfold weight compensates for evaluating one in four hemisphere samples.
  if(quality==2&&!(pad&128)&&found&&h.kind!=5&&k%4==0){float3 ht=tangent(h.n),hb=cross(h.n,ht);float2 s2=rotateSample(disk(k/4,4),rotation+1.7);float3 d2=normalize(ht*s2.x+hb*s2.y+h.n*sqrt(saturate(1-dot(s2,s2))));Hit second;float3 extra=trace(h.p+h.n*.004,d2,second)?(second.kind==5?float3(0,0,0):secondary(second)):sky(d2);incoming+=h.albedo*extra*.55*4;}
  diffuse+=base.rgb*incoming*.7/giSamples;
 }
 if(kind==3||kind==4||((quality>0||(pad&256))&&kind>=6&&rough<.62)){float3 reflected=reflect(-v,n),rt=tangent(reflected),rb=cross(reflected,rt);float3 reflectedColor=0;
  [loop]for(uint k=0;k<reflectionSamples;++k){float2 s=rotateSample(disk(k,reflectionSamples),rotation+2.3)*rough*rough;float3 d=normalize(reflected+rt*s.x+rb*s.y);Hit h;
   reflectedColor+=((quality==0&&!(pad&256))?sky(d):(trace(origin,d,h)?secondary(h):sky(d)))/reflectionSamples;}
  float3 f0=kind==4?base.rgb*.75:float3(.035,.035,.035);float3 fresnel=f0+(1-f0)*pow(1-ndv,5);diffuse*=1-fresnel;spec+=reflectedColor*fresnel;
 }
 bool skin=kind==0||guide.w>0;result[p]=float4(max(diffuse+(quality==2&&skin&&!(pad&8192)?0:spec),0),1);
 if(quality==2&&skin&&!(pad&8192))sharpResult[p]=float4(spec,1);
}
