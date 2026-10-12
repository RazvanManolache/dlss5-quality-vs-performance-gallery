// The fixed ReferenceDecode math is supplied by the unchanged native bridge.
Texture2D<float4> original:register(t0);
Texture2D<float4> neural:register(t1);
Texture2D<float4> position:register(t2);
RWTexture2D<float4> target:register(u0);
cbuffer Params:register(b0){uint width,height,offset,mode,activeWidth,activeHeight;float divider,gain;uint fitted;};
[numthreads(16,16,1)]void main(uint3 p:SV_DispatchThreadID){
 if(p.x>=width||p.y>=height)return;
 float3 o=original.Load(int3(p.xy,0)).rgb,n=neural.Load(int3(p.xy,0)).rgb;
 float kind=position.Load(int3(p.xy,0)).w;
 float weight=mode==0?1:(((kind>0&&kind<=2)||kind==7||kind==8||(kind>=11&&frac(kind)>.1))?gain:0);
 target[p.xy]=float4(lerp(o,ReferenceDecode(o,n),weight),1);
}
