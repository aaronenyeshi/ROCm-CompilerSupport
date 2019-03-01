/*******************************************************************************
*
* University of Illinois/NCSA
* Open Source License
*
* Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* with the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
*     * Redistributions of source code must retain the above copyright notice,
*       this list of conditions and the following disclaimers.
*
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimers in the
*       documentation and/or other materials provided with the distribution.
*
*     * Neither the names of Advanced Micro Devices, Inc. nor the names of its
*       contributors may be used to endorse or promote products derived from
*       this Software without specific prior written permission.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
* THE SOFTWARE.
*
*******************************************************************************/

#include "amd_comgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
typedef struct Kernel_Code_Prop {
  int kernarg_segment_size;
  int group_segment_fixed_size;
  int private_segment_fixed_size;
  int kernarg_segment_align;
  int wavefront_size;
  int num_sgprs;
  int num_vgprs;
  int max_flat_work_group_size;
  int num_spilled_sgprs;
  int num_spilled_vgprs;
} Kernel_Code_Prop;

typedef struct Kernel_Arg {
  char *name;
  char *type_name;
  int size;
  int align;
  char *value_kind;
  char *value_type;
  int pointee_align;
  char *addr_space_qual;
  char *actual_acc_qual;
  bool is_const;
  bool is_restrict;
  bool is_volatile;
  bool is_pipe;
} Kernel_Arg;

typedef struct Kernel_Attr {
  int reqd_work_group_size[3];
  int work_group_size_hint[3];
  char *vec_type_hint;
  char *runtime_handle;
} Kernel_Attr;

typedef struct Kernel_Metadata {
  char *name;
  char *symbol_name;
  char *language;
  int language_version_major;
  int language_version_minor;
  int num_attrs;
  int num_args;
  int num_code_props;
  Kernel_Attr *attrs;
  Kernel_Arg *args;
  Kernel_Code_Prop *code_props;
} Kernel_Metadata;

typedef struct Program_Metadata {
  int version_major;
  int version_minor;
  int num_kernels;
  Kernel_Metadata *kernels;
} Program_Metadata;

char* create_string_from_string_node(amd_comgr_metadata_node_t printNode) {
    amd_comgr_status_t hcc_stat;
    size_t printSize;
    char *printString;
    hcc_stat = amd_comgr_get_metadata_string(printNode, &printSize, NULL);
    checkError(hcc_stat, "amd_comgr_get_metadata_string");
    printf("printSize: %lu\n", printSize);
    printString = (char*)malloc(printSize);
    hcc_stat = amd_comgr_get_metadata_string(printNode, &printSize, printString);
    checkError(hcc_stat, "amd_comgr_get_metadata_string");
    printf("printString: %s\n", printString);
    return printString;
}

int main(int argc, char *argv[]) {
  char *arg = NULL;
  long size1, size2;
  char *buf, *bufHcc;
  amd_comgr_data_t dataIn;
  amd_comgr_status_t status;
  amd_comgr_metadata_kind_t mkind = AMD_COMGR_METADATA_KIND_NULL;
  char *input_name = malloc(strlen(TEST_OBJ_DIR) + strlen("/dump-gfx900.hsaco") + 1);
  input_name[0] = '\0';
  strcat(input_name, TEST_OBJ_DIR);
  strcat(input_name, "/dump-gfx900.hsaco");

  // Read input file
  size1 = setBuf(input_name, &buf);
  size2 = setBuf(input_name, &bufHcc);

  // Create data object
  {
    printf("Test create input data object\n");

    status = amd_comgr_create_data(AMD_COMGR_DATA_KIND_RELOCATABLE, &dataIn);
    checkError(status, "amd_comgr_create_data");

    status = amd_comgr_set_data(dataIn, size1, buf);
    checkError(status, "amd_comgr_set_data");

    status = amd_comgr_set_data_name(dataIn, arg);
    checkError(status, "amd_comgr_set_data_name");
  }

  // Get metadata from data object
  {
    printf("Get metadata from %s\n", input_name);

    amd_comgr_metadata_node_t meta;
    status = amd_comgr_get_data_metadata(dataIn, &meta);
    checkError(status, "amd_comgr_get_data_metadata");

    // the root must be map
    status = amd_comgr_get_metadata_kind(meta, &mkind);
    checkError(status, "amd_comgr_get_metadata_kind");
    if (mkind != AMD_COMGR_METADATA_KIND_MAP) {
      printf("Root is not map\n");
      exit(1);
    }

    amd_comgr_metadata_node_t metaLookup;
    amd_comgr_metadata_kind_t mkindLookup;
    status = amd_comgr_metadata_lookup(meta, "Version", &metaLookup);
    checkError(status, "amd_comgr_metadata_lookup");
    status = amd_comgr_get_metadata_kind(metaLookup, &mkindLookup);
    checkError(status, "amd_comgr_get_metadata_kind");
    if (mkindLookup != AMD_COMGR_METADATA_KIND_LIST) {
      printf("Lookup of Version should return a list\n");
      exit(1);
    }
    status = amd_comgr_destroy_metadata(metaLookup);
    checkError(status, "amd_comgr_destroy_metadata");

    // print code object metadata
    int indent = 0;
    status = amd_comgr_iterate_map_metadata(meta, print_entry, (void *)&indent);
    checkError(status, "amd_comgr_iterate_map_metadata");
  }

  {
    size_t sizeHcc;
    amd_comgr_metadata_node_t printNode;
    char *printString;

    printf("\nStarting Aaron's Parsing code!! \n");
    amd_comgr_status_t hcc_stat;
    amd_comgr_data_t dataHcc;
    hcc_stat = amd_comgr_create_data(AMD_COMGR_DATA_KIND_RELOCATABLE, &dataHcc);
    checkError(hcc_stat, "amd_comgr_create_data");

    hcc_stat = amd_comgr_set_data(dataHcc, size2, bufHcc);
    checkError(hcc_stat, "amd_comgr_set_data");

    hcc_stat = amd_comgr_set_data_name(dataHcc, "Testing HCC");
    checkError(hcc_stat, "amd_comgr_set_data_name");

    amd_comgr_metadata_node_t metaHcc;
    hcc_stat = amd_comgr_get_data_metadata(dataHcc, &metaHcc);
    checkError(hcc_stat, "amd_comgr_get_data_metadata");

    // Root is always a map
    amd_comgr_metadata_kind_t mkindHcc;
    hcc_stat = amd_comgr_get_metadata_kind(metaHcc, &mkindHcc);
    checkError(hcc_stat, "amd_comgr_get_metadata_kind");
    if (mkindHcc != AMD_COMGR_METADATA_KIND_MAP) {
      printf("Root is not map\n");
      exit(1);
    }
    Program_Metadata program;

    // Now grab the version list
    amd_comgr_metadata_node_t metaLookupHcc;
    amd_comgr_metadata_kind_t mkindLookupHcc;
    hcc_stat = amd_comgr_metadata_lookup(metaHcc, "Version", &metaLookupHcc);
    checkError(hcc_stat, "amd_comgr_metadata_lookup");
    hcc_stat = amd_comgr_get_metadata_kind(metaLookupHcc, &mkindLookupHcc);
    if (mkindLookupHcc != AMD_COMGR_METADATA_KIND_LIST) {
      printf("Lookup of Version didn't return a list\n");
      exit(1);
    }
    hcc_stat = amd_comgr_index_list_metadata(metaLookupHcc, 0, &printNode);
    checkError(hcc_stat, "amd_comgr_index_list_metadata");
    printString = create_string_from_string_node(printNode);
    if (printString)
      program.version_major=atoi(printString);
    free(printString);

    hcc_stat = amd_comgr_index_list_metadata(metaLookupHcc, 1, &printNode);
    checkError(hcc_stat, "amd_comgr_index_list_metadata");
    printString = create_string_from_string_node(printNode);
    program.version_minor=atoi(printString);
    free(printString);

    // Kernels is a list of MAPS!!
    hcc_stat = amd_comgr_metadata_lookup(metaHcc, "Kernels", &metaLookupHcc);
    checkError(hcc_stat, "amd_comgr_metadata_lookup");
    hcc_stat = amd_comgr_get_metadata_kind(metaLookupHcc, &mkindLookupHcc);
    if (mkindLookupHcc != AMD_COMGR_METADATA_KIND_LIST) {
      printf("Lookup of Kernels didn't return a list\n");
      exit(1);
    }

    hcc_stat = amd_comgr_get_metadata_list_size(metaLookupHcc, &sizeHcc);
    checkError(status, "amd_comgr_get_metadata_list_size");
    program.kernels = (Kernel_Metadata*) malloc(sizeHcc * sizeof(Kernel_Metadata));
    program.num_kernels = sizeHcc;
    printf("num_kernels: %lu\n", sizeHcc);
    for (int i = 0; i < program.num_kernels; i++) {
      Kernel_Metadata *kern_md = &program.kernels[i];
      kern_md->num_attrs=0;kern_md->num_args=0;kern_md->num_code_props=0;

      amd_comgr_metadata_node_t kernelMap;
      hcc_stat = amd_comgr_index_list_metadata(metaLookupHcc, i, &kernelMap);
      checkError(hcc_stat, "amd_comgr_index_list_metadata");

      hcc_stat = amd_comgr_metadata_lookup(kernelMap, "Name", &printNode);
      checkError(hcc_stat, "amd_comgr_metadata_lookup");
      kern_md->name = create_string_from_string_node(printNode);

      hcc_stat = amd_comgr_metadata_lookup(kernelMap, "SymbolName", &printNode);
      checkError(hcc_stat, "amd_comgr_metadata_lookup");
      kern_md->symbol_name = create_string_from_string_node(printNode);

      amd_comgr_metadata_node_t kernAttrMap;
      hcc_stat = amd_comgr_metadata_lookup(kernelMap, "Attrs", &kernAttrMap);
      if (hcc_stat == AMD_COMGR_STATUS_SUCCESS ) {
        kern_md->attrs = (Kernel_Attr*) malloc(sizeof(Kernel_Attr));
        kern_md->num_attrs = 1;
        for (int k_at = 0; k_at < kern_md->num_attrs; k_at++) {
          Kernel_Attr *kern_at = &kern_md->attrs[k_at];
        }
      }
      printf("  num_attrs: %d\n", kern_md->num_attrs);

      amd_comgr_metadata_node_t kernArgMap;
      hcc_stat = amd_comgr_metadata_lookup(kernelMap, "Args", &kernArgMap);
      if (hcc_stat == AMD_COMGR_STATUS_SUCCESS ) {
        hcc_stat = amd_comgr_get_metadata_list_size(kernArgMap, &sizeHcc);
        checkError(status, "amd_comgr_get_metadata_list_size");
        kern_md->args = (Kernel_Arg*) malloc(sizeHcc * sizeof(Kernel_Arg));
        kern_md->num_args = sizeHcc;
        for (int k_ar = 0; k_ar < kern_md->num_args; k_ar++) {
          Kernel_Arg *kern_ar = &kern_md->args[k_ar];
        }
      }
      printf("  num_args: %d\n", kern_md->num_args);

      amd_comgr_metadata_node_t kernCodePropMap;
      hcc_stat = amd_comgr_metadata_lookup(kernelMap, "CodeProps", &kernCodePropMap);
      if (hcc_stat == AMD_COMGR_STATUS_SUCCESS ) {
        kern_md->num_code_props = 1;
        kern_md->code_props = (Kernel_Code_Prop*) malloc(sizeof(Kernel_Code_Prop));
        for (int k_cp = 0; k_cp < kern_md->num_code_props; k_cp++) {
          Kernel_Code_Prop *kern_cp = &kern_md->code_props[k_cp];
          hcc_stat = amd_comgr_metadata_lookup(kernCodePropMap, "KernargSegmentSize", &printNode);
          checkError(hcc_stat, "amd_comgr_metadata_lookup");
          printString = create_string_from_string_node(printNode);
          kern_cp->kernarg_segment_size = atoi(printString);
          free(printString);
        }
      }
      printf("  num_code_props: %d\n", kern_md->num_code_props);
    }

    printf("  Clean up Aaron HCC ...\n\n");
    hcc_stat = amd_comgr_destroy_metadata(metaLookupHcc);
    checkError(hcc_stat, "amd_comgr_destroy_metadata");
    hcc_stat = amd_comgr_destroy_metadata(printNode);
    checkError(hcc_stat, "amd_comgr_destroy_metadata");

    char indent[50];
    strcpy(indent, "  "); indent[2]='\0';
    printf("Printing all the Metadata Structure\n");
    printf("Program Metadata\n");
    printf("%sVersion: %d , %d\n", indent, program.version_major, program.version_minor);
    for (int k = 0; k < program.num_kernels; k++) {
      strcat(indent, "  ");
      Kernel_Metadata *kern_md = &program.kernels[k];
      printf("%sName: %s\n", indent, kern_md->name);
      printf("%sSymbolName: %s\n", indent, kern_md->symbol_name);
      for (int k_at = 0; k_at < kern_md->num_attrs; k_at++) {
        Kernel_Attr *kern_at = &kern_md->attrs[k_at];
      }
      for (int k_ar = 0; k_ar < kern_md->num_args; k_ar++) {
        Kernel_Arg *kern_ar = &kern_md->args[k_ar];
      }
      for (int k_cp = 0; k_cp < kern_md->num_code_props; k_cp++) {
        Kernel_Code_Prop *kern_cp = &kern_md->code_props[k_cp];
        printf("%sKernargSegmentSize: %d\n", indent, kern_cp->kernarg_segment_size);
      }
    }

    // Clean up everything I've malloc
    for (int k = 0; k < program.num_kernels; k++) {
      Kernel_Metadata *kern_md = &program.kernels[k];
      for (int k_at = 0; k_at < kern_md->num_attrs; k_at++) {
        Kernel_Attr *kern_at = &kern_md->attrs[k_at];
      }
      if(kern_md->attrs && kern_md->num_attrs)
        free(kern_md->attrs);
      for (int k_ar = 0; k_ar < kern_md->num_args; k_ar++) {
        Kernel_Arg *kern_ar = &kern_md->args[k_ar];
      }
      if(kern_md->args && kern_md->num_args)
        free(kern_md->args);
      for (int k_cp = 0; k_cp < kern_md->num_code_props; k_cp++) {
        Kernel_Code_Prop *kern_cp = &kern_md->code_props[k_cp];
      }
      if(kern_md->code_props && kern_md->num_code_props)
        free(kern_md->code_props);
    }
    free(program.kernels);
  }

  {
    printf("Cleanup ...\n");
    status = amd_comgr_release_data(dataIn);
    checkError(status, "amd_comgr_release_data");
    free(buf);
  }

  return 0;
}
