// Shared highlight shoulder and restrained optical glow, before inference.
Texture2D<float4> source:register(t0);
Texture2D<float4> bloom:register(t1);
Texture2D<float4> spare:register(t2);
RWTexture2D<float4> target:register(u0);
cbuffer Params:register(b0){uint width,height,reset,mode,a,b;float divider,gain;uint resolve;};
float3 film(float3 c){float3 x=max(c*.93,0);return saturate((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14));}
[numthreads(16,16,1)]void main(uint3 id:SV_DispatchThreadID){
 if(id.x>=width||id.y>=height)return;int2 p=id.xy;float3 sum=0;float weights=0;
 [unroll]for(int k=-6;k<=6;++k){float w=exp(-k*k/18.0);
#if HORIZONTAL
  int2 q=clamp(p+int2(k*4,0),int2(0,0),int2(width-1,height-1));float3 c=source.Load(int3(q,0)).rgb;
  float peak=max(c.r,max(c.g,c.b));sum+=c*max(peak-1.0,0)/max(peak,.001)*w;
#else
  int2 q=clamp(p+int2(0,k*4),int2(0,0),int2(width-1,height-1));sum+=bloom.Load(int3(q,0)).rgb*w;
#endif
  weights+=w;
 }
#if HORIZONTAL
 target[p]=float4(sum/weights,1);
#else
 target[p]=float4(film(source.Load(int3(p,0)).rgb+sum/weights*.055),1);
#endif
}
