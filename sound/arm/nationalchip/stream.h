#ifndef __GX8010_STREAN_H__
#define __GX8010_STREAN_H__

#define WAIT_TIME 80

enum aout_spd_mode_sel {
	SPD_PLAY_OFF = 0,
	SPD_PLAY_IDLE,
	SPD_PLAY_FROM_I2S,
};

enum aout_dac_bits_sel {
	I2S_DAC_BIT_16 = 0,
	I2S_DAC_BIT_20,
	I2S_DAC_BIT_24,
};

enum aout_dac_mclk_sel {
	I2S_DAC_FS_256 = 0,
	I2S_DAC_FS_512,
	I2S_DAC_FS_1024,
};

enum aout_dac_format_sel {
	I2S_DAC_FORMAT_I2S = 0,
	I2S_DAC_FORMAT_LEFT_JUSTIFIED,
	I2S_DAC_FORMAT_RIGHT_JUSTIFIED,
};

enum aout_dev_source {
	DEV_SRC_0 = 0,
	DEV_SRC_1,
	DEV_SRC_BYPASS,
	DEV_SRC_OFF
};

extern int gx8010_stream_init     (struct aout_stream *stream,
		struct aout_substream* (*search_substream_callback)(enum aout_subdevice subdev),
		unsigned int (*max_subdev_callback)(void));
extern int gx8010_stream_uninit   (struct aout_stream *stream);
extern int gx8010_stream_power_suspend(struct aout_stream *stream);
extern int gx8010_stream_power_resume(struct aout_stream *stream);
extern int gx8010_stream_interrupt(struct aout_stream *stream);

extern int gx8010_stream_open  (struct aout_substream *substream,
		int (*__callback)(void *priv),
		void *priv);
extern int gx8010_stream_close (struct aout_substream *substream);
extern int gx8010_stream_set_global_volume  (struct aout_stream *stream, long int  dbValue);
extern int gx8010_stream_get_global_volume  (struct aout_stream *stream, long int *dbValue);
extern int gx8010_stream_set_global_mute    (struct aout_stream *stream, long int  mute);
extern int gx8010_stream_get_global_mute    (struct aout_stream *stream, long int *mute);
extern int gx8010_stream_set_global_track   (struct aout_stream *stream, enum aout_track  track);
extern int gx8010_stream_get_global_track   (struct aout_stream *stream, enum aout_track *track);
extern int gx8010_stream_set_global_dac_mute(struct aout_stream *stream, long int  dacMute);
extern int gx8010_stream_get_global_dac_mute(struct aout_stream *stream, long int *dacMute);
extern int gx8010_stream_set_global_dac_volume(struct aout_stream *stream, long int  dacDBValue);
extern int gx8010_stream_get_global_dac_volume(struct aout_stream *stream, long int *dacDBValue);


extern int gx8010_stream_config(struct aout_substream *substream, struct aout_params *params);
extern int gx8010_stream_run   (struct aout_substream *substream);
extern int gx8010_stream_rerun (struct aout_substream *substream);
extern int gx8010_stream_stop  (struct aout_substream *substream);
extern int gx8010_stream_pause (struct aout_substream *substream);
extern int gx8010_stream_resume(struct aout_substream *substream);
extern int gx8010_stream_read  (struct aout_substream *substream, unsigned char *buf, unsigned int size);
extern int gx8010_stream_write (struct aout_substream *substream, unsigned char *buf, unsigned int size);
extern int gx8010_stream_set_volume(struct aout_substream *substream, long int  dbValue);
extern int gx8010_stream_get_volume(struct aout_substream *substream, long int *dbValue);
extern int gx8010_stream_set_mute  (struct aout_substream *substream, long int  mute);
extern int gx8010_stream_get_mute  (struct aout_substream *substream, long int *mute);
extern int gx8010_stream_set_track (struct aout_substream *substream, enum aout_track  track);
extern int gx8010_stream_get_track (struct aout_substream *substream, enum aout_track *track);

extern int gx8010_stream_volume_range(long int *minDBValue, long int *maxDBValue);
extern int gx8010_stream_dac_volume_range(long int *minDBValue, long int *maxDBValue);

extern unsigned int gx8010_stream_free_space(struct aout_substream *substream);

#endif
