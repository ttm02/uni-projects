#include "measure.h"

#include <stdint.h>
#include <inttypes.h>
#include <math.h>
#include <malloc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
//#include <regex.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>
#include <sys/sysinfo.h>
#include <float.h>

struct list_elem {
	double value;
	struct list_elem* next;
};

//int main(int argc, char** argv) {
int main() {

	printf("Configure measurement:\n");

	GKeyFile* config_file = g_key_file_new();

	// finde alle disks
	FILE* fd = popen("lsblk", "r");
	if (fd == 0) {
		printf("Error reading device list\n");
		return -1;
	}

	char* name = malloc(MAX_DEVICE_NAME_LENGTH * sizeof(char));
	char* device_name = malloc(MAX_DEVICE_NAME_LENGTH * sizeof(char));
	char* size = malloc(MAX_DEVICE_NAME_LENGTH * sizeof(char));
	char* type = malloc(MAX_DEVICE_NAME_LENGTH * sizeof(char));
	char* mountpoint = malloc(MAX_DEVICE_NAME_LENGTH * sizeof(char));

	int maj, min, rm, ro;

	char* line = NULL;
	size_t len = 0;

	while (getline(&line, &len, fd) != -1) {
		type[0] = 0;// do not use last type if there is no type present
		sscanf(line,
				"%" MAX_DEVICE_NAME_LENGTH_S "s %i:%i %i %" MAX_DEVICE_NAME_LENGTH_S "s %i %" MAX_DEVICE_NAME_LENGTH_S "s %" MAX_DEVICE_NAME_LENGTH_S "s",
				device_name, &maj, &min, &rm, size, &ro, type, mountpoint);
		if (strcmp(type, "disk") == 0) {
			printf("Choose setting for block-device %s\n", device_name);
			if (getline(&line, &len, fd) != -1) {
				printf("mountpoints for the device partitions:\n");
				sscanf(line,
						"%" MAX_DEVICE_NAME_LENGTH_S "s %i:%i %i %" MAX_DEVICE_NAME_LENGTH_S "s %i %" MAX_DEVICE_NAME_LENGTH_S "s %" MAX_DEVICE_NAME_LENGTH_S "s",
						name, &maj, &min, &rm, size, &ro, type, mountpoint);
				while (strcmp(type, "part") == 0) {
					printf("%s	", mountpoint);
					if (getline(&line, &len, fd) != -1) {
						sscanf(line,
								"%" MAX_DEVICE_NAME_LENGTH_S "s %i:%i %i %" MAX_DEVICE_NAME_LENGTH_S "s %i %" MAX_DEVICE_NAME_LENGTH_S "s %" MAX_DEVICE_NAME_LENGTH_S "s",
								name, &maj, &min, &rm, size, &ro, type, mountpoint);
					} else {
						break;
					}

				}
				printf("\n");

				printf(
						"type:\n -1 do not measure this device\n 0 set up metrics for this device but set the default-settign for them to off\n 1 measure this device\n");
				int setting = 2;
				while (setting != 0 && setting != 1 && setting != -1) {
					printf("choose setting for %s:", device_name);
					scanf("%d", &setting);
				}
				printf("\n");

				// set setting
				if (setting != -1) {
					g_key_file_set_integer(config_file, "disk", device_name, setting);

				}

			}
		}

	}
	free(line);

	free(device_name);
	free(name);
	free(size);
	free(type);
	free(mountpoint);

	pclose(fd);

	DIR *dir;
	struct dirent *entry;

	dir = opendir("/sys/class/net");
	if (dir == NULL) {
		printf("Error reading network devices");
		return -1;
	}
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
			printf("Choose setting for net-device %s\n", entry->d_name);
			printf(
					"type:\n -1 do not measure this device\n 0 set up metrics for this device but set the default-settign for them to off\n 1 measure this device\n");
			int setting = 2;
			while (setting != 0 && setting != 1 && setting != -1) {
				printf("choose setting for %s:", entry->d_name);
				scanf("%d", &setting);
			}
			printf("\n");

			// set setting
			if (setting != -1) {
				g_key_file_set_integer(config_file, "net", entry->d_name, setting);
			}
		}
	}
	closedir(dir);

	g_key_file_set_comment(config_file, NULL, NULL, "config for measurement version " VERSION, NULL);
	g_key_file_save_to_file(config_file, CONFIG_FILE_NAME, NULL);

	g_key_file_free(config_file);
	printf("configuration completed\n");
	return 0;
}
