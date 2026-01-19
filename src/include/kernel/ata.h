#ifndef __ATA_H__
#define __ATA_H__

#include <stddef.h>
#include <stdint-gcc.h>
#include <kernel/block_dev.h>

/* standard ATA IO ports */
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_ISR     (14 + 32) /* IRQ 14 is remapped to ISR vector 46 */

#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_ISR   (15 + 32) /* IRQ 15 is remapped to ISR vector 47 */

/* drive selection */
#define ATA_MASTER          0xA0
#define ATA_SLAVE           0xB0

block_dev *ata_probe(uint16_t base, uint16_t master, uint8_t slave, const char *name, uint8_t irq);

#endif