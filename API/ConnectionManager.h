/**
 * @file ConnectionManager.h
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
#ifndef __CONNECTION_MANAGER_H__
#define __CONNECTION_MANAGER_H__
#include "mbed.h"
#include "LogManager.h"
#include <string>
#include "mbed-os/drivers/LowPowerTimeout.h"
#include "BG96Interface.h"
#include "BG96MQTTClient.h"

typedef enum {
    TRYING_TO_CONNECT, CONNECTION_FAILED, DISCONNECTING, DISCONNECTED, CONNECTED_TO_SERVER
} CONN_STATE;



class ConnectionManager
{
public:
    ConnectionManager(BG96Interface *bg96, Mutex * bg96mutex);
    ~ConnectionManager();

    bool sendDeviceToSystemMessage(std::string &device_to_system_message, int timeout);
    bool getSystemToDeviceMessage(std::string &system_message, int timeout);
    void trackSystemToDeviceMessages();
    void getRSSI(double &rssi);
    bool tryGetSystemToDeviceMessage(std::string &system_message, int tries, int timeout);
    bool trySendDeviceToSystemMessage(std::string &device_to_system_message);
    void setConnectionStatus(CONN_STATE status);
    bool checkSystemToDeviceMessage(std::string &system_message);
    int  connectToServer(void);
    int  subscribe(char *topic, int qos, MQTTMessageHandler handler);
    void publish(void);
    void publish(std::string &msg);
    void disconnect(void);
    void newSystemMessage(char * msg, size_t len);
    bool sendAllMessages(LogManager *log_m, int timeout);
    LogManager * getLogManager(){ return _log_m;};

private:
    size_t  replace_str(char * initial, char * token, char * replacement);
    int     get_seconds_since_epoch(size_t* seconds);
    int     SignAuthPayload(const char* key, const char* stringToSign, unsigned char** output, size_t* len);
    size_t  generate_sas_token(char *out, const char * resourceUri, const char * key, const char * policyName, int expiryInSeconds);

    LowPowerTimeout _timeout;
    BG96Interface *_bg96;
    BG96MQTTClient * _mqtt;
    LogManager *_log_m;
    unsigned char generated_sig[80];
    char username[256]={0};
    char clientid[80] = {0};
    char sas_token[BG96MQTTCLIENT_MAX_SAS_TOKEN_LENGTH] = {0};
    std::string _system_message;
    std::string _device_message;
    bool _msg_received;
    bool _msg_sent;
//    Thread *_connect_thread;
    Mutex * _connect_mutex;
    CONN_STATE _conn_state;
    double _rssi;
};

#endif //__CONNECTION_MANAGER_H__
