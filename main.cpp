#include "mbed.h"
#include "string.h"
#include "NetworkInterface.h"
#include "BG96Interface.h"
#include "BG96.h"
#include "BG96MQTTClient.h"
#include "BG96TLSSocket.h"
#include "MQTT_server_setting.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed_events.h"
#include "NTPClient.h"
#include "mbedtls/error.h"
#include "azure_c_shared_utility/azure_c_shared_utility/umock_c_prod.h"
#include "azure_c_shared_utility/azure_c_shared_utility/strings.h"
#include "azure_c_shared_utility/azure_c_shared_utility/sastoken.h"
#include "azure_c_shared_utility/azure_c_shared_utility/agenttime.h" 
#include "azure_c_shared_utility/azure_c_shared_utility/urlencode.h" 
#include "azure_c_shared_utility/azure_c_shared_utility/hmacsha256.h" 
#include "azure_c_shared_utility/azure_c_shared_utility/base64.h" 
#include "azure_c_shared_utility/azure_c_shared_utility/xlogging.h" 
#include "azure_c_shared_utility/azure_c_shared_utility/crt_abstractions.h" 

#define INDEFINITE_TIME ((time_t)-1)

unsigned char generated_sas[256];

bool running;
/* This function looks for every occurences of the token in initial string and replaces each one by replacement */
size_t replace_str(char * initial, char * token, char * replacement) {
    if ( initial == NULL || token == NULL || replacement == NULL ) return 0;
    printf("SAS before replacement:\r\n");
    printf("%s\r\n",initial);
    STRING_HANDLE buffer = STRING_new();
    char * str = initial;
    char * ptr = NULL;
    ptr = strtok(str,token);
    if (ptr != NULL) STRING_concat(buffer, ptr);
    while( (ptr = strtok(NULL,token)) != NULL) {
        STRING_concat(buffer,replacement);
        STRING_concat(buffer,ptr);
    }
    strcpy(initial,STRING_c_str(buffer));
    STRING_delete(buffer);
    printf("SAS after replacement:\r\n");
    printf("%s\r\n",initial);
    return strlen(initial);
}

static int get_seconds_since_epoch(size_t* seconds)
{
    int result;
    time_t current_time;
    if ((current_time = get_time(NULL)) == INDEFINITE_TIME)
    {
        LogError("Failed getting the current local time (get_time() failed)");
        result = __FAILURE__;
    }
    else
    {
        *seconds = (size_t)get_difftime(current_time, (time_t)0);
        result = 0;
    }
    return result;
}

int SignAuthPayload(const char* key, const char* stringToSign, unsigned char** output, size_t* len)
{
    int result;
    size_t payload_len = strlen(stringToSign);
    if (key == NULL || stringToSign == NULL)
    {
        LogError("Invalid parameters passed to sign function");
        printf("Invalid parameters passed to sign function\r\n");
        result = __FAILURE__;
    }
    else
    {
        BUFFER_HANDLE decoded_key;
        BUFFER_HANDLE output_hash;

        if ((decoded_key = Base64_Decoder(key)) == NULL)
        {
            LogError("Failed decoding symmetrical key");
            printf("Failed decoding symmetrical key\r\n");
            result = __FAILURE__;
        }
        else if ((output_hash = BUFFER_new()) == NULL)
        {
            LogError("Failed allocating output hash buffer");
            printf("Failed allocating output hash buffer\r\n");
            BUFFER_delete(decoded_key);
            result = __FAILURE__;
        }
        else if (HMACSHA256_ComputeHash(BUFFER_u_char(decoded_key), BUFFER_length(decoded_key), (const unsigned char*)stringToSign, payload_len, output_hash) != HMACSHA256_OK)
        {
            LogError("Failed computing HMAC Hash");
            printf("Failed computing HMAC Hash\r\n");
            BUFFER_delete(decoded_key);
            BUFFER_delete(output_hash);
            result = __FAILURE__;
        }
        else
        {
            *len = BUFFER_length(output_hash);
            if (*len > 256)
            {
                LogError("Generated SAS token length longer than storage buffer");
                printf("Generated SAS token length longer than storage buffer\r\n");
                result = __FAILURE__;
            }
            else
            {
                memcpy(generated_sas, BUFFER_u_char(output_hash), *len+1);
                *output = generated_sas;
                result = 0;
            }
            BUFFER_delete(decoded_key);
            BUFFER_delete(output_hash);
        }
    }
    return result;
}

size_t generate_sas_token(char *out, const char * resourceUri, const char * key, const char * policyName, int expiryInSeconds)
{
    size_t result = 1;
    size_t sec_since_epoch;
    STRING_HANDLE scope = STRING_construct(resourceUri);
    STRING_HANDLE encoded_uri = URL_Encode(scope);
    if (get_seconds_since_epoch(&sec_since_epoch) != 0)
    {
        /* Codes_SRS_IoTHub_Authorization_07_020: [ If any error is encountered IoTHubClient_Auth_Get_ConnString shall return NULL. ] */
        LogError("failure getting seconds from epoch");
        printf("failure getting seconds from epoch\r\n");
        result = 0;
    }
    else 
    {
        char expiry_token[16] = {0};
        size_t expiry_time = sec_since_epoch+expiryInSeconds;
        if (size_tToString(expiry_token, sizeof(expiry_token), expiry_time) != 0) {
            LogError("Failure when creating expire token");
            printf("Failure when creating expire token\r\n");
            result = 0;
        } else {
            unsigned char* data_value;
            size_t data_len;
            STRING_HANDLE string_to_sign = STRING_construct_sprintf("%s\n%s",STRING_c_str(encoded_uri),expiry_token);
            if (SignAuthPayload(key, STRING_c_str(string_to_sign), &data_value, &data_len) == 0) {
                STRING_HANDLE urlEncodedSignature = NULL;
                STRING_HANDLE signature = NULL;
                signature = Base64_Encode_Bytes(data_value, data_len);
                if (signature == NULL)
                {
                    result = 0;
                    LogError("Failure constructing encoding.");
                    printf("Failure constructing encoding.\r\n");
                }
                else if ((urlEncodedSignature = URL_Encode(signature)) == NULL)
                {
                    result = 0;
                    LogError("Failure constructing url Signature.");
                    printf("Failure constructing url Signature.\r\n");
                }
                char * buffer = (char *)malloc(25+STRING_length(encoded_uri)+1+
                                        5+STRING_length(urlEncodedSignature)+1+
                                        4+strlen(expiry_token)+1+
                                        5+strlen(policyName))+1+8;
                if (buffer == NULL) {
                    LogError("Failure allocating the buffer for the sas_token.");
                    printf("Failure allocating the buffer for the sas_token.\r\n");
                    result = 0;
                } 
                sprintf(buffer,"SharedAccessSignature sr=%s&sig=%s&se=%s", STRING_c_str(encoded_uri), 
                                                                           STRING_c_str(urlEncodedSignature),
                                                                           expiry_token);
                if (policyName != NULL) {
                    strcat(buffer, "&skn=");
                    strcat(buffer, policyName);
                }
                printf("DEBUG: generate_sas_token SAS was generated.\r\n");
                if (strlen(buffer) > BG96MQTTCLIENT_MAX_SAS_TOKEN_LENGTH) {
                    printf("DEBUG: generate_sas_token. Generated SAS token is too large.\r\n");
                    result = 0;
                } else {
                    strcpy(out, buffer);
                    result = 1;
                }
                STRING_delete(urlEncodedSignature);
                STRING_delete(signature);
            }
            STRING_delete(string_to_sign);
            //free(data_value);
        }     
    }
    STRING_delete(encoded_uri);
    STRING_delete(scope);
    return result ? result:strlen(out);
}

// void release_message(MQTTMessage *msg){
//     printf("Entering release message\r\n");
//     if (msg != NULL) {
//         char * cpayload = msg->msg.payload;
//         msg->msg.payload = NULL;
//         char * ctopic = msg->topic.payload;
//         if (cpayload != NULL) free(cpayload); 
//         if (ctopic !=NULL) free(ctopic);
//         free(msg);
//         msg = NULL;
//     }
//     printf("Leaving release message\r\n");
// }

void message_handler(MQTTMessage *msg)
{
    if (msg == NULL) {
        printf("Message handler called with null pointer.\r\n");
        return;
    }
    printf("Received MQTT message on topic %s\r\n", msg->topic.payload);
    printf("Message payload is -> \r\n");
    printf("%s\r\n",msg->msg.payload);
    if (strcmp(msg->msg.payload,"stop")==0) running = false;
    //release_message(msg);
    //running = false;
}

int main(void)
{
    char username[256]={0};
    char clientid[80] = {0};
    char sas_token[BG96MQTTCLIENT_MAX_SAS_TOKEN_LENGTH] = {0};
    int rc = -1;
    BG96Interface* bg96 = NULL;
    BG96MQTTClient* mqtt = NULL;
    NetworkInterface* itf = NULL;
    //BG96TLSSocket* tls;

    running = true;
    printf("Starting MQTTClient test program...\r\n");

    printf("Instantiate BG96Interface\r\n");

    bg96 = new BG96Interface();

    if (bg96 == NULL) return -1;

    printf("Succesfully instantiated BG96 Interface.\r\n");

    printf("Initialize BG96 modem...\r\n");

    if (!bg96->initializeBG96()) {
        printf("Error when initializing BG96 modem.\r\n");
        return -1;
    }

    printf("Succesfully initialized BG96 modem.\r\n");

    printf("Instantiate MQTT Client object...\r\n");

    mqtt = bg96->getBG96MQTTClient(NULL);

    if (mqtt == NULL) return -1;

    printf("Succesfully instantiated MQTT Client.\r\n");

    BG96_PDP_Ctx pdp_ctx;
    pdp_ctx.pdp_id = DEFAULT_PDP; 
    pdp_ctx.apn = (const char*)DEFAULT_APN;
    printf("APN will be set to %s\r\n",pdp_ctx.apn);
    char apn_username[11];
    char apn_password[11];
    strcpy(apn_username,"Ianthomson");
    strcpy(apn_password, "Waleed29");
    pdp_ctx.username = apn_username;
    pdp_ctx.password = apn_password;

    printf("Configuring PDP context...\r\n");
    rc = mqtt->configure_pdp_context(&pdp_ctx);

    if (rc < 0) {
        printf("Error when configuring pdp context %d.\r\n", pdp_ctx.pdp_id);
        return -1;
    }
    printf("Succesfully configured pdp context %d.\r\n", pdp_ctx.pdp_id);

    printf("Activate PDP context...\r\n");

    rc= bg96->connect();

    if (!rc){
        printf("Error when activating the PDP context.\r\n");
        return -1;
    }

    printf("Succesfully activated the PDP context.\r\n");

    

    printf("Now trying to set system time...\r\n");

    time_t current_time;

    if (bg96->getNetworkGMTTime(&current_time) != NSAPI_ERROR_OK) {
        itf = (NetworkInterface *)bg96; // We need a NetworkInterface reference for NTPClient
        NTPClient ntp = NTPClient(itf);
        current_time = ntp.get_timestamp();
    }

    set_time(current_time);
    char buffer[32];
    current_time = time(NULL);
    strftime(buffer, 32, "%I:%M %p\n", localtime(&current_time));

    printf("Time is now %s\r\n", buffer);


    MQTTClientOptions mqtt_options = BG96MQTTClientOptions_Initializer;
    mqtt_options.will_qos       = 0;
    mqtt_options.cleansession   = 0;
    mqtt_options.sslenable      = 1;

    printf("Configuring MQTT options...\r\n");

    rc = mqtt->configure_mqtt(&mqtt_options);

    if (rc < 0 ) {
        printf("Error when configuring MQTT options (%d)\r\n", rc);
        return -1;
    }

    printf("Succesfully configured MQTT options\r\n");

    MQTTConstString cacert;
    cacert.payload=SSL_CA_PEM;
    cacert.len = strlen(cacert.payload);

    MQTTNetwork_Ctx network_ctx;

    network_ctx.ca_cert = cacert;
    network_ctx.client_cert.payload = SSL_CLIENT_CERT_PEM;
    network_ctx.client_cert.len = strlen(SSL_CLIENT_CERT_PEM);
    network_ctx.client_key.payload  = SSL_CLIENT_PRIVATE_KEY_PEM;
    network_ctx.client_key.len = strlen(SSL_CLIENT_PRIVATE_KEY_PEM);
    network_ctx.hostname.payload = (const char*)MQTT_SERVER_HOST_NAME;
    network_ctx.port = MQTT_SERVER_PORT;

    printf("Opening a network socket to %s:%d\r\n", network_ctx.hostname.payload, network_ctx.port);

    rc = mqtt->open(&network_ctx);

    if (rc < 0) {
        printf("Error opening the network socket (%d)\r\n", rc);
//        tls = bg96->getBG96TLSSocket();
//        tls->connect(network_ctx.hostname.payload, network_ctx.port);
        return -1;
    }
    printf("Successfully opened a network socket.\r\n");

    char scope[80] = {0};
    int i = 0;
    for (i = 0; i < 3; i++) {
        printf("Generating SAS token...\r\n");

        sprintf(scope,"%s/devices/%s", MQTT_SERVER_HOST_NAME, DEVICE_ID);
        //STRING_HANDLE scope = STRING_construct_sprintf("%s/devices/%s", MQTT_SERVER_HOST_NAME, DEVICE_ID);
        generate_sas_token(sas_token, scope, DEVICE_KEY, NULL, AZURE_IOTHUB_SAS_TOKEN_DEFAULT_EXPIRY_TIME);
        if (sas_token == NULL){
            printf("Error when generating SAS token.\r\n");
            return -1;
        } 
        printf("Generated a new SAS Token: \r\n");
        printf("%s\r\n", sas_token);
        replace_str(sas_token, (char *)"%", (char *)"%%");

        strcpy(clientid, DEVICE_ID);
        MQTTConnect_Ctx connect_ctx;
        connect_ctx.client_id.payload = clientid;
        connect_ctx.client_id.len = strlen(connect_ctx.client_id.payload);
        connect_ctx.username.payload = username;
        strcpy(connect_ctx.username.payload,MQTT_SERVER_HOST_NAME);
        strcat(connect_ctx.username.payload,"/");
        strcat(connect_ctx.username.payload,DEVICE_ID);
        strcat(connect_ctx.username.payload,"/");
        strcat(connect_ctx.username.payload,"api-version=2018-06-30");
        connect_ctx.username.len = strlen(connect_ctx.username.payload);
        connect_ctx.password.payload = sas_token;
        connect_ctx.password.len = strlen(connect_ctx.password.payload);

        printf("Connecting to the IoT Hub server %s...\r\n",network_ctx.hostname.payload);
   
        rc = mqtt->connect(&connect_ctx);
        connect_ctx.client_id.payload = NULL;
        connect_ctx.password.payload = NULL;

        if (rc == 0) {
            break;
        } else {
            mqtt->disconnect();
        }
    }
    if (rc < 0) {
        printf("Error (%d) connecting to the IoT Hub server.\r\n", rc);
        return -1;
    }

    printf("Succesful connection to the IoT Hub server.\r\n");

    char topictoreadfrom[128] = "devices/";
    strcat(topictoreadfrom, DEVICE_ID);
    strcat(topictoreadfrom,"/messages/devicebound/#");

    char topictowriteto[128] = "devices/";
    strcat(topictowriteto, DEVICE_ID);
    strcat(topictowriteto, "/messages/events/");

    printf("Subscribing to topic %s\r\n", topictoreadfrom);

    if (mqtt->subscribe(topictoreadfrom, 0, message_handler) < 0) {
        printf("Error while subcribing to topic %s.\r\n", topictoreadfrom);
    } else {
        printf("Successfully subscribred to topic %s\r\n", topictoreadfrom);
    }

    printf("Publishing message to topic %s\r\n", topictowriteto);

    char hello[80] ="Hello from ";
    strcat(hello, DEVICE_ID);

    MQTTMessage msgtopublish;
    msgtopublish.qos = 1;
    msgtopublish.retain = 0;
    msgtopublish.topic.payload = topictowriteto;
    msgtopublish.topic.len = strlen(topictowriteto);
    msgtopublish.msg.len = strlen(hello);
    msgtopublish.msg.payload = hello;

    rc = mqtt->publish(&msgtopublish);

    if (rc < 0) {
        printf("Error while trying to publish message.\r\n");
        return -1;
    }

    printf ("Succesfully published message.\r\n");

    if (mqtt->dowork() < 0) {
        printf("Error when starting mqtt task thread\r\n");
        return -1;
    }

    printf("Successfully started the mqtt task thread.\r\n");

    while(running) {wait(1);}

    printf("Disconnecting from server...\r\n");

    mqtt->disconnect();
    printf("Closing network socket...\r\n");
    mqtt->close();
    printf("Program end.\r\n");
    delete(mqtt);
    delete(bg96);
    return 1;
}