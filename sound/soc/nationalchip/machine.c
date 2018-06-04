#include <linux/module.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#define MAX_LINK_CODECS 4

struct snd_soc_dai_link_component gxasoc_link_codecs[MAX_LINK_CODECS] = {
	{
		.name     = "gxasoc-lodac",
		.dai_name = "gxasoc-lodac-dai",
	},
};

static struct snd_soc_dai_link gxasoc_machine_dai_link = {
	.name           = "nationalchip-asoc",
	.stream_name    = "Playback",
	.platform_name  = "gxasoc-platform",
	.cpu_dai_name   = "gxasoc-platform-dai",
	.codecs         = gxasoc_link_codecs,
	.num_codecs     = MAX_LINK_CODECS,
};

static struct snd_soc_card gxasoc_soc_card = {
	.name      = "alsa-gxasoc",
	.owner     = THIS_MODULE,
	.dai_link  = &gxasoc_machine_dai_link,
	.num_links = 1,
};

static int gxasoc_machine_dt_parse(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const char *codec_name, *cup_dai_name, *platform_name, *codec_dai_name;
	u32 num_codecs = 0;
	int ret = 0, i = 0;

	ret = of_property_read_string_index(np, "platform-name", 0, &platform_name);
	if (ret == 0)
		gxasoc_machine_dai_link.platform_name = platform_name;

	ret = of_property_read_u32(np, "num_codecs", &num_codecs);
	if (ret == 0) {
		gxasoc_machine_dai_link.num_codecs = num_codecs;
		for (i = 0; i < num_codecs; i++) {
			ret = of_property_read_string_index(np, "codec-name", i, &codec_name);
			if (ret == 0)
				gxasoc_machine_dai_link.codecs[i].name = codec_name;

			ret = of_property_read_string_index(np, "codec-dai-name", i, &codec_dai_name);
			if (ret == 0)
				gxasoc_machine_dai_link.codecs[i].dai_name = codec_dai_name;
		}
	}

	ret = of_property_read_string_index(np, "cup-dai-name", 0, &cup_dai_name);
	if (ret == 0)
		gxasoc_machine_dai_link.cpu_dai_name = cup_dai_name;

	return 0;
}

static int gxasoc_machine_probe(struct platform_device *pdev)
{
	gxasoc_soc_card.dev = &pdev->dev;
	gxasoc_machine_dt_parse(pdev);

	return devm_snd_soc_register_card(&pdev->dev, &gxasoc_soc_card);
}

static int gxasoc_machine_suspend(struct platform_device *pdev, pm_message_t state)
{
	return snd_soc_suspend(&pdev->dev);
}

static int gxasoc_machine_resume(struct platform_device *pdev)
{
	return snd_soc_resume(&pdev->dev);
}

static const struct of_device_id gxasoc_machine_of_match[] = {
    { .compatible = "NationalChip,ASoc-Machine"},
    {},
};

static struct platform_driver gxasoc_machine_drv = {
	.probe = gxasoc_machine_probe,
	.suspend = gxasoc_machine_suspend,
	.resume  = gxasoc_machine_resume,
	.driver = {
		.name = "gxasoc-machine",
		.of_match_table = gxasoc_machine_of_match,
	},
};

module_platform_driver(gxasoc_machine_drv);
MODULE_LICENSE("GPL");
