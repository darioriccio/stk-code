// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_LAYOUT_HPP
#define HEADER_SCINTILLA_LAYOUT_HPP
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace ScintillaDefense {
struct Vec {
    double x=0,y=0,z=0;
    Vec(double a=0,double b=0,double c=0):x(a),y(b),z(c){}
    Vec operator+(const Vec& b)const{return Vec(x+b.x,y+b.y,z+b.z);}
    Vec operator-(const Vec& b)const{return Vec(x-b.x,y-b.y,z-b.z);}
    Vec operator*(double s)const{return Vec(x*s,y*s,z*s);}
    double dot(const Vec& b)const{return x*b.x+y*b.y+z*b.z;}
    double length()const{return std::sqrt(dot(*this));}
    bool finite()const{return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z);}
};
enum class Sector { Front=0, Right=1, Rear=2, Left=3, None=4 };
enum class Context { None=0, WorldOffense=1, DefenseSector=2, ServiceBoost=3, ReadOnlyRear=4, ReservedHUD=5, PartyBoost=6 };
struct Surface {Context context=Context::None;Sector sector=Sector::None;};
struct Layout {
    static const unsigned Revision=1;
    static const unsigned MissionRevision=2;
    static const unsigned CrewRevision=3;
    unsigned width=0,height=0;
    bool mission=false,terminal=false,crew=false;
    double rear[4]={0,0,0,0}; // pixel left,top,right,bottom; rounded ONCE
    static Layout create(unsigned w,unsigned h,bool cargo=false,bool finished=false,bool coordinated=false) {
        Layout l;l.width=w;l.height=h;l.mission=cargo;l.terminal=cargo&&finished;l.crew=coordinated;const double s=std::min(w,h);
        l.rear[0]=std::round(w*.5-.18*s);l.rear[1]=std::round(h*.5-.35*s);
        l.rear[2]=std::round(w*.5+.18*s);l.rear[3]=std::round(h*.5-.17*s);return l;
    }
    unsigned revision()const{return crew?CrewRevision:mission?MissionRevision:Revision;}
    const char* profile()const{return crew?"front_rear_crew_v3":mission?"front_rear_cargo_v2":"front_rear_readonly_v1";}
    bool valid()const{return width>=256&&height>=256&&rear[2]>rear[0]&&rear[3]>rear[1];}
    void node(unsigned i,double* x,double* y,double* radius)const {
        const double scale=std::min(width,height);
        *x=(width*.5+(i<4?-.6+.4*i:0)*scale*.5)/width;
        *y=(height*.5+(i<4?.62:.85)*scale*.5)/height;*radius=(i<4?.07:.06)*scale*.5;
    }
    // Like node(): UV centre and pixel radius. Paint and interaction masks use
    // this exact circle; no stroke or glow may extend beyond it.
    void cargo(unsigned i,double* x,double* y,double* radius)const {
        const double scale=std::min(width,height);
        *x=(width*.5+(-.18+.18*i)*scale*.5)/width;
        *y=(height*.5-.16*scale*.5)/height;*radius=.06*scale*.5;
    }
    void party(double* x,double* y,double* radius)const {
        const double scale=std::min(width,height);
        *x=(width*.5+.22*scale*.5)/width;
        *y=(height*.5+.85*scale*.5)/height;*radius=.06*scale*.5;
    }
    void beacon(unsigned i,double* x,double* y,double* radius)const {
        const double scale=std::min(width,height);
        *x=(width*.5+(-.18+.18*i)*scale*.5)/width;
        *y=(height*.5-.27*scale*.5)/height;*radius=.025*scale*.5;
    }
    double aspect()const{return (rear[2]-rear[0])/(rear[3]-rear[1]);}
    Surface classify(double u,double v)const {
        Surface s;if(!valid()||!std::isfinite(u)||!std::isfinite(v)||u<0||u>1||v<0||v>1)return s;
        const double scale=std::min(width,height),x=(u*width-width*.5)*2/scale,y=(v*height-height*.5)*2/scale;
        if(terminal&&std::abs(x)<=.76&&y>=-.08&&y<=.40){s.context=Context::ReservedHUD;return s;}
        if(mission)for(unsigned i=0;i<3;++i)
            if(std::hypot(x-(-.18+.18*i),y+.16)<=.06||
               std::hypot(x-(-.18+.18*i),y+.27)<=.025){s.context=Context::ReservedHUD;return s;}
        const Sector sectors[4]={Sector::Left,Sector::Front,Sector::Rear,Sector::Right};
        for(unsigned i=0;i<4;++i)if(std::hypot(x-(-.6+.4*i),y-.62)<=.07){s.context=Context::DefenseSector;s.sector=sectors[i];return s;}
        if(std::hypot(x,y-.85)<=.06){s.context=Context::ServiceBoost;return s;}
        if(crew&&std::hypot(x-.22,y-.85)<=.06){s.context=Context::PartyBoost;return s;}
        if(u*width>=rear[0]&&u*width<rear[2]&&v*height>=rear[1]&&v*height<rear[3]){s.context=Context::ReadOnlyRear;return s;}
        if((std::abs(x)<=.42&&((y>=.42&&y<=.46)||(y>=.48&&y<=.52)))||badge(x,y))s.context=Context::ReservedHUD;
        else s.context=Context::WorldOffense;return s;
    }
    bool worldRectFree(double left,double top,double right,double bottom)const {
        if(!valid()||!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom)||
           left<0||top<0||right>=1||bottom>=1||left>right||top>bottom)return false;
        auto intersects=[&](double l,double t,double r,double b){return left<=r&&right>=l&&top<=b&&bottom>=t;};
        if(intersects(rear[0]/width,rear[1]/height,rear[2]/width,rear[3]/height))return false;
        for(unsigned i=0;i<5;++i){double x,y,r;node(i,&x,&y,&r);const double cx=std::max(left,std::min(right,x)),cy=std::max(top,std::min(bottom,y));if(std::hypot((x-cx)*width,(y-cy)*height)<=r)return false;}
        if(crew){double x,y,r;party(&x,&y,&r);const double cx=std::max(left,std::min(right,x)),cy=std::max(top,std::min(bottom,y));if(std::hypot((x-cx)*width,(y-cy)*height)<=r)return false;}
        const double s=std::min(width,height);
        if(terminal&&intersects(.5-.38*s/width,.5-.04*s/height,.5+.38*s/width,.5+.20*s/height))return false;
        if(mission)for(unsigned i=0;i<3;++i)for(unsigned kind=0;kind<2;++kind){
            double x,y,r;if(kind)beacon(i,&x,&y,&r);else cargo(i,&x,&y,&r);
            const double cx=std::max(left,std::min(right,x)),cy=std::max(top,std::min(bottom,y));
            if(std::hypot((x-cx)*width,(y-cy)*height)<=r)return false;
        }
        for(unsigned i=0;i<2;++i)if(intersects(.5-.21*s/width,.5+(.21+.03*i)*s/height,.5+.21*s/width,.5+(.23+.03*i)*s/height))return false;
        for(unsigned i=0;i<5;++i)for(double y:{.08,.25})if(intersects(.5+(-.32+.16*i-.055)*s*.5/width,.5+(y-.055)*s*.5/height,.5+(-.32+.16*i+.055)*s*.5/width,.5+(y+.055)*s*.5/height))return false;
        return true;
    }
    static bool badge(double x,double y){
        for(unsigned i=0;i<5;++i)if(std::abs(x-(-.32+.16*i))<=.055&&(std::abs(y-.08)<=.055||std::abs(y-.25)<=.055))return true;
        return false;
    }
    Vec rearDirection(double s,double t)const {
        const double h=1.1917535925942098;Vec d(-(2*s-1)*h,(1-2*t)*h/aspect(),-1);return d*(1/d.length());
    }
    bool rearUV(const Vec& d,double* u,double* v)const {
        if(!valid()||!d.finite()||d.z>=0)return false;
        const double h=1.1917535925942098,s=.5*(1-d.x/(-d.z*h)),t=.5*(1-d.y/(-d.z*h/aspect()));
        if(s<0||s>=1||t<0||t>=1)return false;
        *u=(rear[0]+s*(rear[2]-rear[0]))/width;*v=(rear[1]+t*(rear[3]-rear[1]))/height;return true;
    }
};
}
#endif
