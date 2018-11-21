#ifndef _LIGHT_EFFECT_H
#define _LIGHT_EFFECT_H

extern int (*yodabase_led_draw)(char *buf, int length);
extern int (*yodabase_is_led_busy)(void);
extern int (*yodabase_led_brightness_set)(int brightness);

#endif /* _LIGHT_EFFECT_H */