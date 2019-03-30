#include <string>

#include "mbed.h"
#include "mbed_events.h"
#include "mbed-trace/mbed_trace.h"
#include "mbedtls/error.h"

#include "ntp-client/NTPClient.h"
#include "http_request.h"
#include "network-helper.h"
#include "mbed_mem_trace.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "MQTT_server_setting.h"


#define MQTT_MAX_CONNECTIONS    5
#define MQTT_MAX_PACKET_SIZE    1024

#define USE_WIFI        1

void dump_response(HttpResponse* res) {
    // printf("Status: %d - %s\n", res->get_status_code(), res->get_status_message().c_str());
 
    // printf("Headers:\n");
    // for (size_t ix = 0; ix < res->get_headers_length(); ix++) {
    //     printf("\t%s: %s\n", res->get_headers_fields()[ix]->c_str(), res->get_headers_values()[ix]->c_str());
    // }
    if (res->get_status_code() == 400)
        printf("\nNo More Data\n");
    else
        printf("%s\n", res->get_body_as_string().c_str());
}
DigitalOut led1(LED1);
DigitalOut led2(LED2);

InterruptIn btn1(BUTTON1, PullUp);
InterruptIn btn2(BUTTON2, PullUp);        // Must setting the Pullup option
NetworkInterface *net;
EventQueue queue(32 * EVENTS_EVENT_SIZE);
Thread t;

MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>* mqttClient = NULL;
std::string mqtt_topic_pub;

#if USE_WIFI
WiFiInterface *wifi;

int Wifi_AP_connect()
{
    wifi = WiFiInterface::get_default_instance();
    if (!wifi)
    {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0)
    {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Success\n\n");
    printf("MAC: %s\n", wifi->get_mac_address());
    printf("IP: %s\n", wifi->get_ip_address());
    printf("Netmask: %s\n", wifi->get_netmask());
    printf("Gateway: %s\n", wifi->get_gateway());
    printf("RSSI: %d\n\n", wifi->get_rssi());

    net = (NetworkInterface*)wifi;

    return 0;
}
#endif

void Net_Disconnect()
{
    if(net)
        net->disconnect();
}


void Set_NTP()
{
    // sync the real time clock (RTC)
    NTPClient ntp(net);
    ntp.set_server("time.google.com", 123);
    time_t now = ntp.get_timestamp();
    set_time(now);
    printf("Time is now %s\r\n", ctime(&now));
}

void messageArrived(MQTT::MessageData& md)
{
    printf("sub topic\r\n");
    MQTT::Message &message = md.message;
    //    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", 
    //            message.qos, message.retained, message.dup, message.id);
    printf("%.*s\r\n", (char*)message.payload);
}
void btn2_handler()
{
    led2 = 1;

#if 1
 HttpRequest* get_req2 = new HttpRequest(net, HTTP_GET, "http://ubuntu.hanukoon.com:8080/api/service/page/prev/odinevk");
 
        HttpResponse* get_res2 = get_req2->send();
        if (!get_res2) {
            printf("HttpRequest failed (error code %d)\n", get_req2->get_error());
        }
 
        printf("\n----- HTTP GET response -----\n");
        dump_response(get_res2);
 
        delete get_req2;
#else
    printf("btn\n");
#endif

    led2 = 0;
}
void btn_handler()
{
    led1 = 1;

#if 1
 HttpRequest* get_req = new HttpRequest(net, HTTP_GET, "http://ubuntu.hanukoon.com:8080/api/service/page/next/odinevk");
 
        HttpResponse* get_res = get_req->send();
        if (!get_res) {
            printf("HttpRequest failed (error code %d)\n", get_req->get_error());
        }
 
        printf("\n----- HTTP GET response -----\n");
        dump_response(get_res);
 
        delete get_req;
#else
    printf("btn\n");
#endif

    led1 = 0;
}

int main()
{
    int ret;
    //mbed_trace_init();

    printf("WiFi example\n");

#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif

#if USE_WIFI
    ret = Wifi_AP_connect();

#else
    ret = ETH_connect();
#endif

    if(ret != 0)
    {
        printf("Network connect failed!\n");
        return -1;
    }

    Set_NTP();

    /* Establish a network connection. */
    nsapi_error_t status;
    MQTTNetwork mqttNetwork(net);

    status = mqttNetwork.open();
    if (status != NSAPI_ERROR_OK)
    {
        printf("Mqtt open fail %d\r\n", status);
        Net_Disconnect();
        return -1;
    }

    printf("Connecting to host %s:%d ...\r\n", MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT);
    status = mqttNetwork.connect(MQTT_SERVER_HOST_NAME,
                                 MQTT_SERVER_PORT,
                                 SSL_CA_PEM,
                                 SSL_CLIENT_CERT_PEM,
                                 SSL_CLIENT_PRIVATE_KEY_PEM);
    if (status != NSAPI_ERROR_OK)
    {
        printf("Mqtt connect fail %d\r\n", status);
        mqttNetwork.close();
        Net_Disconnect();
        return -1;
    }
    printf("Connection established.\r\n\r\n");

    /* Establish a MQTT connection. */
    printf("MQTT client is connecting to the service ...\r\n");
    //MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS> mqttClient(mqttNetwork);
    mqttClient = new MQTT::Client<MQTTNetwork, Countdown, MQTT_MAX_PACKET_SIZE, MQTT_MAX_CONNECTIONS>(mqttNetwork);
    // Generate username, reference : https://docs.microsoft.com/ko-kr/azure/iot-hub/iot-hub-mqtt-support
    std::string username = std::string(MQTT_SERVER_HOST_NAME) + "/" + DEVICE_ID + "/?api-version=2018-06-30";

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 4; // 3 = 3.1 4 = 3.1.1
    data.clientID.cstring = (char*)DEVICE_ID;
    data.username.cstring = (char*)username.c_str();
    data.password.cstring = (char*)"ignored";

    int rc = mqttClient->connect(data);
    if (rc != MQTT::SUCCESS)
    {
        printf("ERROR: rc from MQTT connect is %d\r\n", rc);
        return -1;
    }
    printf("Client connected.\r\n\r\n");

    // Generates topic names from user's setting 
    mqtt_topic_pub = std::string("devices/") + DEVICE_ID + "/messages/events/";
    std::string mqtt_topic_sub = std::string("devices/") + DEVICE_ID + "/messages/devicebound/#";

    /* Subscribe a topic. */
    printf("Client is trying to subscribe a topic \"%s\".\r\n", mqtt_topic_sub.c_str());
    rc = mqttClient->subscribe(mqtt_topic_sub.c_str(), MQTT::QOS0, messageArrived);
    if (rc != MQTT::SUCCESS)
    {
        printf("ERROR: rc from MQTT subscribe is %d\r\n", rc);
        return -1;
    }
    printf("Client has subscribed a topic \"%s\".\r\n\r\n", mqtt_topic_sub.c_str());

    t.start(callback(&queue, &EventQueue::dispatch_forever));
    btn1.fall(queue.event(btn_handler));
    btn2.fall(queue.event(btn2_handler));


    while(1)
    {
        if(!mqttClient->isConnected())
        {
            printf("MQTT Client disconnect\r\n");
            return -1;
        }
        /* Waits a message and handles keepalive. */
        if(mqttClient->yield(100) != MQTT::SUCCESS)
        {
            printf("failed mqtt yield\r\n");
            //return -1;
        }
    }
}