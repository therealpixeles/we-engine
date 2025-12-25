#pragma once
// wineng.hpp — single-header Win32 2D engine (C++17), software renderer, ECS, tile physics, lighting, UI, WIC images
//
// Build (MinGW g++):
//   g++ main.cpp -O2 -std=c++17 -Wall -Wextra -lgdi32 -luser32 -lole32 -luuid -lwindowscodecs
//
// Notes:
// - Uses Win32 for window/input only.
// - Software backbuffer rendering (manual pixels).
// - Image loading uses WIC (built-in Windows codecs): png/jpg/bmp/...

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <wincodec.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef WE_ASSERT
#define WE_ASSERT(x) do { if(!(x)) { *(volatile int*)0=0; } } while(0)
#endif

namespace we {

// ============================================================
// Basic types
// ============================================================
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using i32 = int32_t;
using f32 = float;

// ============================================================
// Color (0xAARRGGBB)
// ============================================================
static inline u32 RGBA(u32 r,u32 g,u32 b,u32 a){ return ((a&255u)<<24)|((r&255u)<<16)|((g&255u)<<8)|((b&255u)<<0); }
static inline u32 A(u32 c){ return (c>>24)&255u; }
static inline u32 R(u32 c){ return (c>>16)&255u; }
static inline u32 G(u32 c){ return (c>> 8)&255u; }
static inline u32 B(u32 c){ return (c>> 0)&255u; }

static inline u32 blend_over(u32 dst, u32 src){
    u32 sa=A(src);
    if(sa==255) return src;
    if(sa==0) return dst;
    u32 inv=255u-sa;
    u32 rr=(R(src)*sa + R(dst)*inv)/255u;
    u32 gg=(G(src)*sa + G(dst)*inv)/255u;
    u32 bb=(B(src)*sa + B(dst)*inv)/255u;
    return RGBA(rr,gg,bb,255);
}

// ============================================================
// Scary-looking math (useful, but intimidating 😈)
// ============================================================
static inline f32 minf(f32 a,f32 b){ return a<b?a:b; }
static inline f32 maxf(f32 a,f32 b){ return a>b?a:b; }
static inline f32 clampf(f32 x,f32 a,f32 b){ return x<a?a:(x>b?b:x); }
static inline i32 clampi(i32 x,i32 a,i32 b){ return x<a?a:(x>b?b:x); }

static inline f32 lerp(f32 a,f32 b,f32 t){ return a + (b-a)*t; }
static inline f32 fract(f32 x){ return x - std::floor(x); }

static inline f32 smoothstep(f32 t){ t=clampf(t,0,1); return t*t*(3.0f-2.0f*t); }
static inline f32 smootherstep(f32 t){ t=clampf(t,0,1); return t*t*t*(t*(t*6-15)+10); }
static inline f32 ease_out_expo(f32 t){ t=clampf(t,0,1); return (t>=1)?1.0f:1.0f-std::pow(2.0f,-10.0f*t); }
static inline f32 ease_in_out_cubic(f32 t){ t=clampf(t,0,1); return (t<0.5f)?4*t*t*t:1.0f-std::pow(-2*t+2,3)/2; }

struct v2 { f32 x=0, y=0; };
struct v3 { f32 x=0, y=0, z=0; };
struct v4 { f32 x=0, y=0, z=0, w=0; };

static inline v2 V2(f32 x,f32 y){ return {x,y}; }
static inline v3 V3(f32 x,f32 y,f32 z){ return {x,y,z}; }
static inline v4 V4(f32 x,f32 y,f32 z,f32 w){ return {x,y,z,w}; }

static inline v2 add(v2 a,v2 b){ return {a.x+b.x,a.y+b.y}; }
static inline v2 sub(v2 a,v2 b){ return {a.x-b.x,a.y-b.y}; }
static inline v2 mul(v2 a,f32 s){ return {a.x*s,a.y*s}; }
static inline f32 dot(v2 a,v2 b){ return a.x*b.x + a.y*b.y; }
static inline f32 len2(v2 a){ return dot(a,a); }
static inline f32 len(v2 a){ return std::sqrt(len2(a)); }
static inline v2 norm(v2 a){ f32 L=len(a); return (L>0)?mul(a,1.0f/L):v2{0,0}; }

struct m3 {
    // 2D affine in 3x3
    f32 m[3][3]{};
};

static inline m3 m3_identity(){
    m3 M{};
    M.m[0][0]=1; M.m[1][1]=1; M.m[2][2]=1;
    return M;
}
static inline m3 m3_translate(f32 tx,f32 ty){
    m3 M=m3_identity();
    M.m[0][2]=tx; M.m[1][2]=ty;
    return M;
}
static inline m3 m3_scale(f32 sx,f32 sy){
    m3 M=m3_identity();
    M.m[0][0]=sx; M.m[1][1]=sy;
    return M;
}
static inline m3 m3_rotate(f32 r){
    f32 c=std::cos(r), s=std::sin(r);
    m3 M=m3_identity();
    M.m[0][0]=c;  M.m[0][1]=-s;
    M.m[1][0]=s;  M.m[1][1]=c;
    return M;
}
static inline m3 m3_mul(const m3& A,const m3& B){
    m3 R{};
    for(int i=0;i<3;i++) for(int j=0;j<3;j++)
        R.m[i][j]=A.m[i][0]*B.m[0][j]+A.m[i][1]*B.m[1][j]+A.m[i][2]*B.m[2][j];
    return R;
}
static inline v2 m3_mul_v2(const m3& M, v2 v){
    f32 x=M.m[0][0]*v.x + M.m[0][1]*v.y + M.m[0][2];
    f32 y=M.m[1][0]*v.x + M.m[1][1]*v.y + M.m[1][2];
    return {x,y};
}
static inline bool m3_inverse_affine(const m3& M, m3& out){
    // [ a b tx ]
    // [ c d ty ]
    // [ 0 0  1 ]
    f32 a=M.m[0][0], b=M.m[0][1], tx=M.m[0][2];
    f32 c=M.m[1][0], d=M.m[1][1], ty=M.m[1][2];
    f32 det = a*d - b*c;
    if(std::fabs(det) < 1e-8f) return false;
    f32 inv=1.0f/det;

    out=m3_identity();
    out.m[0][0]= d*inv;
    out.m[0][1]=-b*inv;
    out.m[1][0]=-c*inv;
    out.m[1][1]= a*inv;

    out.m[0][2]=-(out.m[0][0]*tx + out.m[0][1]*ty);
    out.m[1][2]=-(out.m[1][0]*tx + out.m[1][1]*ty);
    return true;
}

// ============================================================
// Input (edge-based, reliable buttons)
// ============================================================
struct Input {
    int mouse_x=0, mouse_y=0;
    int mouse_dx=0, mouse_dy=0;
    int wheel=0;

    std::array<bool,256> key{};
    std::array<bool,256> key_pressed{};
    std::array<bool,256> key_released{};

    std::array<bool,5> mouse{};
    std::array<bool,5> mouse_pressed{};
    std::array<bool,5> mouse_released{};

    void clear_edges(){
        key_pressed.fill(false);
        key_released.fill(false);
        mouse_pressed.fill(false);
        mouse_released.fill(false);
        mouse_dx=0; mouse_dy=0; wheel=0;
    }
};

// ============================================================
// Canvas + primitives
// ============================================================
struct RectI { int x0=0,y0=0,x1=0,y1=0; };

struct Canvas {
    u32* pix=nullptr;
    int w=0,h=0,stride=0;
    RectI clip{};

    void set(u32* p,int W,int H,int S){
        pix=p; w=W; h=H; stride=S;
        clip = {0,0,w-1,h-1};
    }
    void clip_reset(){ clip={0,0,w-1,h-1}; }
    void clip_set(int x,int y,int W,int H){
        RectI r{x,y,x+W-1,y+H-1};
        r.x0=std::max(r.x0,0); r.y0=std::max(r.y0,0);
        r.x1=std::min(r.x1,w-1); r.y1=std::min(r.y1,h-1);
        clip=r;
    }

    void clear(u32 c){
        for(int y=0;y<h;y++){
            u32* row=pix + y*stride;
            for(int x=0;x<w;x++) row[x]=c;
        }
    }

    void rect_fill(int x,int y,int W,int H,u32 c){
        if(W==0||H==0) return;
        int x0=x, y0=y, x1=x+W-1, y1=y+H-1;
        if(x0>x1) std::swap(x0,x1);
        if(y0>y1) std::swap(y0,y1);

        x0=std::max(x0,clip.x0); y0=std::max(y0,clip.y0);
        x1=std::min(x1,clip.x1); y1=std::min(y1,clip.y1);
        if(x0>x1||y0>y1) return;

        u32 a=A(c);
        for(int yy=y0;yy<=y1;yy++){
            u32* row=pix + yy*stride;
            if(a==255){
                for(int xx=x0;xx<=x1;xx++) row[xx]=c;
            }else{
                for(int xx=x0;xx<=x1;xx++) row[xx]=blend_over(row[xx],c);
            }
        }
    }

    void rect_outline(int x,int y,int W,int H,int t,u32 c){
        if(t<=0) return;
        rect_fill(x,y,W,t,c);
        rect_fill(x,y+H-t,W,t,c);
        rect_fill(x,y,t,H,c);
        rect_fill(x+W-t,y,t,H,c);
    }

    void line(int x0,int y0,int x1,int y1,u32 c){
        int dx=std::abs(x1-x0), sx=x0<x1?1:-1;
        int dy=-std::abs(y1-y0), sy=y0<y1?1:-1;
        int err=dx+dy;
        for(;;){
            if(x0>=clip.x0 && x0<=clip.x1 && y0>=clip.y0 && y0<=clip.y1){
                u32* p = pix + y0*stride + x0;
                *p = blend_over(*p,c);
            }
            if(x0==x1 && y0==y1) break;
            int e2=2*err;
            if(e2>=dy){ err+=dy; x0+=sx; }
            if(e2<=dx){ err+=dx; y0+=sy; }
        }
    }

    void circle_fill(int cx,int cy,int r,u32 c){
        if(r<=0) return;
        int x0=std::max(cx-r,clip.x0), x1=std::min(cx+r,clip.x1);
        int y0=std::max(cy-r,clip.y0), y1=std::min(cy+r,clip.y1);
        int rr=r*r;
        for(int y=y0;y<=y1;y++){
            int dy=y-cy; int dy2=dy*dy;
            u32* row=pix + y*stride;
            for(int x=x0;x<=x1;x++){
                int dx=x-cx;
                if(dx*dx + dy2 <= rr) row[x]=blend_over(row[x],c);
            }
        }
    }
};

// ============================================================
// Built-in bitmap font (robust, spacing fixed, no random '?')
// ============================================================
static inline void glyph5x7(char ch, u8 out[7]){
    // uppercase
    if(ch>='a' && ch<='z') ch = char(ch - 'a' + 'A');

    // default '?'
    u8 q[7]={0x0E,0x11,0x02,0x04,0x04,0x00,0x04};
    std::memcpy(out,q,7);

    auto set=[&](std::initializer_list<u8> rows){
        int i=0;
        for(u8 v: rows){ out[i++]=v; }
    };

    switch(ch){
        case ' ': set({0,0,0,0,0,0,0}); return;
        case '.': set({0,0,0,0,0,0,0x04}); return;
        case ',': set({0,0,0,0,0x04,0x04,0x08}); return;
        case '!': set({0x04,0x04,0x04,0x04,0x04,0,0x04}); return;
        case '-': set({0,0,0,0x1F,0,0,0}); return;
        case '+': set({0,0x04,0x04,0x1F,0x04,0x04,0}); return;
        case ':': set({0,0x04,0,0,0x04,0,0}); return;
        case '/': set({0x01,0x02,0x04,0x08,0x10,0,0}); return;

        case '0': set({0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}); return;
        case '1': set({0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}); return;
        case '2': set({0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}); return;
        case '3': set({0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}); return;
        case '4': set({0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}); return;
        case '5': set({0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}); return;
        case '6': set({0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}); return;
        case '7': set({0x1F,0x01,0x02,0x04,0x08,0x08,0x08}); return;
        case '8': set({0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}); return;
        case '9': set({0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}); return;

        case 'A': set({0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}); return;
        case 'B': set({0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}); return;
        case 'C': set({0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}); return;
        case 'D': set({0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}); return;
        case 'E': set({0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}); return;
        case 'F': set({0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}); return;
        case 'G': set({0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}); return;
        case 'H': set({0x11,0x11,0x11,0x1F,0x11,0x11,0x11}); return;
        case 'I': set({0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}); return;
        case 'J': set({0x07,0x02,0x02,0x02,0x12,0x12,0x0C}); return;
        case 'K': set({0x11,0x12,0x14,0x18,0x14,0x12,0x11}); return;
        case 'L': set({0x10,0x10,0x10,0x10,0x10,0x10,0x1F}); return;
        case 'M': set({0x11,0x1B,0x15,0x15,0x11,0x11,0x11}); return;
        case 'N': set({0x11,0x19,0x15,0x13,0x11,0x11,0x11}); return;
        case 'O': set({0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}); return;
        case 'P': set({0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}); return;
        case 'Q': set({0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}); return;
        case 'R': set({0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}); return;
        case 'S': set({0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}); return;
        case 'T': set({0x1F,0x04,0x04,0x04,0x04,0x04,0x04}); return;
        case 'U': set({0x11,0x11,0x11,0x11,0x11,0x11,0x0E}); return;
        case 'V': set({0x11,0x11,0x11,0x11,0x0A,0x0A,0x04}); return;
        case 'W': set({0x11,0x11,0x11,0x15,0x15,0x15,0x0A}); return;
        case 'X': set({0x11,0x0A,0x0A,0x04,0x0A,0x0A,0x11}); return;
        case 'Y': set({0x11,0x11,0x0A,0x04,0x04,0x04,0x04}); return;
        case 'Z': set({0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}); return;
    }
}

static inline int text_width(const char* s,int scale){
    int adv=6*scale;
    int w=0, best=0;
    for(const char* p=s; *p; ++p){
        char ch=*p;
        if(ch=='\r') continue;
        if(ch=='\n'){ best=std::max(best,w); w=0; continue; }
        if(ch=='\t'){ w += 4*adv; continue; }
        w += adv;
    }
    best=std::max(best,w);
    return best;
}
static inline int text_line_h(int scale){ return 7*scale; }

static inline void draw_text(Canvas& c,int x,int y,int scale,u32 col,const char* s){
    int adv=6*scale;
    int lh=text_line_h(scale);
    int gap=2*scale;

    int cx=x, cy=y;
    for(const char* p=s; *p; ++p){
        char ch=*p;
        if(ch=='\r') continue;
        if(ch=='\n'){ cx=x; cy += lh + gap; continue; }
        if(ch=='\t'){ cx += 4*adv; continue; }

        u8 rows[7]; glyph5x7(ch, rows);

        for(int ry=0; ry<7; ry++){
            u8 bits=rows[ry];
            for(int rx=0; rx<5; rx++){
                bool on = (bits & (1u << (4-rx))) != 0;
                if(on){
                    c.rect_fill(cx + rx*scale, cy + ry*scale, scale, scale, col);
                }
            }
        }
        cx += adv;
    }
}

// ============================================================
// WIC image loader (PNG/JPG/BMP/...)
// ============================================================

struct Image {
    int w=0,h=0;
    std::vector<u32> px; // AARRGGBB

    bool empty() const { return w<=0 || h<=0 || px.empty(); }
};

struct WIC {
    IWICImagingFactory* fac = nullptr;
    bool com_inited = false;
    bool com_need_uninit = false;

    bool init() {
        if (!com_inited) {
            HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            com_inited = true;
            if (SUCCEEDED(hr)) com_need_uninit = true;
        }
        if (!fac) {
            HRESULT hr = CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_IWICImagingFactory,
                (void**)&fac
            );
            if (FAILED(hr) || !fac) return false;
        }
        return true;
    }

    void shutdown() {
        if (fac) { fac->Release(); fac = nullptr; }
        if (com_need_uninit) { CoUninitialize(); com_need_uninit = false; }
    }

    static bool mb_to_wide(const char* src, std::wstring& out) {
        int need = MultiByteToWideChar(CP_UTF8, 0, src, -1, nullptr, 0);
        UINT cp = CP_UTF8;
        if (need == 0) {
            cp = CP_ACP;
            need = MultiByteToWideChar(cp, 0, src, -1, nullptr, 0);
            if (need == 0) return false;
        }
        out.resize((size_t)need);
        MultiByteToWideChar(cp, 0, src, -1, out.data(), need);
        return true;
    }

    bool load(Image& out, const char* path) {
        out = {};
        if (!init()) return false;

        std::wstring wpath;
        if (!mb_to_wide(path, wpath)) return false;

        IWICBitmapDecoder* dec = nullptr;
        HRESULT hr = fac->CreateDecoderFromFilename(
            wpath.c_str(),
            nullptr,
            GENERIC_READ,
            WICDecodeMetadataCacheOnLoad,
            &dec
        );
        if (FAILED(hr) || !dec) return false;

        IWICBitmapFrameDecode* frame = nullptr;
        hr = dec->GetFrame(0, &frame);
        if (FAILED(hr) || !frame) { dec->Release(); return false; }

        IWICFormatConverter* conv = nullptr;
        hr = fac->CreateFormatConverter(&conv);
        if (FAILED(hr) || !conv) { frame->Release(); dec->Release(); return false; }

        hr = conv->Initialize(
            frame,
            GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom
        );
        if (FAILED(hr)) { conv->Release(); frame->Release(); dec->Release(); return false; }

        UINT W = 0, H = 0;
        hr = conv->GetSize(&W, &H);
        if (FAILED(hr) || W == 0 || H == 0) {
            conv->Release(); frame->Release(); dec->Release();
            return false;
        }

        out.w = (int)W;
        out.h = (int)H;
        out.px.assign((size_t)W * (size_t)H, 0);

        const UINT stride = W * 4u;
        std::vector<u8> tmp((size_t)stride * (size_t)H);

        hr = conv->CopyPixels(nullptr, stride, (UINT)tmp.size(), tmp.data());
        if (FAILED(hr)) {
            conv->Release(); frame->Release(); dec->Release();
            out = {};
            return false;
        }

        // BGRA -> AARRGGBB
        for (UINT y = 0; y < H; y++) {
            const u8* row = tmp.data() + (size_t)y * stride;
            for (UINT x = 0; x < W; x++) {
                const u8* p = row + x * 4u;
                u8 B_ = p[0], G_ = p[1], R_ = p[2], A_ = p[3];
                out.px[(size_t)y * (size_t)W + x] = RGBA(R_, G_, B_, A_);
            }
        }

        conv->Release();
        frame->Release();
        dec->Release();
        return true;
    }
};

// ============================================================
// Blitting (nearest/bilinear) + tint
// ============================================================
static inline u32 texel_clamp(const Image& img,int x,int y){
    x=clampi(x,0,img.w-1);
    y=clampi(y,0,img.h-1);
    return img.px[(size_t)y*(size_t)img.w + (size_t)x];
}
static inline u32 sample_bilinear(const Image& img,f32 u,f32 v){
    int x0=(int)std::floor(u), y0=(int)std::floor(v);
    int x1=x0+1, y1=y0+1;
    f32 tx=u-x0, ty=v-y0;

    u32 c00=texel_clamp(img,x0,y0);
    u32 c10=texel_clamp(img,x1,y0);
    u32 c01=texel_clamp(img,x0,y1);
    u32 c11=texel_clamp(img,x1,y1);

    auto lf=[&](u32 a,u32 b,f32 t){ return lerp((f32)a,(f32)b,t); };
    f32 r0=lf(R(c00),R(c10),tx), g0=lf(G(c00),G(c10),tx), b0=lf(B(c00),B(c10),tx), a0=lf(A(c00),A(c10),tx);
    f32 r1=lf(R(c01),R(c11),tx), g1=lf(G(c01),G(c11),tx), b1=lf(B(c01),B(c11),tx), a1=lf(A(c01),A(c11),tx);

    u32 rr=(u32)lerp(r0,r1,ty);
    u32 gg=(u32)lerp(g0,g1,ty);
    u32 bb=(u32)lerp(b0,b1,ty);
    u32 aa=(u32)lerp(a0,a1,ty);
    return RGBA(rr,gg,bb,aa);
}
static inline u32 mul_color(u32 c,u32 tint){
    // multiply RGB, alpha keeps src alpha * tint alpha
    u32 rr=(R(c)*R(tint))/255u;
    u32 gg=(G(c)*G(tint))/255u;
    u32 bb=(B(c)*B(tint))/255u;
    u32 aa=(A(c)*A(tint))/255u;
    return RGBA(rr,gg,bb,aa);
}

static inline void blit(Canvas& dst, int dx,int dy,int dw,int dh,
                        const Image& img, int sx,int sy,int sw,int sh,
                        bool blend=true, bool bilinear=true, u32 tint=RGBA(255,255,255,255))
{
    if(img.empty()) return;
    if(dw==0||dh==0||sw<=0||sh<=0) return;

    int x0=dx, y0=dy, x1=dx+dw-1, y1=dy+dh-1;
    if(x0>x1) std::swap(x0,x1);
    if(y0>y1) std::swap(y0,y1);

    x0=std::max(x0,dst.clip.x0); y0=std::max(y0,dst.clip.y0);
    x1=std::min(x1,dst.clip.x1); y1=std::min(y1,dst.clip.y1);
    if(x0>x1||y0>y1) return;

    for(int y=y0;y<=y1;y++){
        f32 v=(f32)(y-dy)/(f32)dh;
        f32 py=(f32)sy + v*(f32)sh;
        u32* row = dst.pix + y*dst.stride;

        for(int x=x0;x<=x1;x++){
            f32 u=(f32)(x-dx)/(f32)dw;
            f32 px=(f32)sx + u*(f32)sw;

            u32 src = bilinear ? sample_bilinear(img,px,py) : texel_clamp(img,(int)(px+0.5f),(int)(py+0.5f));
            src = mul_color(src,tint);

            if(!blend || A(src)==255) row[x]=src;
            else row[x]=blend_over(row[x],src);
        }
    }
}

// ============================================================
// Camera2D (fixed zoom + perfect screen_to_world)
// ============================================================
struct Camera2D {
    v2 pos{0,0};
    f32 zoom=1.0f;
    f32 rot=0.0f;
    v2 viewport{1280,720};

    m3 view() const {
        // screen = T(vp/2) * R(rot) * S(zoom) * T(-pos)
        m3 T1=m3_translate(viewport.x*0.5f, viewport.y*0.5f);
        m3 R_=m3_rotate(rot);
        m3 S_=m3_scale(zoom, zoom);
        m3 T0=m3_translate(-pos.x, -pos.y);
        return m3_mul(T1, m3_mul(R_, m3_mul(S_, T0)));
    }
    m3 inv_view() const {
        m3 V=view();
        m3 inv{};
        bool ok=m3_inverse_affine(V, inv);
        if(!ok) return m3_identity();
        return inv;
    }
    v2 screen_to_world(int sx,int sy) const {
        return m3_mul_v2(inv_view(), V2((f32)sx,(f32)sy));
    }
    v2 world_to_screen(v2 wp) const {
        return m3_mul_v2(view(), wp);
    }
};

// ============================================================
// Win32 App (window, backbuffer, timing, input)
// ============================================================
struct AppConfig {
    int w=1100, h=700;
    const wchar_t* title=L"wineng++";
    bool resizable=true;
};

struct App {
    HWND hwnd=nullptr;
    bool running=true;

    BITMAPINFO bmi{};
    Canvas fb{};

    LARGE_INTEGER qpf{};
    LARGE_INTEGER qpc_last{};
    f32 dt=0;

    Input in{};

    void* backbuf_mem=nullptr;
    size_t backbuf_bytes=0;

    WIC wic{};

    static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
        App* app=(App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

        switch(msg){
            case WM_CREATE:{
                CREATESTRUCTW* cs=(CREATESTRUCTW*)lp;
                app=(App*)cs->lpCreateParams;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
            } return 0;

            case WM_CLOSE:
                if(app) app->running=false;
                return 0;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            case WM_SIZE:
                if(app){
                    int cw=LOWORD(lp), ch=HIWORD(lp);
                    app->resize_backbuffer(cw,ch);
                }
                return 0;

            case WM_MOUSEMOVE:
                if(app){
                    int x=(short)LOWORD(lp), y=(short)HIWORD(lp);
                    app->in.mouse_dx += x - app->in.mouse_x;
                    app->in.mouse_dy += y - app->in.mouse_y;
                    app->in.mouse_x=x; app->in.mouse_y=y;
                }
                return 0;

            case WM_LBUTTONDOWN:
                if(app){
                    if(!app->in.mouse[0]) app->in.mouse_pressed[0]=true;
                    app->in.mouse[0]=true;
                    SetCapture(hwnd);
                } return 0;

            case WM_LBUTTONUP:
                if(app){
                    if(app->in.mouse[0]) app->in.mouse_released[0]=true;
                    app->in.mouse[0]=false;
                    ReleaseCapture();
                } return 0;

            case WM_RBUTTONDOWN:
                if(app){
                    if(!app->in.mouse[1]) app->in.mouse_pressed[1]=true;
                    app->in.mouse[1]=true;
                    SetCapture(hwnd);
                } return 0;

            case WM_RBUTTONUP:
                if(app){
                    if(app->in.mouse[1]) app->in.mouse_released[1]=true;
                    app->in.mouse[1]=false;
                    ReleaseCapture();
                } return 0;

            case WM_MOUSEWHEEL:
                if(app) app->in.wheel += GET_WHEEL_DELTA_WPARAM(wp);
                return 0;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if(app && wp<256){
                    if(!app->in.key[wp]) app->in.key_pressed[wp]=true;
                    app->in.key[wp]=true;
                } return 0;

            case WM_KEYUP:
            case WM_SYSKEYUP:
                if(app && wp<256){
                    if(app->in.key[wp]) app->in.key_released[wp]=true;
                    app->in.key[wp]=false;
                } return 0;
        }

        return DefWindowProcW(hwnd,msg,wp,lp);
    }

    void resize_backbuffer(int w,int h){
        if(w<=0||h<=0) return;
        if(backbuf_mem){
            VirtualFree(backbuf_mem,0,MEM_RELEASE);
            backbuf_mem=nullptr;
        }
        backbuf_bytes = (size_t)w*(size_t)h*4u;
        backbuf_mem = VirtualAlloc(nullptr, backbuf_bytes, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        WE_ASSERT(backbuf_mem);

        std::memset(&bmi,0,sizeof(bmi));
        bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth=w;
        bmi.bmiHeader.biHeight=-h;
        bmi.bmiHeader.biPlanes=1;
        bmi.bmiHeader.biBitCount=32;
        bmi.bmiHeader.biCompression=BI_RGB;

        fb.set((u32*)backbuf_mem, w, h, w);
    }

    bool init(const AppConfig& cfg){
        wic.init();

        QueryPerformanceFrequency(&qpf);
        QueryPerformanceCounter(&qpc_last);

        HINSTANCE inst=GetModuleHandleW(nullptr);

        WNDCLASSEXW wc{};
        wc.cbSize=sizeof(wc);
        wc.style=CS_HREDRAW|CS_VREDRAW;
        wc.lpfnWndProc=wndproc;
        wc.hInstance=inst;
        wc.hCursor=LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName=L"WINENGPP_CLASS_V1";
        RegisterClassExW(&wc);

        DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        if(!cfg.resizable) style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

        RECT r{0,0,cfg.w,cfg.h};
        AdjustWindowRect(&r, style, FALSE);

        hwnd = CreateWindowExW(0,wc.lpszClassName,cfg.title,style,
                               CW_USEDEFAULT,CW_USEDEFAULT,
                               r.right-r.left, r.bottom-r.top,
                               nullptr,nullptr,inst,(void*)this);
        if(!hwnd) return false;

        RECT cr{};
        GetClientRect(hwnd,&cr);
        resize_backbuffer(cr.right-cr.left, cr.bottom-cr.top);

        running=true;
        return true;
    }

    void shutdown(){
        if(backbuf_mem){
            VirtualFree(backbuf_mem,0,MEM_RELEASE);
            backbuf_mem=nullptr;
        }
        if(hwnd){
            DestroyWindow(hwnd);
            hwnd=nullptr;
        }
        wic.shutdown();
    }

    bool frame_begin(){
        if(!running) return false;

        in.clear_edges();

        MSG msg{};
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){ running=false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        double d=(double)(now.QuadPart - qpc_last.QuadPart) / (double)qpf.QuadPart;
        qpc_last=now;
        dt=(f32)d;
        if(dt>0.05f) dt=0.05f;

        return running;
    }

    void frame_end(){
        if(!hwnd || !fb.pix) return;

        HDC dc=GetDC(hwnd);
        RECT cr{}; GetClientRect(hwnd,&cr);
        int dw=cr.right-cr.left, dh=cr.bottom-cr.top;

        StretchDIBits(dc, 0,0,dw,dh, 0,0, fb.w,fb.h, fb.pix, &bmi, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(hwnd, dc);
    }
};

// ============================================================
// UI (immediate mode, fixed spacing/layout)
// ============================================================
static inline u32 fnv1a(const char* s){
    u32 h=2166136261u;
    while(*s){ h ^= (u8)*s++; h *= 16777619u; }
    return h;
}
static inline bool pt_in(int px,int py,int x,int y,int w,int h){
    return px>=x && py>=y && px<x+w && py<y+h;
}

struct UI {
    Canvas* c=nullptr;
    const Input* in=nullptr;

    int mx=0,my=0;
    bool md=false, mp=false, mr=false;

    u32 hot=0, active=0;
    u32 counter=0;

    struct WindowState {
        u32 id=0;
        int x=20,y=20,w=320,h=260;
        int cx=0, cy=0;
        bool open=true;
        bool dragging=false;
        int drag_off_x=0, drag_off_y=0;
    } win;

    std::array<RectI,16> clip_stack{};
    int clip_top=0;

    void begin(Canvas& canvas,const Input& input){
        c=&canvas; in=&input;
        mx=input.mouse_x; my=input.mouse_y;
        md=input.mouse[0]; mp=input.mouse_pressed[0]; mr=input.mouse_released[0];
        hot=0; counter=0;

        clip_top=0;
        clip_stack[clip_top++] = c->clip;
    }
    void end(){
        // restore clip
        if(clip_top>0) c->clip = clip_stack[0];
        if(mr) active=0;
        if(!md) active=0;
    }

    void push_clip(int x,int y,int w,int h){
        if(clip_top>= (int)clip_stack.size()) return;
        clip_stack[clip_top++] = c->clip;
        c->clip_set(x,y,w,h);
    }
    void pop_clip(){
        if(clip_top<=1) return;
        clip_top--;
        c->clip = clip_stack[clip_top-1];
    }

    bool window_begin(const char* title, int& x,int& y,int& w,int& h, bool& open){
        if(!open) return false;

        if(w<140) w=140;
        if(h<90)  h=90;

        const int th=28;
        const int pad=8;

        u32 id = fnv1a(title);
        win.id=id;

        // close button
        int cbw=18, cbh=18;
        int cbx = x + w - cbw - 6;
        int cby = y + (th-cbh)/2;

        u32 close_id = id ^ 0xC105Eu;

        bool close_hot = pt_in(mx,my,cbx,cby,cbw,cbh);
        if(mp && close_hot) active = close_id;
        if(mr && active==close_id && close_hot){
            open=false;
            return false;
        }

        // drag titlebar (excluding close button)
        bool in_title = pt_in(mx,my,x,y,w,th) && !close_hot;
        if(mp && in_title){
            active=id;
            win.dragging=true;
            win.drag_off_x = mx - x;
            win.drag_off_y = my - y;
        }
        if(!md) win.dragging=false;
        if(win.dragging && active==id){
            x = mx - win.drag_off_x;
            y = my - win.drag_off_y;
        }

        // draw
        c->rect_fill(x,y,w,h, RGBA(24,26,32,235));
        c->rect_fill(x,y,w,th, RGBA(32,35,44,245));
        c->rect_outline(x,y,w,h,1, RGBA(10,10,12,255));

        // title
        draw_text(*c, x+pad, y+6, 2, RGBA(235,235,240,255), title);

        // close visuals
        u32 cbc = close_hot ? RGBA(255,120,120,240) : RGBA(200,90,90,200);
        c->rect_fill(cbx,cby,cbw,cbh,cbc);
        c->rect_outline(cbx,cby,cbw,cbh,1,RGBA(10,10,12,255));
        draw_text(*c, cbx+5, cby+3, 2, RGBA(20,20,24,255), "X");

        // content
        int cx = x+pad;
        int cy = y+th+pad;
        int cw = w - 2*pad;
        int ch = h - th - 2*pad;

        push_clip(cx,cy,cw,ch);

        win.x=x; win.y=y; win.w=w; win.h=h;
        win.cx=cx; win.cy=cy;
        return true;
    }

    void window_end(){
        pop_clip();
    }

    void next_line(int h){
        win.cy += h + 6;
    }

    void label(const char* s){
        draw_text(*c, win.cx, win.cy, 2, RGBA(225,225,235,255), s);
        next_line(text_line_h(2));
    }

    bool button(const char* label, int w=160, int h=28){
        if(w<=0) w=160;
        if(h<=0) h=28;
        int x=win.cx, y=win.cy;

        u32 id = fnv1a(label) ^ (win.id*0x9E3779B1u) ^ (++counter);
        bool inside = pt_in(mx,my,x,y,w,h);

        if(inside) hot=id;
        if(mp && inside) active=id;

        bool clicked=false;
        if(mr && active==id && inside) clicked=true;

        u32 bg=RGBA(50,50,62,220);
        if(hot==id) bg=RGBA(70,70,88,235);
        if(active==id) bg=RGBA(90,90,112,245);

        c->rect_fill(x,y,w,h,bg);
        c->rect_outline(x,y,w,h,1,RGBA(10,10,12,255));

        int tw=text_width(label,2);
        int tx=x + (w-tw)/2;
        int ty=y + (h-text_line_h(2))/2;
        draw_text(*c, tx,ty,2, RGBA(235,235,240,255), label);

        next_line(h);
        return clicked;
    }

    bool checkbox(const char* label, bool& v){
        int box=18;
        int x=win.cx, y=win.cy;
        int w = box + 10 + text_width(label,2);
        int h = box;

        u32 id = fnv1a(label) ^ (win.id*0x85EBCA77u) ^ (++counter);
        bool inside = pt_in(mx,my,x,y,w,h);
        if(inside) hot=id;
        if(mp && inside) active=id;

        bool toggled=false;
        if(mr && active==id && inside){
            v=!v; toggled=true;
        }

        c->rect_fill(x,y,box,box,RGBA(36,38,45,230));
        c->rect_outline(x,y,box,box,1,RGBA(10,10,12,255));
        if(v) c->rect_fill(x+4,y+4,box-8,box-8,RGBA(120,200,255,245));

        draw_text(*c, x+box+10,y+2,2,RGBA(225,225,235,255), label);
        next_line(box);
        return toggled;
    }

    f32 sliderf(const char* label, f32 v, f32 vmin, f32 vmax, int w=240, int h=18){
        if(w<=0) w=240;
        if(h<=0) h=18;

        int x=win.cx, y=win.cy;
        draw_text(*c, x,y-18,2,RGBA(220,220,230,255), label);

        u32 id = fnv1a(label) ^ (win.id*0x27D4EB2Du) ^ (++counter);
        bool inside = pt_in(mx,my,x,y,w,h);
        if(inside) hot=id;
        if(mp && inside) active=id;

        f32 t = (v - vmin) / (vmax - vmin);
        t = clampf(t,0,1);

        if(active==id && md){
            f32 nt = (f32)(mx - x)/(f32)w;
            nt = clampf(nt,0,1);
            v = vmin + nt*(vmax-vmin);
            t = nt;
        }

        c->rect_fill(x,y,w,h,RGBA(36,38,45,230));
        int knob = std::max(h,10);
        int kx = x + (int)((w-knob)*t);
        u32 kc = (active==id)?RGBA(190,230,255,255):((hot==id)?RGBA(160,215,255,245):RGBA(120,200,255,235));
        c->rect_fill(kx,y,knob,h,kc);
        c->rect_outline(x,y,w,h,1,RGBA(10,10,12,255));

        next_line(h+18);
        return v;
    }
};

// ============================================================
// World (chunked tiles) + physics helper
// ============================================================
static constexpr int CHUNK = 32;

struct Tileset {
    const Image* img=nullptr;
    int tile_w=16, tile_h=16;
    int cols=0, rows=0;
};

static inline Tileset make_tileset(const Image* img,int tw,int th){
    Tileset t{};
    t.img=img; t.tile_w=tw; t.tile_h=th;
    if(img && tw>0 && th>0){
        t.cols = img->w / tw;
        t.rows = img->h / th;
    }
    return t;
}

struct Chunk {
    int cx=0, cy=0;
    std::vector<u16> tiles; // layer0 only for simplicity
    bool used=false;
};

struct World {
    Tileset ts{};
    int tile_px=32; // world pixels per tile
    u32 seed=0xC0FFEEu;

    std::unordered_map<long long, Chunk> map; // key->chunk
    bool bilinear=true;
    bool blend=true;

    // solid rule
    bool solid(u16 t) const { return t!=0; }

    static inline long long key(int cx,int cy){
        return ( (long long)(u32)cx << 32 ) ^ (long long)(u32)cy;
    }

    Chunk& get_chunk(int cx,int cy){
        long long k=key(cx,cy);
        auto it=map.find(k);
        if(it!=map.end()) return it->second;

        Chunk c{};
        c.cx=cx; c.cy=cy; c.used=true;
        c.tiles.assign((size_t)CHUNK*(size_t)CHUNK, 0);

        // default gen (simple terrain)
        for(int ty=0; ty<CHUNK; ty++){
            for(int tx=0; tx<CHUNK; tx++){
                int wx = cx*CHUNK + tx;
                int wy = cy*CHUNK + ty;

                // wavy surface
                f32 h = std::sin(wx*0.08f)*4.0f + std::sin(wx*0.02f)*10.0f;
                int ground = (int)(18.0f + h);

                u16 tile=0;
                if(wy>ground){
                    tile=1; // dirt
                    if(wy>ground+10) tile=2; // stone
                } else if(wy==ground){
                    tile=4; // grass
                }
                c.tiles[(size_t)ty*(size_t)CHUNK + (size_t)tx]=tile;
            }
        }

        auto res = map.emplace(k, std::move(c));
        return res.first->second;
    }

    u16 get(int wx,int wy){
        int cx = (wx>=0)? (wx/CHUNK) : -(((-wx)+CHUNK-1)/CHUNK);
        int cy = (wy>=0)? (wy/CHUNK) : -(((-wy)+CHUNK-1)/CHUNK);
        int lx = wx - cx*CHUNK;
        int ly = wy - cy*CHUNK;
        Chunk& c = get_chunk(cx,cy);
        return c.tiles[(size_t)ly*(size_t)CHUNK + (size_t)lx];
    }

    void set(int wx,int wy,u16 v){
        int cx = (wx>=0)? (wx/CHUNK) : -(((-wx)+CHUNK-1)/CHUNK);
        int cy = (wy>=0)? (wy/CHUNK) : -(((-wy)+CHUNK-1)/CHUNK);
        int lx = wx - cx*CHUNK;
        int ly = wy - cy*CHUNK;
        Chunk& c = get_chunk(cx,cy);
        c.tiles[(size_t)ly*(size_t)CHUNK + (size_t)lx]=v;
    }

    void draw(Canvas& dst, const Camera2D& cam){
        // cull by viewport (no rotation assumed for culling; still works fine for rot=0 typical)
        f32 invz = (cam.zoom!=0)? (1.0f/cam.zoom) : 1.0f;
        f32 left   = cam.pos.x - cam.viewport.x*0.5f*invz;
        f32 right  = cam.pos.x + cam.viewport.x*0.5f*invz;
        f32 top    = cam.pos.y - cam.viewport.y*0.5f*invz;
        f32 bottom = cam.pos.y + cam.viewport.y*0.5f*invz;

        int tsz=tile_px;
        int tx0=(int)std::floor(left/(f32)tsz)-2;
        int tx1=(int)std::floor(right/(f32)tsz)+2;
        int ty0=(int)std::floor(top/(f32)tsz)-2;
        int ty1=(int)std::floor(bottom/(f32)tsz)+2;

        m3 V = cam.view();

        for(int ty=ty0; ty<=ty1; ty++){
            for(int tx=tx0; tx<=tx1; tx++){
                u16 t = get(tx,ty);
                if(t==0) continue;

                v2 wp = V2((f32)(tx*tsz), (f32)(ty*tsz));
                v2 sp = m3_mul_v2(V, wp);

                int sx=(int)std::floor(sp.x);
                int sy=(int)std::floor(sp.y);

                // tileset draw if available
                if(ts.img && ts.cols>0){
                    int tw=ts.tile_w, th=ts.tile_h;
                    int id=(int)t;
                    int cx=id % ts.cols;
                    int cy=id / ts.cols;
                    blit(dst, sx,sy, (int)(tsz*cam.zoom),(int)(tsz*cam.zoom),
                         *ts.img, cx*tw, cy*th, tw,th,
                         blend, bilinear);
                } else {
                    // debug colors
                    u32 col=RGBA(92,72,56,255);
                    if(t==2) col=RGBA(110,110,120,255);
                    if(t==4) col=RGBA(70,160,80,255);
                    dst.rect_fill(sx,sy,(int)(tsz*cam.zoom),(int)(tsz*cam.zoom), col);
                }
            }
        }
    }
};

// ============================================================
// Lighting (tile lightmap, fast flood fill in visible region)
// ============================================================
struct LightSource {
    v2 pos_px{0,0};   // in world pixels
    int radius_tiles=10;
    u8  intensity=255; // 0..255
};

struct LightMap {
    int w=0,h=0;          // in tiles (visible region)
    int ox=0, oy=0;       // world tile origin of [0,0]
    std::vector<u8> L;    // light 0..255
    u8 ambient=40;

    void build(World& world, const Camera2D& cam, const std::vector<LightSource>& lights) {
        // visible tile bounds
        f32 invz = (cam.zoom!=0)? (1.0f/cam.zoom) : 1.0f;
        f32 left   = cam.pos.x - cam.viewport.x*0.5f*invz;
        f32 right  = cam.pos.x + cam.viewport.x*0.5f*invz;
        f32 top    = cam.pos.y - cam.viewport.y*0.5f*invz;
        f32 bottom = cam.pos.y + cam.viewport.y*0.5f*invz;

        int tsz=world.tile_px;
        int tx0=(int)std::floor(left/(f32)tsz)-4;
        int tx1=(int)std::floor(right/(f32)tsz)+4;
        int ty0=(int)std::floor(top/(f32)tsz)-4;
        int ty1=(int)std::floor(bottom/(f32)tsz)+4;

        ox=tx0; oy=ty0;
        w = (tx1-tx0+1);
        h = (ty1-ty0+1);
        if(w<=0||h<=0){ w=h=0; L.clear(); return; }

        L.assign((size_t)w*(size_t)h, ambient);

        struct Node{ int x,y; u8 v; };
        std::vector<Node> q;
        q.reserve(4096);

        auto push=[&](int x,int y,u8 v){
            if(x<0||y<0||x>=w||y>=h) return;
            size_t idx=(size_t)y*(size_t)w+(size_t)x;
            if(v<=L[idx]) return;
            L[idx]=v;
            q.push_back({x,y,v});
        };

        // seed from lights
        for(const auto& ls: lights){
            int tx=(int)std::floor(ls.pos_px.x/(f32)tsz);
            int ty=(int)std::floor(ls.pos_px.y/(f32)tsz);
            int lx=tx-ox, ly=ty-oy;
            push(lx,ly,ls.intensity);
        }

        // BFS propagation
        for(size_t qi=0; qi<q.size(); qi++){
            Node n=q[qi];
            if(n.v<=1) continue;

            // block light slightly by solid tiles
            int wx = ox+n.x;
            int wy = oy+n.y;
            bool block = world.solid(world.get(wx,wy));
            u8 decay = block ? 18 : 12; // solids eat more light

            auto step=[&](int dx,int dy){
                int nx=n.x+dx, ny=n.y+dy;
                u8 nv = (n.v>decay)? (u8)(n.v - decay) : 0;
                push(nx,ny,nv);
            };

            step(1,0); step(-1,0); step(0,1); step(0,-1);
        }
    }

    u8 sample_tile(int world_tx,int world_ty) const {
        int x=world_tx-ox;
        int y=world_ty-oy;
        if(x<0||y<0||x>=w||y>=h) return ambient;
        return L[(size_t)y*(size_t)w+(size_t)x];
    }

    void draw_darkness_overlay(Canvas& dst, const World& world, const Camera2D& cam){
        // Draw per-tile black overlay so EVERYTHING (tiles + sprites + particles) gets darkened.
        if(w<=0||h<=0) return;

        int tsz=world.tile_px;
        m3 V=cam.view();

        for(int y=0;y<h;y++){
            for(int x=0;x<w;x++){
                u8 lv = L[(size_t)y*(size_t)w+(size_t)x];
                // alpha based on missing light
                int missing = 255 - (int)lv;
                int a = clampi((missing*220)/255, 0, 220);
                if(a<=0) continue;

                int wx = ox + x;
                int wy = oy + y;

                v2 wp = V2((f32)(wx*tsz), (f32)(wy*tsz));
                v2 sp = m3_mul_v2(V, wp);

                int sx=(int)std::floor(sp.x);
                int sy=(int)std::floor(sp.y);
                int sw=(int)(tsz*cam.zoom);
                int sh=(int)(tsz*cam.zoom);

                dst.rect_fill(sx,sy,sw,sh, RGBA(0,0,0,(u32)a));
            }
        }
    }
};

// ============================================================
// Particles (simple, fast)
// ============================================================
struct RNG { u32 s=0xA341316Cu; };
static inline void rng_seed(RNG& r,u32 seed){ r.s = seed?seed:0xA341316Cu; }
static inline u32 rng_u32(RNG& r){
    u32 x=r.s;
    x^=x<<13; x^=x>>17; x^=x<<5;
    r.s=x; return x;
}
static inline f32 rng_f01(RNG& r){ return (rng_u32(r)>>8)*(1.0f/16777216.0f); }
static inline f32 rng_fr(RNG& r,f32 a,f32 b){ return lerp(a,b,rng_f01(r)); }

struct Particle {
    v2 p{}, v{};
    f32 life=0, ttl=0;
    f32 size=3;
    u32 c0=RGBA(255,255,255,255);
    u32 c1=RGBA(255,255,255,0);
};

struct Particles {
    std::vector<Particle> p;
    RNG rng{};

    void init(int cap,u32 seed){
        p.reserve((size_t)cap);
        rng_seed(rng,seed);
    }

    void emit_burst(v2 at,int count,f32 sp0,f32 sp1,f32 life0,f32 life1,f32 sz0,f32 sz1,u32 c0,u32 c1){
        for(int i=0;i<count;i++){
            if(p.size() > p.capacity()-1) break;
            f32 ang = rng_fr(rng,0,6.2831853f);
            f32 sp  = rng_fr(rng,sp0,sp1);
            Particle q{};
            q.p=at;
            q.v=V2(std::cos(ang)*sp, std::sin(ang)*sp);
            q.ttl=rng_fr(rng,life0,life1);
            q.life=q.ttl;
            q.size=rng_fr(rng,sz0,sz1);
            q.c0=c0; q.c1=c1;
            p.push_back(q);
        }
    }

    static inline u32 color_lerp(u32 a,u32 b,f32 t){
        t=clampf(t,0,1);
        u32 rr=(u32)lerp((f32)R(a),(f32)R(b),t);
        u32 gg=(u32)lerp((f32)G(a),(f32)G(b),t);
        u32 bb=(u32)lerp((f32)B(a),(f32)B(b),t);
        u32 aa=(u32)lerp((f32)A(a),(f32)A(b),t);
        return RGBA(rr,gg,bb,aa);
    }

    void update(f32 dt){
        size_t w=0;
        for(size_t i=0;i<p.size();i++){
            Particle q=p[i];
            q.life -= dt;
            if(q.life<=0) continue;
            q.v.y += 520.0f*dt;
            q.v.x *= std::exp(-2.0f*dt);
            q.v.y *= std::exp(-2.0f*dt);
            q.p = add(q.p, mul(q.v,dt));
            p[w++]=q;
        }
        p.resize(w);
    }

    void draw(Canvas& c, const Camera2D& cam){
        for(const auto& q: p){
            f32 t = 1.0f - (q.life/q.ttl);
            u32 col=color_lerp(q.c0,q.c1,ease_in_out_cubic(t));
            int r=(int)std::max(1.0f, lerp(q.size, 0.0f, t));
            v2 sp = cam.world_to_screen(q.p);
            c.circle_fill((int)sp.x,(int)sp.y,r,col);
        }
    }
};

// ============================================================
// ECS (simple but real): Registry + components + systems
// ============================================================
using Entity = u32; // packed: [gen:16][idx:16] (enough for small games)

static inline u16 ent_idx(Entity e){ return (u16)(e & 0xFFFFu); }
static inline u16 ent_gen(Entity e){ return (u16)(e >> 16); }
static inline Entity make_ent(u16 idx,u16 gen){ return (Entity)(((u32)gen<<16) | (u32)idx); }

struct Registry {
    std::vector<u16> gen;        // generation per slot
    std::vector<u16> free_list;  // free indices

    Entity create(){
        u16 idx;
        if(!free_list.empty()){
            idx = free_list.back(); free_list.pop_back();
        }else{
            idx = (u16)gen.size();
            gen.push_back(1);
        }
        return make_ent(idx, gen[idx]);
    }
    bool alive(Entity e) const {
        u16 idx=ent_idx(e);
        if(idx>=gen.size()) return false;
        return gen[idx]==ent_gen(e);
    }
    void destroy(Entity e){
        u16 idx=ent_idx(e);
        if(idx>=gen.size()) return;
        if(gen[idx]!=ent_gen(e)) return;
        gen[idx]++; // bump generation
        free_list.push_back(idx);
    }
};

// Components (kept clean for main.cpp)
struct CTransform { v2 pos{0,0}; f32 rot=0; v2 scale{1,1}; };
struct CVel      { v2 v{0,0}; };
struct CCollider { v2 half{14,20}; bool on_ground=false; };
struct CPlayer   { f32 move_speed=320.0f; f32 jump_speed=520.0f; };
struct CSprite   { const Image* img=nullptr; int sx=0,sy=0,sw=0,sh=0; bool bilinear=true; bool blend=true; u32 tint=RGBA(255,255,255,255); };
struct CLight    { int radius_tiles=10; u8 intensity=255; };

// Sparse component storage (scary but efficient)
template<typename T>
struct Pool {
    std::vector<u16> dense_idx; // dense -> entity idx
    std::vector<T>   dense_val;
    std::vector<i32> sparse;    // entity idx -> dense index, -1 if none

    void ensure_sparse(size_t n){
        if(sparse.size()<n) sparse.resize(n, -1);
    }

    bool has(Entity e) const {
        u16 idx=ent_idx(e);
        if(idx>=sparse.size()) return false;
        int di=sparse[idx];
        return di>=0 && di<(int)dense_idx.size() && dense_idx[(size_t)di]==idx;
    }

    T* get(Entity e){
        u16 idx=ent_idx(e);
        if(idx>=sparse.size()) return nullptr;
        int di=sparse[idx];
        if(di<0) return nullptr;
        if(di>=(int)dense_idx.size()) return nullptr;
        if(dense_idx[(size_t)di]!=idx) return nullptr;
        return &dense_val[(size_t)di];
    }

    T& add(Entity e, const T& v=T{}){
        ensure_sparse(ent_idx(e)+1);
        u16 idx=ent_idx(e);
        int di=sparse[idx];
        if(di>=0 && (size_t)di<dense_idx.size() && dense_idx[(size_t)di]==idx){
            dense_val[(size_t)di]=v;
            return dense_val[(size_t)di];
        }
        sparse[idx]=(int)dense_idx.size();
        dense_idx.push_back(idx);
        dense_val.push_back(v);
        return dense_val.back();
    }

    void remove(Entity e){
        u16 idx=ent_idx(e);
        if(idx>=sparse.size()) return;
        int di=sparse[idx];
        if(di<0) return;
        size_t last=dense_idx.size()-1;
        if((size_t)di!=last){
            dense_idx[(size_t)di]=dense_idx[last];
            dense_val[(size_t)di]=dense_val[last];
            sparse[dense_idx[(size_t)di]]=(int)di;
        }
        dense_idx.pop_back();
        dense_val.pop_back();
        sparse[idx]=-1;
    }

    // iterate: dense arrays
    size_t size() const { return dense_idx.size(); }
    Entity entity_at(size_t i, const Registry& r) const {
        u16 idx=dense_idx[i];
        return make_ent(idx, r.gen[idx]);
    }
    T& value_at(size_t i){ return dense_val[i]; }
    const T& value_at(size_t i) const { return dense_val[i]; }
};

struct ECS {
    Registry reg;

    Pool<CTransform> tr;
    Pool<CVel>       vel;
    Pool<CCollider>  col;
    Pool<CPlayer>    player;
    Pool<CSprite>    spr;
    Pool<CLight>     light;
};

// ============================================================
// Tile physics (AABB vs solid tiles)
// ============================================================
static inline int floor_div_tile(f32 x, int tile_px){
    return (int)std::floor(x / (f32)tile_px);
}

static inline bool aabb_overlaps_tile(v2 pos, v2 half, int tile_px, int tx,int ty){
    f32 x0=pos.x-half.x, x1=pos.x+half.x;
    f32 y0=pos.y-half.y, y1=pos.y+half.y;

    f32 t0=(f32)(tx*tile_px);
    f32 t1=t0+(f32)tile_px;
    f32 s0=(f32)(ty*tile_px);
    f32 s1=s0+(f32)tile_px;

    return !(x1<=t0 || x0>=t1 || y1<=s0 || y0>=s1);
}

static inline void resolve_axis_x(ECS& ecs, World& world, Entity e, f32 dt){
    auto* t=ecs.tr.get(e);
    auto* v=ecs.vel.get(e);
    auto* c=ecs.col.get(e);
    if(!t||!v||!c) return;

    t->pos.x += v->v.x * dt;

    int tp=world.tile_px;
    int min_tx = floor_div_tile(t->pos.x - c->half.x, tp) - 1;
    int max_tx = floor_div_tile(t->pos.x + c->half.x, tp) + 1;
    int min_ty = floor_div_tile(t->pos.y - c->half.y, tp) - 1;
    int max_ty = floor_div_tile(t->pos.y + c->half.y, tp) + 1;

    for(int ty=min_ty; ty<=max_ty; ty++){
        for(int tx=min_tx; tx<=max_tx; tx++){
            u16 tile=world.get(tx,ty);
            if(!world.solid(tile)) continue;
            if(!aabb_overlaps_tile(t->pos,c->half,tp,tx,ty)) continue;

            // push out along x
            f32 tile_left  = (f32)(tx*tp);
            f32 tile_right = tile_left + (f32)tp;
            if(v->v.x > 0){
                t->pos.x = tile_left - c->half.x;
            }else if(v->v.x < 0){
                t->pos.x = tile_right + c->half.x;
            }
            v->v.x = 0;
        }
    }
}

static inline void resolve_axis_y(ECS& ecs, World& world, Entity e, f32 dt){
    auto* t=ecs.tr.get(e);
    auto* v=ecs.vel.get(e);
    auto* c=ecs.col.get(e);
    if(!t||!v||!c) return;

    c->on_ground=false;
    t->pos.y += v->v.y * dt;

    int tp=world.tile_px;
    int min_tx = floor_div_tile(t->pos.x - c->half.x, tp) - 1;
    int max_tx = floor_div_tile(t->pos.x + c->half.x, tp) + 1;
    int min_ty = floor_div_tile(t->pos.y - c->half.y, tp) - 1;
    int max_ty = floor_div_tile(t->pos.y + c->half.y, tp) + 1;

    for(int ty=min_ty; ty<=max_ty; ty++){
        for(int tx=min_tx; tx<=max_tx; tx++){
            u16 tile=world.get(tx,ty);
            if(!world.solid(tile)) continue;
            if(!aabb_overlaps_tile(t->pos,c->half,tp,tx,ty)) continue;

            f32 tile_top    = (f32)(ty*tp);
            f32 tile_bottom = tile_top + (f32)tp;
            if(v->v.y > 0){
                // moving down
                t->pos.y = tile_top - c->half.y;
                c->on_ground=true;
            }else if(v->v.y < 0){
                // moving up
                t->pos.y = tile_bottom + c->half.y;
            }
            v->v.y = 0;
        }
    }
}

// Systems
static inline void sys_player(ECS& ecs, const Input& in, f32 dt){
    (void)dt;
    for(size_t i=0;i<ecs.player.size();i++){
        Entity e = ecs.player.entity_at(i, ecs.reg);
        if(!ecs.reg.alive(e)) continue;

        auto* pl=ecs.player.get(e);
        auto* v =ecs.vel.get(e);
        auto* c =ecs.col.get(e);
        if(!pl||!v||!c) continue;

        f32 ax=0;
        if(in.key['A']) ax -= 1;
        if(in.key['D']) ax += 1;

        f32 speed=pl->move_speed;
        if(in.key[VK_SHIFT]) speed *= 1.6f;

        // "scary smoothing"
        f32 target = ax * speed;
        v->v.x = lerp(v->v.x, target, 1.0f - std::exp(-18.0f*dt));

        // jump
        if(in.key_pressed[VK_SPACE] && c->on_ground){
            v->v.y = -pl->jump_speed;
            c->on_ground=false;
        }
    }
}

static inline void sys_physics(ECS& ecs, World& world, f32 dt){
    const f32 gravity = 1200.0f;

    // apply gravity
    for(size_t i=0;i<ecs.vel.size();i++){
        Entity e = ecs.vel.entity_at(i, ecs.reg);
        if(!ecs.reg.alive(e)) continue;
        auto* v=ecs.vel.get(e);
        if(!v) continue;
        v->v.y += gravity * dt;
        v->v.y = std::min(v->v.y, 3000.0f);
    }

    // resolve X then Y for entities with collider
    for(size_t i=0;i<ecs.col.size();i++){
        Entity e = ecs.col.entity_at(i, ecs.reg);
        if(!ecs.reg.alive(e)) continue;
        if(!ecs.tr.get(e) || !ecs.vel.get(e)) continue;
        resolve_axis_x(ecs, world, e, dt);
        resolve_axis_y(ecs, world, e, dt);
    }
}

static inline void sys_render_world(Canvas& c, World& world, const Camera2D& cam){
    world.draw(c, cam);
}

static inline void sys_render_sprites(Canvas& c, ECS& ecs, const Camera2D& cam){
    m3 V=cam.view();
    for(size_t i=0;i<ecs.spr.size();i++){
        Entity e = ecs.spr.entity_at(i, ecs.reg);
        if(!ecs.reg.alive(e)) continue;
        auto* t=ecs.tr.get(e);
        auto* s=ecs.spr.get(e);
        if(!t||!s||!s->img||s->img->empty()) continue;

        v2 sp = m3_mul_v2(V, t->pos);
        int dx=(int)std::floor(sp.x);
        int dy=(int)std::floor(sp.y);

        // default sprite rect if not set
        int sw = (s->sw>0)?s->sw:s->img->w;
        int sh = (s->sh>0)?s->sh:s->img->h;

        int draw_w = (int)(sw * cam.zoom);
        int draw_h = (int)(sh * cam.zoom);

        blit(c, dx - draw_w/2, dy - draw_h/2, draw_w, draw_h,
             *s->img, s->sx, s->sy, sw, sh,
             s->blend, s->bilinear, s->tint);
    }
}

static inline void gather_lights(ECS& ecs, std::vector<LightSource>& out){
    out.clear();
    out.reserve(128);

    for(size_t i=0;i<ecs.light.size();i++){
        Entity e = ecs.light.entity_at(i, ecs.reg);
        if(!ecs.reg.alive(e)) continue;
        auto* t=ecs.tr.get(e);
        auto* l=ecs.light.get(e);
        if(!t||!l) continue;

        LightSource ls;
        ls.pos_px = t->pos;
        ls.radius_tiles = l->radius_tiles;
        ls.intensity = l->intensity;
        out.push_back(ls);
    }
}

} // namespace we
