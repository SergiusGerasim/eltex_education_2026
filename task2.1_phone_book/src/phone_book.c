#include "phone_book.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int copy_string(char *destination, size_t destination_size, const char *source){
    if (destination == NULL || destination_size == 0 || source == NULL) {
        return -1;
    }

    if (strlen(source) >= destination_size) return -1;

    int written = snprintf(destination, destination_size, "%s", source);

    if (written < 0 || (size_t)written >= destination_size) {
        return -1;
    }

    return 0;
}

static phone_book_node_t *phone_book_node_create(
    unsigned int id,
    const char *name,
    const char *last_name,
    const char *patronymic,
    const char *phone_number,
    const char *home_address,
    const char *workplace,
    const char *job_title
){
    if (name == NULL || last_name == NULL) return NULL;

    phone_book_node_t *new_node = malloc(sizeof(*new_node));
    //не забыть free(node); :D
    if (new_node == NULL) return NULL;
    
    new_node->id = id;
    new_node->next = NULL;
    new_node->phone_count = 0;
    new_node->email_count = 0;
    new_node->social_link_count = 0;
    size_t i = 0;
    for(; name[i] != '\0' && i < sizeof(new_node->name) - 1; i++){
        new_node->name[i] = name[i];
    }
    new_node->name[i] = '\0';
    
    snprintf(new_node->last_name, sizeof(new_node->last_name), "%s", last_name);
    
    if (patronymic != NULL) snprintf(new_node->patronymic, sizeof(new_node->patronymic), "%s", patronymic);
    else new_node->patronymic[0] = '\0';
    
    if (home_address != NULL) snprintf(new_node->home_address, sizeof(new_node->home_address), "%s", home_address);
    else new_node->home_address[0] = '\0';

    if (workplace != NULL) snprintf(new_node->workplace, sizeof(new_node->workplace), "%s", workplace);
    else new_node->workplace[0] = '\0';

    if (job_title != NULL) snprintf(new_node->job_title, sizeof(new_node->job_title), "%s", job_title);
    else new_node->job_title[0] = '\0';

    if (phone_number != NULL && phone_book_node_add_phone(new_node, phone_number) != 0) {
        free(new_node);
        return NULL;
    }
    
    return new_node;
}

int phone_book_node_add_phone(phone_book_node_t *node, const char *phone_number){
    if (node == NULL || phone_number == NULL || node->phone_count >= MAX_PHONE_NUMBERS) {
        return -1;
    }

    if (copy_string(node->phone_numbers[node->phone_count], PHONESIZE, phone_number) != 0) {
        return -1;
    }

    node->phone_count++;
    return 0;
}

int phone_book_node_add_email(phone_book_node_t *node, const char *email){
    if (node == NULL || email == NULL || node->email_count >= MAX_EMAILS) {
        return -1;
    }

    if (copy_string(node->emails[node->email_count], EMAIL_SIZE, email) != 0) {
        return -1;
    }

    node->email_count++;
    return 0;
}

int phone_book_node_add_social_link(phone_book_node_t *node, const char *social_link){
    if (node == NULL || social_link == NULL || node->social_link_count >= MAX_SOCIAL_LINKS) {
        return -1;
    }

    if (copy_string(node->social_links[node->social_link_count], SOCIAL_LINK_SIZE, social_link) != 0) {
        return -1;
    }

    node->social_link_count++;
    return 0;
}


static int phone_book_add(phone_book_node_t **head, phone_book_node_t *new_node){
    if (head == NULL || new_node == NULL) return -1;

    new_node->next = NULL;

    if (*head == NULL){
        *head = new_node;
        return 0;
    }
    phone_book_node_t *temp_node = *head;
    while (temp_node->next != NULL) temp_node = temp_node->next;

    temp_node->next = new_node;
    return 0;
}

void phone_book_id_manager_init(phone_book_id_manager_t *id_manager){
    if (id_manager == NULL) return;

    id_manager->next_id = 1;
    id_manager->free_ids = NULL;
}

static int phone_book_take_id(phone_book_id_manager_t *id_manager, unsigned int *id){
    if (id_manager == NULL || id == NULL) return -1;

    if (id_manager->free_ids != NULL) {
        phone_book_free_id_node_t *free_id = id_manager->free_ids;
        *id = free_id->id;
        id_manager->free_ids = free_id->next;
        free(free_id);
        return 0;
    }

    if (id_manager->next_id == 0) return -1;

    *id = id_manager->next_id;
    id_manager->next_id++;
    return 0;
}

static int phone_book_release_id(phone_book_id_manager_t *id_manager, unsigned int id){
    if (id_manager == NULL || id == 0) return -1;

    phone_book_free_id_node_t *free_id = malloc(sizeof(*free_id));
    if (free_id == NULL) return -1;

    free_id->id = id;
    free_id->next = id_manager->free_ids;
    id_manager->free_ids = free_id;
    return 0;
}

int phone_book_node_create_and_add(
    const char *name,
    const char *last_name,
    const char *patronymic,
    const char *phone_number,
    const char *home_address,
    const char *workplace,
    const char *job_title,
    phone_book_node_t **head,
    phone_book_id_manager_t *id_manager
){
    if (head == NULL || id_manager == NULL) return -1;

    phone_book_node_t *new_node = phone_book_node_create(
        0,
        name,
        last_name,
        patronymic,
        phone_number,
        home_address,
        workplace,
        job_title
    );

    if (new_node == NULL) return -1;

    if (phone_book_take_id(id_manager, &new_node->id) != 0) {
        free(new_node);
        return -1;
    }

    if (phone_book_add(head, new_node) != 0) {
        phone_book_release_id(id_manager, new_node->id);
        free(new_node);
        return -1;
    }

    return 0;
}

phone_book_node_t *phone_book_find_by_id(phone_book_node_t *head, const unsigned int id){
    while (head != NULL){
        if (head->id == id) return head;
        head = head->next;
    }
    return NULL;
}

static int phone_book_edit_field(char *field, size_t field_size, const char *new_value){
    return copy_string(field, field_size, new_value);
}

int phone_book_edit_name(phone_book_node_t *head, unsigned int id, const char *name){
    phone_book_node_t *node = phone_book_find_by_id(head, id);
    if (node == NULL) return -1;

    return phone_book_edit_field(node->name, sizeof(node->name), name);
}

int phone_book_edit_last_name(phone_book_node_t *head, unsigned int id, const char *last_name){
    phone_book_node_t *node = phone_book_find_by_id(head, id);
    if (node == NULL) return -1;

    return phone_book_edit_field(node->last_name, sizeof(node->last_name), last_name);
}

int phone_book_edit_patronymic(phone_book_node_t *head, unsigned int id, const char *patronymic){
    phone_book_node_t *node = phone_book_find_by_id(head, id);
    if (node == NULL) return -1;

    return phone_book_edit_field(node->patronymic, sizeof(node->patronymic), patronymic);
}

int phone_book_edit_home_address(phone_book_node_t *head, unsigned int id, const char *home_address){
    phone_book_node_t *node = phone_book_find_by_id(head, id);
    if (node == NULL) return -1;

    return phone_book_edit_field(node->home_address, sizeof(node->home_address), home_address);
}

int phone_book_edit_workplace(phone_book_node_t *head, unsigned int id, const char *workplace){
    phone_book_node_t *node = phone_book_find_by_id(head, id);
    if (node == NULL) return -1;

    return phone_book_edit_field(node->workplace, sizeof(node->workplace), workplace);
}

int phone_book_edit_job_title(phone_book_node_t *head, unsigned int id, const char *job_title){
    phone_book_node_t *node = phone_book_find_by_id(head, id);
    if (node == NULL) return -1;

    return phone_book_edit_field(node->job_title, sizeof(node->job_title), job_title);
}

int phone_book_edit_phone(phone_book_node_t *head, unsigned int id, size_t index,
    const char *phone_number){
    phone_book_node_t *node = phone_book_find_by_id(head, id);
    if (node == NULL || index >= node->phone_count) return -1;

    return copy_string(node->phone_numbers[index], PHONESIZE, phone_number);
}

int phone_book_edit_email(phone_book_node_t *head, unsigned int id, size_t index,
    const char *email){
    phone_book_node_t *node = phone_book_find_by_id(head, id);
    if (node == NULL || index >= node->email_count) return -1;

    return copy_string(node->emails[index], EMAIL_SIZE, email);
}

int phone_book_edit_social_link(phone_book_node_t *head, unsigned int id, size_t index,
    const char *social_link){
    phone_book_node_t *node = phone_book_find_by_id(head, id);
    if (node == NULL || index >= node->social_link_count) return -1;

    return copy_string(node->social_links[index], SOCIAL_LINK_SIZE, social_link);
}

void phone_book_clear(phone_book_node_t **head, phone_book_id_manager_t *id_manager){
    if (head != NULL) {
        while (*head != NULL) {
            phone_book_node_t *node_to_delete = *head;
            *head = (*head)->next;
            free(node_to_delete);
        }
    }

    if (id_manager != NULL) {
        while (id_manager->free_ids != NULL) {
            phone_book_free_id_node_t *id_to_delete = id_manager->free_ids;
            id_manager->free_ids = id_manager->free_ids->next;
            free(id_to_delete);
        }

        id_manager->next_id = 1;
    }
}

int phone_book_remove(phone_book_node_t **head, unsigned int id,
    phone_book_id_manager_t *id_manager){
    if (head == NULL || id_manager == NULL) return -1;
    
    phone_book_node_t *previous = NULL;
    phone_book_node_t *current = *head;
    
    while (current != NULL){
        if (current->id == id){
            if (phone_book_release_id(id_manager, id) != 0) return -1;

            if (previous != NULL){
                previous->next = current->next;
            }
            else{
                *head = current->next;
            }
            
            free(current);
            return 0;
        }
        previous = current;
        current = current->next;
    }
    return -1;

}

static phone_book_search_ans_t phone_book_find(phone_book_node_t *head, const char *name, 
    const char *last_name){
    phone_book_search_ans_t ans = {.items = NULL, .count = 0};

    if (name == NULL && last_name == NULL) {
        return ans;
    }

    while (head != NULL) {
        int name_matches = name == NULL || strcmp(head->name, name) == 0;

        int last_name_matches = last_name == NULL || strcmp(head->last_name, last_name) == 0;

        if (name_matches && last_name_matches) {
            phone_book_node_t **new_items = realloc(ans.items, (ans.count + 1) * sizeof(*ans.items));

            if (new_items == NULL) {
                free(ans.items);
                ans.items = NULL;
                ans.count = 0;
                return ans;
            }

            ans.items = new_items;
            ans.items[ans.count] = head;
            ans.count++;
        }

        head = head->next;
    }

    return ans;
}

phone_book_search_ans_t phone_book_find_by_name(phone_book_node_t *head, const char *name){
    return phone_book_find(head, name, NULL);
}

phone_book_search_ans_t phone_book_find_by_last_name(phone_book_node_t *head, const char *last_name){
    return phone_book_find(head, NULL, last_name);
}

phone_book_search_ans_t phone_book_find_by_name_and_last_name(phone_book_node_t *head,
    const char *name, const char *last_name){
    return phone_book_find(head, name, last_name);
}

void phone_book_search_result_free(phone_book_search_ans_t *search_ans){
    if (search_ans == NULL) {
        return;
    }
    free(search_ans->items);
    //указатель ссылается на память, которая уже освобожденна, поэтому надо явно 
    // ему сказать, что обращаться к этой памяти уже нельзя.
    search_ans->items = NULL;
    search_ans->count = 0;
}
