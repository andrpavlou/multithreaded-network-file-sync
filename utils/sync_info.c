#include "sync_info.h"

void print_all_sync_info(sync_info_mem_store* head){
    if (head == NULL) {
        printf("No monitored directories.\n");
        return;
    }

    sync_info_mem_store* curr = head;
    while(curr != NULL){
        printf("Source: %s\t-> Target:%s\tWFD%d\n", curr->source, curr->target, curr->watch_fd);
        curr = curr->next;
    }
}


int add_sync_info(sync_info_mem_store** head, const char* source, const char* target){
    sync_info_mem_store* new_node = malloc(sizeof(sync_info_mem_store));
    if(new_node == NULL) return 1;

    strncpy(new_node->source, source, BUFFSIZ);
    new_node->source[BUFFSIZ - 1] = '\0';
    
    strncpy(new_node->target, target, BUFFSIZ);
    new_node->target[BUFFSIZ - 1] = '\0';


    // zero for now, might need to be changed.
    new_node->active        = 0;
    new_node->error_count   = 0;
    new_node->last_sync     = 0;
    new_node->worker_pid    = -1;
    new_node->last_sync     = 0;
    new_node->watch_fd      = -1;
    new_node->next          = NULL;

    if(*head == NULL){
        (*head) = new_node;
        return 0;
    }

    sync_info_mem_store *last = *head;
    while(last->next != NULL){
        last = last->next;
    }
    last->next = new_node;

    return 0;
}


sync_info_mem_store* find_sync_info(sync_info_mem_store* head, const char* source){
    if(head == NULL) return NULL;
    sync_info_mem_store *found = head;
    
    while(found != NULL && strcmp(found->source, source)){
        found = found->next;
    }

    return found;
}



sync_info_mem_store* find_by_watch_fd(sync_info_mem_store* head, int wd) {
    if(head == NULL) return NULL;
    sync_info_mem_store *found = head;

    while(found != NULL && found->watch_fd != wd){
        found = found->next;
    }
    return found;
}

sync_info_mem_store* find_by_pid(sync_info_mem_store *head, pid_t pid){
    if(head == NULL) return NULL;
    sync_info_mem_store *found = head;

    while(found != NULL && found->worker_pid != pid){
        found = found->next;
    }

    return found;
}


int remove_sync_info(sync_info_mem_store** head, const char* source){
    sync_info_mem_store* prev = NULL;
    sync_info_mem_store* curr = *head;

    while(curr != NULL && strcmp(curr->source, source)){
        prev = curr;
        curr = curr->next;
    }

    if(curr == NULL) return 1;

    if(prev == NULL){
        *head = curr->next;
    } else {
        prev->next = curr->next;
    }
    

    free(curr);
    return 0;
}



void free_all_sync_info(sync_info_mem_store** head){
    if(head == NULL || *head == NULL) return;

    sync_info_mem_store* curr = *head;
    sync_info_mem_store* next;

    while(curr != NULL){
        next = curr->next;
        free(curr);
        curr = next;
    }

    *head = NULL;
}



