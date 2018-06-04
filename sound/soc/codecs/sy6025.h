#ifndef __ASOC_SY6025_H__
#define __ASOC_SY6025_H__

#define SY6025_MIN_DB_VALUE (-127)
#define SY6025_MAX_DB_VALUE (0)
#define SY6025_INIT_DB_VALUE (-50)

#define SY6025_BQ_BASEADDR (0x30)
#define SY6025_BQ_NUM      (20)

enum audio_effect {
	DEFAULT_MODE,
	ROCK_MODE,
	CLASSICAL_MODE,
	DANCE_MODE,
	POP_MODE,
};

struct sy6025_info {
	struct i2c_client *i2c;
	long int dbValue;
	enum audio_effect audioEffect;
};

#endif
