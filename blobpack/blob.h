#ifndef _BLOB_H
#define _BLOB_H

// Previous unknown fields grabbed from 
// http://nv-tegra.nvidia.com/gitweb/?p=android/platform/build.git;a=commitdiff;h=0adc4478615891636d8b7c476c20c2014b788537

#define MAGIC       "MSM-RADIO-UPDATE"
#define SEC_MAGIC       "-SIGNED-BY-SIGNBLOB-"
#define MAGIC_SIZE  16
#define SEC_MAGIC_SIZE  20
#define PART_NAME_LEN 4

typedef struct
{
    unsigned char magic[20];
} secure_header_type;

typedef struct
{
    unsigned char magic[MAGIC_SIZE];
    unsigned int version; // Always 0x00010000
    unsigned int size; // Size of header
    unsigned int part_offset; // Same as size
    unsigned int num_parts; // Number of partitions
    unsigned int unknown[7]; // Always zero
} header_type;

typedef struct
{
    char name[PART_NAME_LEN]; // Name of partition. Has to match an existing tegra2 partition name (e.g. LNX, SOS)
    unsigned int offset; // offset in blob where this partition starts
    unsigned int size; // Size of partition
    unsigned int version; // Version is variable, but is always 1 in this app
} part_type;

#endif /* _BLOB_H*/

