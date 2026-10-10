#pragma once
// Cached GDI text raster: Chinese glyphs without per-glyph GL draw calls.
void chinesePanel(const std::wstring& message){
 constexpr int width=372,height=174;
 static std::wstring cached;static DWORD tick=0;static std::vector<DWORD> pixels(width*height);
 static HGLRC context=nullptr;static GLuint texture=0;static IDirect3DTexture9* dxTexture=nullptr;static IDirect3DDevice9* dxOwner=nullptr;
 bool dirty=cached.empty()||(cached!=message&&GetTickCount()-tick>=150);
 if(dirty){
  HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=width;bi.bmiHeader.biHeight=-height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;
  HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
  if(!dc||!bitmap||!bits){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return;}
  HGDIOBJ old=SelectObject(dc,bitmap);RECT rect{0,0,width,height};HBRUSH background=CreateSolidBrush(RGB(17,29,40));FillRect(dc,&rect,background);DeleteObject(background);
  HFONT font=CreateFontW(-17,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Microsoft YaHei UI");HGDIOBJ oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(223,236,243));
  rect={14,10,width-12,height-8};DrawTextW(dc,message.c_str(),-1,&rect,DT_LEFT|DT_TOP|DT_NOPREFIX);
  GdiFlush();memcpy(pixels.data(),bits,pixels.size()*sizeof(DWORD));for(auto&pixel:pixels)pixel|=0xff000000;
  SelectObject(dc,oldFont);DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);cached=message;tick=GetTickCount();
 }
 if(glDrawing){
  glEnd();HGLRC current=wglGetCurrentContext();if(context!=current){context=current;texture=0;dirty=true;}
  if(!texture){glGenTextures(1,&texture);dirty=true;}
  glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
  glPixelStorei(GL_UNPACK_ALIGNMENT,4);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
  if(dirty)glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,0x80E1,GL_UNSIGNED_BYTE,pixels.data());
  glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();glMatrixMode(GL_MODELVIEW);glColor4ub(255,255,255,245);glBegin(GL_TRIANGLE_STRIP);
  glTexCoord2f(0,0);glVertex2f(18,18);glTexCoord2f(1,0);glVertex2f(18+width,18);glTexCoord2f(0,1);glVertex2f(18,18+height);glTexCoord2f(1,1);glVertex2f(18+width,18+height);glEnd();
  glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(GL_MODELVIEW);glDisable(GL_TEXTURE_2D);glBegin(GL_TRIANGLES);
 }else if(drawing){
  if(dxOwner!=drawing){if(dxTexture)dxTexture->Release();dxTexture=nullptr;dxOwner=drawing;}
  if(!dxTexture){if(FAILED(drawing->CreateTexture(width,height,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&dxTexture,nullptr)))return;dirty=true;}
  if(dirty){D3DLOCKED_RECT lock{};if(SUCCEEDED(dxTexture->LockRect(0,&lock,nullptr,0))){for(int y=0;y<height;y++)memcpy((char*)lock.pBits+y*lock.Pitch,pixels.data()+y*width,width*4);dxTexture->UnlockRect(0);}}
  struct TV{float x,y,z,w;DWORD c;float u,v;};TV v[4]={{17.5f,17.5f,0,1,0xffffffff,0,0},{17.5f+width,17.5f,0,1,0xffffffff,1,0},{17.5f,17.5f+height,0,1,0xffffffff,0,1},{17.5f+width,17.5f+height,0,1,0xffffffff,1,1}};
  drawing->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1);drawing->SetTexture(0,dxTexture);drawing->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);drawing->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);drawing->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,sizeof(TV));
 }
}

