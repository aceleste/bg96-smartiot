#include "mbed.h"
#include "string.h"
#include "ConnectionManager.h"
#include "NetworkInterface.h"
#include "BG96Interface.h"
#include "BG96MQTTClient.h"
#include "BG96.h"
#include "MQTT_server_setting.h"
#include "MQTTClient_Settings.h"
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

bool _timeout_triggered;

void device_to_system_timeout(void);
void system_to_device_timeout();
void system_to_device_message_handler(MQTTMessage *msg, void *param);

ConnectionManager::ConnectionManager(BG96Interface *bg96)
{
    _bg96 = bg96;
    _mqtt = _bg96->getBG96MQTTClient(NULL);
    _rssi = 0.0;
    _log_m = NULL;
}

ConnectionManager::~ConnectionManager()
{
    _bg96->disconnect();
    _bg96->powerDown();
}

/* This function looks for every occurences of the token in initial string and replaces each one by replacement */
size_t ConnectionManager::replace_str(char * initial, char * token, char * replacement) {
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

int ConnectionManager::get_seconds_since_epoch(size_t* seconds)
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

int ConnectionManager::SignAuthPayload(const char* key, const char* stringToSign, unsigned char** output, size_t* len)
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
            if (*len > 80)
            {
                LogError("Generated signature token length longer than storage buffer");
                printf("Generated signature token length longer than storage buffer\r\n");
                result = __FAILURE__;
            }
            else
            {
                memcpy(generated_sig, BUFFER_u_char(output_hash), *len+1);
                *output = generated_sig;
                result = 0;
            }
            BUFFER_delete(decoded_key);
            BUFFER_delete(output_hash);
        }
    }
    return result;
}

size_t ConnectionManager::generate_sas_token(char *out, const char * resourceUri, const char * key, const char * policyName, int expiryInSeconds)
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
                // char * buffer = (char *)malloc(25+STRING_length(encoded_uri)+1+
                //                         5+STRING_length(urlEncodedSignature)+1+
                //                         4+strlen(expiry_token)+1+
                //                         5+strlen(policyName))+1+8;
                // if (buffer == NULL) {
                //     LogError("Failure allocating the buffer for the sas_token.");
                //     printf("Failure allocating the buffer for the sas_token.\r\n");
                //     result = 0;
                // } 
                if (34+STRING_length(encoded_uri)+STRING_length(urlEncodedSignature)+strlen(expiry_token)+5+strlen(policyName) > BG96MQTTCLIENT_MAX_SAS_TOKEN_LENGTH-1) {
                    printf("Error - the generated SAS token is longer than the storage buffer.\r\n");
                    result = 0;
                } else {
                    sprintf(out,"SharedAccessSignature sr=%s&sig=%s&se=%s", STRING_c_str(encoded_uri), 
                                                                                      STRING_c_str(urlEncodedSignature),
                                                                                      expiry_token);
                    if (policyName != NULL) {
                        strcat(out, "&skn=");
                        strcat(out, policyName);
                    }
                    result = 1;
                }   
                STRING_delete(urlEncodedSignature);
                STRING_delete(signature);
            }
            STRING_delete(string_to_sign);
        }     
    }
    STRING_delete(encoded_uri);
    STRING_delete(scope);
    return result ? result:strlen(out);
}

void ConnectionManager::newSystemMessage(char *msg)
{
    _system_message = msg;
    _msg_received = true;
}

void system_to_device_message_handler(MQTTMessage *msg, void *param)
{
    if (msg == NULL || param == NULL) {
        printf("Message handler called with null pointer.\r\n");
        return;
    }
    ConnectionManager *conn_m = (ConnectionManager *)param;
    // printf("Received MQTT message on topic %s\r\n", msg->topic.payload);
    // printf("Message payload is -> \r\n");
    // printf("%s\r\n",msg->msg.payload);
    if (strlen(msg->msg.payload) > BG96_MQTT_CLIENT_MAX_PUBLISH_MSG_SIZE)  return;
    conn_m->newSystemMessage(msg->msg.payload);
}

void system_to_device_timeout()
{
    _timeout_triggered = true;
}

void ConnectionManager::getRSSI(double &rssi)
{
    rssi = _rssi;
}

int ConnectionManager::connectToServer()
{
    int rc;
    BG96_PDP_Ctx pdp_ctx;
    pdp_ctx.pdp_id = DEFAULT_PDP; 
    pdp_ctx.apn = (const char*)DEFAULT_APN;
    printf("APN will be set to %s\r\n",pdp_ctx.apn);
    char apn_username[11]={0};
    char apn_password[11]={0};
#ifdef APN_USERNAME
    strcpy(apn_username,(const char*)APN_USERNAME);
#endif

#ifdef APN_PASSWORD
    strcpy(apn_password, (const char*)APN_PASSWORD);
#endif
    pdp_ctx.username = apn_username;
    pdp_ctx.password = apn_password;

    printf("Configuring PDP context...\r\n");
    rc = _mqtt->configure_pdp_context(&pdp_ctx);

    if (rc < 0) {
        printf("Error when configuring pdp context %d.\r\n", pdp_ctx.pdp_id);
        return -1;
    }
    printf("Succesfully configured pdp context %d.\r\n", pdp_ctx.pdp_id);

    printf("Activate PDP context...\r\n");

    rc= _bg96->connect();

    if (!rc){
        printf("Error when activating the PDP context.\r\n");
        return -1;
    }

    printf("Succesfully activated the PDP context.\r\n");

    

    printf("Now trying to set system time...\r\n");

    time_t current_time;

    if (_bg96->getNetworkGMTTime(&current_time) != NSAPI_ERROR_OK) {
        NetworkInterface *itf = (NetworkInterface *)&_bg96; // We need a NetworkInterface reference for NTPClient
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

    rc = _mqtt->configure_mqtt(&mqtt_options);

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

    rc = _mqtt->open(&network_ctx);

    if (rc < 0) {
        printf("Error opening the network socket (%d)\r\n", rc);
//        tls = bg96->getBG96TLSSocket();
//        tls->connect(network_ctx.hostname.payload, network_ctx.port);
        return -1;
    }
    printf("Successfully opened a network socket.\r\n");

    char scope[80] = {0};
 
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

    rc = _mqtt->connect(&connect_ctx);
    return rc;

}

void ConnectionManager::setConnectionStatus(CONN_STATE status)
{
    _conn_state = status;
}

int ConnectionManager::subscribe(char *topic, int qos, MQTTMessageHandler handler)
{
    return _mqtt->subscribe(topic, qos, handler, this);
}

void get_system_to_device(ConnectionManager *conn_m) 
{
    if (conn_m == NULL) return;
    conn_m->setConnectionStatus(TRYING_TO_CONNECT);
    if (conn_m->connectToServer()==0) {
        conn_m->setConnectionStatus(CONNECTED_TO_SERVER);
        char topictoreadfrom[128] = "devices/";
        strcat(topictoreadfrom, DEVICE_ID);
        strcat(topictoreadfrom,"/messages/devicebound/#");
        if (conn_m->subscribe(topictoreadfrom, 0, system_to_device_message_handler) < 0) {
            printf("Error while subcribing to topic %s.\r\n", topictoreadfrom);
        } else {
            printf("Successfully subscribred to topic %s\r\n", topictoreadfrom);
        }
    } else {
        conn_m->setConnectionStatus(CONNECTION_FAILED);
    }
    while(true) {wait(10);}
}

void ConnectionManager::disconnect(void)
{
    setConnectionStatus(DISCONNECTING);
    if (_mqtt->disconnect()) setConnectionStatus(DISCONNECTED);
    _bg96->powerDown();
}

bool ConnectionManager::getSystemToDeviceMessage(std::string &system_message, int timeout)
{
    printf("trying to get system to device message.\r\n");
    _msg_received = false;
    if (!_mqtt->startMQTTClient()) return false;
    _bg96->disallowPowerOff();
    _rssi = (double) _bg96->get_rssi();
    _timeout_triggered = false;
    _timeout.attach(system_to_device_timeout, timeout);
    _connect_thread.start(callback(get_system_to_device,this));
    while(!_timeout_triggered) { if (_msg_received) break;};
    printf("timeout triggered or message received\r\n");
    _connect_thread.terminate();
    _timeout.detach();
    _bg96->allowPowerOff();
    printf("shutting down modem\r\n");
    disconnect();
    if (_msg_received) {
        system_message = _system_message;
        return true;
    } else {
        system_message = "";
        return false;
    }
}

void device_to_system_timeout(void)
{
    _timeout_triggered = true;
}

void ConnectionManager::publish(void)
{
    char topictowriteto[128] = "devices/";
    strcat(topictowriteto, DEVICE_ID);
    strcat(topictowriteto, "/messages/events/");
    MQTTMessage msgtopublish;
    msgtopublish.qos = 1;
    msgtopublish.retain = 0;
    msgtopublish.topic.payload = topictowriteto;
    msgtopublish.topic.len = strlen(topictowriteto);
    msgtopublish.msg.len = _device_message.length();
    strcpy(msgtopublish.msg.payload,_device_message.c_str());
    if (_mqtt->publish(&msgtopublish)) {
        _msg_sent = true;
    } else {
        _msg_sent = false;
    }
}

void ConnectionManager::publish(std::string &msg)
{
    char topictowriteto[128] = "devices/";
    strcat(topictowriteto, DEVICE_ID);
    strcat(topictowriteto, "/messages/events/");
    MQTTMessage msgtopublish;
    msgtopublish.qos = 1;
    msgtopublish.retain = 0;
    msgtopublish.topic.payload = topictowriteto;
    msgtopublish.topic.len = strlen(topictowriteto);
    msgtopublish.msg.len = msg.length();
    strcpy(msgtopublish.msg.payload,msg.c_str());
    if (_mqtt->publish(&msgtopublish)) {
        _msg_sent = true;
    } else {
        _msg_sent = false;
    }    
}

void send_device_to_system(ConnectionManager *conn_m)
{
    if (conn_m==NULL) return;
    conn_m->setConnectionStatus(TRYING_TO_CONNECT);
    if (conn_m->connectToServer()==0) {
        conn_m->setConnectionStatus(CONNECTED_TO_SERVER);
        char topictoreadfrom[128] = "devices/";
        strcat(topictoreadfrom, DEVICE_ID);
        strcat(topictoreadfrom,"/messages/devicebound/#");
        if (conn_m->subscribe(topictoreadfrom, 0, system_to_device_message_handler) < 0) {
            printf("Error while subcribing to topic %s.\r\n", topictoreadfrom);
        } else {
            printf("Successfully subscribred to topic %s\r\n", topictoreadfrom);
        }
        conn_m->publish();
    } else {
        conn_m->setConnectionStatus(CONNECTION_FAILED);
    }
    while(true) {wait(10);}    
}

void send_all_device_to_system(ConnectionManager *conn_m)
{
    if (conn_m==NULL) return;
    conn_m->setConnectionStatus(TRYING_TO_CONNECT);
    if (conn_m->connectToServer()==0) {
        conn_m->setConnectionStatus(CONNECTED_TO_SERVER);
        char topictoreadfrom[128] = "devices/";
        strcat(topictoreadfrom, DEVICE_ID);
        strcat(topictoreadfrom,"/messages/devicebound/#");
        if (conn_m->subscribe(topictoreadfrom, 0, system_to_device_message_handler) < 0) {
            printf("Error while subcribing to topic %s.\r\n", topictoreadfrom);
        } else {
            printf("Successfully subscribred to topic %s\r\n", topictoreadfrom);
        }
        BTLLogManager *log_m = conn_m->getLogManager();
        FILE_HANDLE fh;
        std::string dts;
        log_m->startDeviceToSystemDumpSession(fh);
        while (log_m->getNextDeviceToSystemMessage(fh, dts)) conn_m->publish(dts);
        log_m->flushDeviceToSystemFile(fh);
        log_m->stopDeviceSystemDumpSession(fh);
    } else {
        conn_m->setConnectionStatus(CONNECTION_FAILED);
    }
    while(true) {wait(10);}      
}

bool ConnectionManager::sendAllMessages(BTLLogManager *log_m, int timeout)
{
    _log_m = log_m;
    if (!_mqtt->startMQTTClient()) return false;
    _bg96->disallowPowerOff();
    _timeout_triggered = false;
    _timeout.attach(device_to_system_timeout, timeout);
    _connect_thread.start(callback(send_all_device_to_system,this));
    while(!_timeout_triggered) { if (_msg_sent) break;};
    _connect_thread.terminate();
    _timeout.detach();
    _bg96->allowPowerOff();
    disconnect();
    if (_msg_sent) {
        return true;
    } else {
        return false;
    }   
}

bool ConnectionManager::sendDeviceToSystemMessage(std::string &device_to_system_message, int timeout)
{
    if (!_mqtt->startMQTTClient()) return false;
    _bg96->disallowPowerOff();
    _timeout_triggered = false;
    _timeout.attach(device_to_system_timeout, timeout);
    _device_message = device_to_system_message;
    _connect_thread.start(callback(send_device_to_system,this));
    while(!_timeout_triggered) { if (_msg_sent) break;};
    _connect_thread.terminate();
    _timeout.detach();
    _bg96->allowPowerOff();
    disconnect();
    if (_msg_sent) {
        return true;
    } else {
        return false;
    }
}

bool ConnectionManager::checkSystemToDeviceMessage(std::string &system_message)
{
    if (_msg_received) {
        system_message = _system_message;
        return true;
    } else {
        system_message = "";
        return false;
    }
}


