struct GBuffer {float4 albedo:SV_Target0;float2 motion:SV_Target1;float4 normal:SV_Target2;float4 position:SV_Target3;float4 previousWorld:SV_Target4;};
GBuffer gbuffer(Pixel i){
 GBuffer o;float3 n=normalize(i.n),albedo=material.xyz;float rough=.68;
 if(material.w<2||material.w>=6){
  float bias=experiment.x>1.5?0:experiment.x>.5?.35:1.6;
  if(((uint)experiment.w&16)||(material.w>=10&&i.uv.x>=.333333&&((uint)experiment.w&1)))bias=0;
  if(material.w>=10&&i.uv.x<.333333)bias=min(bias,.65);
  float4 tex=colorMap.SampleBias(samp,i.uv,bias);if(material.w>=10)clip(colorMap.SampleLevel(samp,i.uv,0).a-.5);albedo=decode(tex.rgb);
  float3 t=normalize(i.t.xyz-n*dot(i.t.xyz,n)),b=cross(n,t)*i.t.w;
  float3 nm=normalMap.SampleBias(samp,i.uv,bias).xyz*2-1;
  float strength=experiment.x<.5?.4:1;
  if(((uint)experiment.w&32)||(material.w>=10&&i.uv.x>=.333333&&((uint)experiment.w&2)))strength=1;
  if(material.w>=10)strength*=i.uv.x<.333333?.52:.55;
  nm=normalize(lerp(float3(0,0,1),nm,strength));n=normalize(t*nm.x+b*nm.y+n*nm.z);
  rough=material.w==0?.48:max(.22,roughMap.SampleBias(samp,i.uv,bias).r);
 }
 if(material.w==2&&experiment.x>.5){float grain=sin(i.p.x*37+i.p.z*29)*sin(i.p.y*41-i.p.z*11);albedo*=1+.035*grain;rough=.62;}
 if(material.w==3){float2 tile=abs(frac(i.p.xz*.7)-.5),footprint=max(fwidth(i.p.xz*.7),.0001);float2 joint=smoothstep(.493-footprint,.493+footprint,tile);albedo*=lerp(1,.65,max(joint.x,joint.y));rough=.24;}
 if(material.w==4)rough=.26;
 bool face=material.w>=10&&i.uv.x<.333333;float tag=material.w+1+(face?.25:0);
 o.albedo=float4(albedo,rough);o.normal=float4(n,material.w==0||face?length(eye.xyz-i.p):0);o.position=float4(i.p,tag);
 float2 current=i.current.xy/i.current.w*float2(.5,-.5)+.5,previous=i.previous.xy/i.previous.w*float2(.5,-.5)+.5;
 o.previousWorld=float4(i.previousWorld,tag);o.motion=(previous-current)*experiment.yz;return o;
}
