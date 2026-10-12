cbuffer Scene : register(b0) {
 row_major float4x4 world, viewProjection, previousViewProjection;
 float4 eye, options, material, light;
 row_major float4x4 lightProjection;
 float4 experiment;
};
Texture2D<float4> colorMap:register(t0);
Texture2D<float4> normalMap:register(t1);
Texture2D<float4> roughMap:register(t2);
SamplerState samp:register(s0);
struct Vertex {float3 position:POSITION;float3 previousPosition:POSITION1;float3 normal:NORMAL;float2 uv:TEXCOORD;float4 tangent:TANGENT;};
struct Pixel {float4 position:SV_Position;float3 p:POSITION;float3 n:NORMAL;float2 uv:TEXCOORD0;float4 t:TANGENT;
 float4 current:TEXCOORD1;float4 previous:TEXCOORD2;float3 previousWorld:TEXCOORD3;};
Pixel vs(Vertex v) {
 Pixel o;float4 p=mul(float4(v.position,1),world);o.p=p.xyz;
 o.position=mul(p,viewProjection);o.current=o.position;o.previous=mul(float4(v.previousPosition,1),previousViewProjection);o.previousWorld=mul(float4(v.previousPosition,1),lightProjection).xyz;
 o.n=normalize(mul(float4(v.normal,0),world).xyz);o.t=float4(normalize(mul(float4(v.tangent.xyz,0),world).xyz),v.tangent.w);o.uv=v.uv;return o;
}
float3 decode(float3 c){return lerp(c/12.92,pow(max((c+.055)/1.055,0),2.4),step(.04045,c));}
struct Result {float4 color:SV_Target0;float2 motion:SV_Target1;};
Result ps(Pixel i) {
 Result o;float3 n=normalize(i.n),albedo;
 // options: lighting, constant texture, normal maps, material specular.
 if(material.w<2) {
  albedo=options.y>0 ? (material.w==0?float3(.43,.24,.16):float3(.53,.51,.46)) : decode(colorMap.Sample(samp,i.uv).rgb);
  if(options.z>0) {float3 t=normalize(i.t.xyz-n*dot(i.t.xyz,n)),b=cross(n,t)*i.t.w;
   float3 nm=normalMap.Sample(samp,i.uv).xyz*2-1;n=normalize(t*nm.x+b*nm.y+n*nm.z);}
 } else albedo=material.xyz;
 float3 v=normalize(eye.xyz-i.p),l=normalize(light.xyz),h=normalize(v+l);
 float ndl=saturate(dot(n,l)),ndv=max(saturate(dot(n,v)),.01),ndh=saturate(dot(n,h)),vdh=saturate(dot(v,h));
 float rough=material.w==0?.48:(material.w==1?max(.18,roughMap.Sample(samp,i.uv).r):.65);
 float a=rough*rough,a2=a*a,d=a2/(3.14159265*pow(ndh*ndh*(a2-1)+1,2)+1e-5);
 float k=pow(rough+1,2)/8,g=(ndl/(ndl*(1-k)+k))*(ndv/(ndv*(1-k)+k));
 float3 f=.028+(1-.028)*pow(1-vdh,5),spec=d*g*f/(4*ndv*max(ndl,.001));
 float3 ambient=albedo*(.085+.095*saturate(n.y));
 float3 directional=albedo*ndl*.85+spec*ndl*.7*options.w;
 float3 c=lerp(albedo*.52,ambient+directional,saturate(options.x));
 o.color=float4(max(c,0),1);
 float2 current=i.current.xy/i.current.w*float2(.5,-.5)+.5;
 float2 previous=i.previous.xy/i.previous.w*float2(.5,-.5)+.5;
 o.motion=(previous-current)*experiment.yz;return o;
}

Texture2D<float4> sourceImage:register(t3);
Texture2D<float4> enhancedImage:register(t4);
cbuffer Display:register(b1){uint displayMode;float divider;uint width,height;};
struct Full {float4 p:SV_Position;float2 uv:TEXCOORD;};
Full fullscreen(uint id:SV_VertexID){Full o;o.uv=float2((id<<1)&2,id&2);o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
Result studio(Full i) {
 // A camera-fixed studio environment, rendered before the depth-tested scene.
 // It has no enclosing geometry and stays visible throughout a full orbit.
 Result o;float horizon=exp(-pow((i.uv.y-.58)*2.4,2));
 float vignette=saturate(1-.28*dot(i.uv-.5,i.uv-.5));
 o.color=float4(lerp(float3(.018,.025,.036),float3(.075,.1,.14),horizon)*vignette,1);
 o.motion=0; // The environment stays fixed in screen space.
 return o;
}
float3 encode(float3 c){return lerp(c*12.92,1.055*pow(max(c,0),1/2.4)-.055,step(.0031308,c));}
float4 display(Full i):SV_Target {
 int2 p=min(int2(i.uv*float2(width,height)),int2(width-1,height-1));
 float3 a=sourceImage.Load(int3(p,0)).rgb,b=enhancedImage.Load(int3(p,0)).rgb;
 float3 c=displayMode==0?a:displayMode==1?b:displayMode==2?(i.uv.x<divider?a:b):abs(a-b)*6;
 if(displayMode==2&&abs(i.uv.x-divider)<.0008)return float4(.1,.8,.95,1);
 return float4(saturate(encode(c)),1);
}
