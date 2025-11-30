#define _GNU_SOURCE // asprintf
#include <stdio.h>
#include <curl/curl.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>

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
    // url = "https://api.siliconflow.cn/v1/chat/completions";
    url = "https://www.dmxapi.com/v1/chat/completions";
    char *auth_header = "Authorization: Bearer " DMX_COM_TOKEN;//GUIJI_TOKEN;
    char *content_header = "Content-Type: application/json";
    char *accept_header = "Accept: application/json";
    char *data = NULL;
    
    // Build JSON request properly using json-c library
    json_object *request_obj = json_object_new_object();
    json_object_object_add(request_obj, "model", json_object_new_string("gpt-3.5-turbo"));
    
    // Create messages array
    json_object *messages_array = json_object_new_array();
    json_object *message_obj = json_object_new_object();
    json_object_object_add(message_obj, "role", json_object_new_string("user"));
    json_object_object_add(message_obj, "content", json_object_new_string(prompt));
    json_object_array_add(messages_array, message_obj);
    json_object_object_add(request_obj, "messages", messages_array);
    
    json_object_object_add(request_obj, "max_tokens", json_object_new_int(MAX_TOKENS));
    json_object_object_add(request_obj, "temperature", json_object_new_double(temperature));
    
    data = (char *)json_object_to_json_string(request_obj);
    data = strdup(data); // Make a copy since json_object will be freed later
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

    // Free JSON object and data
    if (request_obj != NULL)
    {
        json_object_put(request_obj);
    }
    if (data != NULL)
    {
        free(data);
    }

    curl_global_cleanup();
    return answer;
}

char *construct_prompt_stall(char *protocol_name, char *examples, char *history)
{
    char *template = "CRITICAL: Return ONLY the raw protocol command, NO explanations, NO analysis, NO markdown.\\n\\n"
                     "Protocol: %s\\n\\n"
                     "Communication History:\\n"
                     "\\\"\\\"\\\"\\n%s\\\"\\\"\\\"\\n\\n"
                     "Desired format example:\\n%s\\n\\n"
                     "Task: Generate the next proper client request that can affect the server's state.\\n"
                     "Output: Return ONLY the raw command line (e.g., \"PASS password\\r\\n\" or \"CWD /path\\r\\n\"), nothing else.";

    char *prompt = NULL;
    asprintf(&prompt, template, protocol_name, protocol_name, protocol_name, examples, history);

    char *final_prompt = NULL;

    asprintf(&final_prompt, "[{\"role\": \"system\", \"content\": \"You are a helpful assistant.\"}, {\"role\": \"user\", \"content\": \"%s\"}]", prompt);

    free(prompt);

    return final_prompt;
}


char *construct_enhanced_prompt_stall(char *protocol_name, char *examples, char *history)  
{  
    char *enhanced_template =   
        "You are an expert %s protocol fuzzer with deep knowledge of network protocols and vulnerability discovery. "  
        "Your goal is to generate the next client request that will maximize code coverage and potentially trigger edge cases or vulnerabilities.\\n\\n"  
          
        "COMMUNICATION HISTORY:\\n"  
        "The following shows the complete request-response sequence between client and server:\\n"  
        "\\\"\\\"\\\"%s\\\"\\\"\\\"\\n\\n"  
          
        "DESIRED OUTPUT FORMAT:\\n"  
        "%s\\n"  
          
        "GENERATION GUIDELINES:\\n"  
        "1. Analyze the communication flow to identify the current protocol state\\n"  
        "2. Consider edge cases: malformed headers, boundary values, unexpected sequences\\n"  
        "3. Focus on requests that could trigger new code paths or error conditions\\n"  
        "4. Maintain protocol syntax while exploring boundary conditions\\n"  
        "5. Generate ONE complete, well-formed %s request\\n\\n"  
          
        "Generate the next proper client request that can affect the server's state:";  
  
    char *prompt = NULL;  
    asprintf(&prompt, enhanced_template, protocol_name, history, examples, protocol_name);  
  
    char *final_prompt = NULL;  
    asprintf(&final_prompt,   
             "[{\"role\": \"system\", \"content\": \"You are an expert network protocol security researcher and fuzzing specialist.\"}, "  
             "{\"role\": \"user\", \"content\": \"%s\"}]",   
             prompt);  
  
    free(prompt);  
    return final_prompt;  
}

char *construct_prompt_for_templates(char *protocol_name, char **final_msg)
{
    // Give one example for learning formats with type constraints
    char *prompt_rtsp_example = "For the RTSP protocol, the DESCRIBE client request template is:\\n"
                                "DESCRIBE: [\\\"DESCRIBE <<PATH>>\\\\r\\\\n\\\","
                                "\\\"CSeq: <<INTEGER:1-65535>>\\\\r\\\\n\\\","
                                "\\\"User-Agent: <<STRING:1-256>>\\\\r\\\\n\\\","
                                "\\\"Accept: <<ENUM:application/sdp,application/x-rtsp-tunnelled>>\\\\r\\\\n\\\","
                                "\\\"\\\\r\\\\n\\\"]\\n\\n"
                                "Note: Use type constraints in the format:\\n"
                                "- <<INTEGER:min-max>> for integer ranges (e.g., <<INTEGER:0-65535>>)\\n"
                                "- <<STRING:min_len-max_len>> for string length ranges (e.g., <<STRING:1-256>>)\\n"
                                "- <<ENUM:val1,val2,val3>> for enumerated values (e.g., <<ENUM:GET,POST,PUT>>)\\n"
                                "- <<IP>> for IP addresses\\n"
                                "- <<PATH>> for file paths\\n"
                                "- <<HEX>> for hexadecimal values\\n"
                                "- <<VALUE>> for unconstrained values (backward compatibility)";

    char *prompt_http_example = "For the HTTP protocol, the GET client request template is:\\n"
                                "GET: [\\\"GET <<PATH>>\\\\r\\\\n\\\","
                                "\\\"Host: <<IP>>\\\\r\\\\n\\\","
                                "\\\"User-Agent: <<STRING:1-512>>\\\\r\\\\n\\\","
                                "\\\"\\\\r\\\\n\\\"]";  //零样本学习

    char *msg = NULL;
    asprintf(&msg, "%s\\n%s\\nFor the %s protocol, all of client request templates are (use type constraints where appropriate):", prompt_rtsp_example, prompt_http_example, protocol_name);
    *final_msg = msg;
    /** Format of prompt_grammars
    prompt_grammars = [
        {"role": "system", "content": "You are a helpful assistant."},
        {"role": "user", "content": msg}
    ]
     **/
    char *prompt_grammars = NULL;

    asprintf(&prompt_grammars, "[{\"role\": \"system\", \"content\": \"You are a helpful assistant specialized in network protocol analysis. Always use type constraints in templates when you can infer the value type from the protocol specification.\"}, {\"role\": \"user\", \"content\": \"%s\"}]", msg);

    return prompt_grammars;
}

char *construct_prompt_for_remaining_templates(char *protocol_name, char *first_question, char *first_answer)
{
    char *second_question = NULL;
    asprintf(&second_question, "For the %s protocol, other templates of client requests are (use type constraints where appropriate, same format as before):", protocol_name);

    json_object *answer_str = json_object_new_string(first_answer);
    // printf("The First Question\n%s\n\n", first_question);
    // printf("The First Answer\n%s\n\n", first_answer);
    // printf("The Second Question\n%s\n\n", second_question);
    const char *answer_str_escaped = json_object_to_json_string(answer_str);

    char *prompt = NULL;

    asprintf(&prompt,
             "["
             "{\"role\": \"system\", \"content\": \"You are a helpful assistant.\"},"
             "{\"role\": \"user\", \"content\": \"%s\"},"
             "{\"role\": \"assistant\", \"content\": %s },"
             "{\"role\": \"user\", \"content\": \"%s\"}"
             "]",
             first_question, answer_str_escaped, second_question);

    json_object_put(answer_str);
    free(second_question);

    return prompt;
}

// Extract command from LLM response (handles explanations, markdown, etc.)
static char *extract_command_from_response(const char *response, size_t response_len) {
    if (!response || response_len == 0) return NULL;
    
    // Strategy 1: Look for code blocks (```...```)
    const char *code_start = strstr(response, "```");
    if (code_start) {
        code_start += 3; // Skip ```
        // Skip language identifier
        while (code_start < response + response_len && 
               (*code_start == '\n' || *code_start == ' ' || 
                (*code_start >= 'a' && *code_start <= 'z') ||
                (*code_start >= 'A' && *code_start <= 'Z'))) {
            code_start++;
        }
        const char *code_end = strstr(code_start, "```");
        if (code_end) {
            size_t len = code_end - code_start;
            char *result = malloc(len + 1);
            strncpy(result, code_start, len);
            result[len] = '\0';
            // Extract first line (the command)
            char *newline = strchr(result, '\n');
            if (newline) *newline = '\0';
            return result;
        }
    }
    
    // Strategy 2: Look for "Request-1:", "Final Answer:", etc.
    const char *markers[] = {
        "Request-1:",
        "request-1:",
        "Final Answer:",
        "final answer:",
        "Answer:",
        "answer:",
        "Command:",
        "command:",
        "Next request:",
        "next request:"
    };
    
    for (int i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        const char *marker = strstr(response, markers[i]);
        if (marker) {
            marker += strlen(markers[i]);
            // Skip whitespace
            while (marker < response + response_len && 
                   (*marker == ' ' || *marker == '\n' || *marker == '\r' || *marker == '\t')) {
                marker++;
            }
            
            // Extract until next newline or end
            const char *end = marker;
            while (end < response + response_len && *end != '\n' && *end != '\r' && *end != '\0') {
                end++;
            }
            
            size_t len = end - marker;
            if (len > 0) {
                char *result = malloc(len + 1);
                strncpy(result, marker, len);
                result[len] = '\0';
                return result;
            }
        }
    }
    
    // Strategy 3: Look for protocol commands (USER, PASS, CWD, etc.) followed by parameters
    // Pattern: COMMAND followed by optional parameters and \r\n
    const char *protocol_commands[] = {
        "PASS ", "USER ", "CWD ", "RETR ", "STOR ", "LIST ", "DELE ", 
        "MKD ", "RMD ", "RNFR ", "RNTO ", "TYPE ", "PASV ", "PORT ",
        "QUIT", "SYST", "NOOP", "PWD", "HELP", "STAT", "FEAT"
    };
    
    for (int i = 0; i < sizeof(protocol_commands) / sizeof(protocol_commands[0]); i++) {
        const char *cmd = strstr(response, protocol_commands[i]);
        if (cmd) {
            // Find the end of the command line
            const char *line_end = cmd;
            while (line_end < response + response_len && 
                   *line_end != '\n' && *line_end != '\r' && *line_end != '\0') {
                line_end++;
            }
            
            size_t len = line_end - cmd;
            if (len > 0 && len < 200) { // Reasonable command length
                char *result = malloc(len + 1);
                strncpy(result, cmd, len);
                result[len] = '\0';
                // Trim trailing whitespace
                char *trim = result + len - 1;
                while (trim > result && (*trim == ' ' || *trim == '\t')) {
                    *trim = '\0';
                    trim--;
                }
                return result;
            }
        }
    }
    
    // Strategy 4: Original regex-based extraction (fallback)
    int errornumber;
    size_t erroroffset;
    pcre2_code *extracter = pcre2_compile("\r?\n?.*?\r?\n", PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL);
    if (extracter) {
        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(extracter, NULL);
        int rc = pcre2_match(extracter, response, response_len, 0, 0, match_data, NULL);
        char *res = NULL;
        if (rc >= 0) {
            size_t *ovector = pcre2_get_ovector_pointer(match_data);
            if (ovector[1] < response_len) {
                res = strdup(response + ovector[1]);
            }
        }
        pcre2_match_data_free(match_data);
        pcre2_code_free(extracter);
        if (res) return res;
    }
    
    return NULL;
}

char *extract_stalled_message(char *message, size_t message_len)
{
    if (!message || message_len == 0) return NULL;
    
    // Use improved extraction function
    char *extracted = extract_command_from_response(message, message_len);
    
    if (extracted) {
        // Clean up: remove any remaining markdown, explanations
        char *cleaned = extracted;
        
        // Remove leading/trailing whitespace
        while (*cleaned == ' ' || *cleaned == '\n' || *cleaned == '\r' || *cleaned == '\t') {
            cleaned++;
        }
        char *end = cleaned + strlen(cleaned) - 1;
        while (end > cleaned && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
            *end = '\0';
            end--;
        }
        
        // Remove markdown code markers if present
        if (cleaned[0] == '`' && cleaned[1] == '`' && cleaned[2] == '`') {
            cleaned += 3;
            char *newline = strchr(cleaned, '\n');
            if (newline) cleaned = newline + 1;
        }
        
        // If we modified the pointer, create new string
        if (cleaned != extracted) {
            char *result = strdup(cleaned);
            free(extracted);
            return result;
        }
        
        return extracted;
    }
    
    // Fallback: original simple extraction
    int errornumber;
    size_t erroroffset;
    pcre2_code *extracter = pcre2_compile("\r?\n?.*?\r?\n", PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL);
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(extracter, NULL);
    int rc = pcre2_match(extracter, message, message_len, 0, 0, match_data, NULL);
    char *res = NULL;
    if (rc >= 0)
    {
        size_t *ovector = pcre2_get_ovector_pointer(match_data);
        if (ovector[1] < message_len) {
            res = strdup(message + ovector[1]);
        }
    }

    pcre2_match_data_free(match_data);
    pcre2_code_free(extracter);

    return res;
}

char *format_request_message(char *message)
{

    int message_len = strlen(message);
    int max_len = message_len;
    int res_len = 0;
    char *res = ck_alloc(message_len * sizeof(char));
    for (int i = 0; i < message_len; i++)
    {
        // If an \n is not padded with an \r before, we add it
        if (message[i] == '\n' && (i == 0 || (message[i - 1] != '\r')))
        {
            if (res_len == max_len)
            {
                res = ck_realloc(res, max_len + 10);
                max_len += 10;
            }
            res[res_len++] = '\r';
        }

        if (res_len == max_len)
        {
            res = ck_realloc(res, max_len + 10);
            max_len += 10;
        }
        res[res_len++] = message[i];
    }

    // Add \r\n\r\n to ensure that the packet is accepted
    for (int i = 0; i < 2; i++)
    {
        if (res_len == max_len)
        {
            res = ck_realloc(res, max_len + 10);
            max_len += 10;
        }
        res[res_len++] = '\r';
        if (res_len == max_len)
        {
            res = ck_realloc(res, max_len + 10);
            max_len += 10;
        }
        res[res_len++] = '\n';
    }

    if (res_len == max_len)
    {
        res = ck_realloc(res, max_len + 1);
        max_len++;
    }
    res[res_len++] = '\0';
    free(message);
    return res;
}

//获取协议的所有状态
char *construct_prompt_for_protocol_message_types(char *protocol_name)
{
    /***
     * Prompt to ask the protocol states as follow:
     * ```
     * In the RTSP protocol, the protocol states are:
     *
     * Desired format:
     * <comma_separated_list_of_states_in_uppercase>
     * ```
     * ***/
    char *prompt = NULL;

    // transfer the prompt into string
    asprintf(&prompt, "In the %s protocol, the message types are: \\n\\nDesired format:\\n<comma_separated_list_of_states_in_uppercase_and_without_whitespaces>", protocol_name);

    return prompt;
}

char *construct_prompt_for_requests_to_states(const char *protocol_name,
                                              const char *protocol_state,
                                              const char *example_requests)
{
    /***
     Prompt to ask the sequence of client requests to reach a protocol state as follows:
        ```
        In the RTSP protocol, if the server just starts, to reach the PLAYING state, the sequence of client requests can be:
        DESCRIBE rtsp://127.0.0.1:8554/aacAudioTest RTSP/1.0
        CSeq: 2
        User-Agent: ./testRTSPClient (LIVE555 Streaming Media v2018.08.28)
        Accept: application/sdp

        SETUP rtsp://127.0.0.1:8554/aacAudioTest/track1 RTSP/1.0
        CSeq: 3
        User-Agent: ./testRTSPClient (LIVE555 Streaming Media v2018.08.28)
        Transport: RTP/AVP;unicast;client_port=38784-38785

        PLAY rtsp://127.0.0.1:8554/aacAudioTest/ RTSP/1.0
        CSeq: 4
        User-Agent: ./testRTSPClient (LIVE555 Streaming Media v2018.08.28)
        Session: 000022B8
        Range: npt=0.000-

        Similarly, in the RTSP protocol, if the server just starts, to reach the RECORD state, the sequence of client requests can be:
     ***/

    // Transfer formats of example_requests
    json_object *example_requests_json = json_object_new_string(example_requests);
    const char *example_requests_json_str = json_object_to_json_string(example_requests_json);

    json_object *protocol_state_json = json_object_new_string(protocol_state);
    const char *protocol_state_json_str = json_object_to_json_string(protocol_state_json);

    char *prompt = NULL;

    int example_request_len = strlen(example_requests_json_str) - 2;
    if (example_request_len > EXAMPLE_SEQUENCE_PROMPT_LENGTH)
    {
        example_request_len = EXAMPLE_SEQUENCE_PROMPT_LENGTH;
    }

    asprintf(&prompt,
             "In the %s protocol, if the server just starts, to reach the INIT state, the sequence of client requests can be:\\n"
             "%.*s\\nSimilarly, in the %s protocol, if the server just starts, to reach the %.*s state, the sequence of client requests can be:\\n",
             protocol_name,
             example_request_len,
             example_requests_json_str + 1,
             protocol_name,
             (int)strlen(protocol_state_json_str) - 2,
             protocol_state_json_str + 1);

    json_object_put(protocol_state_json);
    json_object_put(example_requests_json);

    return prompt;
}

void extract_message_grammars(char *answers, klist_t(gram) * grammar_list)
{

    char *ptr = answers;
    int len = strlen(answers);

    while (ptr < answers + len)
    {
        char *start = strchr(ptr, '[');
        if (start == NULL)
            break;
        char *end = strchr(start, ']');
        if (end == NULL)
            break;
        int count = end - start + 1;
        char *temp = (char *)ck_alloc(count + 1);
        strncpy(temp, start, count);
        temp[count] = '\0';
        ptr = end + 1;

        // conver temp to json object and save it to the list
        json_object *jobj = json_tokener_parse(temp);
        // Only add valid JSON arrays to the list
        if (jobj != NULL && json_object_get_type(jobj) == json_type_array)
        {
            *kl_pushp(gram, grammar_list) = jobj;
        }
        else
        {
            // Free invalid JSON object
            if (jobj != NULL)
                json_object_put(jobj);
        }
        ck_free(temp);

        // printf("%s\n", temp);
    }
}

// Helper function to extract constraint from matched group
static type_constraint_t *extract_constraint_from_match(const char *str, PCRE2_SIZE start, PCRE2_SIZE end)
{
    if (start == (PCRE2_SIZE)-1 || end == (PCRE2_SIZE)-1 || start >= end)
        return NULL;
    
    size_t len = end - start;
    char *constraint_str = (char *)ck_alloc(len + 1);
    strncpy(constraint_str, str + start, len);
    constraint_str[len] = '\0';
    
    type_constraint_t *constraint = parse_constraint(constraint_str);
    ck_free(constraint_str);
    return constraint;
}

int parse_pattern(pcre2_code *replacer, pcre2_match_data *match_data, const char *str, size_t len, char *pattern, type_constraint_t **out_constraint)
{
    strcat(pattern, "(?:");
    // offset == 3;
    int rc = pcre2_match(replacer, str, len, 0, 0, match_data, NULL);

    if (rc < 0)
    {
        switch (rc)
        {
        case PCRE2_ERROR_NOMATCH:
            // printf("No match for %s!\n", str);
            break;
        default:
            // printf("Matching error %d\n", rc);
            break;
        }
        pcre2_match_data_free(match_data);
        pcre2_code_free(replacer);
        if (out_constraint) *out_constraint = NULL;
        return 0;
    }
    // printf("RC is %d\n",rc);
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
    // for(int i = 1; i<rc;i++){
    //     printf("Start %d, end %d\n",ovector[2*i],ovector[2*i+1]);
    // }

    if (rc == 4)
    { // matched the first option - there is a special value
        strncat(pattern, str + ovector[2], ovector[3] - ovector[2]);
        // offset += ovector[3] - ovector[2];

        // Extract constraint from the matched value (group 2)
        if (out_constraint)
        {
            *out_constraint = extract_constraint_from_match(str, ovector[4], ovector[5]);
        }

        strcat(pattern, "(.*)");
        // offset += 3;

        strncat(pattern, str + ovector[6], ovector[7] - ovector[6]);
        // offset += ovector[7] - ovector[6];
    }
    else if (rc == 5)
    {
        // matched the second option - there is no special value
        strncat(pattern, str + ovector[8], ovector[9] - ovector[8]);
        if (out_constraint) *out_constraint = NULL;
        // offset += ovector[9] - ovector[8];
    }
    else
    {
        FATAL("Regex groups were updated but not the handling code.");
    }
    strcat(pattern, ")");
    return 1;
}

// Global storage for pattern constraints
// Maps pattern pointer to array of constraints for each capture group
static khash_t(pattern_constraints_map) *pattern_constraints_storage = NULL;

// Initialize pattern constraints storage
static void init_pattern_constraints_storage(void)
{
    if (pattern_constraints_storage == NULL)
    {
        pattern_constraints_storage = kh_init(pattern_constraints_map);
    }
}

// Store constraints for a pattern
static void store_pattern_constraints(pcre2_code **patterns, type_constraint_t **header_constraints, int header_count, type_constraint_t **field_constraints, int field_count)
{
    init_pattern_constraints_storage();
    
    // Use pattern pointer as key
    khint_t k;
    int ret;
    k = kh_put(pattern_constraints_map, pattern_constraints_storage, (void *)patterns, &ret);
    
    // Allocate structure to store constraints
    pattern_with_constraints_t *pc = (pattern_with_constraints_t *)ck_alloc(sizeof(pattern_with_constraints_t));
    pc->patterns = patterns;
    pc->header_constraints = header_constraints;
    pc->field_constraints = field_constraints;
    pc->header_constraint_count = header_count;
    pc->field_constraint_count = field_count;
    
    kh_value(pattern_constraints_storage, k) = pc;
}

// Get constraints for a pattern
pattern_with_constraints_t *get_pattern_constraints(pcre2_code **patterns)
{
    if (pattern_constraints_storage == NULL)
        return NULL;
    
    khint_t k = kh_get(pattern_constraints_map, pattern_constraints_storage, (void *)patterns);
    if (k == kh_end(pattern_constraints_storage))
        return NULL;
    
    return kh_value(pattern_constraints_storage, k);
}

// If successful, puts 2 patterns in the patterns array, the first one is the header, the second is the fields
// Else returns an array with the first element being NULL
char *extract_message_pattern(const char *header_str, khash_t(field_table) * field_table, pcre2_code **patterns, int debug_file, const char *debug_file_name)
{
    int errornumber;
    size_t erroroffset;
    char header_pattern[128] = {0};
    char fields_pattern[1024] = {0};
    pcre2_code *replacer = pcre2_compile("(?:(.*)(?:<<(.*)>>)(.*))|(.+)", PCRE2_ZERO_TERMINATED, PCRE2_DOTALL, &errornumber, &erroroffset, NULL);
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(replacer, NULL);
    char *message_type = NULL;
    
    // Arrays to store constraints
    type_constraint_t **header_constraints = NULL;
    int header_constraint_count = 0;
    type_constraint_t **field_constraints = NULL;
    int field_constraint_count = 0;
    int field_constraint_capacity = 0;
    
    // int offset = 0;
    /**
     * Example output
     * patterns[0] = (?:PLAY (.*)\r\n)
     * patterns[1] = (?|(?:CSeq: (.*)\r\n)|(?:User-Agent: (.*)\r\n)|(?:Range: (.*)\r\n)|(?:\r\n))
     */

    {
        // We use the string in such an escaped format for easier debugging as the regex library supports parsing it properly
        // The string contains quotations so they are ignored
        header_str++;

        int message_len = 0;
        while (header_str[message_len] != '\0' 
        && header_str[message_len] != ' ' 
        && header_str[message_len] != '\n' 
        && header_str[message_len] != '\r' 
        && header_str[message_len] != '\\' )
        {
            message_len++;
        }
        message_type = ck_alloc(message_len + 1);
        memcpy(message_type, header_str, message_len);
        message_type[message_len] = '\0';

        size_t len = strlen(header_str) - 1;
        strcat(header_pattern, "^"); // Ensure that it captures the start of the string
        
        type_constraint_t *header_constraint = NULL;
        if (!parse_pattern(replacer, match_data, header_str, len, header_pattern, &header_constraint))
        {
            patterns[0] = NULL;
            pcre2_match_data_free(match_data);
            pcre2_code_free(replacer);
            return NULL;
        }
        
        // Store header constraint if found
        if (header_constraint && header_constraint->type != CONSTRAINT_NONE)
        {
            header_constraints = (type_constraint_t **)ck_alloc(sizeof(type_constraint_t *));
            header_constraints[0] = header_constraint;
            header_constraint_count = 1;
        }
        else if (header_constraint)
        {
            free_constraint(header_constraint);
        }
    }

    int first = 1;

    strcat(fields_pattern, "(?|");
    for (khiter_t field_t_iter = kh_begin(field_table); field_t_iter != kh_end(field_table); ++field_t_iter)
    {
        if (!kh_exist(field_table, field_t_iter) || kh_value(field_table, field_t_iter) < (TEMPLATE_CONSISTENCY_COUNT / 2 + (TEMPLATE_CONSISTENCY_COUNT % 2)))
            continue;

        if (!first)
        {
            strcat(fields_pattern, "|");
        }
        else
        {
            first = 0;
        }

        json_object *field_v = json_object_new_string(kh_key(field_table, field_t_iter));
        const char *str = json_object_to_json_string(field_v);
        // We use the string in such an escaped format for easier debugging as the regex library supports parsing it properly
        // The string contains quotations so they are ignored
        str++;
        size_t len = strlen(str) - 1;
        
        type_constraint_t *field_constraint = NULL;
        int matched = parse_pattern(replacer, match_data, str, len, fields_pattern, &field_constraint);
        json_object_put(field_v);
        if (!matched)
        {
            patterns[0] = NULL;
            // Free constraints collected so far
            if (header_constraints) {
                for (int i = 0; i < header_constraint_count; i++) {
                    free_constraint(header_constraints[i]);
                }
                ck_free(header_constraints);
            }
            if (field_constraints) {
                for (int i = 0; i < field_constraint_count; i++) {
                    if (field_constraints[i]) free_constraint(field_constraints[i]);
                }
                ck_free(field_constraints);
            }
            pcre2_match_data_free(match_data);
            pcre2_code_free(replacer);
            return NULL;
        }
        
        // Store field constraint if found
        if (field_constraint && field_constraint->type != CONSTRAINT_NONE)
        {
            if (field_constraint_count >= field_constraint_capacity)
            {
                field_constraint_capacity = field_constraint_capacity == 0 ? 8 : field_constraint_capacity * 2;
                field_constraints = (type_constraint_t **)ck_realloc(field_constraints, field_constraint_capacity * sizeof(type_constraint_t *));
            }
            field_constraints[field_constraint_count++] = field_constraint;
        }
        else if (field_constraint)
        {
            free_constraint(field_constraint);
        }
    }

    strcat(fields_pattern, ")");

    if (first == 1)
    { // convert from (?|) to (.+) when the group is empty
        fields_pattern[1] = '.';
        fields_pattern[2] = '+';
    }

    pcre2_match_data_free(match_data);
    pcre2_code_free(replacer);
    printf("Header pattern is %s\n", header_pattern);
    printf("Fields pattern is %s\n", fields_pattern);

    if (debug_file != -1 && debug_file_name != NULL)
    {
        ck_write(debug_file, header_pattern, strlen(header_pattern), debug_file_name);
        ck_write(debug_file, "\n", 1, debug_file_name);
        ck_write(debug_file, fields_pattern, strlen(fields_pattern), debug_file_name);
    }

    {
        pcre2_code *p = pcre2_compile(header_pattern, PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL);
        pcre2_jit_compile(p, PCRE2_JIT_COMPLETE);
        patterns[0] = p;
    }
    {
        pcre2_code *p = pcre2_compile(fields_pattern, PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL);
        pcre2_jit_compile(p, PCRE2_JIT_COMPLETE);
        patterns[1] = p;
    }
    
    // Store constraints for this pattern
    if (header_constraint_count > 0 || field_constraint_count > 0)
    {
        store_pattern_constraints(patterns, header_constraints, header_constraint_count, 
                                  field_constraints, field_constraint_count);
    }
    else
    {
        // Free constraint arrays if no constraints found
        if (header_constraints) ck_free(header_constraints);
        if (field_constraints) ck_free(field_constraints);
    }
    
    return message_type;
}

range_list starts_with(char *line, int length, pcre2_code *pattern, pcre2_code **patterns_array)
{
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(pattern, NULL);

    int rc = pcre2_match(pattern, line, length, 0, 0, match_data, NULL); // find the first range

    // Get constraints for this pattern if available
    pattern_with_constraints_t *pc = NULL;
    if (patterns_array)
    {
        pc = get_pattern_constraints(patterns_array);
    }

    // printf("starts_with rc is %d\n", rc);
    if (rc < 0)
    {
        switch (rc)
        {
        case PCRE2_ERROR_NOMATCH:
            // printf("No match!\n");
            break;
        default:
            // printf("Matching error %d\n", rc);
            break;
        }
        pcre2_match_data_free(match_data);
        range_list res;
        kv_init(res);
        return res;
    }

    range_list dyn_ranges;
    kv_init(dyn_ranges);
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
    for (int i = 1; i < rc; i++)
    {
        if (ovector[2 * i] == (PCRE2_SIZE)-1)
            continue;
        // printf("Group %d %d %d\n",i, ovector[2 * i], ovector[2 * i + 1]);
        range v = {.start = ovector[2 * i], .len = ovector[2 * i + 1] - ovector[2 * i], .mutable = 1, .constraint = NULL};
        
        // Associate constraint if available (header constraints for header pattern)
        if (pc && pc->header_constraints && i - 1 < pc->header_constraint_count)
        {
            // Duplicate constraint for this range
            type_constraint_t *constraint = pc->header_constraints[i - 1];
            if (constraint)
            {
                type_constraint_t *constraint_copy = (type_constraint_t *)ck_alloc(sizeof(type_constraint_t));
                memcpy(constraint_copy, constraint, sizeof(type_constraint_t));
                // For ENUM, we need to copy the values array
                if (constraint->type == CONSTRAINT_ENUM)
                {
                    constraint_copy->constraint.enum_values.values = (char **)ck_alloc(constraint->constraint.enum_values.count * sizeof(char *));
                    for (int j = 0; j < constraint->constraint.enum_values.count; j++)
                    {
                        constraint_copy->constraint.enum_values.values[j] = strdup(constraint->constraint.enum_values.values[j]);
                    }
                }
                v.constraint = constraint_copy;
            }
        }
        
        kv_push(range, dyn_ranges, v);
        // kv_push(range, dyn_ranges, v);
        //  ranges[0][i - 1] = v;
    }
    range v = {.start = ovector[0], .len = ovector[1] - ovector[0], .mutable = 1, .constraint = NULL};
    kv_push(range, dyn_ranges, v); // add the global range at the end

    pcre2_match_data_free(match_data);
    return dyn_ranges;
}

range_list get_mutable_ranges(char *line, int length, int offset, pcre2_code *pattern, pcre2_code **patterns_array)
{
    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(pattern, NULL);

    // Get constraints for this pattern if available
    pattern_with_constraints_t *pc = NULL;
    if (patterns_array)
    {
        pc = get_pattern_constraints(patterns_array);
    }

    range_list dyn_ranges;
    kv_init(dyn_ranges);
    int constraint_index = 0; // Track which field constraint to use

    for (;;) // catch all the other ranges
    {
        int rc = pcre2_match(pattern, line, length, offset, 0, match_data, NULL);
        if (rc < 0)
        {
            switch (rc)
            {
            case PCRE2_ERROR_NOMATCH:
                // printf("No match!\n");
                break;
            default:
                // printf("Matching error %d\n", rc);
                break;
            }
            pcre2_match_data_free(match_data);
            match_data = NULL;
            break;
        }
        PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
        if (offset != ovector[0])
        {
            range v = {.start = offset, .len = ovector[0] - offset, .mutable = 1, .constraint = NULL};
            kv_push(range, dyn_ranges, v);
        }

        // printf("Matched over %d %d\n", ovector[0], ovector[1]);
        for (int i = 1; i < rc; i++)
        {
            if (ovector[2 * i] == (PCRE2_SIZE)-1)
                continue;
            // printf("Group %d %d %d\n",i, ovector[2 * i], ovector[2 * i + 1]);
            range v = {.start = ovector[2 * i], .len = ovector[2 * i + 1] - ovector[2 * i], .mutable = 1, .constraint = NULL};
            
            // Associate constraint if available (field constraints for fields pattern)
            if (pc && pc->field_constraints && constraint_index < pc->field_constraint_count)
            {
                type_constraint_t *constraint = pc->field_constraints[constraint_index];
                if (constraint)
                {
                    type_constraint_t *constraint_copy = (type_constraint_t *)ck_alloc(sizeof(type_constraint_t));
                    memcpy(constraint_copy, constraint, sizeof(type_constraint_t));
                    // For ENUM, we need to copy the values array
                    if (constraint->type == CONSTRAINT_ENUM)
                    {
                        constraint_copy->constraint.enum_values.values = (char **)ck_alloc(constraint->constraint.enum_values.count * sizeof(char *));
                        for (int j = 0; j < constraint->constraint.enum_values.count; j++)
                        {
                            constraint_copy->constraint.enum_values.values[j] = strdup(constraint->constraint.enum_values.values[j]);
                        }
                    }
                    v.constraint = constraint_copy;
                }
                constraint_index++;
            }
            
            kv_push(range, dyn_ranges, v);
            // ranges[0][i - 1] = v;
        }
        if (offset == ovector[1])
        { // in the case the match is empty, we just move a step forward
            offset++;
        }
        else
        {
            offset = ovector[1];
        }
    }

    if (offset < length) // catch anything past the last matched pattern
    {
        range v = {.start = offset, .len = length - offset, .mutable = 1, .constraint = NULL};
        kv_push(range, dyn_ranges, v);
    }

    if (match_data != NULL)
    {
        pcre2_match_data_free(match_data);
    }
    return dyn_ranges;
}

char *unescape_string(const char *input)
{
    size_t length = strlen(input);
    char *output = (char *)malloc((length + 1) * sizeof(char));

    if (output == NULL)
    {
        printf("Memory allocation failed.\n");
        return NULL;
    }

    size_t i, j = 0;
    for (i = 0; i < length; i++)
    {
        if (input[i] == '\\')
        {
            i++; // Skip the backslash
            switch (input[i])
            {
            case 'n':
                output[j++] = '\n';
                break;
            case 't':
                output[j++] = '\t';
                break;
            case 'r':
                output[j++] = '\r';
                break;
            case '\\':
                output[j++] = '\\';
                break;
            default:
                output[j++] = input[i];
                break;
            }
        }
        else
        {
            output[j++] = input[i];
        }
    }

    output[j] = '\0'; // Add null-terminator to the output string
    return output;
}

void write_new_seeds(char *enriched_file, char *contents)
{
    FILE *fp = fopen(enriched_file, "w");
    if (fp == NULL)
    {
        printf("Error in opening the file %s\n", enriched_file);
        exit(1);
    }

    // remove the newline and whiltespace in the beginning of the string if any
    while (contents[0] == '\n' || contents[0] == ' ' || contents[0] == '\t' || contents[0] == '\r')
    {
        contents++;
    }

    // Check if last 4 characters of the client_request_answer string are \r\n\r\n
    // If not, add them
    int len = strlen(contents);
    if (contents[len - 1] != '\n' || contents[len - 2] != '\r' || contents[len - 3] != '\n' || contents[len - 4] != '\r')
    {
        fprintf(fp, "%s\r\n\r\n", contents);
    }
    else
    {
        fprintf(fp, "%s", contents);
    }

    fclose(fp);
}

char *format_string(char *state_string)
{
    // remove the newline and whiltespace in the beginning of the string if any
    while (state_string[0] == '\n' || state_string[0] == ' ' || state_string[0] == '\t' || state_string[0] == '\r')
    {
        state_string++;
    }

    int len = strlen(state_string);
    while (state_string[len - 1] == '\n' || state_string[len - 1] == '\r' || state_string[len - 1] == ' ' || state_string[len - 1] == '.')
    {
        state_string[len - 1] = '\0';
        len--;
    }

    return state_string;
}

/***
 * Get the protocol states based on self-consistency check
 * pass the parameters: protocol_name, states_set, states_string
 ***/
void get_protocol_message_types(char *state_prompt, khash_t(strSet) * states_set)
{
    khash_t(strMap) *state_to_times = kh_init(strMap); // map from state to times

    char *full_prompt = NULL;
    asprintf(&full_prompt, "%s", state_prompt);

    for (int i = 0; i < CONFIDENT_TIMES; i++)
    {
        char *state_answer = chat_with_llm(full_prompt, "instruct", MESSAGE_TYPE_RETRIES, 0.5);
        if (state_answer == NULL)
            continue;

        state_answer = format_string(state_answer);

        char *state_tokens = strtok(state_answer, ",");
        while (state_tokens != NULL)
        {
            char *protocol_state = state_tokens;
            protocol_state = format_string(protocol_state);

            int ret;
            khiter_t k = kh_put(strMap, state_to_times, protocol_state, &ret);
            if (ret == 0)
            {
                kh_value(state_to_times, k)++;
            }
            else
            {
                kh_value(state_to_times, k) = 1;
            }

            state_tokens = strtok(NULL, ",");
        }
    }

    for (khiter_t k = kh_begin(state_to_times); k != kh_end(state_to_times); ++k)
    {
        if (kh_exist(state_to_times, k))
        {
            if (kh_value(state_to_times, k) >= 0.5 * CONFIDENT_TIMES)
            {
                const char *protocol_state = kh_key(state_to_times, k);

                int ret;
                kh_put(strSet, states_set, protocol_state, &ret);
            }
        }
    }

    free(full_prompt);
}

khash_t(strSet) * duplicate_hash(khash_t(strSet) * set)
{
    khash_t(strSet) *new_set = kh_init(strSet);

    for (khiter_t k = kh_begin(set); k != kh_end(set); ++k)
    {
        if (kh_exist(set, k))
        {
            const char *val = kh_key(set, k);
            int ret;
            kh_put(strSet, new_set, val, &ret);
        }
    }

    return new_set;
}

void make_combination(khash_t(strSet)* sequence, char** data , message_set_list* res,khiter_t st, khiter_t end, int index, int size);

message_set_list message_combinations(khash_t(strSet)* sequence, int size)
{
    message_set_list res;
    kv_init(res);
    char* data[size];
    make_combination(sequence,data, &res, kh_begin(sequence), kh_end(sequence), 0, size);
    return res;
}

void make_combination(khash_t(strSet)* sequence, char** data , message_set_list* res,khiter_t st, khiter_t end,
                     int index, int size)
{

    if (index == size)
    {
        khash_t(strSet)* combination = kh_init(strSet);
        int absent;
        for (int j=0; j<size; j++){
            kh_put(strSet,combination, data[j],&absent );
        }
        kv_push(khash_t(strSet)*,*res,combination);
        return;
    }
    for (khiter_t i=st; i != end && end-i+1 >= size-index; i++)
    {
        if(!kh_exist(sequence,i))
            continue;
        data[index] = kh_key(sequence,i);
        make_combination(sequence, data,res, i+1, end, index+1, size);
    }
}



int min(int a, int b) {
    return a < b ? a : b;
}

// Extract pure sequence from LLM response (remove explanations, markdown, etc.)
static char *extract_sequence_from_response(const char *response) {
    if (!response) return NULL;
    
    // Try to find code blocks first (```)
    const char *code_start = strstr(response, "```");
    if (code_start) {
        code_start += 3; // Skip ```
        // Skip language identifier if present
        while (*code_start == '\n' || *code_start == ' ' || 
               (*code_start >= 'a' && *code_start <= 'z') ||
               (*code_start >= 'A' && *code_start <= 'Z')) {
            code_start++;
        }
        const char *code_end = strstr(code_start, "```");
        if (code_end) {
            int len = code_end - code_start;
            char *result = malloc(len + 1);
            strncpy(result, code_start, len);
            result[len] = '\0';
            return result;
        }
    }
    
    // Try to find sequence between "Modified sequence:" or similar markers
    const char *markers[] = {
        "Modified sequence:",
        "modified sequence:",
        "Sequence:",
        "sequence:",
        "Output:",
        "output:"
    };
    
    for (int i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        const char *marker = strstr(response, markers[i]);
        if (marker) {
            marker += strlen(markers[i]);
            // Skip whitespace and newlines
            while (*marker == ' ' || *marker == '\n' || *marker == '\r' || *marker == '\t') {
                marker++;
            }
            
            // Find the end (either end of string or next section)
            const char *end = marker;
            while (*end != '\0' && 
                   !(end[0] == '\n' && (strstr(end, "Note:") || strstr(end, "---") || strstr(end, "**")))) {
                end++;
            }
            
            int len = end - marker;
            if (len > 0) {
                char *result = malloc(len + 1);
                strncpy(result, marker, len);
                result[len] = '\0';
                return result;
            }
        }
    }
    
    // If no markers found, try to extract lines that look like commands
    // (lines that start with uppercase letters, possibly with parameters)
    char *result = malloc(strlen(response) + 1);
    char *out = result;
    const char *in = response;
    int in_command_block = 0;
    
    while (*in) {
        // Look for patterns like "USER", "PASS", "CWD", etc. at start of line
        if ((*in == '\n' || in == response) && 
            ((in[1] >= 'A' && in[1] <= 'Z') || 
             (in[1] >= 'a' && in[1] <= 'z'))) {
            in_command_block = 1;
        }
        
        if (in_command_block) {
            if (*in == '\n' && 
                !((in[1] >= 'A' && in[1] <= 'Z') || 
                  (in[1] >= 'a' && in[1] <= 'z') ||
                  (in[1] == ' ' || in[1] == '\t') ||
                  (in[1] == '\0'))) {
                // End of command block
                break;
            }
            *out++ = *in;
        }
        in++;
    }
    *out = '\0';
    
    // If we extracted something meaningful, return it
    if (strlen(result) > 10) {
        return result;
    }
    
    free(result);
    // Last resort: return original response
    return strdup(response);
}

char *enrich_sequence(char *sequence, khash_t(strSet) * missing_message_types)
{
    const char *prompt_template =
        "CRITICAL: Return ONLY the modified sequence, NO explanations, NO analysis, NO markdown.\\n\\n"
        "Original sequence:\\n"
        "%.*s\\n\\n"
        "Task: Add these message types: %.*s\\n"
        "Insert them at appropriate locations.\\n\\n"
        "Output format: Return ONLY the sequence, one command per line, like this:\\n"
        "```\\n"
        "USER username\\n"
        "PASS password\\n"
        "CWD /path\\n"
        "QUIT\\n"
        "```\\n\\n"
        "Modified sequence:";

    int missing_fields_len = 0;
    int missing_fields_capacity = 100;
    char *missing_fields_seq = ck_alloc(missing_fields_capacity);

    khiter_t k;
    int i = 0;
    for (k = kh_begin(missing_message_types); 
    k != kh_end(missing_message_types) && i < min(MAX_ENRICHMENT_MESSAGE_TYPES, kh_size(missing_message_types)); 
    ++k)
    {
        if (!kh_exist(missing_message_types, k))
            continue;
        ++i; // Increment only after seeing a message type
        const char *message_type = kh_key(missing_message_types, k);
        int needed_len = strlen(message_type) + 2; // add for the ', '

        if (missing_fields_len + needed_len > missing_fields_capacity)
        {
            missing_fields_capacity += 2 * needed_len;
            missing_fields_seq = ck_realloc(missing_fields_seq, missing_fields_capacity);
        }

        memcpy(missing_fields_seq + missing_fields_len, message_type, strlen(message_type));
        memcpy(missing_fields_seq + missing_fields_len + needed_len - 2, ", ", 2);

        missing_fields_len += needed_len;
    }
    missing_fields_len -= 2; // ignore the last ', '

    char *prompt = NULL;

    json_object *sequence_escaped = json_object_new_string(sequence);
    const char *sequence_escaped_str = json_object_to_json_string(sequence_escaped);
    sequence_escaped_str++;

    int sequence_len = strlen(sequence_escaped_str) - 1;
    int allowed_tokens = (MAX_TOKENS - strlen(prompt_template) - missing_fields_len);
    if (sequence_len > allowed_tokens)
    {
        sequence_len = allowed_tokens;
    }
    asprintf(&prompt, prompt_template, sequence_len, sequence_escaped_str, missing_fields_len, missing_fields_seq);
    ck_free(missing_fields_seq);
    json_object_put(sequence_escaped);

    char *response = chat_with_llm(prompt, "instruct", ENRICHMENT_RETRIES, 0.5);

    free(prompt);

    if (response == NULL) {
        return NULL;
    }

    // Extract pure sequence from response (remove explanations, markdown, etc.)
    char *clean_response = extract_sequence_from_response(response);
    free(response);
    
    if (clean_response == NULL) {
        return NULL;
    }
    
    // Remove markdown code block markers if still present
    char *cleaned = clean_response;
    if (cleaned[0] == '`' && cleaned[1] == '`' && cleaned[2] == '`') {
        cleaned += 3;
        // Skip language identifier
        while (*cleaned == '\n' || *cleaned == ' ' || 
               (*cleaned >= 'a' && *cleaned <= 'z') ||
               (*cleaned >= 'A' && *cleaned <= 'Z')) {
            cleaned++;
        }
        // Remove trailing ```
        char *end = strstr(cleaned, "```");
        if (end) {
            *end = '\0';
        }
    }
    
    // Trim whitespace
    while (*cleaned == ' ' || *cleaned == '\n' || *cleaned == '\r' || *cleaned == '\t') {
        cleaned++;
    }
    char *end = cleaned + strlen(cleaned) - 1;
    while (end > cleaned && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    
    // If we modified the pointer, need to create new string
    if (cleaned != clean_response) {
        char *result = strdup(cleaned);
        free(clean_response);
        return result;
    }
    
    return clean_response;
}

// Parse constraint from string like "<<INTEGER:0-65535>>" or "<<ENUM:GET,POST>>"
type_constraint_t *parse_constraint(const char *str)
{
    if (!str || str[0] != '<' || str[1] != '<')
        return NULL;
    
    type_constraint_t *constraint = (type_constraint_t *)ck_alloc(sizeof(type_constraint_t));
    constraint->type = CONSTRAINT_NONE;
    
    // Skip "<<"
    const char *start = str + 2;
    const char *end = strstr(start, ">>");
    if (!end)
    {
        constraint->type = CONSTRAINT_NONE;
        return constraint;
    }
    
    size_t len = end - start;
    char *constraint_str = (char *)ck_alloc(len + 1);
    strncpy(constraint_str, start, len);
    constraint_str[len] = '\0';
    
    // Check for VALUE (no constraint)
    if (strncmp(constraint_str, "VALUE", 5) == 0)
    {
        constraint->type = CONSTRAINT_NONE;
        ck_free(constraint_str);
        return constraint;
    }
    
    // Check for INTEGER:min-max
    if (strncmp(constraint_str, "INTEGER:", 8) == 0)
    {
        constraint->type = CONSTRAINT_INTEGER;
        const char *range = constraint_str + 8;
        char *dash = strchr(range, '-');
        if (dash)
        {
            constraint->constraint.integer_range.min = atoi(range);
            constraint->constraint.integer_range.max = atoi(dash + 1);
        }
        else
        {
            // No range specified, use default
            constraint->constraint.integer_range.min = 0;
            constraint->constraint.integer_range.max = 65535;
        }
        ck_free(constraint_str);
        return constraint;
    }
    
    // Check for STRING:min-max
    if (strncmp(constraint_str, "STRING:", 7) == 0)
    {
        constraint->type = CONSTRAINT_STRING;
        const char *range = constraint_str + 7;
        char *dash = strchr(range, '-');
        if (dash)
        {
            constraint->constraint.string_range.min_len = atoi(range);
            constraint->constraint.string_range.max_len = atoi(dash + 1);
        }
        else
        {
            constraint->constraint.string_range.min_len = 1;
            constraint->constraint.string_range.max_len = 256;
        }
        ck_free(constraint_str);
        return constraint;
    }
    
    // Check for ENUM:val1,val2,val3
    if (strncmp(constraint_str, "ENUM:", 5) == 0)
    {
        constraint->type = CONSTRAINT_ENUM;
        const char *values = constraint_str + 5;
        
        // Count commas to determine number of values
        int count = 1;
        const char *p = values;
        while (*p)
        {
            if (*p == ',') count++;
            p++;
        }
        
        constraint->constraint.enum_values.count = count;
        constraint->constraint.enum_values.values = (char **)ck_alloc(count * sizeof(char *));
        
        // Parse values
        char *values_copy = strdup(values);
        char *token = strtok(values_copy, ",");
        int i = 0;
        while (token && i < count)
        {
            // Trim whitespace
            while (*token == ' ') token++;
            char *end_token = token + strlen(token) - 1;
            while (end_token > token && *end_token == ' ') *end_token-- = '\0';
            
            constraint->constraint.enum_values.values[i] = strdup(token);
            token = strtok(NULL, ",");
            i++;
        }
        free(values_copy);
        ck_free(constraint_str);
        return constraint;
    }
    
    // Check for IP
    if (strcmp(constraint_str, "IP") == 0)
    {
        constraint->type = CONSTRAINT_IP;
        ck_free(constraint_str);
        return constraint;
    }
    
    // Check for PATH
    if (strcmp(constraint_str, "PATH") == 0)
    {
        constraint->type = CONSTRAINT_PATH;
        ck_free(constraint_str);
        return constraint;
    }
    
    // Check for HEX
    if (strcmp(constraint_str, "HEX") == 0)
    {
        constraint->type = CONSTRAINT_HEX;
        ck_free(constraint_str);
        return constraint;
    }
    
    // Unknown constraint type, treat as VALUE
    constraint->type = CONSTRAINT_NONE;
    ck_free(constraint_str);
    return constraint;
}

    // Free constraint memory
void free_constraint(type_constraint_t *constraint)
{
    if (!constraint)
        return;
    
    if (constraint->type == CONSTRAINT_ENUM)
    {
        for (int i = 0; i < constraint->constraint.enum_values.count; i++)
        {
            if (constraint->constraint.enum_values.values[i])
                free(constraint->constraint.enum_values.values[i]);
        }
        ck_free(constraint->constraint.enum_values.values);
    }
    
    ck_free(constraint);
}

// Generate a value according to the constraint
char *generate_value_by_constraint(type_constraint_t *constraint)
{
    if (!constraint || constraint->type == CONSTRAINT_NONE)
    {
        // Generate a random string
        int len = 10 + (random() % 20);
        char *value = (char *)ck_alloc(len + 1);
        for (int i = 0; i < len; i++)
        {
            value[i] = 32 + (random() % 95); // Printable ASCII
        }
        value[len] = '\0';
        return value;
    }
    
    switch (constraint->type)
    {
        case CONSTRAINT_INTEGER:
        {
            int min = constraint->constraint.integer_range.min;
            int max = constraint->constraint.integer_range.max;
            int value = min + (random() % (max - min + 1));
            char *str = (char *)ck_alloc(32);
            snprintf(str, 32, "%d", value);
            return str;
        }
        
        case CONSTRAINT_STRING:
        {
            int min_len = constraint->constraint.string_range.min_len;
            int max_len = constraint->constraint.string_range.max_len;
            int len = min_len + (random() % (max_len - min_len + 1));
            char *value = (char *)ck_alloc(len + 1);
            for (int i = 0; i < len; i++)
            {
                value[i] = 32 + (random() % 95); // Printable ASCII
            }
            value[len] = '\0';
            return value;
        }
        
        case CONSTRAINT_ENUM:
        {
            int count = constraint->constraint.enum_values.count;
            if (count > 0)
            {
                int idx = random() % count;
                return strdup(constraint->constraint.enum_values.values[idx]);
            }
            return strdup("");
        }
        
        case CONSTRAINT_IP:
        {
            char *ip = (char *)ck_alloc(16);
            snprintf(ip, 16, "%d.%d.%d.%d", 
                     (int)(random() % 256), (int)(random() % 256), 
                     (int)(random() % 256), (int)(random() % 256));
            return ip;
        }
        
        case CONSTRAINT_PATH:
        {
            const char *path_prefixes[] = {"/", "/tmp/", "/var/", "/usr/", "/home/"};
            const char *path_suffixes[] = {"file", "test", "data", "config", "log"};
            int prefix_idx = random() % (sizeof(path_prefixes) / sizeof(path_prefixes[0]));
            int suffix_idx = random() % (sizeof(path_suffixes) / sizeof(path_suffixes[0]));
            int num = random() % 1000;
            char *path = (char *)ck_alloc(256);
            snprintf(path, 256, "%s%s%d", path_prefixes[prefix_idx], path_suffixes[suffix_idx], num);
            return path;
        }
        
        case CONSTRAINT_HEX:
        {
            int len = 4 + (random() % 20);
            char *hex = (char *)ck_alloc(len + 1);
            const char *hex_chars = "0123456789ABCDEFabcdef";
            for (int i = 0; i < len; i++)
            {
                hex[i] = hex_chars[random() % 22];
            }
            hex[len] = '\0';
            return hex;
        }
        
        default:
        {
            char *value = (char *)ck_alloc(16);
            snprintf(value, 16, "value%d", (int)(random() % 10000));
            return value;
        }
    }
}

// Mutate a value according to the constraint
void mutate_value_by_constraint(u8 *buf, u32 len, type_constraint_t *constraint, u32 offset)
{
    if (!constraint || constraint->type == CONSTRAINT_NONE || offset >= len)
        return;
    
    u32 available_len = len - offset;
    if (available_len == 0)
        return;
    
    switch (constraint->type)
    {
        case CONSTRAINT_INTEGER:
        {
            // Try to parse as integer and mutate
            char *str = (char *)ck_alloc(available_len + 1);
            memcpy(str, buf + offset, available_len);
            str[available_len] = '\0';
            
            int value = atoi(str);
            int min = constraint->constraint.integer_range.min;
            int max = constraint->constraint.integer_range.max;
            
            // Mutate: add/subtract small value, or set to boundary
            int mutation_type = (int)(random() % 4);
            switch (mutation_type)
            {
                case 0: value += (random() % 10) - 5; break; // Small change
                case 1: value = min; break; // Min boundary
                case 2: value = max; break; // Max boundary
                case 3: value = min + (random() % (max - min + 1)); break; // Random in range
            }
            
            // Clamp to range
            if (value < min) value = min;
            if (value > max) value = max;
            
            // Write back
            char new_str[32];
            int new_len = snprintf(new_str, 32, "%d", value);
            if (new_len < available_len)
            {
                memcpy(buf + offset, new_str, new_len);
                // Pad with spaces if needed
                for (int i = new_len; i < available_len && i < len; i++)
                {
                    buf[offset + i] = ' ';
                }
            }
            else
            {
                memcpy(buf + offset, new_str, available_len);
            }
            ck_free(str);
            break;
        }
        
        case CONSTRAINT_STRING:
        {
            int min_len = constraint->constraint.string_range.min_len;
            // int max_len = constraint->constraint.string_range.max_len; // Not used currently
            
            // Mutate: change random character, or change length
            int mutation_type = (int)(random() % 3);
            switch (mutation_type)
            {
                case 0: // Change random character
                    if (available_len > 0)
                    {
                        int pos = random() % available_len;
                        buf[offset + pos] = 32 + (random() % 95);
                    }
                    break;
                case 1: // Set to min length
                    if (min_len < available_len)
                    {
                        for (int i = min_len; i < available_len && i < len; i++)
                        {
                            buf[offset + i] = ' ';
                        }
                    }
                    break;
                case 2: // Inject special characters
                    if (available_len > 0)
                    {
                        const char *special = "\x00\x01\xff\n\r\t";
                        int pos = random() % available_len;
                        buf[offset + pos] = special[random() % 6];
                    }
                    break;
            }
            break;
        }
        
        case CONSTRAINT_ENUM:
        {
            // Switch to a different enum value
            int count = constraint->constraint.enum_values.count;
            if (count > 0)
            {
                int idx = random() % count;
                const char *new_value = constraint->constraint.enum_values.values[idx];
                int new_len = strlen(new_value);
                if (new_len < available_len)
                {
                    memcpy(buf + offset, new_value, new_len);
                    // Pad with spaces
                    for (int i = new_len; i < available_len && i < len; i++)
                    {
                        buf[offset + i] = ' ';
                    }
                }
                else
                {
                    memcpy(buf + offset, new_value, available_len);
                }
            }
            break;
        }
        
        case CONSTRAINT_IP:
        {
            // Mutate IP address
            int mutation_type = (int)(random() % 3);
            switch (mutation_type)
            {
                case 0: // Random IP
                {
                    char ip[16];
                    snprintf(ip, 16, "%d.%d.%d.%d", 
                             (int)(random() % 256), (int)(random() % 256), 
                             (int)(random() % 256), (int)(random() % 256));
                    int ip_len = strlen(ip);
                    if (ip_len < available_len)
                    {
                        memcpy(buf + offset, ip, ip_len);
                    }
                    else
                    {
                        memcpy(buf + offset, ip, available_len);
                    }
                    break;
                }
                case 1: // Invalid IP format
                {
                    const char *invalid = "999.999.999.999";
                    int invalid_len = strlen(invalid);
                    memcpy(buf + offset, invalid, invalid_len < available_len ? invalid_len : available_len);
                    break;
                }
                case 2: // Boundary IP
                {
                    const char *boundary = "0.0.0.0";
                    memcpy(buf + offset, boundary, strlen(boundary) < available_len ? strlen(boundary) : available_len);
                    break;
                }
            }
            break;
        }
        
        case CONSTRAINT_PATH:
        {
            // Mutate path: add traversal, special chars, etc.
            int mutation_type = (int)(random() % 3);
            switch (mutation_type)
            {
                case 0: // Path traversal
                {
                    const char *traversal = "../../../etc/passwd";
                    int trav_len = strlen(traversal);
                    memcpy(buf + offset, traversal, trav_len < available_len ? trav_len : available_len);
                    break;
                }
                case 1: // Special characters
                {
                    if (available_len > 0)
                    {
                        buf[offset] = '/';
                        for (int i = 1; i < available_len && i < len; i++)
                        {
                            buf[offset + i] = 32 + (random() % 95);
                        }
                    }
                    break;
                }
                case 2: // Long path
                {
                    const char *long_path = "/very/long/path/that/exceeds/normal/length";
                    int path_len = strlen(long_path);
                    memcpy(buf + offset, long_path, path_len < available_len ? path_len : available_len);
                    break;
                }
            }
            break;
        }
        
        case CONSTRAINT_HEX:
        {
            // Mutate hex: change random character
            const char *hex_chars = "0123456789ABCDEFabcdef";
            if (available_len > 0)
            {
                int pos = random() % available_len;
                buf[offset + pos] = hex_chars[random() % 22];
            }
            break;
        }
        
        default:
            // Generic mutation: flip random bit
            if (available_len > 0)
            {
                int pos = random() % available_len;
                buf[offset + pos] ^= (1 << (random() % 8));
            }
            break;
    }
}
