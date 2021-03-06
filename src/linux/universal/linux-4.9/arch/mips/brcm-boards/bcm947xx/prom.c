/*
 * Early initialization code for BCM94710 boards
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: prom.c,v 1.8 2010-07-09 06:00:16 $
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/config.h>
#endif
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <bcmendian.h>
#include <hndsoc.h>
#include <siutils.h>
#include <hndcpu.h>
#include <mipsinc.h>
#include <mips74k_core.h>
#ifdef	CONFIG_CFE
#include <asm/fw/cfe/cfe_api.h>
#endif

#include "bcm947xx.h"

/* Global SB handle */
extern si_t *bcm947xx_sih;

/* Convenience */
#define sih bcm947xx_sih

#define MB      << 20

#ifdef  CONFIG_HIGHMEM

#define EXTVBASE        0xc0000000
#define ENTRYLO(x)      ((pte_val(pfn_pte((x) >> PAGE_SHIFT, PAGE_KERNEL_UNCACHED)) >> 6) | 1)
#define UNIQUE_ENTRYHI(idx) (CKSEG0 + ((idx) << (PAGE_SHIFT + 1)))


static unsigned long tmp_tlb_ent __initdata;

#if defined(CONFIG_CFE) && defined(CONFIG_EARLY_PRINTK)
void prom_putchar(char c)
{
	while (cfe_write(cfe_cons_handle, &c, 1) == 0)
		;
}
#endif

/* Initialize the wired register and all tlb entries to 
 * known good state.
 */
void __init
early_tlb_init(void)
{
	unsigned long  index;
	struct cpuinfo_mips *c = &current_cpu_data;

	tmp_tlb_ent = c->tlbsize;

	/* printk(KERN_ALERT "%s: tlb size %ld\n", __FUNCTION__, c->tlbsize); */

	/*
	* initialize entire TLB to uniqe virtual addresses
	* but with the PAGE_VALID bit not set
	*/
	write_c0_pagemask(PM_DEFAULT_MASK);
	write_c0_wired(0);

	write_c0_entrylo0(0);   /* not _PAGE_VALID */
	write_c0_entrylo1(0);

	for (index = 0; index < c->tlbsize; index++) {
		/* Make sure all entries differ. */
		write_c0_entryhi(UNIQUE_ENTRYHI(index+32));
		write_c0_index(index);
		mtc0_tlbw_hazard();
		tlb_write_indexed();
	}

	tlbw_use_hazard();

}

void __init
add_tmptlb_entry(unsigned long entrylo0, unsigned long entrylo1,
		 unsigned long entryhi, unsigned long pagemask)
{
/* write one tlb entry */
	--tmp_tlb_ent;
	write_c0_index(tmp_tlb_ent);
	tlbw_use_hazard();	/* What is the hazard here? */
	write_c0_pagemask(pagemask);
	write_c0_entryhi(entryhi);
	write_c0_entrylo0(entrylo0);
	write_c0_entrylo1(entrylo1);
	mtc0_tlbw_hazard();
	tlb_write_indexed();
	tlbw_use_hazard();
}
#endif  /* CONFIG_HIGHMEM */

static unsigned long detectmem;
extern char ram_nvram_buf[];
void __init
prom_init(void)
{
	unsigned long extmem = 0, off, data;
	static unsigned long mem;
	unsigned long off1, data1;
	struct nvram_header *header;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	/* These are not really being used anywhere - LR */
	mips_machgroup = MACH_GROUP_BRCM;
	mips_machtype = MACH_BCM947XX;
#endif

	off = (unsigned long)prom_init;
	data = *(unsigned long *)prom_init;
	off1 = off + 4;
	data1 = *(unsigned long *)off1;

	/* Figure out memory size by finding aliases */
	for (mem = (1 MB); mem < (128 MB); mem <<= 1) {
		if ((*(unsigned long *)(off + mem) == data) &&
			(*(unsigned long *)(off1 + mem) == data1))
			break;
	}
	detectmem = mem;
#if 0// defined(CONFIG_HIGHMEM) && !defined(CONFIG_BCM80211AC)
	if (mem == 128 MB) {

		early_tlb_init();
		/* Add one temporary TLB entries to map SDRAM Region 2.
		*      Physical        Virtual
		*      0x80000000      0xc0000000      (1st: 256MB)
		*      0x90000000      0xd0000000      (2nd: 256MB)
		*/
		add_tmptlb_entry(ENTRYLO(SI_SDRAM_R2),
				 ENTRYLO(SI_SDRAM_R2 + (256 MB)),
				 EXTVBASE, PM_256M);

		off = EXTVBASE + __pa(off);
		for (extmem = (128 MB); extmem < (512 MB); extmem <<= 1) {
			if (*(unsigned long *)(off + extmem) == data)
				break;
		}

		extmem -= mem;
		/* Keep tlb entries back in consistent state */
		early_tlb_init();
	}
#endif  /* CONFIG_HIGHMEM */
	/* Ignoring the last page when ddr size is 128M. Cached
	 * accesses to last page is causing the processor to prefetch
	 * using address above 128M stepping out of the ddr address
	 * space.
	 */
	if (MIPS74K(current_cpu_data.processor_id) && (mem == (128 MB)))
		mem -= 0x1000;

	/* CFE could have loaded nvram during netboot
	 * to top 32KB of RAM, Just check for nvram signature
	 * and copy it to nvram space embedded in linux
	 * image for later use by nvram driver.
	 */
	header = (struct nvram_header *)(KSEG0ADDR(mem - NVRAM_SPACE));
	if (ltoh32(header->magic) == NVRAM_MAGIC) {
		uint32 *src = (uint32 *)header;
		uint32 *dst = (uint32 *)ram_nvram_buf;
		uint32 i;

		printk("Copying NVRAM bytes: %d from: 0x%p To: 0x%p\n", ltoh32(header->len),
			src, dst);
		for (i = 0; i < ltoh32(header->len) && i < NVRAM_SPACE; i += 4)
			*dst++ = ltoh32(*src++);
	}

	add_memory_region(SI_SDRAM_BASE, mem, BOOT_MEM_RAM);

#if 0// defined(CONFIG_HIGHMEM) && !defined(CONFIG_BCM80211AC)
	if (extmem) {
		/* We should deduct 0x1000 from the second memory
		 * region, because of the fact that processor does prefetch.
		 * Now that we are deducting a page from second memory 
		 * region, we could add the earlier deducted 4KB (from first bank)
		 * to the second region (the fact that 0x80000000 -> 0x88000000
		 * shadows 0x0 -> 0x8000000)
		 */
		if (MIPS74K(current_cpu_data.processor_id) && (mem == (128 MB)))
			extmem -= 0x1000;
		add_memory_region(SI_SDRAM_R2 + (128 MB) - 0x1000, extmem, BOOT_MEM_RAM);
	}
#endif  /* CONFIG_HIGHMEM */
}

void __init
prom_free_prom_memory(void)
{
}

#include <asm/tlbmisc.h>
unsigned long extended_ram=0;

extern si_t *bcm947xx_sih;

void __init memory_setup(void)
{
#if  defined(CONFIG_HIGHMEM)
unsigned long extmem = 0, off, data;
unsigned long off1, data1;
off = (unsigned long)prom_init;
data = *(unsigned long *)prom_init;
off1 = off + 4;
data1 = *(unsigned long *)off1;
extmem = 128 MB;
int highmemsupport=0;
uint boardnum;

	/* Get global SB handle */
bcm947xx_sih = si_kattach(SI_OSH);
boardnum = bcm_strtoul( nvram_safe_get( "boardnum" ), NULL, 0 );

	if (boardnum == 0 && nvram_match("boardtype", "0xF5B2")
	    && nvram_match("boardrev", "0x1100")
	    && !nvram_match("pci/2/1/sb20in80and160hr5ghpo", "0"))
		highmemsupport = 1;

	if (boardnum == 00 && nvram_match("boardtype", "0xF5B2")
	    && nvram_match("boardrev", "0x1100")
	    && nvram_match("pci/2/1/sb20in80and160hr5ghpo", "0")) 
		highmemsupport = 1;



	if (extmem && detectmem == 128 MB && highmemsupport) {
		/* We should deduct 0x1000 from the second memory
		 * region, because of the fact that processor does prefetch.
		 * Now that we are deducting a page from second memory 
		 * region, we could add the earlier deducted 4KB (from first bank)
		 * to the second region (the fact that 0x80000000 -> 0x88000000
		 * shadows 0x0 -> 0x8000000)
		 */
		if (MIPS74K(current_cpu_data.processor_id) && (detectmem == (128 MB)))
			extmem -= 0x1000;
		extended_ram = extmem;
		add_memory_region(SI_SDRAM_R2 + (128 MB) - 0x1000, extmem, BOOT_MEM_RAM);
	}
#endif  /* CONFIG_HIGHMEM */


}