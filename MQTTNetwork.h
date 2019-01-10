#ifndef _MQTTNETWORK_H_
#define _MQTTNETWORK_H_

#include "NetworkInterface.h"
#if !defined(APP_USES_BG96_TLS_SOCKET)
#define APP_USES_BG96_TLS_SOCKET 0
#endif
#if APP_USES_BG96_TLS_SOCKET
#include "BG96TLSSocket.h"
#else
#include "TLSSocket.h"
#endif

class MQTTNetwork {
public:
#if APP_USES_BG96_TLS_SOCKET
    MQTTNetwork(BG96Interface* bg96): network((NetworkInterface*) bg96)
    {
        socket = bg96->getBG96TLSSocket();
    }
#else
    MQTTNetwork(NetworkInterface* aNetwork) : network(aNetwork) {
        socket = new TLSSocket(aNetwork);
    }
#endif

    ~MQTTNetwork() {
        delete socket;
    }

    int read(unsigned char* buffer, int len, int timeout) {
        nsapi_size_or_error_t rc = 0;
        socket->set_timeout(timeout);
        rc = socket->recv(buffer, len);
        if (rc == NSAPI_ERROR_WOULD_BLOCK){
            // time out and no data
            // MQTTClient.readPacket() requires 0 on time out and no data.
            return 0;
        }
        return rc;
    }

    int write(unsigned char* buffer, int len, int timeout) {
        // TODO: handle time out
#if APP_USES_BG96_TLS_SOCKET
        socket->set_timeout(timeout);
#endif
        return socket->send(buffer, len);
    }
    
    int connect(const char* hostname, int port, const char *ssl_ca_pem = NULL,
            const char *ssl_cli_pem = NULL, const char *ssl_pk_pem = NULL) {   
        int rc=-1;   
        rc = socket->set_root_ca_cert(ssl_ca_pem);  
        if ( rc != NSAPI_ERROR_OK ) return rc;
        rc = socket->set_client_cert_key(ssl_cli_pem, ssl_pk_pem);
        if ( rc != NSAPI_ERROR_OK ) return rc;
        return socket->connect(hostname, port);
    }
#if APP_USES_BG96_TLS_SOCKET
    bool is_connected(){
        return socket->is_connected();
    }
#endif

    int disconnect() {
        return socket->close();
    }

private:
    NetworkInterface* network;
#if APP_USES_BG96_TLS_SOCKET
    BG96TLSSocket* socket;
#else
    TLSSocket* socket;
#endif
};

#endif // _MQTTNETWORK_H_
