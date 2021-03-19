#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include "gifdec.h"

uint16_t C_RGB(uint8_t r, uint8_t g, uint8_t b){
	return (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

int find(uint16_t needle, const uint16_t* haystack, uint16_t size){
	int i;
	for(i = size-1; i >= 0; i--){
		if(haystack[i] == needle) break;
	}

	return i;
}

uint8_t color_table(gd_GIF* gif, uint16_t** table){
	uint8_t* frame = malloc(gif->width * gif->height * 3);

	*table = NULL;
	uint16_t colors = 0;

	while(gd_get_frame(gif) == 1){
		gd_render_frame(gif, frame);

		for(int i = 0; i < gif->width * gif->height; i++){
			uint16_t pixel = C_RGB(frame[i*3], frame[i*3 + 1], frame[i*3 + 2]);

			if(colors == 0){
				(*table) = realloc(*table, (colors+1) * 2);
				(*table)[colors] = pixel;
				colors++;
				continue;
			}

			if(find(pixel, *table, colors) == -1){
				if(colors == 256){
					free(*table);
					*table = NULL;
					free(frame);
					gd_rewind(gif);
					return 0;
				}

				(*table) = realloc(*table, (colors+1) * 2);
				(*table)[colors] = pixel;
				colors++;
			}
		}
	}

	free(frame);
	gd_rewind(gif);
	return colors;
}

void processGif(gd_GIF* gif, FILE* output){
	fwrite(&gif->width, 2, 1, output);
	fwrite(&gif->height, 2, 1, output);
	fseek(output, 2, SEEK_CUR);

	printf("Width: %d, height: %d\n", gif->width, gif->height);

	uint16_t* table;
	uint8_t tableColors = color_table(gif, &table);

	uint8_t flags = tableColors != 0;
	fwrite(&flags, 1, 1, output);

	if(tableColors != 0){
		fwrite(&tableColors, 1, 1, output);
		fwrite(table, 2, tableColors, output);
	}

	uint16_t noFrames = 0;
	uint8_t* frame = malloc(gif->width * gif->height * 3);
	uint16_t* frameOutput = malloc(gif->width * gif->height * (tableColors ? 1 : 2));
	while(gd_get_frame(gif) == 1){
		gd_render_frame(gif, frame);

		for(int i = 0; i < gif->width * gif->height; i++){
			uint16_t pixel = C_RGB(frame[i*3], frame[i*3 + 1], frame[i*3 + 2]);

			if(tableColors){
				((uint8_t*) frameOutput)[i] = find(pixel, table, tableColors);
			}else{
				frameOutput[i] = pixel;
			}
		}

		gif->gce.delay *= 10;
		fwrite(&gif->gce.delay, 2, 1, output);

		fwrite(frameOutput, (tableColors ? 1 : 2), gif->width * gif->height, output);

		noFrames++;
	}

	free(frameOutput);
	free(frame);

	fseek(output, 4, SEEK_SET);
	fwrite(&noFrames, 2, 1, output);

	printf("No frames: %d\n", noFrames);
	if(tableColors){
		printf("%d colors total, using color table\n", tableColors);
		free(table);
	}
}

void outputHeader(FILE* input, FILE* output, char* name){
	fseek(input, 0, SEEK_END);
	size_t size = ftell(input);

	fseek(input, 0, SEEK_SET);
	fseek(output, 0, SEEK_SET);

	fprintf(output, "const uint8_t %s[%ld] PROGMEM = {\n\t", name, size);
	int cursor = -1;
	size_t pos = 0;

	uint8_t byte;
	while(fread(&byte, 1, 1, input)){
		cursor++;
		if(cursor == 8){
			fprintf(output, "\n\t");
			cursor = 0;
		}

		fprintf(output, "0x%02x", byte);

		if(pos < size){
			fprintf(output, ", ");
		}
		pos++;
	}

	if(cursor != 0){
		fprintf(output, "\n");
	}
	fprintf(output, "};\n");
}

int main(int argc, char* argv[]){
	if(argc != 3){
		printf("./GIF565 <input gif> <output.hpp>");
		return 1;
	}

	char* tempFilename = malloc(strlen(argv[2]) + 6);
	sprintf(tempFilename, "%s.g565", argv[2]);
	FILE* tempFile = fopen(tempFilename, "w+");
	if(tempFile == NULL){
		printf("temp file open error\n");
		return 2;
	}

	gd_GIF* gif = gd_open_gif(argv[1]);
	if(gif == NULL){
		printf("gif open error\n");
		return 2;
	}

	processGif(gif, tempFile);
	gd_close_gif(gif);

	FILE* output = fopen(argv[2], "w");
	if(output == NULL){
		printf("output open error\n");
		return 2;
	}

	char* name = strtok(basename(argv[2]), ".");
	printf("Saving as %s\n", name);
	outputHeader(tempFile, output, name);

	fclose(output);
	fclose(tempFile);
	unlink(tempFilename);
	free(tempFilename);
	return 0;
}
