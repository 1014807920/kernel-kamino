#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/io.h>

static struct snd_soc_dai_driver gxasoc_platform_dai[] = {
	{
		.name = "gxasoc-platform-dai",
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = (SNDRV_PCM_FMTBIT_S16_BE|
						SNDRV_PCM_FMTBIT_S16_LE|
						SNDRV_PCM_FMTBIT_S32_BE|
						SNDRV_PCM_FMTBIT_S32_LE),
		},
	},
};

static const struct snd_soc_component_driver gxasoc_platform_component = {
	.name = "gxasoc-platform-component",
};

static int gxasoc_dai_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;
	const char *dev_name = NULL;

	ret = of_property_read_string_index(np, "dev-name", 0, &dev_name);
	if (ret == 0)
		dev_set_name(&pdev->dev, dev_name);

	return devm_snd_soc_register_component(&pdev->dev,
				&gxasoc_platform_component, gxasoc_platform_dai, ARRAY_SIZE(gxasoc_platform_dai));
}

static const struct of_device_id gxasoc_dai_of_match[] = {
    { .compatible = "NationalChip,ASoc-Platform_Dai"},
    {},
};

static struct platform_driver gxasoc_dai_drv = {
	.probe  = gxasoc_dai_probe,
	.driver = {
		.name = "gxasoc-dai",
		.of_match_table = gxasoc_dai_of_match,
	},
};

module_platform_driver(gxasoc_dai_drv);
MODULE_LICENSE("GPL");
