void resizeCheck(const fs::path& out){
 // Exercise real Win32 client extents, the native swapchain, and complete inference.
 displayMode=1;shading=1;materialScope=false;sceneTime=0;
 const unsigned sizes[][2]={{1280,720},{963,541},{1537,863},{729,973},{1920,1080},{640,384},{2560,1080},{3840,2160},{1280,720}};
 unsigned step=0;
 for(const auto& extent:sizes){clientSize(extent[0],extent[1]);shading=step%3;enabled=true;displayMode=1;
  for(unsigned i=0;i<6;++i){sceneTime+=i?1.f/24:0;render(cases[0],i==0||i==5?out/("resize_"+std::to_string(step)+"_"+std::to_string(i)):fs::path{},false,true);report["frames"].back()["resize_step"]=step;}
  if(step==2){enabled=false;render(cases[0],out/"resized_bypass",false,true);enabled=true;reset=true;render(cases[0],out/"resized_reenabled",false,true);}
  ++step;
 }
 // The normal loop must render no frames while minimized, then reset on restore.
 const unsigned before=serial;ShowWindow(window,SW_MINIMIZE);ShowWindow(window,SW_MINIMIZE);ensure(!updateWindow()&&minimized&&serial==before,"Minimized window must suspend rendering");
 report["minimize_check"]={{"passed",true},{"frames_submitted",serial-before}};
 ShowWindow(window,SW_RESTORE);ensure(updateWindow()&&!minimized&&reset,"Restore must reset history");render(cases[0],out/"restored",false,true);
 ShowWindow(window,SW_MAXIMIZE);ensure(updateWindow(),"Maximized window must be drawable");RECT client{};GetClientRect(window,&client);ensure(W==unsigned(client.right)&&H==unsigned(client.bottom),"Maximized client and render must match");render(cases[0],out/"maximized",false,true);
 ShowWindow(window,SW_RESTORE);ensure(updateWindow(),"Restored window must be drawable");render(cases[0],out/"unmaximized",false,true);
 report["resize_protocol"]={{"requested_client_sizes",9},{"includes_odd_extent",true},{"includes_portrait",true},{"includes_4k",true},{"includes_minimize_restore_maximize",true},{"all_learned_stages",154},{"network_fit","Generate a native recurrent launch plan at the exact client extent. Scene, motion and neural output share dimensions; no fitting or resampling."}};
 ShowWindow(window,SW_HIDE);
}
