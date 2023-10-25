// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <memory.h>
#include <malloc.h>
#include <errno.h>

#include "../include/eepromops.h"
#include "../include/debug.h"

typedef struct EEPROMBlock {
    uint8_t *data;
    uint16_t size;
    bool dirty;
    bool saveonwrite;
    struct EEPROMBlock *next;
    int fid;
} EEPROMBlock;

// Head of our internal linked list for in-memory EEPROM representation
static EEPROMBlock *head_block = NULL;

// EEPROM functions
// Internal functions
static uint16_t eeprom_getsize(EEPROMDescriptor eeprom_descriptor);
// Internal function to create a new EEPROM block
static EEPROMBlock* create_eeprom_block(int fid, size_t size);
static EEPROMBlock* find_block(int fid);
ssize_t eeprom_save(EEPROMDescriptor desc);


// External functions
/**
 * EEPROMDescriptor eeprom_open(const char *pathname, uint16_t eeprom_size)
 * @param pathname
 * @param eeprom_size
 * @return EEPROMDescriptor
 * @brief Open EEPROM file and return descriptor. If no file exists and eeprom_size is 0 return error.
 * If eeprom_size is 0 - try to get size from file.
 * If eeprom_size non zero - try to create file with size eeprom_size. (Not implemented yet)
 */

EEPROMDescriptor eeprom_open(const char *pathname, uint16_t eeprom_size) {
    EEPROMDescriptor desc;
    desc.eeprom_fid = open(pathname, O_RDWR);
    if (desc.eeprom_fid == -1) return desc;  // handle error

    if (eeprom_size == 0) {
        desc.eeprom_size = eeprom_getsize(desc);
        if (!desc.eeprom_size) {
            debug("eeprom_getsize failed\n");
            desc.eeprom_fid = -1;
            return desc;
        }
    } else {
        assert("eeprom_open with non-zero eeprom_size not implemented" && eeprom_size != 0);
        desc.eeprom_size = eeprom_size;
    }

    uint8_t *buffer = (uint8_t *)malloc(desc.eeprom_size);
    if (!buffer) return desc;  // handle error

    lseek(desc.eeprom_fid, 0, SEEK_SET);
    read(desc.eeprom_fid, buffer, desc.eeprom_size);

    EEPROMBlock *block = create_eeprom_block(desc.eeprom_fid, desc.eeprom_size);
    if (!block) return desc;  // handle error

    memcpy(block->data, buffer, desc.eeprom_size);
    free(buffer);

    block->dirty= false;
    block->saveonwrite = true;
    block->fid = desc.eeprom_fid;


    return desc;
}


ssize_t eeprom_read(EEPROMDescriptor eeprom_descriptor, void *buf, uint16_t count, uint16_t offset) {
    //debug("eeprom_read: count: %d offset: %d\n", count, offset);
    if (offset + count > eeprom_descriptor.eeprom_size) {
        debug("eeprom_read: offset + count %i > eeprom.size %lu\n", offset + count, eeprom_descriptor.eeprom_size);
        return -1;
    }

    EEPROMBlock *block = find_block(eeprom_descriptor.eeprom_fid);
    if (!block) {
        debug("eeprom_read: block not found\n");
        return -1;  // Error, block not found
    }

    // Double check that the block size is correct
    if (offset + count > block->size) {
        debug("eeprom_read: offset + count %i > block->size %i\n", offset + count, block->size);
        return -1;
    }

    memcpy(buf, block->data + offset, count);

    return count;
}

uint16_t eeprom_write(EEPROMDescriptor eeprom_descriptor, const void *buf, uint16_t count, uint16_t offset) {
    if (offset + count > eeprom_descriptor.eeprom_size) return -1;

    EEPROMBlock *block = find_block(eeprom_descriptor.eeprom_fid);
    if (!block) return -1;  // Error, block not found

    // Double check that the block size is correct
    if (offset + count > block->size) return -1;

    memcpy(block->data + offset, buf, count);

    block->dirty = true;

    if (block->saveonwrite) {
        // TODO: add check for write errors
        eeprom_save(eeprom_descriptor);
        block->dirty = false;
    }

    return count;
}

int eeprom_close(EEPROMDescriptor desc) {
    EEPROMBlock *current = head_block;
    EEPROMBlock *prev = NULL;

    while (current) {
        if (current->fid == desc.eeprom_fid) {
            if (prev) {
                prev->next = current->next;
            } else {
                head_block = current->next;
            }
            if (current->dirty) {
                eeprom_save(desc);
            }

            free(current->data);
            free(current);
            break;
        }
        prev = current;
        current = current->next;
    }

    return close(desc.eeprom_fid);
}

uint16_t eeprom_getsize(EEPROMDescriptor eeprom_descriptor) {
    // This is a mockup; you'd need to implement the method to get the actual size from the EEPROM or its specifications
    // For instance, using a method specific to your EEPROM hardware or OS APIs to determine the size.
    struct stat eeprom_fstat;
    if (fstat(eeprom_descriptor.eeprom_fid, &eeprom_fstat)) {
        debug("eeprom_getsize: fstat failed %i\n",errno);
        return -1;
    }
    eeprom_descriptor.eeprom_size = eeprom_fstat.st_size;
    return eeprom_descriptor.eeprom_size; // Assuming 8K EEPROM for now
}

static EEPROMBlock* create_eeprom_block(int fid, size_t size) {
    EEPROMBlock *new_block = (EEPROMBlock *)malloc(sizeof(EEPROMBlock));
    if (!new_block) return NULL;

    new_block->fid = fid;
    new_block->data = (uint8_t *)malloc(size);
    if (!new_block->data) {
        free(new_block);
        return NULL;
    }

    new_block->size = size;
    new_block->next = head_block;
    head_block = new_block;

    return new_block;
}

EEPROMBlock *find_block(int fid) {
    EEPROMBlock *current = head_block;
    while (current) {
        if (current->fid == fid) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

ssize_t eeprom_save(EEPROMDescriptor desc) {
    EEPROMBlock *block = find_block(desc.eeprom_fid);
    if (!block) return -1;

    lseek(desc.eeprom_fid, 0, SEEK_SET);
    return write(desc.eeprom_fid, block->data, block->size);
}
