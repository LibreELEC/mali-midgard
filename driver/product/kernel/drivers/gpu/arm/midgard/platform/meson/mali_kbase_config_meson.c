/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * BSD LICENSE
 *
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/clk/clk-conf.h>
#include <linux/pm_runtime.h>

#include <mali_kbase.h>
#include <mali_kbase_defs.h>
#include <mali_kbase_config.h>

static struct device_node *np;

static const struct of_device_id meson_mali_matches[] = {
	{ .compatible = "amlogic,meson-gxm-mali" },
	{},
};
MODULE_DEVICE_TABLE(of, meson_mali_matches);

static struct kbase_io_resources meson_io_resources;

static struct kbase_platform_config meson_platform_config = {
	.io_resources = &meson_io_resources,
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &meson_platform_config;
}

void kbase_platform_prepare_device(struct platform_device *mali_device)
{
	of_dma_configure(&mali_device->dev, np);

	pm_runtime_enable(&mali_device->dev);
}

int kbase_platform_early_init(void)
{
	struct resource res;
	struct clk *core;
	int irq_job, irq_mmu, irq_gpu;
	int ret;

	np = of_find_matching_node(NULL, meson_mali_matches);
	if (!np) {
		pr_err("Couldn't find the mali node\n");
		return -ENODEV;
	}

	core = of_clk_get_by_name(np, "core");
	if (IS_ERR(core)) {
		pr_err("Couldn't retrieve our module clock\n");
		ret = PTR_ERR(core);
		goto err_put_node;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		pr_err("Couldn't retrieve our base address\n");
		goto err_put_clk;
	}

	irq_job = of_irq_get_byname(np, "job");
	if (irq_job < 0) {
		pr_err("Couldn't get 'job' interrupt\n");
		goto err_put_clk;
	}

	irq_mmu = of_irq_get_byname(np, "mmu");
	if (irq_mmu < 0) {
		pr_err("Couldn't get 'mmu' interrupt\n");
		goto err_put_clk;
	}

	irq_gpu = of_irq_get_byname(np, "gpu");
	if (irq_gpu < 0) {
		pr_err("Couldn't get 'gpu' interrupt\n");
		goto err_put_clk;
	}

	/* Setup IO Resources table */
	meson_io_resources.job_irq_number = irq_job;
	meson_io_resources.mmu_irq_number = irq_mmu;
	meson_io_resources.gpu_irq_number = irq_gpu;
	meson_io_resources.io_memory_region.start = res.start;
	meson_io_resources.io_memory_region.end = res.end;

	of_clk_set_defaults(np, false);

	clk_prepare_enable(core);
	
	pr_info("Amlogic Mali glue initialized\n");

	of_node_put(np);

err_put_clk:
	clk_put(core);
err_put_node:
	of_node_put(np);

	return 0;
}
