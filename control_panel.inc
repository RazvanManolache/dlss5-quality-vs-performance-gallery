// Native controls own the interactive view. Benchmark entry points remain separate.
std::unique_ptr<App> peer;
bool panelTesting=false,comparisonWorker=false,panelEnabled=false,panelExpanded=true,pendingImage=false,uiAnimation=true,uiOrbit=false,uiLights=true;
unsigned panelView=2,viewQuality[2]{2,3},viewPhoenix[2]{0,1},comparisonFrames=0,viewCharacterDetail[2]{0,1};
bool sideDirty[2]{true,true};
HWND panel=nullptr,panelBody=nullptr,panelHeader=nullptr,panelStats=nullptr,sideStats[2]{},viewCombo=nullptr,qualityCombo[2]{},phoenixCombo[2]{},animationCheck=nullptr,orbitCheck=nullptr,lightsCheck=nullptr,personCombo=nullptr,framingCombo=nullptr,characterCombo[2]{},faceCheck=nullptr;
HFONT panelFont=nullptr;HBRUSH panelBrush=nullptr;int scrollOffset=0;
struct Widget{HWND handle;int x,y,w,h;};std::vector<Widget> panelWidgets;
ComPtr<ID3D12Resource> comparisonColor[2];unsigned comparisonW=0,comparisonH=0;
fs::path panelOutput;double deliveredFPS=0,sideGpuMs[2]{},frameMs=0;
std::chrono::steady_clock::time_point uiClock=std::chrono::steady_clock::now(),fpsClock=uiClock;
unsigned fpsFrames=0;RECT lastOwnerRect{};
static constexpr int ID_EXPAND=400,ID_VIEW=401,ID_LEFT_QUALITY=402,ID_LEFT_PHOENIX=403,ID_RIGHT_QUALITY=404,ID_RIGHT_PHOENIX=405,ID_ANIMATION=406,ID_ORBIT=407,ID_LIGHTS=408,ID_ROTATE_LEFT=409,ID_ROTATE_RIGHT=410,ID_ZOOM_IN=411,ID_ZOOM_OUT=412,ID_RESET_CAMERA=413,ID_SAVE_IMAGE=414,ID_PERSON=415,ID_PREVIOUS_PERSON=416,ID_NEXT_PERSON=417,ID_FRAMING=418,ID_OVERVIEW=419,ID_LEFT_DETAIL=420,ID_RIGHT_DETAIL=421,ID_FACES=422,ID_BEAGLE=423,ID_DEER=424;

static LRESULT CALLBACK panelProc(HWND h,UINT message,WPARAM w,LPARAM l){
 auto* a=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));
 if(message==WM_NCCREATE){a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}
 if(!a)return DefWindowProcW(h,message,w,l);
 if(message==WM_COMMAND){a->panelCommand(LOWORD(w),HIWORD(w));return 0;}
 if(message==WM_CLOSE){a->panelExpanded=false;a->layoutPanel();return 0;}
 if(message==WM_ERASEBKGND)return 1;
 if(message==WM_PAINT){PAINTSTRUCT p;auto dc=BeginPaint(h,&p);FillRect(dc,&p.rcPaint,a->panelBrush);EndPaint(h,&p);return 0;}
 if(message==WM_CTLCOLORSTATIC||message==WM_CTLCOLORBTN){auto dc=reinterpret_cast<HDC>(w);SetTextColor(dc,RGB(225,231,239));SetBkColor(dc,RGB(25,29,36));return reinterpret_cast<LRESULT>(a->panelBrush);}
 if(message==WM_VSCROLL||message==WM_MOUSEWHEEL){SCROLLINFO info{sizeof(info),SIF_ALL};GetScrollInfo(h,SB_VERT,&info);int next=a->scrollOffset;
  if(message==WM_MOUSEWHEEL)next-=GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*60;
  else switch(LOWORD(w)){case SB_LINEUP:next-=28;break;case SB_LINEDOWN:next+=28;break;case SB_PAGEUP:next-=int(info.nPage);break;case SB_PAGEDOWN:next+=int(info.nPage);break;case SB_THUMBTRACK:next=info.nTrackPos;break;}
  a->scrollOffset=std::clamp(next,0,std::max(0,info.nMax-int(info.nPage)+1));a->layoutPanel();return 0;
 }
 return DefWindowProcW(h,message,w,l);
}
HWND widget(const wchar_t* cls,const wchar_t* text,int id,int x,int y,int w,int h,DWORD style=0){
 HWND control=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,panelBody,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);ensure(control,"Panel control creation");
 SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(panelFont),TRUE);SetWindowTheme(control,L"DarkMode_Explorer",nullptr);panelWidgets.push_back({control,x,y,w,h});return control;
}
HWND combo(int id,int y,const std::initializer_list<const wchar_t*>& choices,unsigned choice){
 HWND h=widget(L"COMBOBOX",L"",id,16,y,260,180,CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL);
 for(auto* text:choices)SendMessageW(h,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text));SendMessageW(h,CB_SETCURSEL,choice,0);return h;
}
void layoutPanel(){
 if(!panel)return;RECT r{};GetClientRect(window,&r);POINT origin{12,12};ClientToScreen(window,&origin);
 const int width=std::min(312,std::max(200,int(r.right)-24));const int height=panelExpanded?std::min(660,std::max(132,int(r.bottom)-24)):78;
 SetWindowPos(panel,HWND_TOP,origin.x,origin.y,width,height,SWP_NOACTIVATE|SWP_NOOWNERZORDER);
 MoveWindow(panelHeader,12,10,width-24,28,TRUE);MoveWindow(panelStats,16,44,width-32,24,TRUE);
 SetWindowTextW(panelHeader,panelExpanded?L"Controls  \x25B4":L"Controls  \x25BE");
 ShowWindow(panelBody,panelExpanded?SW_SHOWNA:SW_HIDE);MoveWindow(panelBody,0,78,width-2,height-80,TRUE);
 RECT b{};GetClientRect(panelBody,&b);SCROLLINFO info{sizeof(info),SIF_RANGE|SIF_PAGE|SIF_POS};info.nMax=939;info.nPage=unsigned(std::max(0L,b.bottom));scrollOffset=std::clamp(scrollOffset,0,std::max(0,940-int(info.nPage)));info.nPos=scrollOffset;SetScrollInfo(panelBody,SB_VERT,&info,TRUE);
 for(const auto& c:panelWidgets)MoveWindow(c.handle,c.x,c.y-scrollOffset,std::min(c.w,std::max(40,int(b.right)-c.x-12)),c.h,TRUE);
 InvalidateRect(panel,nullptr,FALSE);InvalidateRect(panelBody,nullptr,FALSE);
}
void initPanel(const fs::path& output,bool show=true){
 panelEnabled=true;panelOutput=output;divider=.5f;deterministicAnimation=true;animation=false;
 panelBrush=CreateSolidBrush(RGB(25,29,36));panelFont=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 WNDCLASSW wc{};wc.lpfnWndProc=panelProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"PhoenixControls";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
 panel=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"Gallery controls",WS_POPUP|WS_BORDER,0,0,312,620,window,nullptr,wc.hInstance,this);ensure(panel,"Collapsible panel creation");
 panelHeader=CreateWindowW(L"BUTTON",L"Controls",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,12,10,288,28,panel,reinterpret_cast<HMENU>(INT_PTR(ID_EXPAND)),wc.hInstance,nullptr);
 panelStats=CreateWindowW(L"STATIC",L"Loading comparison...",WS_CHILD|WS_VISIBLE,16,44,280,24,panel,nullptr,wc.hInstance,nullptr);
 SendMessageW(panelHeader,WM_SETFONT,reinterpret_cast<WPARAM>(panelFont),TRUE);SendMessageW(panelStats,WM_SETFONT,reinterpret_cast<WPARAM>(panelFont),TRUE);SetWindowTheme(panelHeader,L"DarkMode_Explorer",nullptr);
 panelBody=CreateWindowExW(0,wc.lpszClassName,L"Comparison settings",WS_CHILD|WS_VISIBLE|WS_VSCROLL,0,78,310,540,panel,nullptr,wc.hInstance,this);
 widget(L"STATIC",L"VIEW",0,16,0,260,20);viewCombo=combo(ID_VIEW,23,{L"Compare left / right",L"Left only",L"Right only",L"Difference"},0);
 widget(L"STATIC",L"LEFT",0,16,61,260,20);qualityCombo[0]=combo(ID_LEFT_QUALITY,84,{L"Very light",L"Middle",L"Beauty",L"Light + detail",L"Light + detail + soft shadows",L"Room bounce + contact",L"Sparse GI + reflections"},2);phoenixCombo[0]=combo(ID_LEFT_PHOENIX,117,{L"Phoenix off",L"Phoenix on - protected surfaces",L"Phoenix on - full frame"},0);sideStats[0]=widget(L"STATIC",L"GPU: --",0,16,150,260,20);
 widget(L"STATIC",L"RIGHT",0,16,181,260,20);qualityCombo[1]=combo(ID_RIGHT_QUALITY,204,{L"Very light",L"Middle",L"Beauty",L"Light + detail",L"Light + detail + soft shadows",L"Room bounce + contact",L"Sparse GI + reflections"},3);phoenixCombo[1]=combo(ID_RIGHT_PHOENIX,237,{L"Phoenix off",L"Phoenix on - protected surfaces",L"Phoenix on - full frame"},1);sideStats[1]=widget(L"STATIC",L"GPU: --",0,16,270,260,20);
 animationCheck=widget(L"BUTTON",L"Animate visitors & exhibits",ID_ANIMATION,16,308,260,23,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(animationCheck,BM_SETCHECK,BST_CHECKED,0);
 orbitCheck=widget(L"BUTTON",L"Orbit camera",ID_ORBIT,16,335,260,23,BS_AUTOCHECKBOX|WS_TABSTOP);
 lightsCheck=widget(L"BUTTON",L"Animate lighting",ID_LIGHTS,16,362,260,23,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(lightsCheck,BM_SETCHECK,BST_CHECKED,0);
 widget(L"STATIC",L"FOCUS",0,16,399,260,20);
 personCombo=combo(ID_PERSON,424,{L"Gallery overview"},0);for(auto label:VisitorLabels)SendMessageW(personCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
 widget(L"BUTTON",L"Previous person",ID_PREVIOUS_PERSON,16,459,126,28,BS_PUSHBUTTON);widget(L"BUTTON",L"Next person",ID_NEXT_PERSON,150,459,126,28,BS_PUSHBUTTON);
 framingCombo=combo(ID_FRAMING,494,{L"Whole person",L"Upper body",L"Face"},2);
 widget(L"BUTTON",L"Rotate left",ID_ROTATE_LEFT,16,529,126,28,BS_PUSHBUTTON);widget(L"BUTTON",L"Rotate right",ID_ROTATE_RIGHT,150,529,126,28,BS_PUSHBUTTON);
 widget(L"BUTTON",L"Zoom in",ID_ZOOM_IN,16,564,126,28,BS_PUSHBUTTON);widget(L"BUTTON",L"Zoom out",ID_ZOOM_OUT,150,564,126,28,BS_PUSHBUTTON);
 widget(L"BUTTON",L"Reset camera",ID_RESET_CAMERA,16,599,260,28,BS_PUSHBUTTON);widget(L"BUTTON",L"Gallery overview",ID_OVERVIEW,16,634,260,28,BS_PUSHBUTTON);widget(L"BUTTON",L"Save image",ID_SAVE_IMAGE,16,673,260,30,BS_PUSHBUTTON);
 widget(L"STATIC",L"CHARACTER BODY DETAIL",0,16,716,260,20);
 characterCombo[0]=combo(ID_LEFT_DETAIL,740,{L"Left: full meshes",L"Left: distance detail",L"Left: reduced bodies"},0);
 characterCombo[1]=combo(ID_RIGHT_DETAIL,775,{L"Right: full meshes",L"Right: distance detail",L"Right: reduced bodies"},1);
 faceCheck=widget(L"BUTTON",L"Animate facial expressions",ID_FACES,16,815,260,23,BS_AUTOCHECKBOX|WS_TABSTOP);SendMessageW(faceCheck,BM_SETCHECK,BST_CHECKED,0);
 widget(L"BUTTON",L"Beagle exhibit",ID_BEAGLE,16,858,260,28,BS_PUSHBUTTON);widget(L"BUTTON",L"Deer exhibit",ID_DEER,16,895,260,28,BS_PUSHBUTTON);
 layoutPanel();
 peer=std::make_unique<App>(root);peer->init(false,false,device.Get());peer->comparisonWorker=true;peer->deterministicAnimation=true;peer->animation=false;
 uiClock=fpsClock=std::chrono::steady_clock::now();if(show)ShowWindow(panel,SW_SHOWNOACTIVATE);
}
void panelCommand(unsigned id,unsigned notification){
 if(id==ID_EXPAND){panelExpanded=!panelExpanded;layoutPanel();return;}
 if(id==ID_VIEW&&notification==CBN_SELCHANGE){const unsigned options[]={2,0,1,3};panelView=options[SendMessageW(viewCombo,CB_GETCURSEL,0,0)];sideDirty[0]=sideDirty[1]=true;}
 for(unsigned i=0;i<2;++i){if(id==unsigned(i?ID_RIGHT_QUALITY:ID_LEFT_QUALITY)&&notification==CBN_SELCHANGE){viewQuality[i]=unsigned(SendMessageW(qualityCombo[i],CB_GETCURSEL,0,0));sideDirty[i]=true;}if(id==unsigned(i?ID_RIGHT_PHOENIX:ID_LEFT_PHOENIX)&&notification==CBN_SELCHANGE){viewPhoenix[i]=unsigned(SendMessageW(phoenixCombo[i],CB_GETCURSEL,0,0));sideDirty[i]=true;}}
 for(unsigned i=0;i<2;++i)if(id==unsigned(i?ID_RIGHT_DETAIL:ID_LEFT_DETAIL)&&notification==CBN_SELCHANGE){viewCharacterDetail[i]=unsigned(SendMessageW(characterCombo[i],CB_GETCURSEL,0,0));sideDirty[i]=true;}
 if(id==ID_FACES){facialAnimation=SendMessageW(faceCheck,BM_GETCHECK,0,0)==BST_CHECKED;sideDirty[0]=sideDirty[1]=true;}
 if(id==ID_ANIMATION)uiAnimation=SendMessageW(animationCheck,BM_GETCHECK,0,0)==BST_CHECKED;
 if(id==ID_ORBIT)uiOrbit=SendMessageW(orbitCheck,BM_GETCHECK,0,0)==BST_CHECKED;
 if(id==ID_LIGHTS)uiLights=SendMessageW(lightsCheck,BM_GETCHECK,0,0)==BST_CHECKED;
 if(id==ID_PERSON&&notification==CBN_SELCHANGE)selectPerson(int(SendMessageW(personCombo,CB_GETCURSEL,0,0))-1);
 if(id==ID_NEXT_PERSON)selectPerson(focusPerson<0?0:(focusPerson+1)%int(VisitorCount));
 if(id==ID_PREVIOUS_PERSON)selectPerson(focusPerson<0?int(VisitorCount)-1:(focusPerson+int(VisitorCount)-1)%int(VisitorCount));
 if(id==ID_BEAGLE||id==ID_DEER){selectPerson(-1);focusAnimal=id==ID_BEAGLE?0:1;sideDirty[0]=sideDirty[1]=true;}
 if(id==ID_OVERVIEW)selectPerson(-1);
 if(id==ID_FRAMING&&notification==CBN_SELCHANGE){focusFraming=unsigned(SendMessageW(framingCombo,CB_GETCURSEL,0,0));focusZoom=1;sideDirty[0]=sideDirty[1]=true;}
 if(id==ID_ROTATE_LEFT){if(focusPerson>=0||focusAnimal>=0)focusAngle-=.12f;else yaw-=.12f;}
 if(id==ID_ROTATE_RIGHT){if(focusPerson>=0||focusAnimal>=0)focusAngle+=.12f;else yaw+=.12f;}
 if(id==ID_ZOOM_IN){if(focusPerson>=0||focusAnimal>=0)focusZoom=std::max(.65f,focusZoom-.1f);else cameraRadius=std::max(2.4f,cameraRadius-.3f);}
 if(id==ID_ZOOM_OUT){if(focusPerson>=0||focusAnimal>=0)focusZoom=std::min(2.f,focusZoom+.1f);else cameraRadius=std::min(12.f,cameraRadius+.3f);}
 if(id==ID_RESET_CAMERA){if(focusPerson>=0||focusAnimal>=0){focusAngle=.3f;focusZoom=1;}else{yaw=.42f;cameraRadius=7.6f;}sideDirty[0]=sideDirty[1]=true;}
 if(id==ID_SAVE_IMAGE)pendingImage=true;
}
void comparisonTargets(){
 if(comparisonW==W&&comparisonH==H)return;
 comparisonW=W;comparisonH=H;for(unsigned i=0;i<2;++i){comparisonColor[i]=image(device.Get(),W,H,DXGI_FORMAT_R16G16B16A16_FLOAT,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);srvView(comparisonColor[i].Get(),192+i);}sideDirty[0]=sideDirty[1]=true;
}
void renderComparison(const fs::path& requested={},bool present=true){
 const auto frameStart=std::chrono::steady_clock::now();const float dt=panelTesting?1.f/24:std::min(.1f,std::chrono::duration<float>(frameStart-uiClock).count());uiClock=frameStart;
 if(uiAnimation)sceneTime+=dt;if(uiOrbit){if(focusPerson>=0||focusAnimal>=0)focusAngle+=dt*.18f;else yaw+=dt*.18f;}if(uiLights)lightPhase+=dt*.7f;
 comparisonTargets();if(peer->W!=W||peer->H!=H)peer->clientSize(W,H);
 if(reset){sideDirty[0]=sideDirty[1]=true;reset=false;}if(peer->reset)sideDirty[1]=true;
 fs::path captureDir=requested;if(pendingImage&&captureDir.empty())captureDir=panelOutput/("image_"+std::to_string(comparisonFrames));pendingImage=false;
 if(!captureDir.empty()){ensure(!fs::exists(captureDir),"Fresh comparison image path required");fs::create_directories(captureDir);}
 const bool active[2]={panelView!=1,panelView!=0};Json sides=Json::array();
 for(unsigned i=0;i<2;++i){App& a=i?*peer:*this;if(!active[i]){sides.push_back(nullptr);continue;}
  a.characterDetail=viewCharacterDetail[i];a.facialAnimation=facialAnimation;a.focusAnimal=focusAnimal;a.focusPerson=focusPerson;a.focusFraming=focusFraming;a.focusAngle=focusAngle;a.focusZoom=focusZoom;a.yaw=yaw;a.cameraRadius=cameraRadius;a.sceneTime=sceneTime;a.lightPhase=lightPhase;a.qualityPreset(viewQuality[i]);a.enabled=viewPhoenix[i]!=0;a.materialScope=viewPhoenix[i]==1;a.displayMode=a.enabled?1:0;a.reset=sideDirty[i];a.livingAnimation=uiAnimation;
  auto row=a.render(cases[0],captureDir.empty()?fs::path{}:captureDir/(i?"right":"left"),false,false);row["side"]=i;row["next_sampling_index"]=a.network->next_sampling_index();sideGpuMs[i]=row["processing_gpu_ms"];sides.push_back(row);sideDirty[i]=false;
 }
 begin();using phoenix_native::transition;
 for(unsigned i=0;i<2;++i)if(active[i]){App& a=i?*peer:*this;auto* input=a.enabled?a.enhanced.Get():a.scene.Get();const auto state=a.enabled?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  transition(cmd.Get(),input,state,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(cmd.Get(),comparisonColor[i].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);cmd->CopyResource(comparisonColor[i].Get(),input);transition(cmd.Get(),comparisonColor[i].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);transition(cmd.Get(),input,D3D12_RESOURCE_STATE_COPY_SOURCE,state);
 }
 unsigned index=swap->GetCurrentBackBufferIndex();transition(cmd.Get(),back[index].Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);auto target=rt(index);cmd->OMSetRenderTargets(1,&target,FALSE,nullptr);cmd->SetPipelineState(displayPSO.Get());cmd->SetGraphicsRootSignature(displayRoot.Get());ID3D12DescriptorHeap* heaps[]{srv.Get()};cmd->SetDescriptorHeaps(1,heaps);cmd->SetGraphicsRootDescriptorTable(0,view(192));
 D3D12_VIEWPORT viewport{0,0,float(W),float(H),0,1};D3D12_RECT scissor{0,0,LONG(W),LONG(H)};cmd->RSSetViewports(1,&viewport);cmd->RSSetScissorRects(1,&scissor);struct Display{unsigned mode;float divider;unsigned width,height;} dc{panelView,divider,W,H};cmd->SetGraphicsRoot32BitConstants(1,4,&dc,0);cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);cmd->DrawInstanced(3,1,0,0);
 if(!captureDir.empty()){capture(cmd.Get(),back[index].Get(),rbDisplay,D3D12_RESOURCE_STATE_RENDER_TARGET);for(unsigned i=0;i<2;++i)if(active[i]){Readback rb=readback(device.Get(),comparisonColor[i].Get(),8);capture(cmd.Get(),comparisonColor[i].Get(),rb,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);submit();save(rb,captureDir/(i?"right.rgba16f":"left.rgba16f"));begin();}}
 transition(cmd.Get(),back[index].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);submit();if(present)hr(swap->Present(1,0));ensure(errors()==0,"Comparison display D3D12 errors");
 frameMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count();++comparisonFrames;++fpsFrames;
 auto now=std::chrono::steady_clock::now();double elapsed=std::chrono::duration<double>(now-fpsClock).count();
 if(elapsed>=.4){deliveredFPS=fpsFrames/elapsed;fpsFrames=0;fpsClock=now;wchar_t text[256];swprintf_s(text,L"%.1f FPS  |  %u x %u",deliveredFPS,W,H);SetWindowTextW(panelStats,text);
  for(unsigned i=0;i<2;++i){if(active[i])swprintf_s(text,L"GPU: %.2f ms%s",sideGpuMs[i],viewPhoenix[i]?L"  (includes Phoenix)":L"");else swprintf_s(text,L"Not rendered in single view");SetWindowTextW(sideStats[i],text);}
  swprintf_s(text,L"Phoenix Gallery | %.1f FPS | %u x %u | %s",deliveredFPS,W,H,panelView==0?L"Left only":panelView==1?L"Right only":panelView==3?L"Difference":L"Left / right comparison");SetWindowTextW(window,text);
 }
 Json row={{"frame",comparisonFrames-1},{"width",W},{"height",H},{"view",panelView},{"divider",divider},{"delivered_fps",deliveredFPS},{"frame_wall_ms",frameMs},{"sides",sides}};if(report["comparison_frames"].size()>=600)report["comparison_frames"].erase(report["comparison_frames"].begin(),report["comparison_frames"].begin()+300);report["comparison_frames"].push_back(row);
 if(!captureDir.empty()){save(rbDisplay,captureDir/"display.rgba8");savePNG(rbDisplay,captureDir/"display.png");std::ofstream(captureDir/"comparison.json")<<row.dump(2);}
 RECT ownerRect{};GetWindowRect(window,&ownerRect);if(memcmp(&ownerRect,&lastOwnerRect,sizeof(ownerRect))!=0){lastOwnerRect=ownerRect;layoutPanel();}
}

void panelCheck(const fs::path& out){
 // Drive the same native control notifications used by real mouse interaction.
 panelTesting=true;uiAnimation=false;uiLights=false;sceneTime=.9f;lightPhase=.63f;SendMessageW(animationCheck,BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(lightsCheck,BM_SETCHECK,BST_UNCHECKED,0);
 auto select=[&](HWND control,unsigned index,int id){SendMessageW(control,CB_SETCURSEL,index,0);SendMessageW(panelBody,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),reinterpret_cast<LPARAM>(control));};
 for(unsigned layout=0;layout<4;++layout){select(viewCombo,layout,ID_VIEW);for(unsigned i=0;i<6;++i)renderComparison(i==5?out/("view_"+std::to_string(layout)):fs::path{});}
 select(viewCombo,0,ID_VIEW);select(qualityCombo[0],1,ID_LEFT_QUALITY);select(phoenixCombo[0],2,ID_LEFT_PHOENIX);select(qualityCombo[1],2,ID_RIGHT_QUALITY);select(phoenixCombo[1],0,ID_RIGHT_PHOENIX);
 for(unsigned i=0;i<6;++i)renderComparison(i==5?out/"changed_profiles":fs::path{});
 SendMessageW(panelHeader,BM_CLICK,0,0);ensure(!panelExpanded,"Panel must collapse");renderComparison(out/"collapsed");SendMessageW(panelHeader,BM_CLICK,0,0);ensure(panelExpanded,"Panel must expand");
 clientSize(729,973);renderComparison(out/"portrait");clientSize(963,541);renderComparison(out/"odd_size");
 SendMessageW(window,WM_KEYDOWN,'G',0);ensure(viewQuality[0]==1&&viewQuality[1]==2,"Scene hotkeys must be disabled");
 select(qualityCombo[0],1,ID_LEFT_QUALITY);select(qualityCombo[1],1,ID_RIGHT_QUALITY);select(phoenixCombo[0],1,ID_LEFT_PHOENIX);select(phoenixCombo[1],2,ID_RIGHT_PHOENIX);uiAnimation=true;
 for(unsigned i=0;i<8;++i)renderComparison(i==0||i==7?out/("both_neural_"+std::to_string(i)):fs::path{});
 select(qualityCombo[0],0,ID_LEFT_QUALITY);renderComparison(out/"left_changed_only");
 SendMessageW(GetDlgItem(panelBody,ID_SAVE_IMAGE),BM_CLICK,0,0);ensure(pendingImage,"Save image button must request a capture");renderComparison();
 SendMessageW(GetDlgItem(panelBody,ID_ROTATE_LEFT),BM_CLICK,0,0);renderComparison(out/"camera_button");SendMessageW(GetDlgItem(panelBody,ID_RESET_CAMERA),BM_CLICK,0,0);ensure(yaw==.42f&&cameraRadius==7.6f,"Camera reset button");
 panelTesting=false;report["panel_check"]={{"passed",true},{"independent_contexts",true},{"collapsed",true},{"hotkeys_disabled",true},{"actual_controls",true},{"frames",comparisonFrames}};
 peer->moduleAudit();report["peer"]=peer->report;
}
