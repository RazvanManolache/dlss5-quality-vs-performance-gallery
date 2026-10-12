// True world-space camera framing. Both renderers evaluate the same person route.
int focusPerson=-1;
int focusAnimal=-1;
unsigned focusFraming=2; // 0 whole person, 1 upper body, 2 face
float focusAngle=.3f,focusZoom=1;
float personHeights[VisitorCount]{};
struct CameraPose{XMFLOAT3 eye,target;};
CameraPose cameraPose(){
 if(focusAnimal>=0){ensure(focusAnimal<int(AnimalCount),"Animal camera range");auto state=visitorState(VisitorCount+unsigned(focusAnimal),sceneTime);float height=variety[4+VisitorCount+focusAnimal][0].boundsMax.y;float distance=std::max(1.5f,height*1.9f)*focusZoom,target=.26f+height*.55f;return {{std::clamp(state.x+sinf(.6f+focusAngle)*distance,-11.3f,11.3f),target+.2f,std::clamp(state.z+cosf(.6f+focusAngle)*distance,-11.3f,11.3f)},{state.x,target,state.z}};}
 if(focusPerson<0)return {{sinf(yaw)*cameraRadius,2.25f,cosf(yaw)*cameraRadius},{0,1.15f,0}};
 ensure(focusPerson<int(VisitorCount)&&focusFraming<3,"Person camera selection");
 const unsigned person=unsigned(focusPerson);auto state=visitorState(person,sceneTime);
 const auto anchor=headAnchor(person,sceneTime);
 const float height=anchor.y/.91f,fractions[]={.5f,.72f,.91f},distances[]={2.35f,1.3f,.6f};
 const float targetY=height*fractions[focusFraming]-.025f;
 const float distance=distances[focusFraming]*focusZoom*std::max(1.f,.65f/(float(W)/H));
 // Avoid framing through an exhibit stand or another visitor. Use the same
 // high-detail object bounds at every quality so comparison cameras stay equal.
 struct Obstacle{XMMATRIX inverse;XMFLOAT3 lo,hi;};std::vector<Obstacle> obstacles;
 for(const auto& object:gallery){
  if(object.animationKind==10+person||object.animationKind>=100)continue;
  XMMATRIX world=XMLoadFloat4x4(&object.world);
  if(actorObject(object)){auto other=visitorState(object.animationKind-10,sceneTime);world=XMMatrixRotationY(other.yaw)*XMMatrixTranslation(other.x,-.025f,other.z);}
  auto* m=meshList[object.mesh];obstacles.push_back({XMMatrixInverse(nullptr,world),m->boundsMin,m->boundsMax});
 }
 auto obstructed=[&](const XMFLOAT3& eye,const XMFLOAT3& target){
  for(const auto& obstacle:obstacles){
   XMFLOAT3 origin,end;XMStoreFloat3(&origin,XMVector3TransformCoord(XMLoadFloat3(&eye),obstacle.inverse));XMStoreFloat3(&end,XMVector3TransformCoord(XMLoadFloat3(&target),obstacle.inverse));
   const float start[]={origin.x,origin.y,origin.z},finish[]={end.x,end.y,end.z},lo[]={obstacle.lo.x,obstacle.lo.y,obstacle.lo.z},hi[]={obstacle.hi.x,obstacle.hi.y,obstacle.hi.z};float entry=0,exit=1;bool hit=true;
   for(unsigned axis=0;axis<3;++axis){float delta=finish[axis]-start[axis];if(fabsf(delta)<1e-7f){if(start[axis]<lo[axis]||start[axis]>hi[axis]){hit=false;break;}}else{float a=(lo[axis]-start[axis])/delta,b=(hi[axis]-start[axis])/delta;if(a>b)std::swap(a,b);entry=std::max(entry,a);exit=std::min(exit,b);if(entry>exit){hit=false;break;}}}
   if(hit&&exit>.01f&&entry<.96f)return true;
  }return false;
 };
 const float offsets[]={0,.35f,-.35f,.7f,-.7f,1.05f,-1.05f,1.4f,-1.4f,1.75f,-1.75f,2.1f,-2.1f,2.45f,-2.45f,XM_PI};
 CameraPose best{};unsigned bestBlocked=UINT_MAX;
 for(float offset:offsets){
  const float angle=state.yaw+focusAngle+offset;
  CameraPose candidate{{state.x+sinf(angle)*distance,targetY+.035f,state.z+cosf(angle)*distance},{state.x,targetY,state.z}};
  unsigned blocked=0;const float ys[]={focusFraming==2?.96f:.91f,focusFraming==0?.5f:.72f,focusFraming==0?.12f:focusFraming==1?.45f:.85f};
  for(float y:ys)blocked+=obstructed(candidate.eye,{state.x,height*y-.025f,state.z})?1:0;
  if(blocked<bestBlocked){best=candidate;bestBlocked=blocked;if(!blocked)break;}
 }
 return best;
}
void selectPerson(int person){
 ensure(person>=-1&&person<int(VisitorCount),"Person selection range");focusPerson=person;focusAnimal=-1;focusAngle=.3f;focusZoom=1;
 SendMessageW(personCombo,CB_SETCURSEL,person+1,0);sideDirty[0]=sideDirty[1]=true;
}
