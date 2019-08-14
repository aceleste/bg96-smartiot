/**
 * @file ConnectionManager.cpp
 * @author Alain CELESTE (alain.celeste@polaris-innovation.com)
 * @brief 
 * @version 0.1
 * @date 2019-08-13
 * 
 * @copyright Copyright (c) 2019 Polaris Innovation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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
static char payload[1548];

ConnectionManager::ConnectionManager(BG96Interface *bg96, Mutex * bg96mutex)
{
    _bg96 = bg96;
    _mqtt = _bg96->getBG96MQTTClient(NULL);
    _rssi = 0.0;
    _log_m = NULL;
    _conn_state = DISCONNECTED;
    _msg_sent = false;
    _msg_received = false;
    _connect_mutex = bg96mutex;
//    _connect_thread = NULL;
}

ConnectionManager::~ConnectionManager()
{
    _bg96->disconnect();
    _bg96->powerDown();
}

/* This function looks for every occurrences of the token in initial string and replaces each one by replacement */
size_t ConnectionManager::replace_str(char * initial, char * token, char * replacement) {
    if ( initial == NULL || token == NULL || replacement == NULL ) return 0;
   debug("SAS before replacement:\r\n");
   debug("%s\r\n",initial);
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
   debug("SAS after replacement:\r\n");
   debug("%s\r\n",initial);
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
       debug("Invalid parameters passed to sign function\r\n");
        result = __FAILURE__;
    }
    else
    {
        BUFFER_HANDLE decoded_key;
        BUFFER_HANDLE output_hash;

        if ((decoded_key = Base64_Decoder(key)) == NULL)
        {
            LogError("Failed decoding symmetrical key");
           debug("Failed decoding symmetrical key\r\n");
            result = __FAILURE__;
        }
        else if ((output_hash = BUFFER_new()) == NULL)
        {
            LogError("Failed allocating output hash buffer");
           debug("Failed allocating output hash buffer\r\n");
            BUFFER_delete(decoded_key);
            result = __FAILURE__;
        }
        else if (HMACSHA256_ComputeHash(BUFFER_u_char(decoded_key), BUFFER_length(decoded_key), (const unsigned char*)stringToSign, payload_len, output_hash) != HMACSHA256_OK)
        {
            LogError("Failed computing HMAC Hash");
           debug("Failed computing HMAC Hash\r\n");
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
               debug("Generated signature token length longer than storage buffer\r\n");
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
       debug("failure getting seconds from epoch\r\n");
        result = 0;
    }
    else 
    {
        char expiry_token[16] = {0};
        size_t expiry_time = sec_since_epoch+expiryInSeconds;
        if (size_tToString(expiry_token, sizeof(expiry_token), expiry_time) != 0) {
            LogError("Failure when creating expire token");
           debug("Failure when creating expire token\r\n");
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
                   debug("Failure constructing encoding.\r\n");
                }
                else if ((urlEncodedSignature = URL_Encode(signature)) == NULL)
                {
                    result = 0;
                    LogError("Failure constructing url Signature.");
                   debug("Failure constructing url Signature.\r\n");
                }
                // char * buffer = (char *)malloc(25+STRING_length(encoded_uri)+1+
                //                         5+STRING_length(urlEncodedSignature)+1+
                //                         4+strlen(expiry_token)+1+
                //                         5+strlen(policyName))+1+8;
                // if (buffer == NULL) {
                //     LogError("Failure allocating the buffer for the sas_token.");
                //    debug("Failure allocating the buffer for the sas_token.\r\n");
                //     result = 0;
                // } 
                if (34+STRING_length(encoded_uri)+STRING_length(urlEncodedSignature)+strlen(expiry_token)+5+strlen(policyName) > BG96MQTTCLIENT_MAX_SAS_TOKEN_LENGTH-1) {
                   debug("Error - the generated SAS token is longer than the storage buffer.\r\n");
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

void ConnectionManager::newSystemMessage(char *msg, size_t len)
{
    _connect_mutex->lock();
    _system_message = msg;
    _msg_received = true;
    _mqtt->stopRunning();
    _connect_mutex->unlock();
}

void system_to_device_message_handler(MQTTMessage *msg, void *param)
{
    if (msg == NULL || param == NULL) {
       debug("Message handler called with null pointer.\r\n");
        return;
    }
    ConnectionManager *conn_m = (ConnectionManager *)param;
    //debug("Received MQTT message on topic %s\r\n", msg->topic.payload);
    //debug("Message payload is -> \r\n");
    //debug("%s\r\n",msg->msg.payload);
    if (msg->msg.len > BG96_MQTT_CLIENT_MAX_PUBLISH_MSG_SIZE)  return;
    conn_m->newSystemMessage(msg->msg.payload, msg->msg.len);
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
   debug("APN will be set to %s\r\n",pdp_ctx.apn);
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

   debug("Configuring PDP context...\r\n");
    _connect_mutex->lock();
    rc = _mqtt->configure_pdp_context(&pdp_ctx);
    _connect_mutex->unlock();
    if (rc < 0) {
       debug("Error when configuring pdp context %d.\r\n", pdp_ctx.pdp_id);
        return -1;
    }
   debug("Succesfully configured pdp context %d.\r\n", pdp_ctx.pdp_id);

   debug("Activate PDP context...\r\n");
    _connect_mutex->lock();
    rc= _bg96->connect();
    _connect_mutex->unlock();
    if (!rc){
       debug("Error when activating the PDP context.\r\n");
        return -1;
    }

   debug("Succesfully activated the PDP context.\r\n");

    

   debug("Now trying to set system time...\r\n");

    time_t current_time;
    _connect_mutex->lock();
    if (_bg96->getNetworkGMTTime(&current_time) != NSAPI_ERROR_OK) {
        NetworkInterface *itf = (NetworkInterface *)&_bg96; // We need a NetworkInterface reference for NTPClient
        NTPClient ntp = NTPClient(itf);
        current_time = ntp.get_timestamp();
    }
    _connect_mutex->unlock();
    set_time(current_time);
    char buffer[32];
    current_time = time(NULL);
    strftime(buffer, 32, "%I:%M %p\n", localtime(&current_time));

   debug("Time is now %s\r\n", buffer);


    MQTTClientOptions mqtt_options = BG96MQTTClientOptions_Initializer;
    mqtt_options.will_qos       = 0;
    mqtt_options.cleansession   = 0;
    mqtt_options.sslenable      = 1;

   debug("Configuring MQTT options...\r\n");
    _connect_mutex->lock();
    rc = _mqtt->configure_mqtt(&mqtt_options);
    _connect_mutex->unlock();
    if (rc < 0 ) {
       debug("Error when configuring MQTT options (%d)\r\n", rc);
        return -1;
    }

   debug("Succesfully configured MQTT options\r\n");

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

   debug("Opening a network socket to %s:%d\r\n", network_ctx.hostname.payload, network_ctx.port);
    _connect_mutex->lock();
    rc = _mqtt->open(&network_ctx);
    _connect_mutex->unlock();
    if (rc < 0) {
       debug("Error opening the network socket (%d)\r\n", rc);
//        tls = bg96->getBG96TLSSocket();
//        tls->connect(network_ctx.hostname.payload, network_ctx.port);
        return -1;
    }
   debug("Successfully opened a network socket.\r\n");

    char scope[80] = {0};
 
   debug("Generating SAS token...\r\n");

    sprintf(scope,"%s/devices/%s", MQTT_SERVER_HOST_NAME, DEVICE_ID);
    //STRING_HANDLE scope = STRING_construct_sprintf("%s/devices/%s", MQTT_SERVER_HOST_NAME, DEVICE_ID);
    generate_sas_token(sas_token, scope, DEVICE_KEY, NULL, AZURE_IOTHUB_SAS_TOKEN_DEFAULT_EXPIRY_TIME);
    if (sas_token == NULL){
       debug("Error when generating SAS token.\r\n");
        return -1;
    } 
   debug("Generated a new SAS Token: \r\n");
   debug("%s\r\n", sas_token);
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

   debug("Connecting to the IoT Hub server %s...\r\n",network_ctx.hostname.payload);
    _connect_mutex->lock();
    rc = _mqtt->connect(&connect_ctx);
        _connect_mutex->unlock();
    return rc;

}

void ConnectionManager::setConnectionStatus(CONN_STATE status)
{
    _connect_mutex->lock();
    _conn_state = status;
    _connect_mutex->unlock();
}

int ConnectionManager::subscribe(char *topic, int qos, MQTTMessageHandler handler)
{
    int rc;
    _connect_mutex->lock();
    rc = _mqtt->subscribe(topic, qos, handler, this);
    _connect_mutex->unlock();
    return rc;
}

void get_system_to_device(ConnectionManager *conn_m) 
{
    if (conn_m == NULL) return;
    conn_m->setConnectionStatus(TRYING_TO_CONNECT);
    if (conn_m->connectToServer()==0) {
        conn_m->setConnectionStatus(CONNECTED_TO_SERVER);
       debug("ConnectionManager: Publishing HELLO message.\r\n");
        std::string msg("HELLO");
        conn_m->publish(msg);
        char topictoreadfrom[128] = "devices/";
        strcat(topictoreadfrom, DEVICE_ID);
        strcat(topictoreadfrom,"/messages/devicebound/#");
        if (conn_m->subscribe(topictoreadfrom, 0, system_to_device_message_handler) < 0) {
           debug("Error while subcribing to topic %s.\r\n", topictoreadfrom);
        } else {
           debug("Successfully subscribred to topic %s\r\n", topictoreadfrom);
            conn_m->trackSystemToDeviceMessages();
        }
    } else {
        conn_m->setConnectionStatus(CONNECTION_FAILED);
    }
    while(true) {wait(10);}
}

void ConnectionManager::disconnect(void)
{
    setConnectionStatus(DISCONNECTING);
    _connect_mutex->lock();
    if (_mqtt->disconnect()) setConnectionStatus(DISCONNECTED);
    _bg96->powerDown();
    wait(1);
    _connect_mutex->unlock();
}

bool ConnectionManager::getSystemToDeviceMessage(std::string &system_message, int timeout)
{
    int rc;
   debug("trying to get system to device message.\r\n");
    Thread s1;
    _msg_received = false;
    _connect_mutex->lock();
    rc = _mqtt->startMQTTClient();
    _connect_mutex->unlock();
    if (!rc) return false;
    _connect_mutex->lock();
    _bg96->disallowPowerOff();
    _rssi = (double) _bg96->get_rssi();
    _connect_mutex->unlock();
    _timeout_triggered = false;
    _timeout.attach(system_to_device_timeout, timeout);
    s1.start(callback(get_system_to_device,this));
   // _connect_thread.start(callback(get_system_to_device,this));
    while(!_timeout_triggered) { if (_msg_received) break;};
   debug("timeout triggered or message received\r\n");
    _connect_mutex->lock();
    _mqtt->stopRunning();
    _connect_mutex->unlock();
    s1.terminate();
    s1.join();
    //_connect_thread.terminate();
    _timeout.detach();
   debug("ConnectionManager: Publishing BYE message.\r\n");
    std::string msg("BYE");
    publish(msg);
    _connect_mutex->lock();
    _bg96->allowPowerOff();
    _connect_mutex->unlock();
   debug("shutting down modem\r\n");
    disconnect();
    if (_msg_received) {
        system_message = _system_message;
        return true;
    } else {
        system_message = "";
        return false;
    }
}

void ConnectionManager::trackSystemToDeviceMessages()
{
    _connect_mutex->lock();
    _mqtt->dowork();
    _connect_mutex->unlock();
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
    strcpy(payload, _device_message.c_str());
    msgtopublish.msg.payload = payload;
    _connect_mutex->lock();
    if (_mqtt->publish(&msgtopublish)) {
        _msg_sent = true;
    } else {
        _msg_sent = false;
    }
    _connect_mutex->unlock();
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
    strcpy(payload,msg.c_str());
    msgtopublish.msg.payload = payload;
    _connect_mutex->lock();
    if (_mqtt->publish(&msgtopublish)) {
        _msg_sent = true;
    } else {
        _msg_sent = false;
    }    
    _connect_mutex->unlock();
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
           debug("Error while subcribing to topic %s.\r\n", topictoreadfrom);
        } else {
           debug("Successfully subscribred to topic %s\r\n", topictoreadfrom);
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
           debug("Error while subcribing to topic %s.\r\n", topictoreadfrom);
        } else {
           debug("Successfully subscribred to topic %s\r\n", topictoreadfrom);
        }
        conn_m->trackSystemToDeviceMessages();
        LogManager *log_m = conn_m->getLogManager();
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

bool ConnectionManager::sendAllMessages(LogManager *log_m, int timeout)
{
    int rc;
    _msg_sent = false;
    Thread s1;
    _log_m = log_m;
    _connect_mutex->lock();
    rc = _mqtt->startMQTTClient();
    _connect_mutex->unlock();
    if (!rc) return false;
    _connect_mutex->lock();
    _bg96->disallowPowerOff();
    _connect_mutex->unlock();
    _timeout_triggered = false;
    _timeout.attach(device_to_system_timeout, timeout);
    s1.start(callback(send_all_device_to_system,this));
    while(!_timeout_triggered) { if (_msg_sent) break;};
    s1.terminate();
    s1.join();
    _timeout.detach();
    _connect_mutex->lock();
    _bg96->allowPowerOff();
    _connect_mutex->unlock();
    disconnect();
    if (_msg_sent) {
        return true;
    } else {
        return false;
    }   
}

bool ConnectionManager::sendDeviceToSystemMessage(std::string &device_to_system_message, int timeout)
{
    int rc;
    _msg_sent = false;
    Thread s1;
    _connect_mutex->lock();
    rc = _mqtt->startMQTTClient();
    _connect_mutex->unlock();
    if (!rc) return false;
    _connect_mutex->lock();
    _bg96->disallowPowerOff();
    _connect_mutex->unlock();
    _timeout_triggered = false;
    _timeout.attach(device_to_system_timeout, timeout);
    _device_message = device_to_system_message;
    s1.start(callback(send_device_to_system,this));
    while(!_timeout_triggered) { if (_msg_sent) break;};
    s1.terminate();
    s1.join();
    _timeout.detach();
    _connect_mutex->lock();
    _bg96->allowPowerOff();
    _connect_mutex->unlock();
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
        _msg_received = false; // We assume once it is checked, it has been processed.
        return true;
    } else {
        system_message = "";
        return false;
    }
}

