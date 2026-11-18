/* SPDX-License-Identifier: GPL-3.0-only
 * Diamond Spiral: an original marble-chain shooter for Shanling Q2.
 * Fixed-size data, deterministic integer geometry, no heap or libm.
 */
#include "app.h"
enum { READY=0,RUNNING=1,PAUSED=2,LOST=3,WON=4,SPACING=20,RADIUS=10,START_COUNT=14,FINAL_HEAD=1550 };
static const Point track[]={
 {28,74},{346,74},{346,255},{28,255},{28,122},
 {301,122},{301,217},{79,217},{79,164},{249,164},{249,194},{174,194}
};
#define SEGMENTS ((int)(sizeof(track)/sizeof(track[0]))-1)
static int absi(int n){return n<0?-n:n;}
static int length(int x,int y){ /* integer sqrt, bounded 32-bit coordinates */
 unsigned n=(unsigned)(x*x+y*y),r=0,bit=1u<<30;
 while(bit>n)bit>>=2;
 while(bit){if(n>=r+bit){n-=r+bit;r=(r>>1)+bit;}else r>>=1;bit>>=2;}
 return (int)r;
}
/* Cache all path coordinates once. No sqrt/divisions per marble per frame. */
enum { PATH_CAP=4096 };
static Point path_cache[PATH_CAP];
static int path_count;
static void init_path(void){
 if(path_count)return;
 for(int s=0;s<SEGMENTS;s++){
  Point p=track[s],q=track[s+1];
  int dx=q.x-p.x,dy=q.y-p.y,steps=length(dx,dy);
  for(int k=0;k<steps&&path_count<PATH_CAP;k++){
   Point pt={p.x+dx*k/steps,p.y+dy*k/steps};
   path_cache[path_count++]=pt;
  }
 }
 if(path_count<PATH_CAP)path_cache[path_count++]=track[SEGMENTS];
}
static int total_path(void){init_path();return path_count-1;}
static Point position(int distance){
 init_path();
 if(distance<0)distance=0;
 if(distance>=path_count)distance=path_count-1;
 return path_cache[distance];
}
static unsigned rnd(struct app *a){unsigned x=a->rng;x^=x<<13;x^=x>>17;x^=x<<5;a->rng=x;return x;}
static int color(struct app *a){return (int)(rnd(a)%5u);}
static void begin(struct app *a){
 a->rng^=a->ticks+0x9e3779b9u;if(!a->rng)a->rng=0x12345678u;
 a->phase=RUNNING;a->level=1;a->score=0;a->count=START_COUNT;a->head=START_COUNT*SPACING;
 a->step_acc=a->flash=a->shot_age=0;a->shot_active=a->shots_fired=0;
 a->angle=0;a->wheel=0;
 for(int i=0;i<a->count;i++)a->balls[i]=(unsigned char)color(a);
 a->loaded=color(a);a->next_color=color(a);
 init_path();
}
int app_init(struct app *a,unsigned long best){
 *a=(struct app){0};a->rng=0x712ab47cu;a->best=best<10000000u?(unsigned)best:0;a->phase=READY;return 1;
}
static const Point aim[32]={
 {0,-256},{50,-251},{98,-237},{142,-213},{181,-181},{213,-142},{237,-98},{251,-50},
 {256,0},{251,50},{237,98},{213,142},{181,181},{142,213},{98,237},{50,251},
 {0,256},{-50,251},{-98,237},{-142,213},{-181,181},{-213,142},{-237,98},{-251,50},
 {-256,0},{-251,-50},{-237,-98},{-213,-142},{-181,-181},{-142,-213},{-98,-237},{-50,-251}
};
static void shoot(struct app *a){
 if(a->phase!=RUNNING||a->shot_active)return;
 a->shot_active=1;a->shot_x=187*256;a->shot_y=166*256;
 a->shot_dx=aim[a->angle].x;a->shot_dy=aim[a->angle].y;
 a->shot_color=a->loaded;a->loaded=a->next_color;a->next_color=color(a);
 a->shot_age=0;a->shots_fired++;
}
static void win_if_empty(struct app *a){
 if(a->count==0){a->phase=WON;a->flash=700;if(a->score>a->best){a->best=a->score;a->save_pending=1;}}
}
static void insert_at(struct app *a,int at,int c){
 if(a->count>=MAX_BALLS){a->phase=LOST;return;}
 if(at<0)at=0;
 if(at>a->count)at=a->count;
 for(int i=a->count;i>at;i--)a->balls[i]=a->balls[i-1];
 a->balls[at]=(unsigned char)c;a->count++;
 int lo=at,hi=at;
 while(lo>0&&a->balls[lo-1]==c)lo--;
 while(hi+1<a->count&&a->balls[hi+1]==c)hi++;
 if(hi-lo+1>=3){
  int removed=hi-lo+1;
  for(int i=hi+1;i<a->count;i++)a->balls[i-removed]=a->balls[i];
  a->count-=removed;a->head-=removed*SPACING;
  if(a->head<SPACING)a->head=SPACING;
  a->score+=(unsigned)(removed*10+(removed>3?(removed-3)*15:0));
  a->flash=260;a->sound_pending=1;a->sound_event=1;
  if(a->score>a->best){a->best=a->score;a->save_pending=1;}
  win_if_empty(a);
 }
}
static void step(struct app *a,unsigned ms){
 a->ticks+=ms;
 if(a->flash)a->flash=ms>=a->flash?0:a->flash-ms;
 if(a->phase!=RUNNING)return;
 a->step_acc+=ms;
 while(a->step_acc>=40){a->step_acc-=40;a->head++;} /* 25 pixels/s */
 if(a->count>0&&a->head>=total_path()+RADIUS){a->phase=LOST;a->shot_active=0;return;}
 if(!a->shot_active)return;
 a->shot_age+=ms;
 if(a->shot_age>1800){a->shot_active=0;return;}
 /* 235 pixels per second, fixed-point position. */
 a->shot_x+=a->shot_dx*(int)ms*235/1000;
 a->shot_y+=a->shot_dy*(int)ms*235/1000;
 int x=a->shot_x/256,y=a->shot_y/256;
 if(x<10||x>=WIDTH-10||y<40||y>=HEIGHT-14){a->shot_active=0;return;}
 for(int i=0;i<a->count;i++){
  Point p=position(a->head-i*SPACING);
  int dx=x-p.x,dy=y-p.y;
  if(dx*dx+dy*dy<=(RADIUS*2)*(RADIUS*2)){
   /* Insert on the side of the hit ball nearest the incoming shot. */
   Point ahead=position(a->head-(i-1)*SPACING);
   Point behind=position(a->head-(i+1)*SPACING);
   int da=(x-ahead.x)*(x-ahead.x)+(y-ahead.y)*(y-ahead.y);
   int db=(x-behind.x)*(x-behind.x)+(y-behind.y)*(y-behind.y);
   insert_at(a,da<db?i:i+1,a->shot_color);
   a->shot_active=0;return;
  }
 }
}
static int number(const char **p){
 int n=0,sign=1;while(**p==' ')++*p;
 if(**p=='-'){sign=-1;(*p)++;}
 if(**p<'0'||**p>'9')return 10000;
 while(**p>='0'&&**p<='9'){if(n>10000)return 10000;n=n*10+*(*p)++-'0';}
 return n*sign;
}
int app_input(struct app *a,const char *s){
 if(s[0]=='w'&&s[1]==' '){
  const char *p=s+2;int n=number(&p);if(n<-2000||n>2000)return 0;
  a->wheel-=n;int steps=a->wheel/11;a->wheel%=11;
  if(a->phase==RUNNING||a->phase==PAUSED)a->angle=(a->angle+steps%32+32)%32;
  return steps!=0;
 }
 if(!s[0]||s[1]!=' '||!s[2]||s[3]!=' ')return 0;
 const char *p=s+4;int n=number(&p);
 if(s[0]=='k'){
  if(s[2]!='d')return 0;
  if(n==170){
   if(a->phase==PAUSED)a->quit=1;
   else if(a->phase==READY||a->phase==WON||a->phase==LOST)a->quit=1;
   else {a->phase=PAUSED;}
   return 1;
  }
  if(n==218){
   if(a->phase==READY||a->phase==WON||a->phase==LOST)begin(a);
   else if(a->phase==PAUSED)a->phase=RUNNING;
   else shoot(a);
   return 1;
  }
  /* Physical buttons forwarded by Q2 Pod: left/right as alternate aim. */
  if(n==105||n==106){a->angle=(a->angle+(n==105?31:1))%32;return 1;}
 }
 if(s[0]=='t'&&s[2]=='d'){
  int y=number(&p);
  if(n<0||n>=WIDTH||y<0||y>=HEIGHT)return 0;
  if(a->phase==READY||a->phase==WON||a->phase==LOST)begin(a);
  else if(a->phase==PAUSED)a->phase=RUNNING;
  else{
   /* Aim toward tap by selecting nearest 32-way ray; tap again to fire. */
   int dx=n-187,dy=y-166;int best=-2147483647,choice=0;
   for(int i=0;i<32;i++){int dot=dx*aim[i].x+dy*aim[i].y;if(dot>best){best=dot;choice=i;}}
   if(choice==a->angle)shoot(a);else a->angle=choice;
  }
  return 1;
 }
 return 0;
}
static const unsigned char font[36][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
{1,1,1,1,17,17,14},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,30,1,1,17,14},
{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
{14,17,17,15,1,2,12}
};
/* Cached RGB artwork: original tiny jewel sprites generated at build time.
 * Background and track are rendered once per theme, rather than every frame. */
#include "sprites.h"
static u32 backdrop[WIDTH*HEIGHT];
static int backdrop_theme=-1;
static void pixel(u32 *p,unsigned stride,int x,int y,u32 c){
 if(x>=0&&x<WIDTH&&y>=0&&y<HEIGHT)p[x*stride+HEIGHT-1-y]=c;
}
static void rect(u32 *p,unsigned s,int x,int y,int w,int h,u32 c){
 if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
 if(x+w>WIDTH)w=WIDTH-x;
 if(y+h>HEIGHT)h=HEIGHT-y;
 for(int xx=x;xx<x+w;xx++)for(int yy=y;yy<y+h;yy++)p[xx*s+HEIGHT-1-yy]=c;
}
static void disc(u32 *p,unsigned s,int x,int y,int r,u32 c){
 int rr=r*r;
 for(int dx=-r;dx<=r;dx++)for(int dy=-r;dy<=r;dy++)
  if(dx*dx+dy*dy<=rr)pixel(p,s,x+dx,y+dy,c);
}
static void line(u32 *p,unsigned s,int x,int y,int xx,int yy,int r,u32 c){
 int steps=absi(xx-x)>absi(yy-y)?absi(xx-x):absi(yy-y);
 for(int k=0;k<=steps;k++)disc(p,s,x+(xx-x)*k/(steps?steps:1),y+(yy-y)*k/(steps?steps:1),r,c);
}
/* 21x21 transparent sprite blit: pre-shaded gemstones, no per-pixel math. */
static void jewel(u32 *p,unsigned stride,int cx,int cy,int color){
 if(color<0||color>=5)return;
 const u32 *src=gem_sprites[color];
 for(int dx=0;dx<21;dx++){
  int x=cx+dx-10;if(x<0||x>=WIDTH)continue;
  u32 *dest=p+x*stride;
  for(int dy=0;dy<21;dy++){
   int y=cy+dy-10;
   if(y>=0&&y<HEIGHT){u32 v=src[dy*21+dx];if(v)dest[HEIGHT-1-y]=v;}
  }
 }
}
static void label(u32 *p,unsigned st,const char *s,int y,int z,u32 c){
 int n=0;while(s[n])n++;int x=(WIDTH-(n*6-1)*z)/2;
 for(;*s;s++,x+=6*z){
  int g=(*s>='A'&&*s<='Z')?*s-'A':(*s>='0'&&*s<='9')?*s-'0'+26:-1;
  if(g<0)continue;
  for(int row=0;row<7;row++)for(int col=0;col<5;col++)
   if(font[g][row]&(16u>>col))rect(p,st,x+col*z,y+row*z,z,z,c);
 }
}
static void number_label(u32 *p,unsigned st,unsigned number,int y,u32 c){
 char str[12];int k=0;
 do{str[k++]=(char)('0'+number%10);number/=10;}while(number&&k<10);
 char out[12];for(int j=0;j<k;j++)out[j]=str[k-1-j];out[k]=0;
 label(p,st,out,y,2,c);
}
static void prepare_background(int dark){
 if(backdrop_theme==dark)return;
 backdrop_theme=dark;
 u32 bg=dark?0xff0d1422u:0xffe9e4d9u;
 u32 border=dark?0xff273752u:0xffc0b6a5u;
 u32 rail=dark?0xff445775u:0xff948a7au;
 rect(backdrop,HEIGHT,0,0,WIDTH,HEIGHT,bg);
 /* Quiet decorative lines and tiny stars are drawn only once. */
 for(int x=25;x<WIDTH-20;x+=41)
  for(int y=58;y<HEIGHT-18;y+=43)
   disc(backdrop,HEIGHT,x,y,1,dark?0xff222f44u:0xffd5ccbeu);
 init_path();
 /* Use the path samples to stamp the track once, avoiding repeated lines. */
 for(int i=0;i<path_count;i+=3)disc(backdrop,HEIGHT,path_cache[i].x,path_cache[i].y,13,border);
 for(int i=0;i<path_count;i+=2)disc(backdrop,HEIGHT,path_cache[i].x,path_cache[i].y,8,rail);
 for(int i=0;i<path_count;i+=3)disc(backdrop,HEIGHT,path_cache[i].x,path_cache[i].y,5,dark?0xff263449u:0xffafa596u);
 Point exit=track[SEGMENTS];
 disc(backdrop,HEIGHT,exit.x,exit.y,19,dark?0xff10121bu:0xff5c4b4bu);
 disc(backdrop,HEIGHT,exit.x,exit.y,14,0xffb38a51u);
 disc(backdrop,HEIGHT,exit.x,exit.y,10,0xff3e2b36u);
 disc(backdrop,HEIGHT,exit.x,exit.y,5,0xffd9b773u);
 /* Original diamond insignia around the shooter. */
 disc(backdrop,HEIGHT,187,166,26,dark?0xff35425cu:0xffb0a69bu);
 disc(backdrop,HEIGHT,187,166,22,0xff8b6842u);
 disc(backdrop,HEIGHT,187,166,18,dark?0xff19253du:0xffe5d4b0u);
}
void app_frame(struct app *a,u32 *p,unsigned stride,unsigned ms){
 step(a,ms);
 prepare_background(a->dark_mode);
 /* Rectangular copy is far cheaper than 1000s of radius and sqrt tests. */
 for(int x=0;x<WIDTH;x++)
  for(int y=0;y<HEIGHT;y++)p[x*stride+y]=backdrop[x*HEIGHT+y];
 u32 fg=a->dark_mode?0xfff4f2e9u:0xff29334au;
 label(p,stride,"DIAMOND SPIRAL",11,2,fg);
 number_label(p,stride,a->score,36,fg);
 if(a->phase!=READY){
  for(int i=a->count-1;i>=0;i--){
   Point pos=position(a->head-i*SPACING);
   jewel(p,stride,pos.x,pos.y,a->balls[i]);
  }
 }
 /* Shooter: compact brass ring, an inset diamond and an aim laser. */
 line(p,stride,187,166,187+aim[a->angle].x*37/256,166+aim[a->angle].y*37/256,2,0xffeecb7cu);
 disc(p,stride,187,166,18,0xff141e31u);
 disc(p,stride,187,166,14,0xffc9a364u);
 disc(p,stride,187,166,11,0xff26334du);
 jewel(p,stride,187,166,a->loaded);
 disc(p,stride,213,182,10,0xff26334du);
 jewel(p,stride,213,182,a->next_color);
 if(a->shot_active)jewel(p,stride,a->shot_x/256,a->shot_y/256,a->shot_color);
 if(a->flash)disc(p,stride,187,166,2,0xfffff5d5u);
 if(a->phase==READY){rect(p,stride,40,272,295,31,a->dark_mode?0xff0d1422u:0xffe9e4d9u);label(p,stride,"CENTER TO START",278,2,fg);}
 else if(a->phase==PAUSED){rect(p,stride,47,268,281,39,a->dark_mode?0xff0d1422u:0xffe9e4d9u);label(p,stride,"PAUSED",273,2,fg);label(p,stride,"CENTER RESUME",291,1,fg);}
 else if(a->phase==LOST){rect(p,stride,40,268,295,39,a->dark_mode?0xff0d1422u:0xffe9e4d9u);label(p,stride,"GAME OVER",275,2,fg);label(p,stride,"CENTER RETRY",294,1,fg);}
 else if(a->phase==WON){rect(p,stride,40,268,295,39,a->dark_mode?0xff0d1422u:0xffe9e4d9u);label(p,stride,"CHAIN CLEARED",275,2,fg);label(p,stride,"CENTER AGAIN",294,1,fg);}
 else label(p,stride,"WHEEL AIM CENTER FIRE",301,1,fg);
}
int app_animated(const struct app *a){return a->phase==RUNNING||a->flash!=0;}
