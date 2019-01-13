#include "BG96Interface.h"
#include "BG96.h"
#include "BG96MQTTClient.h"
#include "MQTT_server_setting.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed_events.h"
#include "NTPClient.h"
#include "mbedtls/error.h"



int main(void)
{
    int rc = -1;
    BG96Interface* bg96;
    BG96MQTTClient* mqtt;

    bg96 = new BG96Interface();

    if (bg96 == NULL) return -1;

    BG96_PDP_Ctx pdp_ctx;
    pdp_ctx.pdp_id = DEFAULT_PDP; 
    sprintf(pdp_ctx.apn, "%s", DEFAULT_APN);
    pdp_ctx.username = NULL;
    pdp_ctx.password = NULL;
    rc = mqtt->configure_pdp_context(&pdp_ctx);

    if (rc < 0) {
        printf("Error when configuring pdp context %d.\r\n", pdp_ctx.pdp_id);
        return -1;
    }
    printf("Succesfully configured pdp context %d.\r\n", pdp_ctx.pdp_id);

    mqtt = bg96->getBG96MQTTClient(NULL);

    MQTTClientOptions mqtt_options = BG96MQTTClientOptions_Initializer;
    mqtt_options.will_qos       = 1;
    mqtt_options.cleansession   = 0;
    mqtt_options.sslenable      = 1;

    rc = mqtt->configure_mqtt(&mqtt_options);

    if (rc < 0 ) {
        printf("Error when configuring MQTT options (%d)\r\n", rc);
        return -1;
    }

    printf("Succesfully configured MQTT options");

    MQTTString cacert;
    sprintf(cacert.payload, "%", SSL_CA_PEM);
    cacert.len = strlen(cacert.payload);

    MQTTNetwork_Ctx network_ctx;

    network_ctx.ca_cert = cacert;
    network_ctx.client_cert.payload = NULL;
    network_ctx.client_key.payload  = NULL;
    sprintf(network_ctx.hostname.payload, "*", MQTT_SERVER_HOST_NAME);
    network_ctx.port = MQTT_SERVER_PORT;

    printf("Opening a network socket...");

    rc = mqtt->open(&network_ctx);

    if (rc < 0) {
        printf("Error opening the network socket (%d)\r\n", rc);
        return -1;
    }
    printf("Successfully opened a network socket.\r\n");

    MQTTConnect_Ctx connect_ctx;

    sprintf(connect_ctx.client_id.payload, "%s", DEVICE_ID);
    connect_ctx.client_id.len = strlen(connect_ctx.client_id.payload);

    sprintf(connect_ctx.username.payload, "%s", MQTT_SERVER_HOST_NAME);
    strcat(connect_ctx.username.payload,"/");
    strcat(connect_ctx.username.payload,DEVICE_ID);
    strcat(connect_ctx.username.payload,"/");
    strcat(connect_ctx.username.payload,"api-version=2018-06-30");
    connect_ctx.username.len = strlen(connect_ctx.username.payload);
    sprintf(connect_ctx.password.payload, "%s", SAS_TOKEN);
    connect_ctx.password.len = strlen(connect_ctx.password.payload);

    printf("Connecting to the IoT Hub server %s...\r\n",network_ctx.hostname.payload);
    rc = mqtt->connect(&connect_ctx);
    if (rc < 0) {
        printf("Error (%d) connecting to the IoT Hub server.\r\n", rc);
        return -1;
    }

    printf("Disconnecting from server...\r\n");
    mqtt->disconnect();
    printf("Closing network socket...\r\n");
    mqtt->close();
    printf("Program end.\r\n");
    delete(mqtt);
    delete(bg96);
    return 1;
}