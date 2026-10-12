// Export our own renderer/control pixels without reading or showing the desktop.
void saveHiddenControls(const fs::path& out){
 RECT extent{};GetClientRect(panel,&extent);const unsigned width=unsigned(extent.right),height=unsigned(extent.bottom);
 HDC dc=CreateCompatibleDC(nullptr);ensure(dc,"Offline panel memory DC");BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=LONG(width);bi.bmiHeader.biHeight=-LONG(height);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 void* data=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&data,nullptr,0);ensure(bitmap&&data,"Offline panel bitmap");auto previous=SelectObject(dc,bitmap);FillRect(dc,&extent,panelBrush);
 auto paint=[&](HWND control,bool body){
  RECT r{};GetClientRect(control,&r);MapWindowPoints(control,panel,reinterpret_cast<POINT*>(&r),2);int saved=SaveDC(dc);
  if(body){RECT clip{};GetClientRect(panelBody,&clip);MapWindowPoints(panelBody,panel,reinterpret_cast<POINT*>(&clip),2);IntersectClipRect(dc,clip.left,clip.top,clip.right,clip.bottom);}
  SetViewportOrgEx(dc,r.left,r.top,nullptr);SendMessageW(control,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND|PRF_CHILDREN);
  // Hidden themed dropdowns print their border but omit selected text. Draw
  // that text from the real widget selection into the same memory DC.
  wchar_t cls[32]{};GetClassNameW(control,cls,32);
  if(_wcsicmp(cls,L"ComboBox")==0){auto choice=SendMessageW(control,CB_GETCURSEL,0,0);if(choice!=CB_ERR){auto length=SendMessageW(control,CB_GETLBTEXTLEN,choice,0);std::vector<wchar_t> label(size_t(length)+1);SendMessageW(control,CB_GETLBTEXT,choice,reinterpret_cast<LPARAM>(label.data()));RECT text{};GetClientRect(control,&text);text.left=4;text.right-=22;text.bottom=24;SelectObject(dc,panelFont);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(225,231,239));DrawTextW(dc,label.data(),-1,&text,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|DT_END_ELLIPSIS);}}
  RestoreDC(dc,saved);
 };
 paint(panelHeader,false);paint(panelStats,false);for(const auto& c:panelWidgets)paint(c.handle,true);GdiFlush();
 std::vector<BYTE> rgb(size_t(width)*height*3);auto* bgra=static_cast<BYTE*>(data);
 for(size_t i=0;i<size_t(width)*height;++i){rgb[i*3]=bgra[i*4+2];rgb[i*3+1]=bgra[i*4+1];rgb[i*3+2]=bgra[i*4];}
 saveRGBPNG(rgb,width,height,out/"controls.png");SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
}
void readmeCapture(const fs::path& out){
 ensure(!IsWindowVisible(window)&&!IsWindowVisible(panel)&&!IsWindowVisible(peer->window),"README capture windows must stay hidden");
 clientSize(1920,1080);panelTesting=true;uiAnimation=false;uiLights=false;sceneTime=.9f;lightPhase=.63f;yaw=.38f;cameraRadius=4.8f;
 SendMessageW(animationCheck,BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(lightsCheck,BM_SETCHECK,BST_UNCHECKED,0);
 auto condition=[&](unsigned mode,const char* name){
  SendMessageW(phoenixCombo[1],CB_SETCURSEL,mode,0);panelCommand(ID_RIGHT_PHOENIX,CBN_SELCHANGE);
  for(unsigned i=0;i<17;++i){renderComparison(i==16?out/name:fs::path{},false);ensure(!IsWindowVisible(window)&&!IsWindowVisible(panel)&&!IsWindowVisible(peer->window),"Offscreen capture must never reveal a window");}
 };
 condition(0,"native");condition(2,"full_frame");condition(1,"protected");
 SetWindowTextW(panelStats,L"Offline capture  |  1920 x 1080");layoutPanel();saveHiddenControls(out);
 report["readme_capture"]={{"passed",true},{"windows_hidden",true},{"desktop_capture",false},{"presentation",false},{"scene_capture","GPU texture readback"},{"controls_capture","Our hidden controls print into a memory bitmap"},{"comparison_frames",comparisonFrames}};
 peer->moduleAudit();report["peer"]=peer->report;
}
