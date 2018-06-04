/*
 *  ALSA Platform Device Driver
 *
 * Copyright (C) 1991-2017 NationalChip Co., Ltd
 * All rights reserved!
 *
 * alsa.c: ALSA Platform Driver Implement
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include "pcm.h"
#include "control.h"
#include "core.h"

static int gx8010_alsa_probe(struct platform_device *dev)
{
	struct snd_card *card;
	int ret;

	if (dev->id >= 0) {
		dev_err(&dev->dev, "PXA2xx has only one AC97 port.\n");
		ret = -ENXIO;
		goto err_dev;
	}

	ret = snd_card_new(&dev->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
			   THIS_MODULE, 0, &card);
	if (ret < 0)
		goto err;

	dev_set_drvdata(&dev->dev, card);
	strlcpy(card->driver, dev->dev.driver->name, sizeof(card->driver));

	card->private_data = (void*)gx8010_core_int(dev);

	ret = gx8010_pcm_new(card);
	if (ret)
		goto err;

	ret = gx8010_control_new(card);
	if (ret)
		goto err;

	snprintf(card->shortname, sizeof(card->shortname), "%s", dev->dev.driver->name);
	snprintf(card->longname,  sizeof(card->longname),  "%s, %s", "nationalchip", dev->dev.driver->name);

	ret = snd_card_register(card);
	if (ret == 0) {
		platform_set_drvdata(dev, card);
		return 0;
	}

err:
	if (card)
		snd_card_free(card);
err_dev:
	return ret;
}

static int gx8010_alsa_remove(struct platform_device *dev)
{
	struct snd_card *card = dev_get_drvdata(&dev->dev);

	if (card) {
		gx8010_core_unit(card->private_data);
		card->private_data = NULL;
		snd_card_free(card);
	}

	return 0;
}

static int gx8010_alsa_suspend(struct platform_device *dev, pm_message_t state)
{
	struct snd_card *card = dev_get_drvdata(&dev->dev);

	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);
	gx8010_pcm_suspend();
	gx8010_core_power_suspend((struct aout_stream *)card->private_data);

	return 0;
}

static int gx8010_alsa_resume(struct platform_device *dev)
{
	struct snd_card *card = dev_get_drvdata(&dev->dev);

	gx8010_core_power_resume((struct aout_stream *)card->private_data);
	gx8010_pcm_resume();
	snd_power_change_state(card, SNDRV_CTL_POWER_D0);

	return 0;
}

static const struct of_device_id gx8010_alsa_of_match[] = {
	{ .compatible = "NationalChip,Alsa-Audio"},
	{},
};

static struct platform_driver gx8010_alsa_driver = {
	.probe     = gx8010_alsa_probe,
	.remove    = gx8010_alsa_remove,
	.suspend   = gx8010_alsa_suspend,
	.resume    = gx8010_alsa_resume,
	.driver    = {
		.name           = "gx8010-alsa",
		.of_match_table = gx8010_alsa_of_match,
	},
};

module_platform_driver(gx8010_alsa_driver);

MODULE_AUTHOR("linxsh");
MODULE_DESCRIPTION("audio driver for the gx8010 chip");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gx8010-alsa");
