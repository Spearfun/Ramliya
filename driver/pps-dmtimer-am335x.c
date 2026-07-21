// SPDX-License-Identifier: GPL-2.0
/*
 * pps-dmtimer: PPS client using AM335x DMTimer hardware edge capture (TCAR1).
 *
 * Timestamp = pps_get_ts() at IRQ time, corrected backwards by
 * (TCRR - TCAR1) ticks of the timer functional clock, i.e. the
 * IRQ latency is removed; residual error ~ 1 tick (41.7 ns @ 24 MHz).
 *
 * Provides LED trigger "pps-dmtimer": a short blink on every captured
 * pulse. Assign it to any LED via linux,default-trigger or sysfs —
 * steady 1 Hz blink = GPS locked (receiver gates PPS by fix validity),
 * dark = no fix.
 *
 * DT binding:
 *   pps_timer: pps-timer {
 *       compatible = "pps-dmtimer";
 *       timer = <&timer7>;
 *       pinctrl-names = "default";
 *       pinctrl-0 = <&timer7_pins>;
 *   };
 *
 * ----------------------------------------------------------------------
 * Copyright (C) 2026 Spearfun
 *
 * Credits & Inspiration:
 *   The original concept of using the AM335x DMTimer capture unit for
 *   Linux PPS support originates from the pps-gmtimer driver for
 *   Linux 3.8 by Dennis Drown:
 *   https://github.com/ddrown/pps-gmtimer
 *
 *   This driver is an independent implementation for modern
 *   Linux 5.10+ kernels. It was completely redesigned and rewritten
 *   around the current OMAP DMTimer framework, the modern Device Tree
 *   model, and the current Linux PPS subsystem.
 * ----------------------------------------------------------------------
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pps_kernel.h>
#include <linux/platform_data/dmtimer-omap.h>
#include <clocksource/timer-ti-dm.h>

#define DRV_NAME	"pps-dmtimer"
#define DRV_VERSION	"1.2"

#ifndef OMAP_TIMER_CTRL_GPOCFG
#define OMAP_TIMER_CTRL_GPOCFG		(1 << 14)
#endif

DEFINE_LED_TRIGGER(pps_dmtimer_led);

struct pps_dmtimer {
    struct device *dev;
    const struct omap_dm_timer_ops *ops;
    struct omap_dm_timer *timer;
    struct platform_device *timer_pdev;
    struct pps_device *pps;
    struct pps_source_info info;
    unsigned long rate;
    int irq;
    u32 count;
    u32 spurious;
    u32 last_delta_ns;	/* removed IRQ latency, last pulse */
};

/*
 * Raw register access, posted-mode safe.
 * reg = _OFFSET | (WP bit << WPSHIFT), as encoded in timer-ti-dm.h.
 */
static u32 dmt_read(struct pps_dmtimer *p, u32 reg)
{
    if (p->timer->posted)
	while (readl_relaxed(p->timer->pend) & (reg >> WPSHIFT))
	    cpu_relax();
    return readl_relaxed(p->timer->func_base + (reg & 0xff));
}

static void dmt_write(struct pps_dmtimer *p, u32 reg, u32 val)
{
    if (p->timer->posted)
	while (readl_relaxed(p->timer->pend) & (reg >> WPSHIFT))
	    cpu_relax();
    writel_relaxed(val, p->timer->func_base + (reg & 0xff));
}

static irqreturn_t pps_dmtimer_irq(int irq, void *dev_id)
{
    struct pps_dmtimer *p = dev_id;
    struct pps_event_time ts;
    unsigned long led_on = 30, led_off = 30;	/* ms */
    u32 status, tcar, tcrr, delta;
    u64 delta_ns;

    status = readl_relaxed(p->timer->irq_stat);
    if (!(status & OMAP_TIMER_INT_CAPTURE)) {
	p->spurious++;
	return IRQ_NONE;
    }

    /* counter "now" and system time — back to back */
    tcrr = dmt_read(p, OMAP_TIMER_COUNTER_REG);
    pps_get_ts(&ts);
    /* counter latched at the PPS edge */
    tcar = dmt_read(p, OMAP_TIMER_CAPTURE_REG);

    /* ack */
    writel_relaxed(OMAP_TIMER_INT_CAPTURE, p->timer->irq_stat);

    delta = tcrr - tcar;	/* ticks since edge; u32 wrap-safe */
    delta_ns = div_u64((u64)delta * NSEC_PER_SEC, p->rate);
    pps_sub_ts(&ts, ns_to_timespec64(delta_ns));

    pps_event(p->pps, &ts, PPS_CAPTUREASSERT, NULL);

    led_trigger_blink_oneshot(pps_dmtimer_led, &led_on, &led_off, 0);

    p->last_delta_ns = (u32)delta_ns;
    p->count++;

    return IRQ_HANDLED;
}

/* ---- sysfs: verification/statistics ---- */

static ssize_t count_show(struct device *dev, struct device_attribute *attr,
	      char *buf)
{
    struct pps_dmtimer *p = dev_get_drvdata(dev);

    return sysfs_emit(buf, "%u\n", p->count);
}
static DEVICE_ATTR_RO(count);

static ssize_t spurious_show(struct device *dev, struct device_attribute *attr,
	         char *buf)
{
    struct pps_dmtimer *p = dev_get_drvdata(dev);

    return sysfs_emit(buf, "%u\n", p->spurious);
}
static DEVICE_ATTR_RO(spurious);

static ssize_t last_delta_ns_show(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
    struct pps_dmtimer *p = dev_get_drvdata(dev);

    return sysfs_emit(buf, "%u\n", p->last_delta_ns);
}
static DEVICE_ATTR_RO(last_delta_ns);

static ssize_t rate_show(struct device *dev, struct device_attribute *attr,
	     char *buf)
{
    struct pps_dmtimer *p = dev_get_drvdata(dev);

    return sysfs_emit(buf, "%lu\n", p->rate);
}
static DEVICE_ATTR_RO(rate);

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
	        char *buf)
{
    return sysfs_emit(buf, "%s\n", DRV_VERSION);
}
static DEVICE_ATTR_RO(version);

static struct attribute *pps_dmtimer_attrs[] = {
    &dev_attr_count.attr,
    &dev_attr_spurious.attr,
    &dev_attr_last_delta_ns.attr,
    &dev_attr_rate.attr,
    &dev_attr_version.attr,
    NULL,
};
ATTRIBUTE_GROUPS(pps_dmtimer);

/* ---- probe / remove ---- */

static int pps_dmtimer_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *timer_node;
    struct dmtimer_platform_data *timer_pdata;
    struct pps_dmtimer *p;
    u32 l;
    int ret;

    if (!dev->of_node)
	return -ENODEV;

    p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
    if (!p)
	return -ENOMEM;

    p->dev = dev;
    platform_set_drvdata(pdev, p);

    timer_node = of_parse_phandle(dev->of_node, "timer", 0);
    if (!timer_node)
	return dev_err_probe(dev, -ENODEV, "missing 'timer' phandle\n");

    p->timer_pdev = of_find_device_by_node(timer_node);
    if (!p->timer_pdev) {
	of_node_put(timer_node);
	return dev_err_probe(dev, -EPROBE_DEFER,
		     "timer device not found\n");
    }

    timer_pdata = dev_get_platdata(&p->timer_pdev->dev);
    if (!timer_pdata) {
	ret = dev_err_probe(dev, -EPROBE_DEFER,
		    "timer pdata not ready\n");
	goto err_put_pdev_node;
    }

    p->ops = timer_pdata->timer_ops;
    if (!p->ops || !p->ops->request_by_node || !p->ops->free ||
        !p->ops->get_irq || !p->ops->set_source || !p->ops->set_load ||
        !p->ops->start || !p->ops->stop || !p->ops->set_int_enable ||
        !p->ops->set_int_disable) {
	ret = dev_err_probe(dev, -EINVAL, "incomplete timer_ops\n");
	goto err_put_pdev_node;
    }

    p->timer = p->ops->request_by_node(timer_node);
    of_node_put(timer_node);
    if (!p->timer) {
	ret = dev_err_probe(dev, -EBUSY, "request_by_node failed\n");
	goto err_put_pdev;
    }

    ret = p->ops->set_source(p->timer, OMAP_TIMER_SRC_SYS_CLK);
    if (ret)
	dev_warn(dev, "set_source(SYS_CLK) returned %d\n", ret);

    p->rate = clk_get_rate(p->timer->fclk);
    if (!p->rate) {
	dev_warn(dev, "fclk rate unknown, assuming 24 MHz\n");
	p->rate = 24000000;
    }

    p->irq = p->ops->get_irq(p->timer);
    if (p->irq < 0) {
	ret = dev_err_probe(dev, p->irq, "get_irq failed\n");
	goto err_free_timer;
    }

    memset(&p->info, 0, sizeof(p->info));
    strscpy(p->info.name, DRV_NAME, sizeof(p->info.name));
    strscpy(p->info.path, "", sizeof(p->info.path));
    p->info.mode = PPS_CAPTUREASSERT | PPS_OFFSETASSERT |
	       PPS_CANWAIT | PPS_TSFMT_TSPEC;
    p->info.owner = THIS_MODULE;

    p->pps = pps_register_source(&p->info,
		     PPS_CAPTUREASSERT | PPS_OFFSETASSERT);
    if (IS_ERR(p->pps)) {
	ret = dev_err_probe(dev, PTR_ERR(p->pps),
		    "pps_register_source failed\n");
	goto err_free_timer;
    }

    led_trigger_register_simple(DRV_NAME, &pps_dmtimer_led);

    ret = devm_request_irq(dev, p->irq, pps_dmtimer_irq,
	           IRQF_NO_THREAD, DRV_NAME, p);
    if (ret) {
	dev_err_probe(dev, ret, "request_irq %d failed\n", p->irq);
	goto err_unreg_pps;
    }

    /*
     * Free-running counter: TLDR=0, start (ST=1, clock stays on),
     * then add autoreload + capture on rising edge + pin as input.
     * CAPT_MODE stays 0 = single capture into TCAR1.
     */
    p->ops->set_load(p->timer, 0);
    p->ops->start(p->timer);

    l = dmt_read(p, OMAP_TIMER_CTRL_REG);
    l |= OMAP_TIMER_CTRL_AR | OMAP_TIMER_CTRL_TCM_LOWTOHIGH |
         OMAP_TIMER_CTRL_GPOCFG;
    l &= ~(OMAP_TIMER_CTRL_TCM_HIGHTOLOW | OMAP_TIMER_CTRL_CAPTMODE);
    dmt_write(p, OMAP_TIMER_CTRL_REG, l);

    p->ops->set_int_enable(p->timer, OMAP_TIMER_INT_CAPTURE);

    pr_info("pps_dmtimer: PPS source %d registered: v%s, irq %d, fclk %lu Hz, TCLR=0x%08x, LED trigger '%s'\n",
	p->pps->id, DRV_VERSION, p->irq, p->rate,
	dmt_read(p, OMAP_TIMER_CTRL_REG), DRV_NAME);
    return 0;

err_unreg_pps:
    led_trigger_unregister_simple(pps_dmtimer_led);
    pps_unregister_source(p->pps);
err_free_timer:
    p->ops->free(p->timer);
err_put_pdev:
    put_device(&p->timer_pdev->dev);
    return ret;

err_put_pdev_node:
    of_node_put(timer_node);
    put_device(&p->timer_pdev->dev);
    return ret;
}

static int pps_dmtimer_remove(struct platform_device *pdev)
{
    struct pps_dmtimer *p = platform_get_drvdata(pdev);

    p->ops->set_int_disable(p->timer, OMAP_TIMER_INT_CAPTURE);
    p->ops->stop(p->timer);
    p->ops->free(p->timer);
    led_trigger_unregister_simple(pps_dmtimer_led);
    pps_unregister_source(p->pps);
    put_device(&p->timer_pdev->dev);

    return 0;
}

static const struct of_device_id pps_dmtimer_of_match[] = {
    { .compatible = "pps-dmtimer" },
    { }
};
MODULE_DEVICE_TABLE(of, pps_dmtimer_of_match);

static struct platform_driver pps_dmtimer_driver = {
    .probe = pps_dmtimer_probe,
    .remove = pps_dmtimer_remove,
    .driver = {
	.name = DRV_NAME,
	.of_match_table = pps_dmtimer_of_match,
	.dev_groups = pps_dmtimer_groups,
    },
};

module_platform_driver(pps_dmtimer_driver);

MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_AUTHOR("Spearfun");
MODULE_DESCRIPTION("PPS source via AM335x DMTimer hardware capture");
