// SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause
/*
 * Copyright (c) 2024, Max Resch
 */

#include <dm.h>
#include <malloc.h>
#include <rng.h>
#include <asm/dma-mapping.h>
#include <asm/types.h>
#include <mach/mbox.h>

/* size of entropy buffer */
#define RNG_BUFFER_SIZE	128U

struct turris_rwtm_rng_priv {
	void *buffer;
	dma_addr_t buffer_handle;
};

static int turris_rwtm_rng_fill_entropy(dma_addr_t entropy, size_t size)
{
	u32 args[3] = { 1, (u32)entropy, size };
	int ret;

	/* flush data cache */
	flush_dcache_range(entropy, entropy + size);

	/*
	 * get entropy
	 * args[0] = 1 copies BYTES array in args[1] of length args[2]
	 */
	ret = mbox_do_cmd(MBOX_CMD_GET_RANDOM, args, 3, NULL, 0);
	if (ret < 0)
		return ret;

	/* invalidate data cache */
	invalidate_dcache_range(entropy, entropy + size);

	return 0;
}

static int turris_rwtm_rng_random_read(struct udevice *dev, void *data, size_t count)
{
	struct turris_rwtm_rng_priv *priv = dev_get_priv(dev);
	size_t size;
	int ret;

	while (count) {
		size = min_t(size_t, RNG_BUFFER_SIZE, count);

		ret = turris_rwtm_rng_fill_entropy(priv->buffer_handle, size);
		if (ret < 0)
			return ret;

		memcpy(data, priv->buffer, size);
		count -= size;
		data = (u8 *)data + size;
	}

	return 0;
}

static int turris_rwtm_rng_probe(struct udevice *dev)
{
	struct turris_rwtm_rng_priv *priv = dev_get_priv(dev);
	u32 args[] = { 0 };
	int ret;

	/*
	 * check if the random command is supported
	 * args[0] = 0 would copy 16 DWORDS entropy to out but we ignore them
	 */
	ret = mbox_do_cmd(MBOX_CMD_GET_RANDOM, args, ARRAY_SIZE(args), NULL, 0);
	if (ret < 0)
		return ret;

	/* entropy buffer address needs to be aligned */
	priv->buffer = dma_alloc_coherent(RNG_BUFFER_SIZE, &priv->buffer_handle);
	if (!priv->buffer)
		return -ENOMEM;

	return 0;
}

static int turris_rwtm_rng_remove(struct udevice *dev)
{
	struct turris_rwtm_rng_priv *priv = dev_get_priv(dev);

	dma_free_coherent(priv->buffer);

	return 0;
}

static const struct dm_rng_ops turris_rwtm_rng_ops = {
	.read = turris_rwtm_rng_random_read,
};

/*
 * only Turris MOX firmware has the RNG but allow all probable devices to be
 * probed the default firmware will just reject the probe
 */
static const struct udevice_id turris_rwtm_rng_match[] = {
	{ .compatible = "cznic,turris-mox-rwtm" },
	{ .compatible = "marvell,armada-3700-rwtm-firmware" },
	{},
};

U_BOOT_DRIVER(turris_rwtm_rng) = {
	.name	= "turris-rwtm-rng",
	.id	= UCLASS_RNG,
	.of_match = turris_rwtm_rng_match,
	.ops	= &turris_rwtm_rng_ops,
	.probe	= turris_rwtm_rng_probe,
	.remove = turris_rwtm_rng_remove,
	.priv_auto = sizeof(struct turris_rwtm_rng_priv),
};
