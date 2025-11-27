#define _GNU_SOURCE  
#include <stdio.h>  
#include <curl/curl.h>  
#include <string.h>  
#include <stdlib.h>  
#include <unistd.h>  
#include <json-c/json.h>  
  
// 修改这里为您的代理API密钥  
#define OPENAI_TOKEN "sk-AIQaabGrgQ4TYkxAU6msW77ojWeHLf1MUpwvaOkz9Ql0LYvo"  
#define MAX_TOKENS 2048  
  
struct MemoryStruct {  
    char *memory;  
    size_t size;  
};  
  
static size_t chat_with_llm_helper(void *contents, size_t size, size_t nmemb, void *userp) {  
    size_t realsize = size * nmemb;  
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;  
  
    mem->memory = realloc(mem->memory, mem->size + realsize + 1);  
    if (mem->memory == NULL) {  
        printf("not enough memory (realloc returned NULL)\n");  
        return 0;  
    }  
  
    memcpy(&(mem->memory[mem->size]), contents, realsize);  
    mem->size += realsize;  
    mem->memory[mem->size] = 0;  
  
    return realsize;  
}  
  
char *chat_with_llm(char *prompt, char *model, int tries, float temperature) {  
    CURL *curl;  
    CURLcode res = CURLE_OK;  
    char *answer = NULL;  
    char *url = NULL;  
      
    // 修改这里为您的代理API端点  
    if (strcmp(model, "instruct") == 0) {  
        url = "https://api.chatanywhere.tech/v1/completions";  // 替换为代理API地址  
    } else {  
        url = "https://api.chatanywhere.tech/v1/chat/completions";  // 替换为代理API地址  
    }  
      
    char *auth_header = "Authorization: Bearer " OPENAI_TOKEN;  
    char *content_header = "Content-Type: application/json";  
    char *accept_header = "Accept: application/json";  
    char *data = NULL;  
      
    if (strcmp(model, "instruct") == 0) {  
        asprintf(&data, "{\"model\": \"gpt-3.5-turbo-instruct\", \"prompt\": \"%s\", \"max_tokens\": %d, \"temperature\": %f}", prompt, MAX_TOKENS, temperature);  
    } else {  
        asprintf(&data, "{\"model\": \"gpt-3.5-turbo\",\"messages\": %s, \"max_tokens\": %d, \"temperature\": %f}", prompt, MAX_TOKENS, temperature);  
    }  
      
    curl_global_init(CURL_GLOBAL_DEFAULT);  
      
    do {  
        struct MemoryStruct chunk;  
        chunk.memory = malloc(1);  
        chunk.size = 0;  
  
        curl = curl_easy_init();  
        if (curl) {  
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
  
            if (res == CURLE_OK) {  
                json_object *jobj = json_tokener_parse(chunk.memory);  
  
                if (json_object_object_get_ex(jobj, "choices", NULL)) {  
                    json_object *choices = json_object_object_get(jobj, "choices");  
                    json_object *first_choice = json_object_array_get_idx(choices, 0);  
                    const char *data;  
  
                    if (strcmp(model, "instruct") == 0) {  
                        json_object *jobj4 = json_object_object_get(first_choice, "text");  
                        data = json_object_get_string(jobj4);  
                    } else {  
                        json_object *jobj4 = json_object_object_get(first_choice, "message");  
                        json_object *jobj5 = json_object_object_get(jobj4, "content");  
                        data = json_object_get_string(jobj5);  
                    }  
                    if (data[0] == '\n') data++;  
                    answer = strdup(data);  
                    printf("API测试成功！响应: %s\n", answer);  
                } else {  
                    printf("Error response is: %s\n", chunk.memory);  
                    sleep(2);  
                }  
                json_object_put(jobj);  
            } else {  
                printf("Error: %s\n", curl_easy_strerror(res));  
            }  
  
            curl_slist_free_all(headers);  
            curl_easy_cleanup(curl);  
        }  
        free(chunk.memory);  
    } while ((res != CURLE_OK || answer == NULL) && (--tries > 0));  
  
    if (data != NULL) {  
        free(data);  
    }  
    curl_global_cleanup();  
    return answer;  
}  
  
int main() {  
    printf("开始测试代理API连接...\n");  
      
    // 测试chat模型  
    char *test_prompt = "[{\"role\": \"user\", \"content\": \"Hello, this is a test message.\"}]";  
    char *result = chat_with_llm(test_prompt, "chat", 3, 0.7);  
      
    if (result) {  
        printf("Chat模型测试成功！\n");  
        free(result);  
    } else {  
        printf("Chat模型测试失败！\n");  
    }  
      
    // 测试instruct模型  
    char *instruct_prompt = "Say hello in Chinese";  
    result = chat_with_llm(instruct_prompt, "instruct", 3, 0.7);  
      
    if (result) {  
        printf("Instruct模型测试成功！\n");  
        free(result);  
    } else {  
        printf("Instruct模型测试失败！\n");  
    }  
      
    return 0;  
}
