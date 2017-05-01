/*
        Copyright 2013 bigbiff/Dees_Troy TeamWin
        This file is part of TWRP/TeamWin Recovery Project.

        TWRP is free software: you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
        (at your option) any later version.

        TWRP is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with TWRP.  If not, see <http://www.gnu.org/licenses/>.
		
		This is a write buffer for use with libtar. Libtar normally writes
		512 bytes at a time but this results in poor file performance
		especially on exFAT fuse file systems. This write buffer fixes that
		problem.
*/

#include <fcntl.h>
#include "libtar/libtar.h"
#include "twcommon.h"

int flush = 0, eot_count = -1;
unsigned char *write_buffer;
unsigned buffer_size = 4096;
unsigned buffer_loc = 0;
int buffer_status = 0;

void reinit_libtar_buffer(void) {
	flush = 0;
	eot_count = -1;
	buffer_loc = 0;
	buffer_status = 1;
}

void init_libtar_buffer(unsigned new_buff_size) {
	if (new_buff_size != 0)
		buffer_size = new_buff_size;

	reinit_libtar_buffer();
	write_buffer = (unsigned char*) malloc(sizeof(char *) * buffer_size);
}

void free_libtar_buffer(void) {
	if (buffer_status > 0)
		free(write_buffer);
	buffer_status = 0;
}

ssize_t write_libtar_buffer(int fd, const void *buffer, size_t size) {
	void* ptr;

	if (flush == 0) {
		ptr = write_buffer + buffer_loc;
		memcpy(ptr, buffer, size);
		buffer_loc += size;
		if (eot_count >= 0 && eot_count < 2)
			eot_count++;
			/* At the end of the tar file, libtar will add 2 blank blocks.
			   Once we have received both EOT blocks, we will immediately
			   write anything in the buffer to the file.
			*/

		if (buffer_loc >= buffer_size || eot_count >= 2) {
			flush = 1;
		}
	}
	if (flush == 1) {
		flush = 0;
		if (buffer_loc == 0) {
			// nothing to write
			return 0;
		}
		if (write(fd, write_buffer, buffer_loc) != buffer_loc) {
			LOGERR("Error writing tar file!\n");
			buffer_loc = 0;
			return -1;
		} else {
			buffer_loc = 0;
			return size;
		}
	} else {
		return size;
	}
	// Shouldn't ever get here
	return -1;
}

void flush_libtar_buffer(int fd) {
	eot_count = 0;
	buffer_status = 2;
}
