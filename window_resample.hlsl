// Fit the live render into a validated native network grid without stretching.
Texture2D<float4> source:register(t0);
#if MOTION
RWTexture2D<float2> target:register(u0);
#else
RWTexture2D<float4> target:register(u0);
#endif
cbuffer Params:register(b0){uint width,height,sourceWidth,sourceHeight,activeWidth,activeHeight;float offsetX,offsetY;uint spare;};
float4 sampleLinear(float2 pixel){
 int2 base=int2(floor(pixel));float2 f=frac(pixel);int2 end=int2(sourceWidth-1,sourceHeight-1);
 float4 top=lerp(source.Load(int3(clamp(base,int2(0,0),end),0)),source.Load(int3(clamp(base+int2(1,0),int2(0,0),end),0)),f.x);
 float4 bottom=lerp(source.Load(int3(clamp(base+int2(0,1),int2(0,0),end),0)),source.Load(int3(clamp(base+1,int2(0,0),end),0)),f.x);
 return lerp(top,bottom,f.y);
}
[numthreads(16,16,1)]void main(uint3 p:SV_DispatchThreadID){
 if(p.x>=width||p.y>=height)return;
 float2 local=float2(p.xy)+.5-float2(offsetX,offsetY);float4 value=0;
 if(all(local>=0)&&all(local<float2(activeWidth,activeHeight))){
  if(activeWidth==sourceWidth&&activeHeight==sourceHeight)value=source.Load(int3(p.xy-uint2(offsetX,offsetY),0));
  else value=sampleLinear(local*float2(sourceWidth,sourceHeight)/float2(activeWidth,activeHeight)-.5);
 }
#if MOTION
 target[p.xy]=value.xy*float2(activeWidth,activeHeight)/float2(sourceWidth,sourceHeight);
#else
 target[p.xy]=float4(value.rgb,1);
#endif
}
