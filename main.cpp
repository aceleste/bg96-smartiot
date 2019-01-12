#include "BG96Interface.h"
#include "MQTT_server_setting.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed_events.h"
#include "NTPClient.h"
#include "mbedtls/error.h"
#include "mosquitto_cert.h"

void main(void)
{
    BG96Interface* bg96;
    BG96MQTTClient* mqtt;

    bg96 = new BG96Interface();

    if (bg96 == null) return -1;

    BG96_PDP_Ctx pdp_ctx;
    pdp_ctx.pdp_id = DEFAULT_PDP; 
    sprintf(pdp_ctx.apn, "%s", DEFAULT_APN);
    pdp_ctx.username = NULL;
    pdp_ctx.password = NULL;
    rc = bg96->configure_pdp_context(&pdp_ctx);

    if (rc < 0) {
        printf("Error when configuring pdp context %d.\r\n", pdp_ctx.pdp_id);
        return -1;
    }
    printf("Succesfully configured pdp context %d.\r\n", pdp_ctx.pdp_id);

    mqtt = getBG96MQTTClient(NULL);

    MQTTClientOptions mqtt_options = BG96MQTTClientOptions_Initializer;
    mqtt_options.will_qos       = 1;
    mqtt_options.cleansession   = 0;
    mqtt_options.sslenable      = 1;

    rc = mqtt->configure_mqtt(mqtt_options);

    if (rc < 0 ) {
        printf("Error when configuring MQTT options (%d)\r\n", rc);
        return -1;
    }

    printf("Succesfully configured MQTT options");



}