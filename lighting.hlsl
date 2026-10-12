// Real shadow sampling and material variants, appended to scene.hlsl.
// QUALITY: 0=cheap, 1=balanced, 2=full reference.
Texture2D<float> shadowDepth:register(t5);
SamplerState shadowPoint:register(s1);
float4 shadowVertex(Vertex v):SV_Position {
 return mul(mul(float4(v.position,1),world),lightProjection);
}
// Vogel disk samples are fixed across frames: no noise or temporal dithering.
float2 disk(uint k,uint count) {
 float a=k*2.39996323,r=sqrt((k+.5)/count);
 return float2(cos(a),sin(a))*r;
}
float visibility(float3 position,float3 normal) {
 float4 projected=mul(float4(position+normal*.003,1),lightProjection);
 float3 q=projected.xyz/projected.w;float2 uv=q.xy*float2(.5,-.5)+.5;
 float shadowFactor=1;
 [branch]if(all(uv>=0)&&all(uv<=1)&&q.z>=0&&q.z<=1){
 float bias=.0002;float radius=1.5/experiment.y;
#if QUALITY > 0
 // PCSS: blocker search determines penumbra width from emitter size and
 // blocker/receiver distances. All iterations contribute to the shadow.
 #if QUALITY == 2
  #define BLOCKERS 32
  #define FILTERS 64
 #else
  #define BLOCKERS 8
  #define FILTERS 16
 #endif
 float blocker=0,count=0;
 [loop]for(uint k=0;k<BLOCKERS;++k){float z=shadowDepth.SampleLevel(shadowPoint,uv+disk(k,BLOCKERS)*.0225,0);
  if(z<q.z-bias){blocker+=z;count+=1;}}
 [branch]if(count>0){
 blocker/=count;radius=max(radius,.0225*max(q.z-blocker,0)/max(blocker,.01));
#else
 #define FILTERS 4
#endif
 float result=0;
 [loop]for(uint k=0;k<FILTERS;++k)result+=step(q.z-bias,shadowDepth.SampleLevel(shadowPoint,uv+disk(k,FILTERS)*radius,0));
 shadowFactor=result/FILTERS;
#if QUALITY > 0
 }
#endif
 }
 return shadowFactor;
}
struct LitResult {
 float4 color:SV_Target0;float2 motion:SV_Target1;
#if QUALITY == 2
 float4 guide:SV_Target2;float4 sharp:SV_Target3;
#endif
};
LitResult lighting(Pixel i) {
 LitResult o;float3 n=normalize(i.n),albedo=material.xyz;
 if(material.w<2){
  albedo=options.y>0?(material.w==0?float3(.43,.24,.16):float3(.53,.51,.46)):decode(colorMap.Sample(samp,i.uv).rgb);
#if QUALITY > 0
  if(options.z>0){float3 t=normalize(i.t.xyz-n*dot(i.t.xyz,n)),b=cross(n,t)*i.t.w;
   float3 nm=normalMap.Sample(samp,i.uv).xyz*2-1;n=normalize(t*nm.x+b*nm.y+n*nm.z);}
#endif
 }
 float3 v=normalize(eye.xyz-i.p),l=normalize(light.xyz),halfway=normalize(v+l);
 float ndl=saturate(dot(n,l)),ndv=max(saturate(dot(n,v)),.01),ndh=saturate(dot(n,halfway));
 float shade=visibility(i.p,n);float3 spec;
#if QUALITY > 0
 float rough=material.w==0?.48:(material.w==1?max(.18,roughMap.Sample(samp,i.uv).r):.65);
 float a=rough*rough,a2=a*a,d=a2/(3.14159265*pow(ndh*ndh*(a2-1)+1,2)+1e-5);
 float k=pow(rough+1,2)/8,g=(ndl/(ndl*(1-k)+k))*(ndv/(ndv*(1-k)+k));
 float3 f=.028+(1-.028)*pow(1-saturate(dot(v,halfway)),5);spec=d*g*f/(4*ndv*max(ndl,.001));
#else
 // A genuinely shorter material shader: no normal/roughness samples or GGX.
 spec=.035*pow(ndh,24);
#endif
 float3 diffuse=albedo*(.085+.095*saturate(n.y)+ndl*.85*shade);
#if QUALITY < 2
 // Cheap local skin wrap, with no screen-space diffusion passes.
 if(material.w==0)diffuse+=albedo*float3(.07,.025,.012)*saturate((dot(n,l)+.3)/1.3)*shade;
#endif
 float3 sharp=spec*ndl*.7*shade*options.w*options.x;
 diffuse=lerp(albedo*.52,diffuse,saturate(options.x));
#if QUALITY == 2
 bool skin=material.w==0;
 o.color=float4(max(diffuse+(skin?0:sharp),0),1);
 o.guide=skin?float4(n,length(eye.xyz-i.p)):0;
 o.sharp=float4(skin?sharp:0,1);
#else
 o.color=float4(max(diffuse+sharp,0),1);
#endif
 float2 current=i.current.xy/i.current.w*float2(.5,-.5)+.5;
 float2 previous=i.previous.xy/i.previous.w*float2(.5,-.5)+.5;
 o.motion=(previous-current)*float2(1920,1080);return o;
}
