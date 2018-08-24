#ifndef __GX8010_CORE_H__
#define __GX8010_CORE_H__

typedef struct aout_stream    AOUT_STREAM;
typedef struct aout_substream AOUT_SUBSTREAM;

struct aout_reg_des {
#define MAX_AOUT_REG_NUM (4)
	unsigned int num;
	const char   *name   [MAX_AOUT_REG_NUM];
	unsigned int baseAddr[MAX_AOUT_REG_NUM];
	unsigned int length  [MAX_AOUT_REG_NUM];
};

struct aout_irq_des {
#define MAX_AOUT_IRQ_NUM (4)
	unsigned int num;
	const char   *name   [MAX_AOUT_IRQ_NUM];
	unsigned int irq    [MAX_AOUT_IRQ_NUM];
	unsigned int irqFlags[MAX_AOUT_IRQ_NUM];
};

enum aout_subdevice {
	AOUT_PLAYBACK0 = 0,
	AOUT_PLAYBACK1 = 1,
	AOUT_PLAYBACK2 = 2,
	AOUT_CAPTURE   = 3,
};

enum aout_status {
	AOUT_IDLE    = 0,
	AOUT_READY,
	AOUT_RUNNING,
	AOUT_PAUSED,
};

enum aout_samplerate {
	SAMPLERATE_48KHZ = 0,
	SAMPLERATE_44KDOT1HZ,
	SAMPLERATE_32KHZ,
	SAMPLERATE_24KHZ,
	SAMPLERATE_22KDOT05HZ,
	SAMPLERATE_16KHZ,
	SAMPLERATE_11KDOT025HZ,
	SAMPLERATE_8KHZ
};

enum aout_bitsize {
	BIT32         = 0,
	BIT16_L_IN_32 = 1,
	BIT16_M_IN_32 = 2,
	BIT16_R_IN_32 = 3,
	BIT16         = 4
};

enum aout_channelnum {
	SIGNAL_CHANNEL = 0,
	DOUBLE_CHANNEL = 1
};

enum aout_endian {
	LITTLE_ENDIAN = 0,
	BIG_ENDIAN,
};

enum aout_interlace {
	INTERLACE_FALSE = 0,
	INTERLACE_TRUE,
};

enum aout_channel {
	CHANNEL_0 = 0,
	CHANNEL_1
};

enum aout_track {
	STEREO_TRACK = 0,
	LEFT_TRACK,
	RIGHT_TRACK,
};

struct aout_params {
	enum aout_samplerate sampleRate;
	enum aout_bitsize    bitSize;
	enum aout_channelnum channelNum;
	enum aout_endian     endian;
	enum aout_interlace  interlace;
};

struct aout_stream {
	struct aout_reg   *optReg;
	struct aout_lodec *lodecReg;
	unsigned int      *rstReg;
#if defined(CONFIG_ARCH_LEO_MPW)
	struct aout_irq   *irqReg;
#endif
	struct completion pauseComp;
	struct completion suspendComp;
	unsigned int      globalMute;
	unsigned int      globalDacMute;
	long     int      globalDacDBValue;
	long     int      globalDBValue;
	enum aout_track   globalTrack;
	unsigned int      (*max_subdev_callback)(void);
	AOUT_SUBSTREAM*   (*search_substream_callback)(enum aout_subdevice subdev);
};

struct aout_buffer {
	unsigned int size;
	unsigned int channel;
	unsigned int startAddr; //绝对地址
	unsigned int physAddr;
	unsigned int alignSize;
	unsigned int readAddr;  //相对地址
	unsigned int writeAddr; //相对地址
	unsigned int frameAddr; //相对地址,下一帧配置给硬件的起始
};

struct aout_substream {
	enum aout_subdevice subdev;
	enum aout_status    status;
	AOUT_STREAM         *stream;
	unsigned int        currentVolume;
	unsigned int        targetVolume;
	unsigned long long  comeFrameCount;
	unsigned int        needData;
	struct aout_buffer  buffer;

#if defined(CONFIG_ARCH_LEO_MPW)
	unsigned int        optIrq;
#endif
	unsigned int         periodBytes;
	unsigned int         periodPoints;
	enum aout_channelnum channelNum;
	enum aout_interlace  interlace;
	unsigned int         mute;
	unsigned int         dBValue;
	enum aout_track      track;
	int                  (*produce_callback)(void *priv);
	int                  (*consume_callback)(void *priv);
	void                 *priv;
};

extern struct aout_stream *gx8010_core_int(struct platform_device *dev);
extern void gx8010_core_unit (struct aout_stream *stream);
extern int gx8010_core_open  (struct aout_stream *stream,
		enum aout_subdevice subdev,
		int (*__callback)(void *priv),
		void *priv);
extern int gx8010_core_power_suspend(struct aout_stream *stream);
extern int gx8010_core_power_resume (struct aout_stream *stream);
extern int gx8010_core_set_global_volume  (struct aout_stream *stream, long int  dbValue);
extern int gx8010_core_get_global_volume  (struct aout_stream *stream, long int *dbValue);
extern int gx8010_core_set_global_mute    (struct aout_stream *stream, long int  mute);
extern int gx8010_core_get_global_mute    (struct aout_stream *stream, long int *mute);
extern int gx8010_core_set_global_track   (struct aout_stream *stream, enum aout_track  track);
extern int gx8010_core_get_global_track   (struct aout_stream *stream, enum aout_track *track);
extern int gx8010_core_set_global_dac_mute(struct aout_stream *stream, long int  dacMute);
extern int gx8010_core_get_global_dac_mute(struct aout_stream *stream, long int *dacMute);
extern int gx8010_core_set_global_dac_volume(struct aout_stream *stream, long int dacDBValue);
extern int gx8010_core_get_global_dac_volume(struct aout_stream *stream, long int *dacDBValue);

extern int gx8010_core_close (enum aout_subdevice subdev);
extern int gx8010_core_config(enum aout_subdevice subdev, struct aout_params *params);
extern int gx8010_core_run   (enum aout_subdevice subdev);
extern int gx8010_core_rerun (enum aout_subdevice subdev);
extern int gx8010_core_stop  (enum aout_subdevice subdev);
extern int gx8010_core_drain (enum aout_subdevice subdev);
extern int gx8010_core_pause (enum aout_subdevice subdev);
extern int gx8010_core_resume(enum aout_subdevice subdev);
extern int gx8010_core_read  (enum aout_subdevice subdev, unsigned char *buf, unsigned int size);
extern int gx8010_core_write (enum aout_subdevice subdev, unsigned char *buf, unsigned int size);
extern int gx8010_core_set_volume(enum aout_subdevice subdev, long int  dbValue);
extern int gx8010_core_get_volume(enum aout_subdevice subdev, long int *dbValue);
extern int gx8010_core_set_mute  (enum aout_subdevice subdev, long int  mute);
extern int gx8010_core_get_mute  (enum aout_subdevice subdev, long int *mute);
extern int gx8010_core_set_track (enum aout_subdevice subdev, enum aout_track  track);
extern int gx8010_core_get_track (enum aout_subdevice subdev, enum aout_track *track);

extern int gx8010_core_volume_range(long int *minDBValue, long int *maxDBValue);
extern int gx8010_core_dac_volume_range(long int *minDBValue, long int *maxDBValue);

extern unsigned int gx8010_core_free_space(enum aout_subdevice subdev);
extern int   gx8010_core_set_priv(enum aout_subdevice subdev, void *priv);
extern void* gx8010_core_get_priv(enum aout_subdevice subdev);

#endif
