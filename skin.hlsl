// Separable, normal/distance-aware diffusion of skin diffuse lighting only.
// Specular is recombined after the vertical pass and remains sharp.
Texture2D<float4> source:register(t0);
Texture2D<float4> guide:register(t1);
Texture2D<float4> sharp:register(t2);
RWTexture2D<float4> target:register(u0);
cbuffer Params:register(b0){uint width,height,reset,mode;uint a,b;float divider,gain;uint resolve;};
[numthreads(16,16,1)]void main(uint3 id:SV_DispatchThreadID) {
 if(id.x>=width||id.y>=height)return;
 int2 p=id.xy;float4 center=guide.Load(int3(p,0));float3 c=source.Load(int3(p,0)).rgb;
 if(center.w<=0){target[p]=float4(c,1);return;}
 float radius=4*3.5/max(center.w,.5);float3 sum=0,weightSum=0;
 [loop]for(int k=-4;k<=4;++k){float t=k/4.0;
#if HORIZONTAL
  int2 q=clamp(p+int2(round(t*radius),0),int2(0,0),int2(width-1,height-1));
#else
  int2 q=clamp(p+int2(0,round(t*radius)),int2(0,0),int2(width-1,height-1));
#endif
  float4 g=guide.Load(int3(q,0));
  float edge=(g.w>0?1:0)*saturate(dot(g.xyz,center.xyz))*exp(-abs(g.w-center.w)*60);
  // Wider red transport than green/blue, a controlled skin diffusion profile.
  float3 weight=exp(-t*t/float3(.32,.08,.025))*edge;
  sum+=source.Load(int3(q,0)).rgb*weight;weightSum+=weight;
 }
 c=sum/max(weightSum,1e-5);
#if !HORIZONTAL
 c+=sharp.Load(int3(p,0)).rgb;
#endif
 target[p]=float4(max(c,0),1);
}
