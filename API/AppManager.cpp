/**
 * @file AppManager.cpp
 * @author Alain CELESTE (alain.celeste@polaris-innovation.com)
 * @brief  Source code for the main API to be called by the user application.
 * @version 0.1
 * @date 2019-08-12
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

#include "AppManager.h"
#include "LogManager.h"
#include "ConnectionManager.h"
#include "LocationManager.h"
#include "mbed.h"
#include "Thread.h"
#include "MQTTClient_Settings.h"

//static TaskParameter param;

typedef struct {
    AppManager  *app_m;
    GNSSLoc     *location; 
    int         tries;
} LocationParam;

typedef struct {
    AppManager *app_m;
    std::string *error;
} ErrorParam;

typedef struct {
    AppManager *app_m;
    int timeout;
    std::string *message;
} ConnectionParam;

static ConnectionParam connect_param;

static LocationParam loc_param;

static ErrorParam error_param;

void logSystemStartEvent(AppManager *app_m)
{
    if (app_m == NULL) return;
    app_m->setLogStatus(false);
    LogManager *log_m = app_m->getLogManager();
    if (log_m == NULL) return;
    app_m->setLogStatus(log_m->logSystemStartEvent());
}

void logNewLocation(LocationParam *param)
{
    if (param == NULL) return;
    if (param->app_m == NULL) return;
    param->app_m->setLogStatus(false);
    LogManager *log_m = param->app_m->getLogManager();
    if (log_m == NULL) return;
    param->app_m->setLogStatus(log_m->logNewLocation(*param->location));
}

void getGNSSLocation(LocationParam *param)
{
    if (param == NULL) return;
    if (param->app_m == NULL) return;
    param->app_m->setLocationStatus(false);
    LocationManager *loc_m = param->app_m->getLocationManager();
    if (loc_m == NULL) return;
    param->app_m->setLocationStatus(loc_m->tryGetGNSSLocation(*param->location, param->tries)); 
}

void appendDeviceToSystemMessage(ConnectionParam *param)
{
    if (param == NULL) return;
    if (param->app_m == NULL) return;
    param->app_m->setQueueMessageStatus(false);
    LogManager *log_m = param->app_m->getLogManager();
    if (log_m == NULL) return;
    param->app_m->setQueueMessageStatus(log_m->appendDeviceToSystemMessage(*param->message));
}

void sendAllMessages(AppManager *app_m)
{
    if (app_m == NULL) return;
    app_m->setConnectionStatus(false);
    LogManager * log_m = app_m->getLogManager();
    ConnectionManager * conn_m = app_m->getConnectionManager();
    if (log_m == NULL || conn_m == NULL) return;
    app_m->setConnectionStatus(conn_m->sendAllMessages(log_m, DEFAULT_CONNECTION_TIMEOUT));
}

void sendMessage(ConnectionParam *param)
{
    if (param == NULL) return;
    if (param->app_m == NULL) return;
    param->app_m->setConnectionStatus(false);
    ConnectionManager * conn_m = param->app_m->getConnectionManager();
    if (conn_m == NULL) return;
    param->app_m->setConnectionStatus(conn_m->sendDeviceToSystemMessage(*param->message, DEFAULT_CONNECTION_TIMEOUT));
}

void getMessage(ConnectionParam *param)
{
    if (param == NULL) return;
    if (param->app_m == NULL) return;
    param->app_m->setConnectionStatus(false);
    ConnectionManager *conn_m = param->app_m->getConnectionManager();
    if (conn_m == NULL) return;
    param->app_m->setConnectionStatus(conn_m->getSystemToDeviceMessage(*param->message, param->timeout));
}

void logAnError(ErrorParam *param)
{
    if (param == NULL) return;
    if (param->app_m == NULL) return;
    param->app_m->setLogStatus(false);
    LogManager *log_m = param->app_m->getLogManager();
    if (log_m == NULL) return;
    param->app_m->setLogStatus(log_m->logAnError(*param->error));
}
/**
 * @brief Construct a new App Manager:: App Manager object
 * 
 * @param conn_m a pointer to a valid ConnectionManager object
 * @param loc_m a pointer to a valid LocationManager object
 * @param log_m a pointer to a valid LogManager object
 */
AppManager::AppManager(ConnectionManager *conn_m,
                       LocationManager *loc_m,
                       LogManager *log_m)
{
    _conn_m = conn_m;
    _loc_m = loc_m;
    _log_m = log_m;
    Thread s1;
    s1.start(callback(logSystemStartEvent, this));
    _thread = &s1;
    s1.join();
    _thread = NULL;
}
/**
 * @brief Destroy the App Manager:: App Manager object
 * 
 */
AppManager::~AppManager()
{
//    log_m->logSystemStopEvent();
}

/**
 * @brief Obtain the GNSS location using BG96 GNSS services
 * 
 * @param location a reference to a GNSSLoc object
 * @return true if location can be obtained
 * @return false otherwise
 */
bool AppManager::getLocation(GNSSLoc& location)
{

    _app_mutex.lock();
    loc_param.app_m = this;
    loc_param.location = &location;
    loc_param.tries = 3;
    Thread s1;
    s1.start(callback(getGNSSLocation, &loc_param));
    _thread = &s1;
    s1.join();
    _thread = NULL;
    _app_mutex.unlock();
    return _loc_result;
}

LogManager * AppManager::getLogManager() 
{
    return _log_m;
}

ConnectionManager * AppManager::getConnectionManager()
{
    return _conn_m;
}

LocationManager * AppManager::getLocationManager()
{
    return _loc_m;
}

/**
 * @brief Use this function to call a callback to process a GNSS location 
 * 
 * @param current_location pointer to a valid GNSSLoc object
 * @param callback the callback function to call to process the location
 */
void AppManager::processLocation(GNSSLoc *current_location, void (*callback)(GNSSLoc *, AppManager *))
{
    callback(current_location, this);
}

/**
 * @brief Use this function to call a callback to process a sytem to device message
 * 
 * @param system_message a reference to the message to process
 * @param callback the callback function to call to process the message
 */
void AppManager::processSystemToDeviceMessage(std::string &system_message, void (*callback)(std::string &, AppManager *))
{
    _app_mutex.lock();
    _system_message = system_message;
    _app_mutex.unlock();
    callback(system_message, this);
}

void AppManager::setQueueMessageStatus(bool queue_status)
{
    _queue_result = queue_status;
}

void AppManager::setConnectionStatus(bool connect_status)
{
    _connect_result = connect_status;
}
/**
 * @brief Use this function to queue a message in a file for later dump to Azure
 * 
 * @param device_message the message to queue
 * @return true if message successfully queued
 * @return false otherwise
 */
bool AppManager::queueDeviceToSystemMessage(std::string &device_message)
{
    _app_mutex.lock();
    connect_param.app_m = this;
    connect_param.message = &device_message;
    Thread s1;
    s1.start(callback(appendDeviceToSystemMessage, &connect_param));
    _thread = &s1;
    s1.join();
    _thread = NULL;
    _app_mutex.unlock();
    return _queue_result;
}

/**
 * @brief Dumps the message queue to Azure IoT hub and empties the queue
 * 
 * @return true if the queue is successfully dumped 
 * @return false otherwise
 */
bool AppManager::sendDeviceToSystemMessageQueue()
{
    _app_mutex.lock();
    Thread s1;
    s1.start(callback(sendAllMessages, this));
    _thread = &s1;
    s1.join();
    _thread = NULL;
    _app_mutex.unlock();
    return _connect_result;
}

/**
 * @brief Sends a device to system message
 * 
 * @param message the message to send to Azure IoT hub
 * @return true if the message is successfully sent
 * @return false otherwise
 */
bool AppManager::sendDeviceToSystemMessage(std::string &message)
{
    _app_mutex.lock();
    connect_param.app_m = this;
    connect_param.message = &message;
    Thread s1;
    s1.start(callback(sendMessage, &connect_param));
    _thread = &s1;
    s1.join();
    _thread = NULL;
    _app_mutex.unlock();
    return _connect_result;
}

/**
 * @brief Listen to incoming message from Azure IoT hub
 * A short protocol is used to open a window or reception. After the device successfully connects to Azure,
 * it publishes a message with the payload 'HELLO', indicating it can now receive a message.
 * The server can send a message. 
 * Once the message is received, the device publishes a message with the payload 'BYE', indicating that reception 
 * is now stopped.
 * 
 * @param message a reference to a string variable to hold the received message
 * @param timeout the time window for receiving data
 * @return true if a message is successfully received from the server
 * @return false otherwise
 */
bool AppManager::getSystemToDeviceMessage(std::string &message, int timeout)
{
    _app_mutex.lock();
    connect_param.app_m = this;
    connect_param.timeout = timeout;
    connect_param.message = &message;
    Thread s1;
    s1.start(callback(getMessage, &connect_param));
    _thread = &s1;
    s1.join();
    _thread = NULL;
    _app_mutex.unlock();
    return _connect_result;
}

void AppManager::setLogStatus(bool status)
{
    _log_result = status;
}

void AppManager::setLocationStatus(bool status)
{
	_loc_result = status;
}
/**
 * @brief Logs an error message in the error log file
 * 
 * @param error the error message to log
 * @return true if the error is successfully logged
 * @return false otherwise
 */
bool AppManager::logError(std::string error)
{
    _app_mutex.lock();
    error_param.app_m = this;
    error_param.error = &error;
    Thread s1;
    s1.start(callback(logAnError, &error_param));
    _thread = &s1;
    s1.join();
    _thread = NULL;
    _app_mutex.unlock();
    return _log_result;
}
/**
 * @brief Logs a connection error in the error file
 * 
 * @return true if the connection error can be logged successfully
 * @return false otherwise
 */
bool AppManager::logConnectionError()
{ 
    time_t now = time(NULL);
    std::string timestr = ctime(&now);
    std::string error;
    error += timestr;
    error += " - CONNECTION ERROR";
    return logError(error);
}

/**
 * @brief Logs a location error
 * When requesting a GNSS location using the <getLocation>() function, the request may fail.
 * Use this function if you want to log in the error file when this happens, for future reference.
 * 
 * @return true if the location error is successfully logged.
 * @return false otherwise.
 */
bool AppManager::logLocationError()
{
    time_t now = time(NULL);
    std::string timestr = ctime(&now);
    std::string error;
    error += timestr;
    error += " - ERROR GETTING GNSS LOCATION";
    return logError(error);
}

/**
 * @brief Logs the location to a location file for future reference.
 * 
 * @param location A reference to the GNSS location to be logged.
 * @return true if the location is successfully logged.
 * @return false otherwise
 */
bool AppManager::logLocation(GNSSLoc &location)
{
    _app_mutex.lock();
    loc_param.app_m = this;
    loc_param.location = &location;
    Thread s1;
    s1.start(callback(logNewLocation, &loc_param));
    _thread = &s1;
    s1.join();
    _thread = NULL;
    return _log_result;
}
