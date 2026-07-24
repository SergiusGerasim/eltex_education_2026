#pragma once
#include <stddef.h>

#define NAMESIZE 100
#define PHONESIZE 12
#define ADDRESSSIZE 100
#define WORKPLACE_SIZE 100
#define JOB_TITLE_SIZE 100
#define EMAIL_SIZE 100
#define SOCIAL_LINK_SIZE 200

#define MAX_PHONE_NUMBERS 5
#define MAX_EMAILS 5
#define MAX_SOCIAL_LINKS 5

typedef struct phone_book_node{
    unsigned int id;
    char name[NAMESIZE];
    char last_name[NAMESIZE];
    char patronymic[NAMESIZE];
    char home_address[ADDRESSSIZE];
    char workplace[WORKPLACE_SIZE];
    char job_title[JOB_TITLE_SIZE];

    char phone_numbers[MAX_PHONE_NUMBERS][PHONESIZE];
    size_t phone_count;

    char emails[MAX_EMAILS][EMAIL_SIZE];
    size_t email_count;

    char social_links[MAX_SOCIAL_LINKS][SOCIAL_LINK_SIZE];
    size_t social_link_count;

    struct phone_book_node *next;
} phone_book_node_t;

typedef struct phone_book_search_ans{
    phone_book_node_t **items;
    size_t count;
} phone_book_search_ans_t;

typedef struct phone_book_free_id_node{
    unsigned int id;
    struct phone_book_free_id_node *next;
} phone_book_free_id_node_t;

typedef struct phone_book_id_manager{
    unsigned int next_id;
    phone_book_free_id_node_t *free_ids;
} phone_book_id_manager_t;

void phone_book_id_manager_init(phone_book_id_manager_t *id_manager);

int phone_book_node_add_phone(phone_book_node_t *node, const char *phone_number);
int phone_book_node_add_email(phone_book_node_t *node, const char *email);
int phone_book_node_add_social_link(phone_book_node_t *node, const char *social_link);

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
);

phone_book_node_t *phone_book_find_by_id(phone_book_node_t *head, const unsigned int id);

int phone_book_edit_name(phone_book_node_t *head, unsigned int id, const char *name);
int phone_book_edit_last_name(phone_book_node_t *head, unsigned int id, const char *last_name);
int phone_book_edit_patronymic(phone_book_node_t *head, unsigned int id, const char *patronymic);
int phone_book_edit_home_address(phone_book_node_t *head, unsigned int id, const char *home_address);
int phone_book_edit_workplace(phone_book_node_t *head, unsigned int id, const char *workplace);
int phone_book_edit_job_title(phone_book_node_t *head, unsigned int id, const char *job_title);

int phone_book_edit_phone(phone_book_node_t *head, unsigned int id, size_t index, const char *phone_number);
int phone_book_edit_email(phone_book_node_t *head, unsigned int id, size_t index, const char *email);
int phone_book_edit_social_link(phone_book_node_t *head, unsigned int id, size_t index, const char *social_link);

phone_book_search_ans_t phone_book_find_by_name(phone_book_node_t *head, const char *name);

phone_book_search_ans_t phone_book_find_by_last_name(phone_book_node_t *head, const char *last_name);

phone_book_search_ans_t phone_book_find_by_name_and_last_name(phone_book_node_t *head, 
    const char *name, const char *last_name);

void phone_book_search_result_free(phone_book_search_ans_t *search_ans);

int phone_book_remove(phone_book_node_t **head, unsigned int id,
    phone_book_id_manager_t *id_manager);

void phone_book_clear(phone_book_node_t **head, phone_book_id_manager_t *id_manager);
