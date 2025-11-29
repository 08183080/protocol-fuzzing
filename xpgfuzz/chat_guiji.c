#define _GNU_SOURCE // asprintf
#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>

#include "chat-llm.h"
#include "alloc-inl.h"
#include "hash.h"

// -lcurl -ljson-c -lpcre2-8
// apt install libcurl4-openssl-dev libjson-c-dev libpcre2-dev libpcre2-8-0

#define MAX_TOKENS 2048
#define CONFIDENT_TIMES 3

struct MemoryStruct
{
    char *memory;
    size_t size;
};

static size_t chat_with_llm_helper(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL)
    {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

char *chat_with_llm(char *prompt, char *model, int tries, float temperature)
{
    CURL *curl;
    CURLcode res = CURLE_OK;
    char *answer = NULL;
    char *url = NULL;

    url = "https://api.siliconflow.cn/v1/chat/completions";

    char *auth_header = "Authorization: Bearer " GUIJI_TOKEN;  //携带者令牌
    char *content_header = "Content-Type: application/json";  //输入是json
    char *accept_header = "Accept: application/json";       //接受json响应
    char *data = NULL;  

    asprintf(&data, "{\"model\": \"Qwen/QwQ-32B\",\"messages\": %s, \"max_tokens\": %d, \"temperature\": %f}", prompt, MAX_TOKENS, temperature);
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    do
    {
        struct MemoryStruct chunk;

        chunk.memory = malloc(1); /* will be grown as needed by the realloc above */
        chunk.size = 0;           /* no data at this point */

        curl = curl_easy_init();
        if (curl)
        {
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, auth_header);
            headers = curl_slist_append(headers, content_header);
            headers = curl_slist_append(headers, accept_header);

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, chat_with_llm_helper);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

            res = curl_easy_perform(curl);

            if (res == CURLE_OK)
            {
                json_object *jobj = json_tokener_parse(chunk.memory);

                // Check if the "choices" key exists
                if (json_object_object_get_ex(jobj, "choices", NULL))
                {
                    json_object *choices = json_object_object_get(jobj, "choices");
                    json_object *first_choice = json_object_array_get_idx(choices, 0);
                    const char *data;

                    json_object *jobj4 = json_object_object_get(first_choice, "message");
                    json_object *jobj5 = json_object_object_get(jobj4, "content");
                    data = json_object_get_string(jobj5);
                    
                    if (data[0] == '\n')
                        data++;
                    answer = strdup(data);
                }
                else
                {
                    printf("Error response is: %s\n", chunk.memory);
                    sleep(2); // Sleep for a small amount of time to ensure that the service can recover
                }
                json_object_put(jobj);
            }
            else
            {
                printf("Error: %s\n", curl_easy_strerror(res));
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }

        free(chunk.memory);
    } while ((res != CURLE_OK || answer == NULL) && (--tries > 0));

    if (data != NULL)
    {
        free(data);
    }

    curl_global_cleanup();
    return answer;
}


int main()
{
    char *chat_prompt = "[{\"role\": \"user\", \"content\": \"网络协议的特点是啥？\"}]";
    
    char *result = NULL;

    printf("--- Starting LLM Chat Request ---\n");
    printf("Model:Qwen/QwQ-32B (Chat Mode)\n");
    printf("Prompt: %s\n", chat_prompt);

    
    result = chat_with_llm(chat_prompt, "Qwen/QwQ-32B", 3, 0.7);

    printf("\n--- LLM Response ---\n");
    if (result)
    {
        printf("Answer: %s\n", result);
        free(result); // Remember to free the returned string
    }
    else
    {
        printf("Failed to get a response from the LLM service.\n");
        printf("Please check your network connection, API token, and the service URL.\n");
    }

    printf("------------------------------\n");
    
    return 0;
}