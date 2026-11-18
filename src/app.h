/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef Q2_ZUMA_APP_H
#define Q2_ZUMA_APP_H
enum { WIDTH=375,HEIGHT=320,MAX_BALLS=80 };
typedef unsigned int u32;
typedef struct { int x,y; } Point;
struct app {
 unsigned rng,score,best,step_acc,flash,ticks,shot_age;
 int quit,save_pending,sound_pending,sound_event,dark_mode;
 int phase,previous_phase,angle,wheel,count,head,shot_x,shot_y,shot_dx,shot_dy;
 int shot_color,loaded,next_color,shot_active,shots_fired,level;
 unsigned char balls[MAX_BALLS];
};
int app_init(struct app *,unsigned long);
int app_input(struct app *,const char *);
void app_frame(struct app *,u32 *,unsigned,unsigned);
int app_animated(const struct app *);
#endif
