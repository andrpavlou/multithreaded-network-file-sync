#include "sync_info.h"



pthread_mutex_t sync_info_mutex = PTHREAD_MUTEX_INITIALIZER;


sync_info_mem_store *add_sync_info(sync_info_mem_store** head, const char* source, const char* target){
    pthread_mutex_lock(&sync_info_mutex);

    sync_info_mem_store* new_node = malloc(sizeof(sync_info_mem_store));
    if(new_node == NULL){
        pthread_mutex_unlock(&sync_info_mutex);
        return NULL;
    } 


    strncpy(new_node->source, source, sizeof(new_node->source) - 1);
    new_node->source[sizeof(new_node->source) - 1] = '\0';


    // Only "ADD" will be inserted, so no need to check if target is null
    strncpy(new_node->target, target, sizeof(new_node->target) - 1);
    new_node->target[sizeof(new_node->target) - 1] = '\0';
    

    new_node->active        = 0;
    new_node->error_count   = 0;
    new_node->last_sync     = 0;
    new_node->worker_pid    = -1;
    new_node->last_sync     = 0;
    // new_node->watch_fd      = -1;
    new_node->next          = NULL;

    if(*head == NULL){
        (*head) = new_node;
        pthread_mutex_unlock(&sync_info_mutex);
        return new_node;
    }

    sync_info_mem_store *last = *head;
    while(last->next != NULL){
        last = last->next;
    }
    last->next = new_node;

    pthread_mutex_unlock(&sync_info_mutex);

    return new_node;
}


sync_info_mem_store* find_sync_info(sync_info_mem_store* head, const char* source){
    pthread_mutex_lock(&sync_info_mutex);

    if(head == NULL){
        pthread_mutex_unlock(&sync_info_mutex);
        return NULL;
    }
    sync_info_mem_store *found = head;
    
    while(found != NULL && strcmp(found->source, source)){
        found = found->next;
    }

    pthread_mutex_unlock(&sync_info_mutex);
    return found;
}




char** find_sync_info_hosts_by_dir(sync_info_mem_store *head, const char* dir, int *dir_count){
    pthread_mutex_lock(&sync_info_mutex);

    if(head == NULL){
        pthread_mutex_unlock(&sync_info_mutex);
        return NULL;
    }
    sync_info_mem_store *found = head;
    
    int count = 0;
    char **found_hosts = NULL;
    while(found != NULL){
        if(strstr(found->source, dir) != NULL){
            found_hosts         = realloc(found_hosts, (count + 1) * sizeof(char*));
            found_hosts[count]  = strdup(found->source);

            count++;
        }

        found = found->next;
    }

    *dir_count = count;
    pthread_mutex_unlock(&sync_info_mutex);
    
    return found_hosts;
}



int remove_sync_info(sync_info_mem_store** head, const char* source){
    pthread_mutex_lock(&sync_info_mutex);

    sync_info_mem_store* prev = NULL;
    sync_info_mem_store* curr = *head;

    while(curr != NULL && strcmp(curr->source, source)){
        prev = curr;
        curr = curr->next;
    }

    if(curr == NULL){
        pthread_mutex_unlock(&sync_info_mutex);
        return 1;
    }

    if(prev == NULL){
        *head = curr->next;
    } else {
        prev->next = curr->next;
    }
    
    free(curr);

    pthread_mutex_unlock(&sync_info_mutex);
    return 0;
}




void free_all_sync_info(sync_info_mem_store** head){
    pthread_mutex_lock(&sync_info_mutex);

    if(head == NULL || *head == NULL) return;

    sync_info_mem_store* curr = *head;
    sync_info_mem_store* next;

    while(curr != NULL){
        next = curr->next;
        free(curr);
        curr = next;
    }

    *head = NULL;
    pthread_mutex_unlock(&sync_info_mutex);

}



