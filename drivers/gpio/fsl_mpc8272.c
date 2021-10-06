#include <linux/init.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>

struct fsl_cpm2_gpio_chip {
	struct gpio_chip gc;
	__be32 __iomem *dir;
	__be32 __iomem *par;
	__be32 __iomem *sor;
	__be32 __iomem *ord;
	__be32 __iomem *dat;
	int bits;
	int port;
	spinlock_t lock;
};

static struct fsl_cpm2_gpio_chip *to_fsl_cpm2_gpio_chip(struct gpio_chip *gc)
{
	return container_of(gc, struct fsl_cpm2_gpio_chip, gc);
}

static unsigned long fsl_cpm2_gpio_pin2mask(struct fsl_cpm2_gpio_chip *bgc, unsigned int pin)
{
	if ( pin < (32 - bgc->bits) ) {
		return 0;
	}
	return 1 << (31 - pin);
}

static int fsl_cpm2_gpio_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct fsl_cpm2_gpio_chip *bgc = to_fsl_cpm2_gpio_chip(gc);
	return in_be32(bgc->dat) & fsl_cpm2_gpio_pin2mask(bgc, gpio);
}

static inline void bb_set(u32 __iomem *p, u32 m)
{
	out_be32(p, in_be32(p) | m);
}

static inline void bb_clr(u32 __iomem *p, u32 m)
{
	out_be32(p, in_be32(p) & ~m);
}

static void fsl_cpm2_gpio_set(struct gpio_chip *gc, unsigned int gpio, int val)
{
	struct fsl_cpm2_gpio_chip *bgc = to_fsl_cpm2_gpio_chip(gc);
	unsigned int mask = fsl_cpm2_gpio_pin2mask(bgc, gpio);
	unsigned long flags;

	spin_lock_irqsave(&bgc->lock, flags);

	if (in_be32(bgc->dir) & mask ) {
		if (val)
			bb_set(bgc->dat, mask);
		else
			bb_clr(bgc->dat, mask);
	}

	spin_unlock_irqrestore(&bgc->lock, flags);

	return;
}

static int fsl_cpm2_gpio_dir_in(struct gpio_chip *gc, unsigned int gpio)
{
	struct fsl_cpm2_gpio_chip *bgc = to_fsl_cpm2_gpio_chip(gc);
	unsigned long flags;

	spin_lock_irqsave(&bgc->lock, flags);

	bb_clr(bgc->dir, fsl_cpm2_gpio_pin2mask(bgc, gpio));

	spin_unlock_irqrestore(&bgc->lock, flags);

	return 0;
}

static int fsl_cpm2_gpio_dir_out(struct gpio_chip *gc, unsigned int gpio, int val)
{
	struct fsl_cpm2_gpio_chip *bgc = to_fsl_cpm2_gpio_chip(gc);
	unsigned long flags;

	spin_lock_irqsave(&bgc->lock, flags);
	bb_set(bgc->dir, fsl_cpm2_gpio_pin2mask(bgc, gpio));
	if ( val ) {
		bb_set(bgc->dat, fsl_cpm2_gpio_pin2mask(bgc, gpio));
	} else {
		bb_clr(bgc->dat, fsl_cpm2_gpio_pin2mask(bgc, gpio));
	}

	spin_unlock_irqrestore(&bgc->lock, flags);
	return 0;
}

static int __devinit fsl_8272_gpio_probe(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct device_node *np;
	struct resource res;
	const u32 *data;
	struct fsl_cpm2_gpio_chip *fsl_cpm2_gpio;
	int len, ret = -ENOMEM;
	static int gpio_base = 0;
	static int port_base = 0x41;

	np = ofdev->dev.of_node;

	fsl_cpm2_gpio = kzalloc(sizeof(struct fsl_cpm2_gpio_chip), GFP_KERNEL);
	if (!fsl_cpm2_gpio)
		goto out;

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		goto out_free_priv;

	if (res.end - res.start < 0x13)
		goto out_free_priv;

	data = of_get_property(np, "fsl,gpio-pin", &len);
	if (!data || len != 4 )
		goto out_free_priv;

	fsl_cpm2_gpio->bits = *data;

	fsl_cpm2_gpio->dir = devm_ioremap(dev, res.start, res.end - res.start + 1);
	if (!fsl_cpm2_gpio->dir)
		goto out_free_priv;

	fsl_cpm2_gpio->par = fsl_cpm2_gpio->dir + 1;
	fsl_cpm2_gpio->sor = fsl_cpm2_gpio->dir + 2;
	fsl_cpm2_gpio->ord = fsl_cpm2_gpio->dir + 3;
	fsl_cpm2_gpio->dat = fsl_cpm2_gpio->dir + 4;

	spin_lock_init(&fsl_cpm2_gpio->lock);

	dev_set_drvdata(dev, fsl_cpm2_gpio);

	fsl_cpm2_gpio->port = port_base;
	fsl_cpm2_gpio->gc.ngpio = 32;
	fsl_cpm2_gpio->gc.direction_input = fsl_cpm2_gpio_dir_in;
	fsl_cpm2_gpio->gc.direction_output = fsl_cpm2_gpio_dir_out;
	fsl_cpm2_gpio->gc.get = fsl_cpm2_gpio_get;
	fsl_cpm2_gpio->gc.set = fsl_cpm2_gpio_set;
	fsl_cpm2_gpio->gc.dev = dev;
	fsl_cpm2_gpio->gc.label = dev_name(dev);
	fsl_cpm2_gpio->gc.base = gpio_base;
	gpio_base += 32;
	port_base ++;

	ret = gpiochip_add(&fsl_cpm2_gpio->gc);
	if (ret) {
		dev_err(dev, "gpiochip_add() failed: %d\n", ret);
		goto out_unmap_regs;
	}

	return 0;

out_unmap_regs:
	dev_set_drvdata(dev, NULL);
	iounmap(fsl_cpm2_gpio->dir);
out_free_priv:
	kfree(fsl_cpm2_gpio);
out:
	return ret;
}

static int fsl_8272_gpio_remove(struct platform_device *ofdev)
{
	int ret;

	struct fsl_cpm2_gpio_chip *fsl_cpm2_gpio = dev_get_drvdata(&ofdev->dev);;
	dev_set_drvdata(&ofdev->dev, NULL);
	devm_iounmap(&ofdev->dev, fsl_cpm2_gpio->dir);
	ret = gpiochip_remove(&fsl_cpm2_gpio->gc);
	kfree(fsl_cpm2_gpio);
	return ret;
}

static struct of_device_id fsl_8272_gpio_match[] = {
	{
		.compatible = "fsl,cpm2-gpio",
	},
	{},
};
MODULE_DEVICE_TABLE(of, fsl_8272_gpio_match);

static struct platform_driver fsl_8272_gpio_driver = {
	.driver = {
		.name = "fsl-mpc8272-gpio",
		.owner = THIS_MODULE,
		.of_match_table = fsl_8272_gpio_match,
	},
	.probe = fsl_8272_gpio_probe,
	.remove = fsl_8272_gpio_remove,
};

static int fsl_8272_gpio_init(void)
{
	return platform_driver_register(&fsl_8272_gpio_driver);
}

static void fsl_8272_gpio_exit(void)
{
	platform_driver_unregister(&fsl_8272_gpio_driver);
}

arch_initcall(fsl_8272_gpio_init);
module_exit(fsl_8272_gpio_exit);
MODULE_AUTHOR("Xiang Wang<xiang.wang@sonos.com>");
MODULE_DESCRIPTION("Freescal MPC8272 GPIO driver");
MODULE_LICENSE("GPL v2");
