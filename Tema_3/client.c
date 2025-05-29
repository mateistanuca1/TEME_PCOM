#include <arpa/inet.h>
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <unistd.h>     /* read, write, close */

#include "helpers.h"
#include "parson.h"
#include "requests.h"

// admin username: matei.stanuca
// password: 3367b05c8af3

int main(int argc, char *argv[]) {
    char *message;
    char *response;
    char *cookies;
    char *acces_token;
    int cookies_exist = 0;
    int acces_token_exist = 0;
    char *admin_data[2];
    char *command = malloc(100);
    char *host = "63.32.125.183";
    int port = 8081;
    int sockfd;
    while (1) {
        fgets(command, 100, stdin);
        command[strcspn(command, "\n")] = '\0';
        if (strcmp(command, "login_admin") == 0) {
            if (cookies_exist) {
                printf("ERROR: Already logged in as another user.\n");
                continue;
            }
            admin_data[0] = malloc(100);
            admin_data[1] = malloc(100);
            printf("username=");
            fgets(admin_data[0], 100, stdin);
            admin_data[0][strcspn(admin_data[0], "\n")] = '\0';
            printf("password=");
            fgets(admin_data[1], 100, stdin);
            admin_data[1][strcspn(admin_data[1], "\n")] = '\0';
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "username", admin_data[0]);
            json_object_set_string(root_object, "password", admin_data[1]);
            char *json_string = json_serialize_to_string_pretty(root_value);
            message = compute_post_request(host, "/api/v1/tema/admin/login",
                                           "application/json", &json_string, 1,
                                           &cookies, cookies_exist, acces_token,
                                           acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            char *cookie = strstr(response, "Set-Cookie: ");
            if (cookie && cookies_exist == 0) {
                cookie += strlen("Set-Cookie: ");
                char *end = strstr(cookie, ";");
                if (end) {
                    *end = '\0';
                }
                cookies_exist = 1;
                cookies = malloc(strlen(cookie) + 1);
                strcpy(cookies, cookie);
            }
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Login successful for admin %s.\n",
                       admin_data[0]);
            } else {
                if (strstr(response, "Already logged in")) {
                    printf("ERROR: Already logged in as admin %s.\n",
                           admin_data[0]);
                } else {
                    printf("ERROR: Login failed for admin %s.\n",
                           admin_data[0]);
                }
            }
            json_free_serialized_string(json_string);
            json_value_free(root_value);
            free(message);
            free(response);
            free(admin_data[0]);
            free(admin_data[1]);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "logout_admin") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(host, "/api/v1/tema/admin/logout",
                                          NULL, &cookies, cookies_exist,
                                          acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Logout successful for admin.\n");
                free(cookies);
                cookies_exist = 0;
            } else {
                printf("ERROR: Logout failed for admin.\n");
            }
            free(message);
            free(response);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "add_user") == 0) {
            char *username = malloc(100);
            char *password = malloc(100);
            printf("username=");
            fgets(username, 100, stdin);
            username[strcspn(username, "\n")] = '\0';
            printf("password=");
            fgets(password, 100, stdin);
            password[strcspn(password, "\n")] = '\0';
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "username", username);
            json_object_set_string(root_object, "password", password);
            char *json_string = json_serialize_to_string_pretty(root_value);
            message = compute_post_request(host, "/api/v1/tema/admin/users",
                                           "application/json", &json_string, 1,
                                           &cookies, cookies_exist, acces_token,
                                           acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "201 CREATED")) {
                printf("SUCCES: User %s added successfully.\n", username);
            } else {
                if (strstr(response, "User already exists")) {
                    printf("ERROR: User %s already exists.\n", username);
                } else if (strstr(response, "Admin privileges required")) {
                    printf("ERROR: Admin privileges required.\n");
                } else {
                    printf("ERROR: Failed to add user %s.\n", username);
                }
            }
            json_free_serialized_string(json_string);
            json_value_free(root_value);
            free(message);
            free(response);
            free(username);
            free(password);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "get_users") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(host, "/api/v1/tema/admin/users",
                                          NULL, &cookies, cookies_exist,
                                          acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Users retrieved successfully.\n");
                char *json_response = basic_extract_json_response(response);
                JSON_Value *root_value = json_parse_string(json_response);
                JSON_Object *root_object = json_value_get_object(root_value);
                JSON_Array *array = json_object_get_array(root_object, "users");
                size_t count = json_array_get_count(array);
                for (size_t i = 0; i < count; i++) {
                    JSON_Object *user_object = json_array_get_object(array, i);
                    const char *username =
                        json_object_get_string(user_object, "username");
                    const char *password =
                        json_object_get_string(user_object, "password");
                    int user_id =
                        (int)json_object_get_number(user_object, "id");
                    printf("#%d: %s:%s\n", user_id, username, password);
                }
                json_value_free(root_value);
            } else {
                if (strstr(response, "Admin privileges required")) {
                    printf("ERROR: Admin privileges required.\n");
                } else {
                    printf("ERROR: Failed to retrieve users.\n");
                }
            }
            free(message);
            free(response);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "delete_user") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            printf("username=");
            char *username = malloc(100);
            fgets(username, 100, stdin);
            username[strcspn(username, "\n")] = '\0';
            message = compute_delete_request(host, "/api/v1/tema/admin/users",
                                             username, &cookies, cookies_exist,
                                             acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: User %s deleted successfully.\n", username);
            } else {
                if (strstr(response, "403 FORBIDDEN")) {
                    printf("ERROR: Admin privileges required.\n");
                } else {
                    printf("ERROR: User %s not found.\n", username);
                }
            }
            free(message);
            free(response);
            free(username);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "login") == 0) {
            if (cookies_exist) {
                printf("ERROR: Already logged in as another user.\n");
                continue;
            }
            char *admin_username = malloc(100);
            char *username = malloc(100);
            char *password = malloc(100);
            printf("admin_username=");
            fgets(admin_username, 100, stdin);
            admin_username[strcspn(admin_username, "\n")] = '\0';
            printf("username=");
            fgets(username, 100, stdin);
            username[strcspn(username, "\n")] = '\0';
            printf("password=");
            fgets(password, 100, stdin);
            password[strcspn(password, "\n")] = '\0';
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "admin_username",
                                   admin_username);
            json_object_set_string(root_object, "username", username);
            json_object_set_string(root_object, "password", password);
            char *json_string = json_serialize_to_string_pretty(root_value);
            message = compute_post_request(host, "/api/v1/tema/user/login",
                                           "application/json", &json_string, 1,
                                           &cookies, cookies_exist, acces_token,
                                           acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Login successful for user %s.\n", username);
                char *cookie = strstr(response, "Set-Cookie: ");
                if (cookie && cookies_exist == 0) {
                    cookie += strlen("Set-Cookie: ");
                    char *end = strstr(cookie, ";");
                    if (end) {
                        *end = '\0';
                    }
                    cookies_exist = 1;
                    cookies = malloc(strlen(cookie) + 1);
                    strcpy(cookies, cookie);
                }
            } else {
                if (strstr(response, "User already logged in")) {
                    printf("ERROR: User %s already logged in.\n", username);
                } else {
                    printf("ERROR: Login failed for user %s.\n", username);
                }
            }
            json_free_serialized_string(json_string);
            json_value_free(root_value);
            free(message);
            free(response);
            free(admin_username);
            free(username);
            free(password);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "get_access") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(host, "/api/v1/tema/library/access",
                                          NULL, &cookies, cookies_exist,
                                          acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Access granted.\n");
                char *json_response = basic_extract_json_response(response);
                JSON_Value *root_value = json_parse_string(json_response);
                JSON_Object *root_object = json_value_get_object(root_value);
                const char *access =
                    json_object_get_string(root_object, "token");
                acces_token = malloc(strlen(access) + 1);
                acces_token_exist = 1;
                strcpy(acces_token, access);

                json_value_free(root_value);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf("ERROR: Failed to retrieve access.\n");
                }
            }
            free(message);
            free(response);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "get_movies") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(host, "/api/v1/tema/library/movies",
                                          NULL, &cookies, cookies_exist,
                                          acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Movies retrieved successfully.\n");
                char *json_response = basic_extract_json_response(response);
                JSON_Value *root_value = json_parse_string(json_response);
                JSON_Object *root_object = json_value_get_object(root_value);
                JSON_Array *array =
                    json_object_get_array(root_object, "movies");
                size_t count = json_array_get_count(array);
                for (size_t i = 0; i < count; i++) {
                    JSON_Object *movie_object = json_array_get_object(array, i);
                    const char *title =
                        json_object_get_string(movie_object, "title");
                    int id = (int)json_object_get_number(movie_object, "id");
                    printf("#%d: %s\n", id, title);
                }
                json_value_free(root_value);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf("ERROR: Failed to retrieve movies.\n");
                }
            }
            free(message);
            free(response);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "add_movie") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            int year;
            char *title = malloc(100);
            char *description = malloc(100);
            float rating;
            printf("title=");
            fgets(title, 100, stdin);
            title[strcspn(title, "\n")] = '\0';
            char buffer[100];
            printf("year=");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0';
            char *endptr_year;
            year = strtol(buffer, &endptr_year, 10);
            printf("description=");
            fgets(description, 100, stdin);
            description[strcspn(description, "\n")] = '\0';
            printf("rating=");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0';
            rating = atof(buffer);
            if (endptr_year == buffer || *endptr_year != '\0') {
                printf("ERROR: Invalid year format.\n");
                free(title);
                free(description);
                close_connection(sockfd);
                continue;
            }
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "title", title);
            json_object_set_number(root_object, "year", year);
            json_object_set_string(root_object, "description", description);
            json_object_set_number(root_object, "rating", rating);
            char *json_string = json_serialize_to_string_pretty(root_value);
            message = compute_post_request(host, "/api/v1/tema/library/movies",
                                           "application/json", &json_string, 1,
                                           &cookies, cookies_exist, acces_token,
                                           acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "201 CREATED")) {
                printf("SUCCES: Movie %s added successfully.\n", title);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf("ERROR: Failed to add movie %s.\n", title);
                }
            }
            json_free_serialized_string(json_string);
            json_value_free(root_value);
            free(message);
            free(response);
            free(title);
            free(description);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "get_movie") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            int id;
            printf("id=");
            scanf("%d", &id);
            char *query_params = malloc(100);
            sprintf(query_params, "%d", id);
            message = compute_get_request(host, "/api/v1/tema/library/movies",
                                          query_params, &cookies, cookies_exist,
                                          acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                char *json_response = basic_extract_json_response(response);
                JSON_Value *root_value = json_parse_string(json_response);
                JSON_Object *root_object = json_value_get_object(root_value);
                id = (int)json_object_get_number(root_object, "id");
                const char *title =
                    json_object_get_string(root_object, "title");
                int year = (int)json_object_get_number(root_object, "year");
                const char *description =
                    json_object_get_string(root_object, "description");
                const char *rating =
                    json_object_get_string(root_object, "rating");
                printf(
                    "{\n"
                    "  \"id\": %d,\n"
                    "  \"title\": \"%s\",\n"
                    "  \"year\": %d,\n"
                    "  \"description\": \"%s\",\n"
                    "  \"rating\": \"%s\"\n"
                    "}\n",
                    id, title, year, description, rating);
                json_value_free(root_value);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf("ERROR: Failed to retrieve movie with id %d.\n", id);
                }
            }
            free(message);
            free(response);
            free(query_params);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "delete_movie") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            int id;
            printf("id=");
            scanf("%d", &id);
            char *query_params = malloc(100);
            sprintf(query_params, "%d", id);
            message = compute_delete_request(
                host, "/api/v1/tema/library/movies", query_params, &cookies,
                cookies_exist, acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Movie with id %d deleted successfully.\n", id);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf("ERROR: Failed to delete movie with id %d.\n", id);
                }
            }
            free(message);
            free(response);
            free(query_params);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "update_movie") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            int id;
            printf("id=");
            scanf("%d", &id);
            char *query_params = malloc(100);
            sprintf(query_params, "%d", id);
            char *title = malloc(100);
            char *description = malloc(100);
            double rating;
            int year;
            while (getchar() != '\n');
            printf("title=");
            fgets(title, 100, stdin);
            title[strcspn(title, "\n")] = '\0';
            char buffer[100];
            printf("year=");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0';
            char *endptr_year;
            year = strtol(buffer, &endptr_year, 10);
            printf("description=");
            fgets(description, 100, stdin);
            description[strcspn(description, "\n")] = '\0';
            printf("rating=");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0';
            char *endptr_rating;
            rating = strtod(buffer, &endptr_rating);
            printf("rating=%lf\n", rating);
            if (endptr_rating == buffer || *endptr_rating != '\0') {
                printf("ERROR: Invalid rating format.\n");
                free(title);
                free(description);
                close_connection(sockfd);
                continue;
            }
            if (endptr_year == buffer || *endptr_year != '\0') {
                printf("ERROR: Invalid year format.\n");
                free(title);
                free(description);
                close_connection(sockfd);
                continue;
            }
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            json_object_set_string(root_object, "title", title);
            json_object_set_number(root_object, "year", year);
            json_object_set_string(root_object, "description", description);
            json_object_set_number(root_object, "rating", rating);
            char *json_string = json_serialize_to_string_pretty(root_value);
            message = compute_put_request(
                host, "/api/v1/tema/library/movies", query_params,
                "application/json", &json_string, 1, &cookies, cookies_exist,
                acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Movie with id %d updated successfully.\n", id);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf("ERROR: Failed to update movie with id %d.\n", id);
                }
            }
            json_free_serialized_string(json_string);
            json_value_free(root_value);
            free(message);
            free(response);
            free(title);
            free(description);
            free(query_params);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "logout") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(host, "/api/v1/tema/user/logout",
                                          NULL, &cookies, cookies_exist,
                                          acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Logout successful for user.\n");
                free(cookies);
                if (acces_token_exist) free(acces_token);
                acces_token_exist = 0;
                cookies_exist = 0;
            } else {
                printf("ERROR: Logout failed for user.\n");
            }
            free(message);
            free(response);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "get_collections") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            message = compute_get_request(
                host, "/api/v1/tema/library/collections", NULL, &cookies,
                cookies_exist, acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Collections retrieved successfully.\n");
                char *json_response = basic_extract_json_response(response);
                JSON_Value *root_value = json_parse_string(json_response);
                JSON_Object *root_object = json_value_get_object(root_value);
                JSON_Array *array =
                    json_object_get_array(root_object, "collections");
                size_t count = json_array_get_count(array);
                for (size_t i = 0; i < count; i++) {
                    JSON_Object *collection_object =
                        json_array_get_object(array, i);
                    const char *name =
                        json_object_get_string(collection_object, "title");
                    int id =
                        (int)json_object_get_number(collection_object, "id");
                    printf("#%d: %s\n", id, name);
                }
                json_value_free(root_value);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf("ERROR: Failed to retrieve collections.\n");
                }
            }
            free(message);
            free(response);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "get_collection") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            int id;
            printf("id=");
            scanf("%d", &id);
            char *query_params = malloc(100);
            sprintf(query_params, "%d", id);
            message = compute_get_request(
                host, "/api/v1/tema/library/collections", query_params,
                &cookies, cookies_exist, acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Collection retrieved successfully.\n");
                char *json_response = basic_extract_json_response(response);
                JSON_Value *root_value = json_parse_string(json_response);
                JSON_Object *root_object = json_value_get_object(root_value);
                const char *name = json_object_get_string(root_object, "title");
                const char *owner =
                    json_object_get_string(root_object, "owner");
                JSON_Array *movies =
                    json_object_get_array(root_object, "movies");
                size_t count = json_array_get_count(movies);
                printf("title: %s\nowner: %s\n", name, owner);
                for (size_t i = 0; i < count; i++) {
                    JSON_Object *movie_object =
                        json_array_get_object(movies, i);
                    const char *title =
                        json_object_get_string(movie_object, "title");
                    int movie_id =
                        (int)json_object_get_number(movie_object, "id");
                    printf("#%d: %s\n", movie_id, title);
                }
                json_value_free(root_value);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf("ERROR: Failed to retrieve collection with id %d.\n",
                           id);
                }
            }
            free(message);
            free(response);
            free(query_params);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "add_collection") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            char *name = malloc(1000);
            printf("title=");
            fgets(name, 1000, stdin);
            name[strcspn(name, "\n")] = '\0';
            char buffer[100];
            printf("num_movies=");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0';
            char *endptr_num_movies;
            int num_movies = strtol(buffer, &endptr_num_movies, 10);
            if (endptr_num_movies == buffer || *endptr_num_movies != '\0') {
                printf("ERROR: Invalid number of movies format.\n");
                free(name);
                close_connection(sockfd);
                continue;
            }

            int movies[num_movies];
            for (int i = 0; i < num_movies; i++) {
                printf("movie_id[%d]=", i);
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = '\0';
                char *endptr_movie_id;
                movies[i] = strtol(buffer, &endptr_movie_id, 10);
                if (endptr_movie_id == buffer || *endptr_movie_id != '\0' ||
                    movies[i] < 0) {
                    printf("ERROR: Invalid movie id format.\n");
                    free(name);
                    close_connection(sockfd);
                    continue;
                }
            }
            JSON_Value *collection_value = json_value_init_object();
            JSON_Object *collection_object =
                json_value_get_object(collection_value);
            json_object_set_string(collection_object, "title", name);
            char *collection_json_string =
                json_serialize_to_string_pretty(collection_value);

            message = compute_post_request(
                host, "/api/v1/tema/library/collections", "application/json",
                &collection_json_string, 1, &cookies, cookies_exist,
                acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);

            json_free_serialized_string(collection_json_string);
            json_value_free(collection_value);
            int cnt = 0;

            if (strstr(response, "201 CREATED")) {
                char *json_response = basic_extract_json_response(response);
                JSON_Value *response_value = json_parse_string(json_response);
                JSON_Object *response_object =
                    json_value_get_object(response_value);
                int id = (int)json_object_get_number(response_object, "id");
                for (int i = 0; i < num_movies; i++) {
                    JSON_Value *movies_body = json_value_init_object();
                    JSON_Object *movies_object =
                        json_value_get_object(movies_body);
                    json_object_set_number(movies_object, "id", movies[i]);
                    char *movies_json_string =
                        json_serialize_to_string_pretty(movies_body);

                    char url[150];
                    sprintf(url, "/api/v1/tema/library/collections/%d/movies",
                            id);
                    message = compute_post_request(
                        host, url, "application/json", &movies_json_string, 1,
                        &cookies, cookies_exist, acces_token,
                        acces_token_exist);
                    send_to_server(sockfd, message);
                    response = receive_from_server(sockfd);
                    if (strstr(response, "201 CREATED")) {
                        cnt++;
                    }
                    json_free_serialized_string(movies_json_string);
                    json_value_free(movies_body);
                }

                json_value_free(response_value);
                if(cnt == num_movies) {
                    printf("SUCCES: Collection %s added successfully.\n", name);
                } else {
                    printf(
                        "ERROR: Collection %s added, but not all movies were "
                        "added.\n",
                        name);
                }

            } else if (strstr(response, "401 UNAUTHORIZED")) {
                printf("ERROR: User not logged in.\n");
            } else {
                printf("ERROR: Failed to add collection %s.\n", name);
            }

            free(message);
            free(response);
            free(name);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "delete_collection") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            int id;
            printf("id=");
            scanf("%d", &id);
            char *query_params = malloc(100);
            sprintf(query_params, "%d", id);
            message = compute_delete_request(
                host, "/api/v1/tema/library/collections", query_params,
                &cookies, cookies_exist, acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf("SUCCES: Collection with id %d deleted successfully.\n",
                       id);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf("ERROR: Failed to delete collection with id %d.\n",
                           id);
                }
            }
            free(message);
            free(response);
            free(query_params);
            close_connection(sockfd);
            continue;
        }
        if (strcmp(command, "add_movie_to_collection") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            int collection_id;
            printf("collection_id=");
            scanf("%d", &collection_id);
            int movie_id;
            printf("movie_id=");
            scanf("%d", &movie_id);
            char *url = malloc(100);
            sprintf(url, "/api/v1/tema/library/collections/%d/movies",
                    collection_id);
            JSON_Value *movie_value = json_value_init_object();
            JSON_Object *movie_object = json_value_get_object(movie_value);
            json_object_set_number(movie_object, "id", movie_id);
            char *movie_json_string =
                json_serialize_to_string_pretty(movie_value);
            message = compute_post_request(
                host, url, "application/json", &movie_json_string, 1, &cookies,
                cookies_exist, acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "201 CREATED")) {
                printf(
                    "SUCCES: Movie with id %d added to collection with id "
                    "%d.\n",
                    movie_id, collection_id);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf(
                        "ERROR: Failed to add movie with id %d to collection "
                        "with id %d.\n",
                        movie_id, collection_id);
                }
            }
        }
        if (strcmp(command, "delete_movie_from_collection") == 0) {
            sockfd = open_connection(host, port, AF_INET, SOCK_STREAM, 0);
            int collection_id;
            printf("collection_id=");
            scanf("%d", &collection_id);
            int movie_id;
            printf("movie_id=");
            scanf("%d", &movie_id);
            char *url = malloc(100);
            sprintf(url, "/api/v1/tema/library/collections/%d/movies/%d",
                    collection_id, movie_id);
            message =
                compute_delete_request(host, url, NULL, &cookies, cookies_exist,
                                       acces_token, acces_token_exist);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            if (strstr(response, "200 OK")) {
                printf(
                    "SUCCES: Movie with id %d deleted from collection with id "
                    "%d.\n",
                    movie_id, collection_id);
            } else {
                if (strstr(response, "401 UNAUTHORIZED")) {
                    printf("ERROR: User not logged in.\n");
                } else {
                    printf(
                        "ERROR: Failed to delete movie with id %d from "
                        "collection with id %d.\n",
                        movie_id, collection_id);
                }
            }
        }
        if (strcmp(command, "exit") == 0) {
            if (cookies_exist) {
                free(cookies);
                cookies_exist = 0;
            }
            if (acces_token_exist) {
                free(acces_token);
                acces_token_exist = 0;
            }
            close_connection(sockfd);
            break;
        }
    }
    free(command);
    return 0;
}
