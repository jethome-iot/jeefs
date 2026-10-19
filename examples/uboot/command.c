// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
#include <command.h>
#include <dm.h>
#include <i2c.h>
#include <limits.h>
#include <stdio.h>

#include "args.h"
#include "read_identity.h"

static int read_i2c(void *context, uint32_t offset, uint8_t *data, uint16_t len) {
    struct udevice *chip = context;
    while (len) {
        uint16_t chunk = len > 64 ? 64 : len;
        int ret = dm_i2c_read(chip, offset, data, chunk);
        if (ret)
            return ret;
        offset += chunk;
        data += chunk;
        len = (uint16_t) (len - chunk);
    }
    return 0;
}

static int do_jeefs_identity(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]) {
    unsigned bus, address, offset_len, size;
    struct udevice *chip;
    struct jeefs_example_identity identity;
    (void) cmdtp;
    (void) flag;
    if (argc != 5 || jeefs_example_parse_arg(argv[1], 10, INT_MAX, &bus) ||
        jeefs_example_parse_arg(argv[2], 16, 0x7f, &address) || jeefs_example_parse_arg(argv[3], 10, 2, &offset_len) ||
        offset_len == 0 || jeefs_example_parse_arg(argv[4], 10, 65535, &size) || size < sizeof(JEEPROMHeaderversion) ||
        (offset_len == 1 && size > 256))
        return CMD_RET_USAGE;
    int ret = i2c_get_chip_for_busnum((int) bus, (int) address, offset_len, &chip);
    if (!ret)
        ret = i2c_set_chip_offset_len(chip, offset_len);
    if (ret) {
        printf("JEEFS: cannot access I2C EEPROM (%d)\n", ret);
        return CMD_RET_FAILURE;
    }
    ret = jeefs_example_read_identity(read_i2c, chip, (uint16_t) size, &identity);
    if (ret < 0) {
        printf("JEEFS: identity read failed (%d)\n", ret);
        return CMD_RET_FAILURE;
    }
    const JEEPROMHeaderv4 *board = &identity.board.v4;
    printf("JEEFS v%u: board=%.*s revision=%.*s serial-slot=%.*s\n", board->version, (int) sizeof(board->boardname),
           board->boardname, (int) sizeof(board->boardversion), board->boardversion, (int) sizeof(board->board_serial),
           (const char *) board->board_serial);
    if (identity.has_device) {
        const DeviceIdentityV1 *device = &identity.device;
        printf("device.id: model=%.*s serial=%.*s revision=%.*s\n", (int) sizeof(device->device_model),
               device->device_model, (int) sizeof(device->device_serial), device->device_serial,
               (int) sizeof(device->hw_revision), device->hw_revision);
    } else {
        puts("JEEFS: no device.id in first slot; remaining chain not searched\n");
    }
    return CMD_RET_SUCCESS;
}

U_BOOT_CMD(jeefs_identity, 5, 0, do_jeefs_identity, "read JEEFS board and first-slot device identity",
           "<bus-decimal> <chip-hex> <offset-bytes:1|2> <image-bytes-decimal>");
