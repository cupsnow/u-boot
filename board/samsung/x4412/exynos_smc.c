/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * refer to samsung's uboot-2010.12 source code
 *
 * SMC command
 */

#include <common.h>
#include <asm/arch/smc.h>

void sdelay(unsigned long);

/* SDMMC */
struct sdmmc_dev {
    unsigned int images_pos;
    unsigned int block_count;
    unsigned int base_addr;
};

/* eMMC */
struct emmc_dev {
    unsigned int block_count;
    unsigned int base_addr;
};

/* SATA */
struct sata_dev {
    u64 read_sector_of_hdd;
    u32 trans_byte;
    u32 * read_buffer;
    u32 position_of_mem;
};

/* SFMC */
struct sfmc_dev {
    unsigned int cs;
    unsigned int byte_offset;
    unsigned int byte_size;
    void * dest_addr;
};

/* SPI SERIAL FLASH */
struct spi_sf_dev {
    u32 flash_read_addr;
    u32 read_size;
    u8 *read_buffer;
};

/* boot device */
union boot_device {
    struct sdmmc_dev sdmmc;
    struct emmc_dev emmc;
    struct sata_dev sata;
    struct sfmc_dev sfmc;
    struct spi_sf_dev spi_sf;
};

/* Signature */
struct ld_image_info {
    u32 image_base_addr;
    u32 size;
    u32 secure_context_base;
    u32 signature_size;
    union boot_device bootdev;
};

static inline u32 exynos_smc(u32 cmd, u32 arg1, u32 arg2, u32 arg3)
{
    register u32 reg0 __asm__("r0") = cmd;
    register u32 reg1 __asm__("r1") = arg1;
    register u32 reg2 __asm__("r2") = arg2;
    register u32 reg3 __asm__("r3") = arg3;

    __asm__ volatile (
            ".arch_extension sec\n"
            "smc    0\n"
            : "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)
            );
    return reg0;
}

void load_uboot_image(u32 boot_device)
{
    struct ld_image_info *info_image;
    info_image = (struct ld_image_info *)CONFIG_IMAGE_INFO_BASE;

    if (boot_device == SDMMC_CH2) {
        info_image->bootdev.sdmmc.images_pos = UBOOT_START_OFFSET;
        info_image->bootdev.sdmmc.block_count = UBOOT_SIZE_BLOC_COUNT;
        info_image->bootdev.sdmmc.base_addr = CONFIG_SYS_TEXT_BASE;
    } else if (boot_device == EMMC44_CH4) {
        info_image->bootdev.emmc.block_count = UBOOT_SIZE_BLOC_COUNT;
        info_image->bootdev.emmc.base_addr = CONFIG_SYS_TEXT_BASE;
    }

    info_image->image_base_addr = CONFIG_SYS_TEXT_BASE;
    info_image->size = COPY_UBOOT_SIZE;
    info_image->secure_context_base = SMC_SECURE_CONTEXT_BASE;
    info_image->signature_size = 0;

    exynos_smc(SMC_CMD_LOAD_UBOOT, boot_device, CONFIG_IMAGE_INFO_BASE, 0);
}

void cold_boot(u32 boot_device)
{
    struct ld_image_info *info_image;
    info_image = (struct ld_image_info *)CONFIG_IMAGE_INFO_BASE;

    if (boot_device == SDMMC_CH2) {
        info_image->bootdev.sdmmc.images_pos = TZSW_START_OFFSET;
        info_image->bootdev.sdmmc.block_count = TZSW_SIZE_BLOC_COUNT;
        info_image->bootdev.sdmmc.base_addr = CONFIG_SYS_TZSW_BASE;
    } else if (boot_device == EMMC44_CH4) {
        sdelay(1000); /* required for eMMC 4.4 */
        info_image->bootdev.emmc.block_count = TZSW_SIZE_BLOC_COUNT;
        info_image->bootdev.emmc.base_addr = CONFIG_SYS_TZSW_BASE;
    }

    info_image->image_base_addr = CONFIG_SYS_TZSW_BASE;
    info_image->size = COPY_TZSW_SIZE;
    info_image->secure_context_base = SMC_SECURE_CONTEXT_BASE;
    info_image->signature_size = 0;

    exynos_smc(SMC_CMD_COLD_BOOT, boot_device, CONFIG_IMAGE_INFO_BASE,
            CONFIG_SYS_LOAD_ADDR);
}
