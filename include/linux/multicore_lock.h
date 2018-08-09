#ifndef __MULTICORE_LOCK_H__
#define __MULTICORE_LOCK_H__

typedef enum {
	MULTICORE_LOCK_OTP = (1 << 0),
} MULTICORE_LOCK;

int multicore_lock(MULTICORE_LOCK lock);
int multicore_unlock(MULTICORE_LOCK lock);

#endif
