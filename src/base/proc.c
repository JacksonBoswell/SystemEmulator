/**************************************************************************
 * STUDENTS: DO NOT MODIFY.
 * 
 * C S 429 system emulator
 * 
 * proc.c - The top-level instruction processing loop of the processor.
 * 
 * Copyright (c) 2022, 2023. 
 * Authors: S. Chatterjee, Z. Leeper. 
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 

#include "archsim.h"
#include "hw_elts.h"
#include "hazard_control.h"
#include<arpa/inet.h>
#include<sys/socket.h>
#include<unistd.h>

extern uint32_t bitfield_u32(int32_t src, unsigned frompos, unsigned width);
extern int64_t bitfield_s64(int32_t src, unsigned frompos, unsigned width);

extern machine_t guest;
extern uint64_t F_PC;
extern mem_status_t dmem_status;

#define VIS_PORT 8888

int runElf(const uint64_t entry) {
    logging(LOG_INFO, "Running ELF executable");
    guest.proc->PC = entry;
    guest.proc->SP = guest.mem->seg_start_addr[STACK_SEG]-8;
    guest.proc->NZCV = PACK_CC(0, 1, 0, 0);
    guest.proc->GPR[30] = RET_FROM_MAIN_ADDR;

    pipe_reg_t **pipes[] = {&F_instr, &D_instr, &X_instr, &M_instr, &W_instr};

    uint64_t sizes[5] = {sizeof(f_instr_impl_t), sizeof(d_instr_impl_t), sizeof(x_instr_impl_t),
                         sizeof(m_instr_impl_t), sizeof(w_instr_impl_t)};
    for (int i = 0; i < 5; i++) {
        *pipes[i] = (pipe_reg_t *)calloc(1, sizeof(pipe_reg_t));
        (*pipes[i])->size = sizes[i];
        (*pipes[i])->in = (pipe_reg_implt_t) calloc(1, sizes[i]);
        (*pipes[i])->out = (pipe_reg_implt_t) calloc(1, sizes[i]);
        (*pipes[i])->ctl = P_BUBBLE;
    }

    /* Will be selected as the first PC */
    F_out->pred_PC = guest.proc->PC;
    F_out->status = STAT_AOK;
    dmem_status = READY;

#ifdef DEBUG
    printf("\n%s%s   Addr      Instr       Op  \tCond\tDest\tSrc1\tSrc2\tImmval   \t\tShift%s\n", 
           ANSI_BOLD, ANSI_COLOR_RED, ANSI_RESET);
#endif
    num_instr = 0;

    // Socket Initialization
    int socket_desc, client_sock, c, read_size, reuse = 1;
    struct sockaddr_in server, client;
    char client_message[2000]; // Buffer for incoming messages

    if (frontend_vis > 0) {
        // Create socket
        socket_desc = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_desc == -1) {
            printf("Could not create socket\n");
        }
        puts("Socket created");

        // Set socket to reuse address
        if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
            perror("setsockopt(SO_REUSEADDR) failed");

        // Prepare the sockaddr_in structure
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = INADDR_ANY;
        server.sin_port = htons(VIS_PORT);
        
        int opt = 1;
        // SOL_SOCKET to manipulate options at the socket API level; SO_REUSEADDR to reuse local addresses
        if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }

        // Bind
        if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
            perror("bind failed. Error");
            return 1;
        }
        puts("bind done");

        // Listen
        listen(socket_desc, 3);
        puts("Waiting for incoming connections...");

        c = sizeof(struct sockaddr_in);
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0) {
            perror("accept failed");
            return 1;
        }
        puts("Connection accepted");
        // End of socket initialization

    }

    do {        
        /* Run each stage (in reverse order, to get the correct effect) */
        /* TODO: rewrite as independent threads */
        wback_instr(W_out);
        memory_instr(M_out, W_in);
        execute_instr(X_out, M_in);   
        decode_instr(D_out, X_in);   
        fetch_instr(F_out, D_in);

        F_in->pred_PC = F_PC;

        /* Set machine state to either continue executing or shutdown */
        guest.proc->status = W_out->status;

        /* Check for hazards and appropriately stall/bubble stages */
        uint8_t D_src1 = (D_out->op == OP_MOVZ) ? 0x1F : bitfield_u32(D_out->insnbits, 5, 5);
        uint8_t D_src2 = (D_out->op != OP_STUR) ? bitfield_u32(D_out->insnbits, 16, 5) : bitfield_u32(D_out->insnbits, 0, 5);
        uint8_t X_dst = X_out->W_sigs.dst_sel ? 30 : X_out->dst;

        uint64_t D_val_a = X_in->val_a;

        /* Hazard handling and pipeline control */
        handle_hazards(D_out->op, D_src1, D_src2, D_val_a, X_out->op, X_dst, M_in->cond_holds);

        /* Print debug output */
        if(debug_level > 0)
            printf("\nPipeline state at end of cycle %ld:\n", num_instr);
        // Replace these five commands with visualizations.

        char* fetch_message = NULL;
        char* decode_message = NULL;
        char* execute_message = NULL;
        char* memory_message = NULL;
        char* writeback_message = NULL;

        show_instr(S_FETCH, debug_level, &fetch_message);
        show_instr(S_DECODE, debug_level, &decode_message);
        show_instr(S_EXECUTE, debug_level, &execute_message);
        show_instr(S_MEMORY, debug_level, &memory_message);
        show_instr(S_WBACK, debug_level, &writeback_message);

        // printf("Fetch: %s\n", fetch_message);
        // printf("Decode: %s\n", decode_message);
        // printf("Execute: %s\n", execute_message);
        // printf("Memory: %s\n", memory_message);
        // printf("Writeback: %s\n", writeback_message);


        if (frontend_vis) {
            size_t bufferSize = 1000;
            char* json_message = malloc(bufferSize);

            snprintf(json_message, bufferSize, "{\"fetch\": %s, \"decode\": %s, \"execute\": %s, \"memory\": %s, \"writeback\": %s}", fetch_message, decode_message, execute_message, memory_message, writeback_message);
            // printf("JSON: %s\n", json_message);

            write(client_sock, json_message, strlen(json_message));
            sleep(0.1);
            char client_message[2000]; // Buffer for incoming messages
            int read_size;
            while ((read_size = recv(client_sock, client_message, 2000, 0)) > 0) {
                client_message[read_size] = '\0';
                char *found = strstr(client_message, "\"key\":\"");
                if (found) {
                    found += strlen("\"key\":\""); // Move pointer to the value
                    char *end = strchr(found, '"'); // Find the end of the value
                    if (end) {
                        *end = '\0'; // Temporarily terminate the string
                        // printf("Key pressed: %s\n", found);
                        if (strcmp(found, "ArrowRight") == 0 || strcmp(found, "ArrowDown") == 0 || strcmp(found, "exit") == 0)
                        {
                            memset(client_message, 0, 2000);
                            break;
                        }
                        *end = '"'; // Restore the '"' character
                    }
                }
                memset(client_message, 0, 2000);

            }
            free(json_message);

        }


        free(fetch_message);
        free(decode_message);
        free(execute_message);
        free(memory_message);
        free(writeback_message);

        if(debug_level > 0)
            printf("\n\n");

        guest.proc->PC = F_PC;

        for (int i = 0; i < 5; i++) {
            pipe_reg_t *pipe = *pipes[i];
            switch(pipe->ctl) {
                case P_LOAD: // Normal, cycle stage
                    memcpy(pipe->out.generic, pipe->in.generic, pipe->size);
                    break;
                case P_ERROR:  // Error, bubble this stage
                    guest.proc->status = STAT_HLT;
                case P_BUBBLE: // Hazard, needs to bubble
                    memset(pipe->out.generic, 0, pipe->size);
                    break;
                case P_STALL: // Hazard, needs to stall
                    break;
            }
        }

        num_instr++;

        // rendering function (interrupt if viisualization)

    } while ((guest.proc->status == STAT_AOK || guest.proc->status == STAT_BUB)
             && num_instr < cycle_max);

    if (frontend_vis) {
        // Close the socket
        if (read_size == 0) {
            puts("Client disconnected");
        } else if (read_size == -1) {
            perror("recv failed");
        }

        close(socket_desc);
        close(client_sock);

    }

    return EXIT_SUCCESS;
}