/*
 * Description: Driver for the Sonos dummy codec
 * Author: Allen Antony <allen.antony@sonos.com>
 *
 * Copyright (c) 2017, Sonos, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#include <sound/soc.h>
#include <linux/module.h>
#include <linux/platform_device.h>

struct sonos_dummy_codec_config
{
	int rate_bitmask;
	int format_bitmask;
	unsigned int channel_count;
	bool stream_is_capture;
	bool stream_is_capture_and_playback;
	const char *stream_name;
	struct snd_soc_pcm_stream *p_stream;
	struct snd_soc_codec_driver soc_codec_dev_sonos_dummy;
	struct snd_soc_dai_driver sonos_dummy_codec_dai;
};


static int sonos_dummy_codec_get_rate_bitmask(uint32_t rate)
{
	switch (rate) {
		case 8000: return SNDRV_PCM_RATE_8000;
		case 16000: return SNDRV_PCM_RATE_16000;
		case 32000: return SNDRV_PCM_RATE_32000;
		case 44100: return SNDRV_PCM_RATE_44100;
		case 48000: return SNDRV_PCM_RATE_48000;
		case 88200: return SNDRV_PCM_RATE_88200;
		case 96000: return SNDRV_PCM_RATE_96000;
		case 192000: return SNDRV_PCM_RATE_192000;
		default: return -EINVAL;
	}
};

static int sonos_dummy_codec_get_format_bitmask(const char *format)
{
	if (strcmp(format, "S16_LE") == 0) {
		return SNDRV_PCM_FMTBIT_S16_LE;
	} else if (strcmp(format, "S16_BE") == 0) {
		return SNDRV_PCM_FMTBIT_S16_BE;
	} else if (strcmp(format, "U16_LE") == 0) {
		return SNDRV_PCM_FMTBIT_U16_LE;
	} else if (strcmp(format, "U16_BE") == 0) {
		return SNDRV_PCM_FMTBIT_U16_BE;
	} else if (strcmp(format, "S24_LE") == 0) {
		return SNDRV_PCM_FMTBIT_S24_LE;
	} else if (strcmp(format, "S24_BE") == 0) {
		return SNDRV_PCM_FMTBIT_S24_BE;
	} else if (strcmp(format, "U24_LE") == 0) {
		return SNDRV_PCM_FMTBIT_U24_LE;
	} else if (strcmp(format, "U24_BE") == 0) {
		return SNDRV_PCM_FMTBIT_U24_BE;
	} else if (strcmp(format, "S32_LE") == 0) {
		return SNDRV_PCM_FMTBIT_S32_LE;
	} else if (strcmp(format, "S32_BE") == 0) {
		return SNDRV_PCM_FMTBIT_S32_BE;
	} else if (strcmp(format, "U32_LE") == 0) {
		return SNDRV_PCM_FMTBIT_U32_LE;
	} else if (strcmp(format, "U32_BE") == 0) {
		return SNDRV_PCM_FMTBIT_U32_BE;
	} else {
		return -EINVAL;
	}
};

static int sonos_dummy_codec_parse_dt(const struct platform_device *pdev)
{
	uint32_t val;
	const char *format;
	const struct device_node *np = pdev->dev.of_node;
	struct sonos_dummy_codec_config *config;

	config = (struct sonos_dummy_codec_config*)platform_get_drvdata(pdev);
	if (IS_ERR(config)) {
		dev_err (&pdev->dev, "Failed to get driver data from platform_device!\n");
	}

	if (!of_property_read_bool(np, "external-rate")) {
		if (of_property_read_u32(np, "rate", &val) < 0) {
			dev_err (&pdev->dev, "Failed to parse the 'rate' property from dt\n");
			return -ENODATA;
		}

		config->rate_bitmask = sonos_dummy_codec_get_rate_bitmask(val);
		if (config->rate_bitmask <= 0) {
			dev_err (&pdev->dev, "Invalid sample rate provided in dt\n");
			return config->rate_bitmask;
		}
	} else {
		config->rate_bitmask = SNDRV_PCM_RATE_8000_192000;
	}

	if (of_property_read_u32(np, "channels", &val) < 0) {
		dev_err (&pdev->dev, "Failed to parse the 'channels' property from dt\n");
		return -ENODATA;
	}

	config->channel_count = val;
	if (config->channel_count < 1) {
		dev_err (&pdev->dev, "Channel count should be a non-zero positive number\n");
		return -EINVAL;
	}

	if (of_property_read_string(np, "format", &format) < 0) {
		dev_err (&pdev->dev, "Failed to parse the 'format' property from dt\n");
		return -ENODATA;
	}

	config->format_bitmask = sonos_dummy_codec_get_format_bitmask(format);
	if (config->format_bitmask < 0) {
		dev_err (&pdev->dev, "Invalid format provided in dt\n");
		return config->format_bitmask;
	}

	config->stream_is_capture = of_property_read_bool(np, "capture");
	config->stream_is_capture_and_playback = of_property_read_bool(np, "capture-and-playback");

	if (of_property_read_string(np, "stream-name", &config->stream_name) < 0) {
		dev_err (&pdev->dev, "Failed to parse the 'stream-name' property from dt\n");
		return -ENODATA;
	}

	return 0;
}

static int sonos_dummy_codec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sonos_dummy_codec_config *config;

	config = devm_kzalloc(dev, sizeof(struct sonos_dummy_codec_config), GFP_KERNEL);
	if (IS_ERR(config)) {
		dev_err (&pdev->dev, "Failed to allocate memory for private structure\n");
	}

	platform_set_drvdata(pdev, (void*)config);

	if (sonos_dummy_codec_parse_dt(pdev) < 0) {
		dev_err (dev, "Failed to parse one or more properties from dt\n");
		return -ENODEV;
	}

	config->sonos_dummy_codec_dai.name = config->stream_name;

	if (config->stream_is_capture || config->stream_is_capture_and_playback) {
		config->p_stream = &config->sonos_dummy_codec_dai.capture;
		config->p_stream->stream_name = config->stream_name;
		config->p_stream->channels_min = config->channel_count;
		config->p_stream->channels_max = config->channel_count;
		config->p_stream->rates = config->rate_bitmask;
		config->p_stream->formats = config->format_bitmask;
	}

	if ((!config->stream_is_capture) || config->stream_is_capture_and_playback) {
		config->p_stream = &config->sonos_dummy_codec_dai.playback;
		config->p_stream->stream_name = config->stream_name;
		config->p_stream->channels_min = config->channel_count;
		config->p_stream->channels_max = config->channel_count;
		config->p_stream->rates = config->rate_bitmask;
		config->p_stream->formats = config->format_bitmask;
	}


	return snd_soc_register_codec(dev, &config->soc_codec_dev_sonos_dummy, &config->sonos_dummy_codec_dai, 1);
}

static int sonos_dummy_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id sonos_dummy_codec_of_match[] = {
	{ .compatible = "Sonos,dummy-codec", },
	{ /* Elephant in Cairo */ }
};

static struct platform_driver sonos_dummy_codec_driver = {
	.probe		= sonos_dummy_codec_probe,
	.remove		= sonos_dummy_codec_remove,
	.driver		= {
		.name	= "sonos-dummy-codec",
		.of_match_table = sonos_dummy_codec_of_match,
	},
};
module_platform_driver(sonos_dummy_codec_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Sonos, Inc");
MODULE_DESCRIPTION("Sonos dummy codec driver");
MODULE_DEVICE_TABLE(of, sonos_dummy_codec_of_match);
