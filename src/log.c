
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

char log_buffer[1000];

FILE *log_file;

void write_log(const char *text, ...) {
    va_list args;
    va_start(args, text);

    vsprintf(log_buffer, text, args);

    if(log_file) {
        fprintf(log_file, "%s", log_buffer);
    }
    fprintf(stdout, "%s", log_buffer);

    va_end(args);
}

void open_log() {
    log_file = fopen("tinygb.log", "w");
    if(!log_file)
        fprintf(stderr, "unable to open tinygb.log for writing, will log to stdout\n");

    write_log("log started\n");
}

void die(int status, const char *msg, ...) {
    destroy_window();

    if(ram) {
#ifdef CGB_DEBUG
        if(is_cgb) {
            cgb_dump_bgpd();
            cgb_dump_obpd();
        }
#endif

        cpu_log();

        FILE *memdump = fopen("memory.bin", "wb");
        if(!memdump) {
            write_log("failed to open memory.bin for writing\n");
        } else {
            fwrite(ram, 1024, 1058, memdump);
            fflush(memdump);
            fclose(memdump);
        }

        free(ram);
    }

    if(vram) {
        FILE *vramdump = fopen("vram.bin", "wb");
        if(!vramdump) {
            write_log("failed to open vram.bin for writing\n");
        } else {
            fwrite(vram, 1, 16384, vramdump);
            fflush(vramdump);
            fclose(vramdump);
        }

        free(vram);
    }

    free(rom);

    if(!status || !msg) {
        if(log_file) fclose(log_file);
        exit(status);
    }

    va_list args;
    va_start(args, msg);

    vsprintf(log_buffer, msg, args);

    if(log_file) {
        fprintf(log_file, "quitting with exit code %d: %s", status, log_buffer);
        fflush(log_file);
        fclose(log_file);
    }
    fprintf(stdout, "quitting with exit code: %d: %s", status, log_buffer);

    va_end(args);

    exit(status);
}
