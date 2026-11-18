#include "app.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 struct app a;
 assert(app_init(&a,0));
 assert(a.phase==0);
 assert(app_input(&a,"k d 218"));
 assert(a.phase==1&&a.count==14);
 assert(app_input(&a,"w -100"));
 assert(a.angle!=0);
 assert(app_input(&a,"k d 218"));
 assert(a.shot_active);
 assert(app_input(&a,"k d 170"));
 assert(a.phase==2);
 assert(app_input(&a,"k d 218"));
 assert(a.phase==1);
 assert(app_input(&a,"k d 170"));
 assert(app_input(&a,"k d 170"));
 assert(a.quit);
 puts("Q2 Zuma portable app tests passed");
 return 0;
}
