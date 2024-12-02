/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/* auto generate by HHB_VERSION "3.0.26-beta" */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <libgen.h>
#include <unistd.h>
#include "io.h"
#include "shl_ref.h"
#include "cmd_parse.h"
#define MIN(x, y)           ((x) < (y) ? (x) : (y))
#define FILE_LENGTH         1028
#define SHAPE_LENGHT        128
#define FILE_PREFIX_LENGTH  (1028 - 2 * 128)

void *csinn_(char *params);
void csinn_update_input_and_run(struct csinn_tensor **input_tensors , void *sess);
#define csinn_nbg(...) NULL

int input_size[] = {1 * 3 * 640 * 640, };
const char model_name[] = "network";



static void print_tensor_info(struct csinn_tensor *t) {
    printf("\n=== tensor info ===\n");
    printf("shape: ");
    for (int j = 0; j < t->dim_count; j++) {
        printf("%d ", t->dim[j]);
    }
    printf("\n");
    if (t->dtype == CSINN_DTYPE_UINT8) {
        printf("scale: %f\n", t->qinfo->scale);
        printf("zero point: %d\n", t->qinfo->zero_point);
    }
    printf("data pointer: %p\n", t->data);
}


/*
 * Postprocess function
 */
static void postprocess(void *sess, const char *filename_prefix) {
    int output_num, input_num;
    struct csinn_tensor *input = csinn_alloc_tensor(NULL);
    struct csinn_tensor *output = csinn_alloc_tensor(NULL);

    input_num = csinn_get_input_number(sess);
    for (int i = 0; i < input_num; i++) {
        input->data = NULL;
        csinn_get_input(i, input, sess);
        print_tensor_info(input);
    }

    output_num = csinn_get_output_number(sess);
    for (int i = 0; i < output_num; i++) {
        output->data = NULL;
        csinn_get_output(i, output, sess);
        print_tensor_info(output);

        struct csinn_tensor *foutput = shl_ref_tensor_transform_f32(output);
        
        char filename[FILE_LENGTH] = {0};
        char shape[SHAPE_LENGHT] = {0};
        shape2string(output->dim, output->dim_count, shape, SHAPE_LENGHT);
        snprintf(filename, FILE_LENGTH, "%s_output%u_%s.txt", filename_prefix, i, shape);
        int output_size = csinn_tensor_size(foutput);
        save_data_to_file(filename, (float*)foutput->data, output_size);

        shl_ref_tensor_transform_free_f32(foutput);
        if (!output->is_const) {
            shl_mem_free(output->data);
        }
    }
    csinn_free_tensor(input);
    csinn_free_tensor(output);
}

void *create_graph(char *params_path) {
    char *params = get_binary_from_file(params_path, NULL);
    if (params == NULL) {
        return NULL;
    }

    char *suffix = params_path + (strlen(params_path) - 7);
    if (strcmp(suffix, ".params") == 0) {
        // create general graph
        return csinn_(params);
    }

    suffix = params_path + (strlen(params_path) - 3);
    if (strcmp(suffix, ".bm") == 0) {
        struct shl_bm_sections *section = (struct shl_bm_sections *)(params + 4128);
        if (section->graph_offset) {
            return csinn_import_binary_model(params);
        } else {
            return csinn_(params + section->params_offset * 4096);
        }
    } else {
        return NULL;
    }
}

int main(int argc, char **argv) {
    char **data_path = NULL;
    int input_num = 1;
    int output_num = 3;
    int input_group_num = 1;
    int i;

    struct cmdline_options *option = cmdline_parser(argc, argv);

    if (option == NULL) {
        return -1;
    } else {
        int cmd_input_index = option->rest_line_index + 1;
        if (get_file_type(argv[cmd_input_index]) == FILE_TXT) {
            data_path = read_string_from_file(argv[cmd_input_index], &input_group_num);
            input_group_num /= input_num;
        } else {
            data_path = argv + cmd_input_index;
            input_group_num = (argc - cmd_input_index) / input_num;
        }
    }

    

    void *sess = create_graph(argv[option->rest_line_index]);

    struct csinn_tensor* input_tensors[input_num];
    input_tensors[0] = csinn_alloc_tensor(NULL);
    input_tensors[0]->dim_count = 4;
    input_tensors[0]->dim[0] = 1;
    input_tensors[0]->dim[1] = 3;
    input_tensors[0]->dim[2] = 640;
    input_tensors[0]->dim[3] = 640;

    float *inputf[input_num];
    char filename_prefix[FILE_PREFIX_LENGTH] = {0};
    uint64_t start_time, end_time;
    for (i = 0; i < input_group_num; i++) {
        /* set input */
        for (int j = 0; j < input_num; j++) {
            if (get_file_type(data_path[i * input_num + j]) != FILE_BIN) {
                printf("Please input binary files, since you compiled the model without preprocess.\n");
                return -1;
            }
            inputf[j] = (float*)get_binary_from_file(data_path[i * input_num + j], NULL);

            input_tensors[j]->data = shl_ref_f32_to_input_dtype(j, inputf[j], sess);
        }
        float time_all=0.0;
        for (int loop = 0; loop < option->loop_time; loop++) {
            start_time = shl_get_timespec();
            csinn_update_input_and_run(input_tensors, sess);
            end_time = shl_get_timespec();
            if(loop!=0)
            {
                time_all += ((float)(end_time-start_time))/1000000;
            }
            printf("Run graph execution time: %.5fms, FPS=%.5f\n", ((float)(end_time-start_time))/1000000,
                        1000000000.0/((float)(end_time-start_time)));

            snprintf(filename_prefix, FILE_PREFIX_LENGTH, "%s", basename(data_path[i * input_num]));
            postprocess(sess, filename_prefix);
        }
        if(option->loop_time>1)
        {
            printf("The number of run: %d\n", option->loop_time);
            printf("Run graph average execution time: %.5fms, FPS=%.5f\n", time_all/(option->loop_time-1), 1000.0*(option->loop_time-1)/time_all);
        }
        for (int j = 0; j < input_num; j++) {
            shl_mem_free(inputf[j]);
            shl_mem_free(input_tensors[j]->data);
        }
    }

    for (int j = 0; j < input_num; j++) {
        csinn_free_tensor(input_tensors[j]);
    }

    free(option);
    csinn_session_deinit(sess);
    csinn_free_session(sess);

    return 0;
}

