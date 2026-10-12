// A small spatial denoiser for sparsely sampled environment radiance.
// Keep scanned surfaces untouched; reject material, normal and tile edges.
Texture2D<float4> source:register(t0);
Texture2D<float4> position:register(t1);
Texture2D<float4> normal:register(t2);
RWTexture2D<float4> target:register(u0);
cbuffer Params:register(b0){uint width,height,reset,mode,a,b;float divider,gain;uint resolve;};
[numthreads(16,16,1)]void main(uint3 id:SV_DispatchThreadID){
 if(id.x>=width||id.y>=height)return;int2 p=id.xy;
 float4 center=source.Load(int3(p,0)),pos=position.Load(int3(p,0));
 if(((reset&64)&&(pos.w==7||pos.w==8))||((reset&4)&&pos.w>=11)||pos.w<=2||(pos.w>=11&&frac(pos.w)>.1)||(mode==2&&(pos.w==7||pos.w==8||pos.w>=11))){target[p]=center;return;}
 if(pos.w==4&&any(abs(frac(pos.xz*.7)-.5)>.48)){target[p]=center;return;}
 float3 n=normal.Load(int3(p,0)).xyz;float3 sum=0;float weights=0;
 [loop]for(int k=-4;k<=4;++k){
#if HORIZONTAL
  int2 q=clamp(p+int2(k*3,0),int2(0,0),int2(width-1,height-1));
#else
  int2 q=clamp(p+int2(0,k*3),int2(0,0),int2(width-1,height-1));
#endif
  float4 color=source.Load(int3(q,0)),other=position.Load(int3(q,0));float3 delta=other.xyz-pos.xyz;
  float edge=(other.w==pos.w?1:0)*step(.98,dot(n,normal.Load(int3(q,0)).xyz))*exp(-abs(dot(delta,n))*60);
  // Keep procedural tile boundaries crisp. The floor's near-black joints
  // should not bleed across neighbouring reflective tiles.
  if(pos.w==4&&any(abs(frac(other.xz*.7)-.5)>.48))edge=0;
  float colorWeight=exp(-dot(color.rgb-center.rgb,color.rgb-center.rgb)/(mode==2?.08:.018));
  float weight=exp(-k*k/12.0)*edge*colorWeight;sum+=color.rgb*weight;weights+=weight;
 }
 target[p]=float4(sum/max(weights,1e-5),1);
}
