
/* tinygb - a tiny gameboy emulator
   (c) 2022 by jewel */

#include <tinygb.h>
#include <stdio.h>
#include <string.h>

#define DEFAULT_A           "z"
#define DEFAULT_B           "x"
#define DEFAULT_START       "return"
#define DEFAULT_SELECT      "rshift"
#define DEFAULT_UP          "up"
#define DEFAULT_DOWN        "down"
#define DEFAULT_LEFT        "left"
#define DEFAULT_RIGHT       "right"
#define DEFAULT_THROTTLE    "space"
#define DEFAULT_SYSTEM      "auto"
#define DEFAULT_PREFERENCE  "cgb"
#define DEFAULT_BORDER      "yes"
#define DEFAULT_SCALING     "2"
#define DEFAULT_PALETTE     "0"

config_file_t config_file;

static FILE *file;

static char nullstr[2];
static char line[200];

static void load_defaults() {
    config_file.a = DEFAULT_A;
    config_file.b = DEFAULT_B;
    config_file.start = DEFAULT_START;
    config_file.select = DEFAULT_SELECT;
    config_file.up = DEFAULT_UP;
    config_file.down = DEFAULT_DOWN;
    config_file.left = DEFAULT_LEFT;
    config_file.right = DEFAULT_RIGHT;

    config_file.throttle = DEFAULT_THROTTLE;

    config_file.system = DEFAULT_SYSTEM;
    config_file.preference = DEFAULT_PREFERENCE;
    config_file.border = DEFAULT_BORDER;
    config_file.scaling = DEFAULT_SCALING;

    scaling = 2;
}

static void lowercase(char *str) {
    for(int i = 0; str[i]; i++) {
        if(str[i] >= 'A' && str[i] <= 'Z') {
            str[i] += 32;
        }
    }
}

char *get_property(char *property) {
    fseek(file, 0L, SEEK_SET);

    int len = strlen(property);

    char *value;

    while(fgets(line, 199, file)) {
        lowercase(line);

        if(!memcmp(line, property, len)) {
            // found property
            int i = len;
            while(line[i] != '=' && line[i] != '\n' && line[i] != '\r') i++;

            if(line[i] == '\n' || line[i] == '\r') {
                // property has no value
                return nullstr;
            }

            while(line[i] == ' ' || line[i] == '=') i++;

            // now copy the value
            value = calloc(strlen(line+i)+1, 1);
            if(!value) {
                write_log("[config] unable to allocate memory for property '%s', assuming defaults\n", property);
                return nullstr;
            }

            int j = 0;
            while(line[i+j] != '\n' && line[i+j] != '\r' && line[i+j] != 0 && line[i+j] != ';' && line[i+j] != ' ') {
                value[j] = line[i+j];
                j++;
            }

            write_log("[config] property '%s' is set to '%s'\n", property, value);
            return value;
        }
    }

    write_log("[config] property '%s' doesn't exist, assuming default\n", property);
    return nullstr;
}

void open_config() {
    nullstr[0] = 0;
    file = fopen("tinygb.ini", "r");
    if(!file) {
        write_log("[config] unable to open tinygb.ini for reading, loading default settings\n");

        load_defaults();
    }

    config_file.a = get_property("a");
    config_file.b = get_property("b");
    config_file.start = get_property("start");
    config_file.select = get_property("select");
    config_file.up = get_property("up");
    config_file.down = get_property("down");
    config_file.left = get_property("left");
    config_file.right = get_property("right");
    config_file.throttle = get_property("throttle");
    config_file.system = get_property("system");
    config_file.preference = get_property("preference");
    config_file.border = get_property("border");
    config_file.scaling = get_property("scaling");
    config_file.palette = get_property("palette");

    fclose(file);

    if(!strcmp(config_file.system, "auto")) config_system = SYSTEM_AUTO;
    else if(!strcmp(config_file.system, "gb")) config_system = SYSTEM_GB;
    else if(!strcmp(config_file.system, "sgb2")) config_system = SYSTEM_SGB2;
    else if(!strcmp(config_file.system, "cgb")) config_system = SYSTEM_CGB;
    else config_system = SYSTEM_AUTO;   // default

    if(!strcmp(config_file.preference, "cgb")) config_preference = PREFER_CGB;
    else if(!strcmp(config_file.preference, "gb")) config_preference = PREFER_GB;
    else config_preference = PREFER_CGB;    // default

    if(!strcmp(config_file.border, "yes")) config_border = 1;
    else if(!strcmp(config_file.border, "no")) config_border = 0;
    else config_border = 1;     // default

    scaling = atoi(config_file.scaling);
    if(!scaling) scaling = 2;   // default

    monochrome_palette = atoi(config_file.palette);
    if(monochrome_palette > 9) monochrome_palette = 0;
}
