/*
MIT License

Copyright (c) 2020 Bl00drav3n

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tinyxml2.h"
#include "tinyxml2.cpp"

struct cry_xml_node_t {
	int32_t name_offset;
	int32_t content_offset;
	int16_t attribute_count;
	int16_t child_count;
	int32_t parent_id;
	int32_t first_attr_idx;
	int32_t first_child_idx;
	int32_t reserved;
};

struct cry_xml_ref_t {
	int32_t name_offset;
	int32_t value_offset;
};

struct cry_xml_value_t {
	int32_t offset;
	char* value;
};

struct read_file_result_t {
	unsigned char *data;
	uint64_t size;
};
read_file_result_t read_file(const char *filename) {
	read_file_result_t result = {};
	FILE* f = fopen(filename, "rb");
	if (f) {
		fseek(f, 0L, SEEK_END);
		size_t size = ftell(f);
		fseek(f, 0L, SEEK_SET);
		unsigned char* data = (unsigned char*)malloc(size);
		if (data && fread(data, 1, size, f) == size) {
			result.data = data;
			result.size = size;
		}
		else {
			fprintf(stderr, "Error reading file %s\n", filename);
		}
		fclose(f);
	}
	else {
		fprintf(stderr, "Error opening file %s\n", filename);
	}
	return result;
}

bool write_file(const char* filename, const unsigned char* data, size_t size) {
	bool result = true;
	FILE *f = fopen(filename, "wb");
	if (f) {
		if (fwrite(data, 1, size, f) != size) {
			fprintf(stderr, "Error writing file %s\n", filename);
			result = false;
		}
		fclose(f);
	}
	else {
		fprintf(stderr, "Error opening file %s\n", filename);
	}
	return result;
}

struct binary_stream_t {
	unsigned char* data;
	uint64_t ptr;
	uint64_t size;
};

uint64_t seek(binary_stream_t* stream, uint64_t offset) {
	assert(offset < stream->size);
	if (offset < stream->size) {
		stream->ptr = offset;
	}
	return stream->ptr;
}

unsigned char peek_byte(binary_stream_t* stream) {
	assert(stream->ptr < stream->size);
	return (stream->ptr < stream->size) ? stream->data[stream->ptr] : 0;
}

char* read_string(binary_stream_t* stream, uint64_t size) {
	char *result = 0;
	assert(stream->ptr + size <= stream->size);
	if (stream->ptr + size <= stream->size) {
		result = (char*)stream->data;
		stream->ptr += size;
	}
	else {
		stream->ptr = stream->size;
	}
	return result;
}

char* read_cstring(binary_stream_t* stream) {
	char *result = (char*)stream->data + stream->ptr;
	for (; stream->data[stream->ptr]; stream->ptr++) {
		assert(stream->ptr < stream->size);
		if (stream->ptr == stream->size) {
			result = 0;
			break;
		}
	}
	if (stream->ptr < stream->size) {
		stream->ptr++;
	}
	return result;
}

unsigned char read_byte(binary_stream_t* stream) {
	unsigned char result = 0;
	assert(stream->ptr < stream->size);
	if (stream->ptr < stream->size) {
		result = stream->data[stream->ptr++];
	}
	return result;
}

uint16_t read_uint16(binary_stream_t* stream) {
	unsigned char bytes[2];
	bytes[0] = read_byte(stream);
	bytes[1] = read_byte(stream);
	return (bytes[1] << 8) | bytes[0];
}

int16_t read_int16(binary_stream_t* stream) {
	return (int16_t)read_uint16(stream);
}

uint32_t read_uint32(binary_stream_t *stream) {
	unsigned char bytes[4];
	bytes[0] = read_byte(stream);
	bytes[1] = read_byte(stream);
	bytes[2] = read_byte(stream);
	bytes[3] = read_byte(stream);
	return (bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
}
int32_t read_int32(binary_stream_t* stream) {
	return (int32_t)read_uint32(stream);
}

void convert_file(const char *filename) {
	binary_stream_t the_stream = {};

	const char *ext_str = "bak";
	read_file_result_t xml_file = read_file(filename);
	
	char *backup_name = (char*)malloc(strlen(filename) + strlen(ext_str));
	sprintf(backup_name, "%s.%s", filename, ext_str);
	if (!write_file(backup_name, xml_file.data, xml_file.size)) {
		fprintf(stderr, "Aborting.\n");
		exit(1);
	}

	binary_stream_t* stream = &the_stream;
	stream->data = xml_file.data;
	stream->size = xml_file.size;

	if (xml_file.data && xml_file.size) {
		unsigned char peek = peek_byte(stream);
		if (peek == '<') {
			fprintf(stdout, "File %s is already XML\n", filename);
			return;
		}
		else if (peek != 'C') {
			fprintf(stderr, "File %s has unknown file format\n", filename);
			return;
		}

		char * header = read_cstring(stream);
		if (header) {
			if (strncmp(header, "CryXmlB", 7) == 0) {
				uint32_t file_size = read_int32(stream);

				uint32_t node_table_offset = read_int32(stream);
				uint32_t node_table_count = read_int32(stream);

				uint32_t attr_table_offset = read_int32(stream);
				uint32_t attr_table_count = read_int32(stream);

				uint32_t child_table_offset = read_int32(stream);
				uint32_t child_table_count = read_int32(stream);

				uint32_t data_table_offset = read_int32(stream);
				uint32_t data_table_size = read_int32(stream);

				cry_xml_node_t *node_table = (cry_xml_node_t*)calloc(node_table_count, sizeof(*node_table));
				seek(stream, node_table_offset);
				for (uint32_t i = 0; i < node_table_count; i++) {
					cry_xml_node_t *node = node_table + i;
					node->name_offset = read_int32(stream);
					node->content_offset = read_int32(stream);
					node->attribute_count = read_int16(stream);
					node->child_count = read_int16(stream);
					node->parent_id = read_int32(stream);
					node->first_attr_idx = read_int32(stream);
					node->first_child_idx = read_int32(stream);
					node->reserved = read_int32(stream);
				}

				cry_xml_ref_t* attr_table = (cry_xml_ref_t*)calloc(attr_table_count, sizeof(*attr_table));
				seek(stream, attr_table_offset);
				for (uint32_t i = 0; i < attr_table_count; i++) {
					attr_table[i].name_offset = read_int32(stream);
					attr_table[i].value_offset = read_int32(stream);
				}

				uint32_t* child_table = (uint32_t*)calloc(child_table_count, sizeof(*child_table));
				seek(stream, child_table_offset);
				for (uint32_t i = 0; i < child_table_count; i++) {
					child_table[i] = read_int32(stream);
				}
				
				tinyxml2::XMLDocument doc;
				tinyxml2::XMLElement **xml_nodes = (tinyxml2::XMLElement**)malloc(node_table_count * sizeof(*xml_nodes));
				uint64_t attr_idx = 0;
				char *data_table = (char*)stream->data + data_table_offset;
				for (uint32_t i = 0; i < node_table_count; i++) {
					cry_xml_node_t *node = node_table + i;
					tinyxml2::XMLElement *elem = doc.NewElement(data_table + node->name_offset);
					for (int16_t j = 0; j < node->attribute_count; j++) {
						elem->SetAttribute(data_table + attr_table[attr_idx].name_offset, data_table + attr_table[attr_idx].value_offset);
						attr_idx++;
					}
					elem->SetText(data_table + node->content_offset);
					xml_nodes[i] = elem;
				}
				for (uint32_t i = 0; i < node_table_count; i++) {
					cry_xml_node_t* node = node_table + i;
					if (node->parent_id == -1) {
						doc.InsertFirstChild(xml_nodes[i]);
					}
					else {
						xml_nodes[node->parent_id]->InsertEndChild(xml_nodes[i]);
					}
				}

				// TODO: switch to non-compact and fix formatting
				doc.SaveFile(filename, true);
			}
			else {
				fprintf(stderr, "Invalid header in file %s\n", filename);
			}
		}
		else {
			fprintf(stderr, "Error reading header of file %s\n", filename);
		}
	}
}

int main(int argc, char* argv[]) {
	if (argc == 2) {
		convert_file(argv[1]);
	}
	else {
		fprintf(stderr, "USAGE: CryXmlB filename\n");
	}
	return 0;
}
