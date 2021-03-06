#include <runtime/lib.h>
#include <kernel/uos.h>
#include <stream/stream.h>
#include <uart/uart.h>
#include <elvees/spi.h>
#include <flash/sdhc-spi.h>
#include <mem/mem.h>
#include <fs/fat.h>
#include <fs/fat-private.h>
#include <elvees/i2c.h>

#define FAT_MAX_BYTES_PER_SECTOR    512

#define FS_ERR_OK               0
#define FS_ERR_SMALL_BUF        -1
#define FS_ERR_BAD_FORMAT       -2
#define FS_ERR_IO               -3
#define FS_ERR_NO_MEM           -4
#define FS_ERR_EOF              -5
#define FS_ERR_BAD_ARG          -6


#define FS_ATTR_CACHE_VALID     1
#define FS_ATTR_DIR             2

#define EOF                     -1

typedef struct __attribute__((packed)) _partition_t
{
    uint8_t  flags;
    uint8_t  start_chs[3];
    uint8_t  type;
    uint8_t  end_chs[3];
    uint32_t offset_lba;
    uint32_t size_in_sectors;
} partition_t;

#define PARTITION_ACTIVE        0x80
#define FIRST_PART_INFO_OFFSET  0x1BE

#define SPI_NUM     0


uart_t uart;
//#define out &debug
#define out &uart

const char *flash_name = "SD over SPI";
sdhc_spi_t flash;

ARRAY (stack, 1000);
ARRAY (player_stack, 1000);
elvees_spim_t spi;
mem_pool_t pool;

uint8_t buf[512] __attribute__((aligned(8)));
uint8_t boot_sector[512] __attribute__((aligned(8)));
fat_fs_t fat;


#define PLAYER_IDLE     0
#define PLAYER_COPYING  1
#define PLAYER_PLAYING  2
#define PLAYER_PAUSED   3
#define PLAYER_ERROR    4

#define CMD_NO_CMD      0
#define CMD_OPEN        1
#define CMD_START_STOP  2
#define CMD_PAUSE       3

#define I2C_SPEED       100		/* in KBits/s */

#define I2C_READ_OP	1
#define SLAVE_ADDR	0x30

#define MFBSP_CHANNEL   3

#define WAVE_FORMAT_PCM 0x0001

typedef struct
{
    uint8_t     ckID[4];
    uint32_t    cksize;
} chunk_hdr_t;

typedef struct __attribute__((packed))
{
    uint16_t    wFormatTag;
    uint16_t    nChannels;
    uint32_t    nSamplesPerSec;
    uint32_t    nAvgBytesPerSec;
    uint16_t    nBlockAlign;
    uint16_t    wBitsPerSample;
} wave_fmt_t;

wave_fmt_t wave_fmt;

elvees_i2c_t i2c;
mutex_t player_mutex;
unsigned player_state = PLAYER_IDLE;
fs_entry_t *player_file = 0;
unsigned player_size;
unsigned player_pos;
unsigned short player_command;


/* software breakpoint */
#define STOP            asm volatile("nop"); \
                        asm volatile("nop"); \
                        asm volatile("nop"); \
                        asm volatile("nop"); \
                        asm volatile(".word 0x4d"); \
                        asm volatile("nop"); \
                        asm volatile("nop"); \
                        asm volatile("nop")
                        
      
#ifndef ELVEES_MC24R2                  
static inline void tlv320_write_reg(uint8_t addr, uint8_t val)
{
    miic_t *c = (miic_t *)&i2c;
    uint8_t tx_mes[] = {addr, val};
    miic_transaction(c, SLAVE_ADDR, tx_mes, 2, 0, 0);
}

static inline uint8_t tlv320_read_reg(uint8_t addr)
{
    uint8_t value = 0;
    miic_t *c = (miic_t *)&i2c;
    miic_transaction(c, SLAVE_ADDR, &addr, 1, &value, 1);
    return value;
}

void tlv320_init()
{
    tlv320_write_reg(0x03, 0x91);
    tlv320_write_reg(0x04, 0x24);
    tlv320_write_reg(0x05, 0x04);
    tlv320_write_reg(0x06, 0xF0);
    tlv320_write_reg(0x07, 0x8A);
    tlv320_write_reg(0x0F, 0x20);
    tlv320_write_reg(0x10, 0x20);
    tlv320_write_reg(0x13, 0x00);
    tlv320_write_reg(0x16, 0x00);
    tlv320_write_reg(0x19, 0x00);
    tlv320_write_reg(0x20, 0x18);
    tlv320_write_reg(0x21, 0x18);
    tlv320_write_reg(0x2B, 0xAF);
    tlv320_write_reg(0x2C, 0xAF);
    tlv320_write_reg(0x2D, 0x2F);
    tlv320_write_reg(0x2E, 0x2F);
    tlv320_write_reg(0x2F, 0xAF);
    tlv320_write_reg(0x33, 0x0C);
    tlv320_write_reg(0x34, 0x2F);
    tlv320_write_reg(0x35, 0x2F);
    tlv320_write_reg(0x36, 0xAF);
    tlv320_write_reg(0x3A, 0x0C);
    tlv320_write_reg(0x3E, 0x2F);
    tlv320_write_reg(0x3F, 0x2F);
    tlv320_write_reg(0x40, 0xAF);
    tlv320_write_reg(0x41, 0x0C);
    tlv320_write_reg(0x45, 0x2F);
    tlv320_write_reg(0x46, 0x2F);
    tlv320_write_reg(0x47, 0xAF);
    tlv320_write_reg(0x48, 0x0C);
    tlv320_write_reg(0x49, 0x2F);
    tlv320_write_reg(0x4A, 0x2F);
    tlv320_write_reg(0x4B, 0xAF);
    tlv320_write_reg(0x4C, 0x2F);
    tlv320_write_reg(0x4D, 0x2F);
    tlv320_write_reg(0x4E, 0xAF);
    tlv320_write_reg(0x4F, 0x08);
    tlv320_write_reg(0x50, 0x2F);
    tlv320_write_reg(0x51, 0x2F);
    tlv320_write_reg(0x52, 0xAF);
    tlv320_write_reg(0x56, 0x08);
    tlv320_write_reg(0x5A, 0x2F);
    tlv320_write_reg(0x5B, 0x2F);
    tlv320_write_reg(0x5C, 0xAF);
    tlv320_write_reg(0x5D, 0x08);
    
    tlv320_write_reg(0x25, 0xC0);
    tlv320_write_reg(0x33, 0x0D);
    tlv320_write_reg(0x41, 0x0D);
    tlv320_write_reg(0x56, 0x09);
    tlv320_write_reg(0x5D, 0x09);
    tlv320_write_reg(0x2B, 0x2F);
    tlv320_write_reg(0x2C, 0x2F);
}

void init_i2s(int port)
{
    MC_CLKEN |= MC_CLKEN_MFBSP;

    MC_MFBSP_CSR(port) = MC_MFBSP_SPI_I2S_EN;

    MC_MFBSP_DIR(port) = MC_MFBSP_RCLK_DIR | MC_MFBSP_TCLK_DIR | 
        MC_MFBSP_TCS_DIR | MC_MFBSP_RCS_DIR | MC_MFBSP_TD_DIR;

	MC_MFBSP_RCTR(port) = 0;    // Пока не используем приёмник

    MC_MFBSP_TCTR_RATE(port) = 
        MC_MFBSP_TCLK_RATE(KHZ / (2 * 32 * wave_fmt.nSamplesPerSec / 1000) - 1) |
        MC_MFBSP_TCS_RATE(15);

	MC_MFBSP_TCTR(port) = MC_MFBSP_TNEG | MC_MFBSP_TDEL |
        MC_MFBSP_TWORDCNT(0) | MC_MFBSP_TCSNEG | MC_MFBSP_TMBF |
        MC_MFBSP_TWORDLEN(15) | MC_MFBSP_TPACK | MC_MFBSP_TSWAP |
        MC_MFBSP_TCS_CONT | MC_MFBSP_TCLK_CONT;

#ifdef ELVEES_MC24R2
    MC_MFBSP_TCTR(port) |= MC_MFBSP_TEN;
#else
    MC_MFBSP_TSTART(port) = 1;
#endif
}

void start_tx_dma(int port, void *buf, int size)
{
    unsigned addr = (unsigned) buf;
    if (size > 0x80000) size = 0x80000;
    MC_IR_MFBSP_TX(port) = mips_virtual_addr_to_physical(addr);
    MC_CSR_MFBSP_TX(port) = MC_DMA_CSR_WN(0) | 
        MC_DMA_CSR_WCX(size / 8 - 1) | MC_DMA_CSR_RUN;
}

int tx_dma_is_running(int port)
{
    return (MC_RUN_MFBSP_TX(port) & 1);
}

void start_play_wave(fs_entry_t *file)
{
    chunk_hdr_t *hdr;
    uint8_t id[5] = {0, 0, 0, 0, 0};
    unsigned limit;
    fsif_t *fs = file->fs;
    
    fs->open(file);
    fs->update_cache(file);
    hdr = (chunk_hdr_t *) file->cache_p;
    if (memcmp(hdr->ckID, "RIFF", 4) != 0) {
        debug_printf ("Not a RIFF file!\n");
        fs->close(file);
        return;
    }
    
    fs->advance(file, sizeof(chunk_hdr_t));
    if (memcmp(file->cache_p, "WAVE", 4) != 0) {
        debug_printf ("Not a WAVE file!\n");
        fs->close(file);
        return;
    }
    
    fs->advance(file, 4);
    hdr = (chunk_hdr_t *) file->cache_p;
    memcpy(id, hdr->ckID, 4);

    if (memcmp(hdr->ckID, "fmt ", 4) == 0) {
        memcpy(&wave_fmt, hdr + 1, sizeof(wave_fmt_t));
    } else {
        debug_printf("Expected \"fmt \" chunk!\n");
        fs->close(file);
        return;
    }

    fs->advance(file, sizeof(chunk_hdr_t) + hdr->cksize);
    hdr = (chunk_hdr_t *) file->cache_p;
    /*
    if (memcmp(hdr->ckID, "data", 4) == 0) {
        debug_printf("\nPlaying wave file...");
        fs->advance(file, sizeof(chunk_hdr_t));
        elvees_i2c_init(&i2c, I2C_SPEED);
        init_i2s(MFBSP_CHANNEL);
        tlv320_init();
        while (!fs_at_end(file)) {
            fs->update_cache(file);
            limit = fs_size_to_end_of_cache(file);
            tx_dma(MFBSP_CHANNEL, file->cache_p, limit);
            fs->advance(file, limit);
        }
    }
    */
    
    unsigned char *psdram = (unsigned char *)0xa0000000;
    if (memcmp(hdr->ckID, "data", 4) == 0) {
        //debug_printf("\nCopying file into SDRAM...");
        player_state = PLAYER_COPYING;
        fs->advance(file, sizeof(chunk_hdr_t));
        while (!fs_at_end(file)) {
            fs->update_cache(file);
            limit = fs_size_to_end_of_cache(file);
            memcpy(psdram, file->cache_p, limit);
            fs->advance(file, limit);
            psdram += limit;
        }
    }
    //debug_printf(" done\n");
    

    elvees_i2c_init(&i2c, I2C_SPEED);
    init_i2s(MFBSP_CHANNEL);
    tlv320_init();
    
    //debug_printf("\nPlaying wave file...");

    //tx_dma(MFBSP_CHANNEL, (void *) 0xa0000000, (unsigned)psdram & 0x3FFFFFF);
    
    fs->close(file);
    //debug_printf(" done\n");
}

void player(void *arg)
{
    unsigned sz;
        
    mutex_lock(&player_mutex);
    
    for (;;) {
        mutex_wait(&player_mutex);
        do {
            switch (player_command) {
            case CMD_OPEN:
                start_play_wave(player_file);
                // Intentionally no break
            case CMD_START_STOP:
                if ((player_state == PLAYER_IDLE || player_state == PLAYER_COPYING) && player_file != 0) {
                    player_size = player_file->size;
                    sz = (player_size < 0x8000) ? player_size : 0x8000;
                    start_tx_dma(MFBSP_CHANNEL, (void *) 0xa0000000, sz);
                    player_pos = sz;
                    player_size -= sz;
                    player_state = PLAYER_PLAYING;
                } else if (player_state == PLAYER_PLAYING) {
                    MC_RUN_MFBSP_TX(MFBSP_CHANNEL) = 0;
                    while (tx_dma_is_running(MFBSP_CHANNEL));
                    player_state = PLAYER_IDLE;
                } else if (player_state == PLAYER_PAUSED) {
                    sz = (player_size < 0x8000) ? player_size : 0x8000;
                    start_tx_dma(MFBSP_CHANNEL, (void *) 0xa0000000, sz);
                    player_pos += sz;
                    player_size -= sz;
                    player_state = PLAYER_PLAYING;                    
                }
                player_command = CMD_NO_CMD;
                break;
            case CMD_PAUSE:
                if (player_state == PLAYER_PLAYING) {
                    MC_RUN_MFBSP_TX(MFBSP_CHANNEL) = 0;
                    while (tx_dma_is_running(MFBSP_CHANNEL));
                    player_state = PLAYER_PAUSED;
                } else if (player_state == PLAYER_PAUSED) {
                    sz = (player_size < 0x8000) ? player_size : 0x8000;
                    start_tx_dma(MFBSP_CHANNEL, (void *) 0xa0000000, sz);
                    player_pos += sz;
                    player_size -= sz;
                    player_state = PLAYER_PLAYING;
                }
                player_command = CMD_NO_CMD;
                break;
            default:
                if (player_state == PLAYER_PLAYING && !tx_dma_is_running(MFBSP_CHANNEL)) {
                    if (player_size == 0) {
                        player_state = PLAYER_IDLE;
                    } else {
                        sz = (player_size < 0x8000) ? player_size : 0x8000;
                        start_tx_dma(MFBSP_CHANNEL, (void *) (0xa0000000 + player_pos), sz);
                        player_pos += sz;
                        player_size -= sz;
                    }
                }
                break;
            }
        } while (player_state == PLAYER_COPYING || player_state == PLAYER_PLAYING);
    }
}
#endif // ELVEES_MC24R2
                        
char *partition_type_to_string(uint8_t type)
{
    char *s;
    switch (type) {
        case 0x00:  s = "EMPTY"; break;
        case 0x01:	s = "FAT12"; break;
        case 0x02:	s = "XENIX root"; break;
        case 0x03:	s = "XENIX usr"; break;
        case 0x04:	s = "FAT16 <= 32MB"; break;
        case 0x05:	s = "Extended"; break;
        case 0x06:	s = "FAT16 > 32MB"; break;
        case 0x07:	s = "NTFS"; break;
        case 0x08:	s = "AIX"; break;
        case 0x09:	s = "AIX boot"; break;
        case 0x0A:  s = "OS/2 boot"; break;
        case 0x0B:  s = "FAT32"; break;
        case 0x0C:  s = "FAT32 LBA"; break;
        case 0x0E:	s = "FAT16 LBA (VFAT)"; break;
        case 0x0F:	s = "Extended LBA"; break;
        case 0x10:	s = "OPUS"; break;
        case 0x11:	s = "FAT32 hidden"; break;
        case 0x12:	s = "Compaq"; break;
        case 0x14:	s = "FAT16 hidden <= 32MB"; break;
        case 0x16:	s = "FAT16 hidden > 32MB"; break;
        case 0x17:	s = "HPFS/NTFS hidden"; break;
        case 0x18:	s = "AST SmartSleep"; break;
        case 0x1B:  s = "FAT32 hidden"; break;
        case 0x1C:  s = "FAT32 hidden LBA"; break;
        case 0x1E:	s = "FAT16 hidden LBA (VFAT)"; break;
        case 0x24:  s = "NEC DOS"; break;
        case 0x27:	s = "NTFS hidden"; break;
        case 0x39:	s = "Plan 9"; break;
        case 0x3C:	s = "Partition Magic"; break;
        case 0x40:	s = "Venix 80286"; break;
        case 0x41:	s = "PPC PReP Boot"; break;
        case 0x42:	s = "SFS"; break;
        case 0x4D:	s = "QNX4.x"; break;
        case 0x4E:	s = "QNX4.x 2nd part"; break;
        case 0x4F:	s = "QNX4.x 3rd part"; break;
        case 0x50:	s = "OnTrack DM"; break;
        case 0x51:	s = "OnTrack DM6 Aux"; break;
        case 0x52:	s = "CP/M"; break;
        case 0x53:	s = "OnTrack DM6 Aux"; break;
        case 0x54:	s = "OnTrackDM6"; break;
        case 0x55:	s = "EZ-Drive"; break;
        case 0x56:	s = "Golden Bow"; break;
        case 0x5C:	s = "Priam Edisk"; break;
        case 0x61:	s = "SpeedStor"; break;
        case 0x62:	s = "GNU HURD"; break;
        case 0x82:	s = "Linux swap"; break;
        case 0x83:	s = "Linux"; break;
        case 0x85:	s = "Linux extended"; break;
        case 0x86:	s = "FAT16 stripe-array WinNT"; break;
        case 0x87:	s = "NTFS stripe-array WinNT"; break;
        case 0x93:	s = "Amoeba"; break;
        case 0x94:	s = "Amoeba BBT"; break;
        case 0xA5:	s = "FreeBSD"; break;
        case 0xA6:	s = "OpenBSD"; break;
        case 0xA7:	s = "NeXTSTEP"; break;
        case 0xA9:	s = "NetBSD"; break;
        case 0xB6:	s = "Mirror master FAT16 WinNT"; break;
        case 0xB7:	s = "Mirror master NTFS WinNT"; break;
        case 0xBE:	s = "Solaris boot"; break;
        case 0xBF:	s = "Solaris"; break;
        case 0xC6:	s = "Mirror slave FAT16 WinNT"; break;
        case 0xC7:	s = "Mirror slave NTFS WinNT"; break;
        case 0xDA:	s = "Raw data"; break;
        case 0xDE:	s = "Dell Utility"; break;
        case 0xEE:	s = "GPT"; break;
        case 0xFD:	s = "Autodetection"; break;
        case 0xFE:	s = "LANstep"; break;
        case 0xFF:	s = "BBT"; break;
        default:    s = "unknown"; break;
    }
    return s;
}
                        
void print_partition_info(partition_t *pinfo)
{
    /*
    printf(out, "Type:             \33[07m%s\33[00m\n", 
        partition_type_to_string(pinfo->type));
    printf(out, "Flags:            %s\n", 
        (pinfo->flags == PARTITION_ACTIVE) ? "active" : "");
    printf(out, "Start in CHS:     %02X%02X%02X\n",
        pinfo->start_chs[2], pinfo->start_chs[1], pinfo->start_chs[0]);
    printf(out, "End in CHS:       %02X%02X%02X\n",
        pinfo->end_chs[2], pinfo->end_chs[1], pinfo->end_chs[0]);
    printf(out, "Offset in LBA:    %08X\n", pinfo->offset_lba);
    printf(out, "Size in sectors:  %08X\n", pinfo->size_in_sectors);
    */
    
    printf(out, "%-16s offset: %08X, sectors: %08X",
        partition_type_to_string(pinfo->type),
        pinfo->offset_lba, pinfo->size_in_sectors);
}

void print_sd_info(flashif_t *f)
{
    printf(out, "%s, size: %u Kb, page size: %db, sector size: %db\n",
        flash_name, (unsigned)(flash_size(f) >> 10),
        flash_page_size(f), flash_sector_size(f));
}

void print_fat_bs_common(void *boot_sector_data)
{
    fat32_bs_t *bs = (fat32_bs_t *)boot_sector_data;
    int i;
    printf(out, "Jump boot code:        %02X, %02X, %02X\n",
        bs->jump_boot[0], bs->jump_boot[1], bs->jump_boot[2]);
    printf(out, "OEM name:              ");
    for (i = 0; i < 8; ++i)
        printf(out, "%c", bs->oem_name[i]);
    printf(out, "\n");
    printf(out, "Bytes per sector:      %d\n", bs->bytes_per_sec);
    printf(out, "Sectors per cluster:   %d\n", bs->sec_per_clus);
    printf(out, "Reserved sectors:      %d\n", bs->rsvd_sec_cnt);
    printf(out, "Number of FATs:        %d\n", bs->num_fats);
    printf(out, "Number of root entries:%d\n", bs->root_ent_cnt);
    printf(out, "Total sector cnt (16): %d\n", bs->tot_sec16);
    printf(out, "Media type:            %02X\n", bs->media);
    printf(out, "FAT size (16):         %d\n", bs->fat_sz16);
    printf(out, "Sectors per track:     %d\n", bs->sec_per_trk);
    printf(out, "Number of heads:       %d\n", bs->num_heads);
    printf(out, "Number of hidden sec:  %d\n", bs->hidd_sec);
    printf(out, "Total sector cnt (32): %d\n", bs->tot_sec32);
}

void print_fat16_bs_specific(void *boot_sector_data)
{
    fat16_bs_t *bs = (fat16_bs_t *)boot_sector_data;
    int i;
    printf(out, "Driver number:         %02X\n", bs->drv_num);
    printf(out, "Boot signature:        %02X\n", bs->boot_sig);
    printf(out, "Volume ID:             %08X\n", bs->vol_id);
    printf(out, "Volume label:          ");
    for (i = 0; i < 11; ++i)
        printf(out, "%c", bs->vol_lab[i]);
    printf(out, "\n");
    printf(out, "File system type:      ");
    for (i = 0; i < 8; ++i)
        printf(out, "%c", bs->file_sys_type[i]);
    printf(out, "\n");
}

void print_fat32_bs_specific(void *boot_sector_data)
{
    fat32_bs_t *bs = (fat32_bs_t *)boot_sector_data;
    int i;
    printf(out, "FAT size (32):         %d\n", bs->fat_sz32);
    printf(out, "External flags:        %04X\n", bs->ext_flags);
    printf(out, "File system version    %d:%d\n", bs->fs_ver >> 8, bs->fs_ver & 0xFF);
    printf(out, "Root cluster:          %d\n", bs->root_clus);
    printf(out, "FS info sector:        %d\n", bs->fs_info);
    printf(out, "Backup boot sector:    %d\n", bs->bk_boot_sec);
    printf(out, "Driver number:         %02X\n", bs->drv_num);
    printf(out, "Boot signature:        %02X\n", bs->boot_sig);
    printf(out, "Volume ID:             %08X\n", bs->vol_id);
    printf(out, "Volume label:          ");
    for (i = 0; i < 11; ++i)
        printf(out, "%c", bs->vol_lab[i]);
    printf(out, "\n");
    printf(out, "File system type:      ");
    for (i = 0; i < 8; ++i)
        printf(out, "%c", bs->file_sys_type[i]);
    printf(out, "\n");
}

void print_boot_sector(flashif_t *f, partition_t *part)
{
    if ((part->type == 0x04) || (part->type == 0x06) ||
        (part->type == 0x0E) || (part->type == 0x14) ||
        (part->type == 0x16) || (part->type == 0x1E)) {
            if (flash_read(f, part->offset_lba, boot_sector, 512) != FLASH_ERR_OK) {
                printf(out, "READ FAILED!\n");
                STOP;
            }
            if (check_fat_type(boot_sector) != FS_TYPE_FAT16) {
                printf(out, "FILE SYSTEM TYPE IS NOT FAT16 AS EXPECTED!\n");
            } else {
                print_fat_bs_common(boot_sector);
                print_fat16_bs_specific(boot_sector);
            }
    } else if ((part->type == 0x0B) ||
        (part->type == 0x0C) || (part->type == 0x11) ||
        (part->type == 0x1B) || (part->type == 0x1C)) {
            if (flash_read(f, part->offset_lba, boot_sector, 512) != FLASH_ERR_OK) {
                printf(out, "READ FAILED!\n");
                STOP;
            }
            if (check_fat_type(boot_sector) != FS_TYPE_FAT32) {
                printf(out, "FILE SYSTEM TYPE IS NOT FAT32 AS EXPECTED!\n");
            } else {
                print_fat_bs_common(boot_sector);
                print_fat32_bs_specific(boot_sector);
            }
    }
}

void draw_partition_table(int current)
{
    printf(out, "PARTITION TABLE:\n\n");
    
    int i;
    for (i = 0; i < 4; ++i) {
        if (i == current)
            printf(out, "\33[07m");
        printf(out, "PARTITION #%d:    ", i);
        print_partition_info((partition_t *)&buf[
            FIRST_PART_INFO_OFFSET + i * sizeof(partition_t)]);
        if (i == current)
            printf(out, "\33[00m");
        printf(out, "\n");
    }
}

int list_dir(fsif_t *fs, fs_entry_t *dir, int current)
{
    fs_entry_t *entry;
    int entry_num = 0;
    
    entry = fs->first_child(dir);
    while (entry != 0) {
        if (entry_num == current)
            printf(out, "\33[07m");
        printf(out, "%02d.%02d.%04d %02d:%02d:%02d   ",
            entry->day, entry->month, entry->year,
            entry->hour, entry->minute, entry->second);
        if (entry->attr & FS_ATTR_DIRECTORY)
            printf(out,"DIR    ");
        else printf(out,"file   ");
        printf(out, "%-15s", entry->name);
        /*
        if (entry->attr & FS_ATTR_READ_ONLY)
            printf(out, "R");
        if (entry->attr & FS_ATTR_HIDDEN)
            printf(out, "H");
        if (entry->attr & FS_ATTR_SYSTEM)
            printf(out, "S");
        if (entry->attr & FS_ATTR_VOLUME_ID)
            printf(out, "V");
        if (entry->attr & FS_ATTR_DIRECTORY)
            printf(out, "D");
        if (entry->attr & FS_ATTR_ARCHIVE)
            printf(out, "A");
        */
        printf(out, "\n");
        if (entry_num == current)
            printf(out, "\33[00m");
        entry_num++;
        fs->free_fs_entry(entry);
        entry = fs->next_child(dir);
    }
    return entry_num;
}

fs_entry_t *selected_entry(fsif_t *fs, fs_entry_t *dir, int selected)
{
    fs_entry_t *entry;
    int entry_num = 0;
    
    entry = fs->first_child(dir);
    while (entry != 0) {
        if (entry_num == selected)
            return entry;
        entry_num++;
        fs->free_fs_entry(entry);
        entry = fs->next_child(dir);
    }
    return 0;
}

void print_text(fs_entry_t *file)
{
    fsif_t *fs = file->fs;
    unsigned limit;
    unsigned i;
    
    debug_printf("\n");
    fs->open(file);
    while (!fs_at_end(file)) {
        fs->update_cache(file);
        limit = fs_size_to_end_of_cache(file);
        for (i = 0; i < limit; ++i)
            debug_printf("%c", file->cache_p[i]);
        fs->advance(file, limit);
    };
    fs->close(file);
}

char cur_path[1024];

void browse(fsif_t *fs)
{
    fs_entry_t *cur_dir = fs->get_root(fs);
    fs_entry_t *sel_entry;
    int cur_line;
    
    strcpy((unsigned char *)cur_path, (unsigned char *)"/");
    
    unsigned short c;
    int cur_menu = 0;
    int nb_entries;
    for (;;) {
        printf(out, "\33[H\33\[2J");
        printf(out, "%s:\n\n", cur_path);
        nb_entries = list_dir(fs, cur_dir, cur_menu);
        cur_line = nb_entries;
        for (; cur_line < 39; cur_line++)
            printf(out, "\n");
        printf(out, "PLAYER: ");
        if (player_state == PLAYER_IDLE)         printf(out, "STOPPED ");
        else if (player_state == PLAYER_COPYING) printf(out, "COPYING ");
        else if (player_state == PLAYER_PLAYING) printf(out, "PLAYING ");
        else if (player_state == PLAYER_PAUSED)  printf(out, "PAUSED  ");
        else if (player_state == PLAYER_ERROR)   printf(out, "ERROR!  ");
        printf(out, "%s ", player_file ? player_file->name : "<no file>");
        printf(out, "(s-start/stop, p-pause)\n");
        c = getchar(out);
        if (c == 0x1b) {
            c = getchar(out);
            if (c == 0x5b) {
                c = getchar(out);
                switch (c) {
                    case 0x41:
                        cur_menu = (cur_menu - 1 + nb_entries) % nb_entries;
                        break;
                    case 0x42:
                        cur_menu = (cur_menu + 1) % nb_entries;
                        break;
                    default:
                        break;
                }
            }
        } else if (c == 0x0d) {
            sel_entry = selected_entry(fs, cur_dir, cur_menu);
            if (sel_entry->attr & FS_ATTR_DIRECTORY) {
                fs->free_fs_entry(cur_dir);
                cur_dir = sel_entry;
                cur_menu = 0;
                if (strcmp((unsigned char *)sel_entry->name, (unsigned char *)".") == 0) {
                    // Do nothing with cur_path
                } else if (strcmp((unsigned char *)sel_entry->name, (unsigned char *)"..") == 0) {
                    *(strrchr((unsigned char *)cur_path, (unsigned char)'/')) = 0;
                    if (strlen((unsigned char *)cur_path) == 0)
                        strcpy((unsigned char *)cur_path, (unsigned char *)"/");
                } else {
                    if (strlen((unsigned char *)cur_path) != 1)
                        strcat((unsigned char *)cur_path, (unsigned char *)"/");
                    strcat((unsigned char *)cur_path, (unsigned char *)cur_dir->name);
                }
                getchar(out);
            } else {
                unsigned char *pdot = strrchr((unsigned char *)sel_entry->name, '.');
                if (pdot) {
                    if (strcmp(pdot, (unsigned char *)".txt") == 0 || strcmp(pdot, (unsigned char *)".c") == 0 ||
                        strcmp(pdot, (unsigned char *)".cpp") == 0 || strcmp(pdot, (unsigned char *)".h") == 0 ||
                        strcmp(pdot, (unsigned char *)".TXT") == 0 || strcmp(pdot, (unsigned char *)".C") == 0 ||
                        strcmp(pdot, (unsigned char *)".CPP") == 0 || strcmp(pdot, (unsigned char *)".H") == 0)
                    {
                        print_text(sel_entry);
                        getchar(out);
                    } else if (strcmp(pdot, (unsigned char *)".wav") == 0 || strcmp(pdot, (unsigned char *)".WAV") == 0) {
                        player_file = sel_entry;
                        player_state = PLAYER_COPYING;
                        player_command = CMD_OPEN;
                        mutex_signal(&player_mutex, 0);
                    }
                }
            }
        } else if (c == 's') {
            player_command = CMD_START_STOP;
            mutex_signal(&player_mutex, 0);
        } else if (c == 'p') {
            player_command = CMD_PAUSE;
            mutex_signal(&player_mutex, 0);
        }
    }
}

void task (void *arg)
{
    flashif_t *f = (flashif_t *)arg;

    if (flash_connect(f) == FLASH_ERR_OK) {
        printf(out, "Found %s, size: %u Kb, nb pages: %u, page size: %d b\n\
            nb sectors: %u, sector size: %d b\n",
            flash_name, (unsigned)(flash_size(f) >> 10),
            flash_nb_pages(f), flash_page_size(f),
            flash_nb_sectors(f), flash_sector_size(f));
    } else {
        printf(out, "%s not found\n", flash_name);
        STOP;
    }
    
    if (flash_read(f, 0, buf, flash_page_size(f)) != FLASH_ERR_OK) {
        printf(out, "READ FAILED!\n");
        STOP;
    }
    
    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        printf(out, "BAD FORMATED FLASH!\n");
        STOP;
    }
    
/*
unsigned short c;
for (;;) {
    c = getchar(out);
    printf(out, "c = %x\n", c);
}
*/
    fat_fs_init(&fat, &pool, f, 0);
    if (fat.fsif.last_error != FS_ERR_OK) {
        printf(out, "FAILED TO INIT FAT FS WITH ERROR %d!\n", fat.fsif.last_error);
        STOP;
    }
    
    if (fat.fsif.fs_type == FS_TYPE_UNKNOWN) {
        unsigned short c;
        int cur_menu = 0;
        for (;;) {
            printf(out, "\33[H\33\[2J");
            print_sd_info(f);
            printf(out, "\n");
            draw_partition_table(cur_menu);
            printf(out, "\n\n\nPartition boot sector info:\n\n");
            print_boot_sector(f, (partition_t *)&buf[
                FIRST_PART_INFO_OFFSET + cur_menu * sizeof(partition_t)]);
            c = getchar(out);
            if (c == 0x1b) {
                c = getchar(out);
                if (c == 0x5b) {
                    c = getchar(out);
                    switch (c) {
                        case 0x41:
                            cur_menu = (cur_menu - 1 + 4) % 4;
                            break;
                        case 0x42:
                            cur_menu = (cur_menu + 1) % 4;
                            break;
                        default:
                            break;
                    }
                }
            } else if (c == 0x0d) {
            }
        }
    } else {
        printf(out, "NO PARTITION TABLE\n\n");
        if (fat.fsif.fs_type == FS_TYPE_FAT16) {
            print_fat_bs_common(buf);
            print_fat16_bs_specific(buf);
        } else if (fat.fsif.fs_type == FS_TYPE_FAT32) {
            print_fat_bs_common(buf);
            print_fat32_bs_specific(buf);
        }
        printf(out, "\n\nFAT FS driver structure:\n");
        printf(out, "fs_type = %d\n", fat.fsif.fs_type);
        printf(out, "sec_per_clus = %d\n", fat.sec_per_clus);
        printf(out, "bytes_per_sec = %d\n", fat.bytes_per_sec);
        printf(out, "first_sec = %d\n", fat.first_sec);
        printf(out, "first_fat_sec = %d\n", fat.first_fat_sec);
        printf(out, "first_data_sec = %d\n", fat.first_data_sec);
        printf(out, "root_ent_cnt = %d\n", fat.root_ent_cnt);
        printf(out, "Press ENTER to view file system content...\n");
        getchar(out);
        browse(&fat.fsif);
    }
    
    printf(out, "FINISHED!\n");

    STOP;
}

void uos_init (void)
{
	debug_printf("\n\nSD card format printer\n\n");
    
    uart_init(out, 0, 90, KHZ, 115200);
    
	/* Выделяем место для динамической памяти */
	extern unsigned __bss_end[];
#ifdef ELVEES_DATA_SDRAM
	/* Динамическая память в SDRAM */
	if (((unsigned) __bss_end & 0xF0000000) == 0x80000000)
		mem_init (&pool, (unsigned) __bss_end, 0x82000000);
	else
		mem_init (&pool, (unsigned) __bss_end, 0xa2000000);
#else
	/* Динамическая память в CRAM */
	extern unsigned _estack[];
	mem_init (&pool, (unsigned) __bss_end, (unsigned) _estack - 256);
#endif
    
    spim_init(&spi, SPI_NUM, SPI_MOSI_OUT | SPI_SS0_OUT | SPI_SS1_OUT | SPI_SS0_OUT | SPI_TSCK_OUT);
    sd_spi_init(&flash, (spimif_t *)&spi, SPI_MODE_CS_NUM(1));
	
	task_create (task, &flash, "task", 10, stack, sizeof (stack));
#ifndef ELVEES_MC24R2
    task_create (player, 0, "player", 5, player_stack, sizeof(player_stack));
#endif // ELVEES_MC24R2
}
