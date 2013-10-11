/*
Copyright (c) 2013 Pauli Nieminen <suokkos@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "switch.h"

const char *switch_types[4] = {
	"IGD",
	"DIS",
	"DIS-Audio",
	"Unknown",
};

int parse_switch(struct switch_data *data)
{
	char buffer[4096];
	char type[30], def[2], power[4], pci[13];
	int lineno = 0;
	int r, id;
	FILE *f = fopen(SWITCH_PATH, "r");
	while (fgets(buffer, sizeof(buffer) - 1, f)) {
		lineno++;
	}

	data->devs = calloc(lineno, sizeof data->devs[0]);
	if (!data->devs) {
		printf("OOM\n");
		return -ENOMEM;
	}
	data->nr = lineno;
	lineno = 0;

	rewind(f);

	while ((r = fscanf(f, "%d:%[^:]:%c:%3s:%12s\n",
				&id, type, def, power, pci)) >= 0) {

		if (r < 5) {
			printf("Matches less than excepted elements. %d = (%d, %s, %c, %s, %s) at %d\n",
					r, id, type, def[0], power, pci, lineno);
			fclose(f);
			return -1;
		}

		if (lineno == data->nr) {
			printf("New device added in middle of run trying afain.\n");
			free(data->devs);
			fclose(f);
			return parse_switch(data);
		}

		if (strcmp("IGD", type) == 0)
			data->devs[lineno].type = SWITCH_IGD;
		else if (strcmp("DIS", type) == 0)
			data->devs[lineno].type = SWITCH_DIS;
		else if (strcmp("DIS-Audio", type) == 0)
			data->devs[lineno].type = SWITCH_DISAUDIO;
		else {
			printf("Unknown device type '%s'! Please report!\n", type);
			data->devs[lineno].type = SWITCH_UNKNOWN;
		}

		data->devs[lineno].id = id;
		strcpy(data->devs[lineno].pci, pci);
		data->devs[lineno].def = (def[0] == '+') ? 1 : 0;
		data->devs[lineno].power = (strcmp("Pwr", power) == 0);
		
		lineno++;
	}

	fclose(f);
	return 0;
}
