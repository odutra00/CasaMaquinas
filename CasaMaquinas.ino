// This firmware manages a machine's pool house with several features.
//Please refer to https://github.com/odutra00/CasaMaquinas
#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include "AppInsights.h"
#include <driver/adc.h> // ADC library
#include <thermistor.h>
#include <esp_rmaker_standard_types.h>

// Set Defalt Values
#define DEFAULT_TEMPERATURE_PISCINA 30
#define DEFAULT_MIN_TEMPERATURE_PISCINA 20
#define DEFAULT_MAX_TEMPERATURE_PISCINA 40
#define DEFAULT_SLIDEBAR_INCREMENT 1
//#define DEFAULT_POWER_MODE true
//#define DEFAULT_POWER_MODE_AQUECIMENTO false
#define NOMINAL_NTC_RES 10000
#define NTC_BETA_PISCINA 3950 //3950
#define NTC_BETA_PLACAS  3950 //3950
#define NOMINAL_NTC_SERIAL_RES_PISCINA 991
#define NOMINAL_NTC_SERIAL_RES_PLACAS 987
#define NUMBER_OF_MINUTES_SEND_TEMP 1
#define NUMBER_OF_MINUTES_KEEP_BOMBLADRAO 5
unsigned long currentTime1 = 0;
unsigned long currentTime2 = 0;
unsigned long timer1StartTime = 0;
unsigned long timer1Duration = NUMBER_OF_MINUTES_SEND_TEMP * 60000; // 1 minute in milliseconds
unsigned long timer2StartTime = 0;
unsigned long timer2Duration = NUMBER_OF_MINUTES_KEEP_BOMBLADRAO * 60000; // 5 minutes in milliseconds
const char *service_name = "CasaMaquinas";
const char *pop = "Odilon";
// Define a global boolean variable
bool FIRST_CONNECTION = false;
unsigned long currentMillis = millis();
unsigned long previousMillis = 0;
unsigned long RECONNECT_INTERVAL = 10000; // Check every 10 seconds

bool FLAG_TRIGGER_BOMBINHA_OFF = false;

// GPIO for push button
#if CONFIG_IDF_TARGET_ESP32C3
static int gpio_0 = 9;
static int gpio_switch = 7;
#else
// GPIO for virtual device
static int gpio_0 = 0;
static int gpio_switch_Filtragem = 16;//pino 25
static int gpio_switch_LEDs = 17;     //pino 27
static int gpio_switch_Aquecimento = 4;      //pino 24
static int gpio_Nivel = 5;            //pino 34
static int gpio_switch_BombinhaLadrao = 18;  //pino 35
static int gpio_switch_ION = 19;      //pino 38
#endif

/* Variable for reading pin status*/
bool switch_state_Filtragem = true; //placas de reles acionados com 0
bool switch_state_LEDs = true; //placas de reles acionados com 0
bool thermostat_state_Aquecimento = true; //placas de reles acionados com 0
bool switch_state_bombinhaLadrao = true;  //placas de reles acionados com 0
bool switch_state_ION = true;             //placas de reles acionados com 0
bool nivel_Interruption_Flag = false;
//Variable to read slidebar value from thermost 
int slidebar_thermostat_state_Aquecimento = DEFAULT_TEMPERATURE_PISCINA;

// The framework provides some standard device types like switch, lightbulb,
// fan, temperaturesensor.
static Switch *switch_Filtragem = NULL;
static Switch *switch_LEDs = NULL;
static Switch *switch_BombinhaLadrao = NULL;
static Switch *switch_ION = NULL;
static TemperatureSensor TemperaturaPiscina("Piscina");
static TemperatureSensor TemperaturaPlacas("Placas");
static Device thermostat_Aquecimento("Aquecimento", "esp.device.thermostat", &gpio_switch_Aquecimento);
THERMISTOR ntc_Piscina (A6, NOMINAL_NTC_RES, NTC_BETA_PISCINA, NOMINAL_NTC_SERIAL_RES_PISCINA);
THERMISTOR ntc_Placas  (A3, NOMINAL_NTC_RES, NTC_BETA_PLACAS, NOMINAL_NTC_SERIAL_RES_PLACAS);

void sysProvEvent(arduino_event_t *sys_event)
{
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32S2
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n",
                      service_name, pop);
        printQR(service_name, pop, "softap");
#else
        Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n",
                      service_name, pop);
        printQR(service_name, pop, "ble");
#endif
        break;
    case ARDUINO_EVENT_PROV_INIT:
        wifi_prov_mgr_disable_auto_stop(10000);
        break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
        wifi_prov_mgr_stop_provisioning();
        break;
    //Try to add feature for forcing reconnecting from time to time
    case SYSTEM_EVENT_STA_GOT_IP:
        FIRST_CONNECTION = true; // Set the boolean to true when connected
        Serial.println("WiFi Connected");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        // wifiConnected = false; // Set the boolean to false when disconnected
        Serial.println("WiFi Disconnected");
        break;
    default:;
    }
}




void write_callback_Filtragem(Device *device, Param *param, const param_val_t val,
                    void *priv_data, write_ctx_t *ctx)
{
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();

    if (strcmp(param_name, "Power") == 0) {
        Serial.printf("Received value = %s for %s - %s\n",
                      val.val.b ? "true" : "false", device_name, param_name);
        switch_state_Filtragem = val.val.b;
        (switch_state_Filtragem == false) ? digitalWrite(gpio_switch_Filtragem, HIGH)
        : digitalWrite(gpio_switch_Filtragem, LOW);
        param->updateAndReport(val);
    }
}

void write_callback_LEDs(Device *device, Param *param, const param_val_t val,
                         void *priv_data, write_ctx_t *ctx)
{
    // Handle changes in the switch_LEDs state here
    // Update GPIO state and report parameter
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();

    if (strcmp(param_name, "Power") == 0) {
        Serial.printf("Received value = %s for %s - %s\n",
                      val.val.b ? "true" : "false", device_name, param_name);
        switch_state_LEDs = val.val.b;
        (switch_state_LEDs == false) ? digitalWrite(gpio_switch_LEDs, HIGH)
        : digitalWrite(gpio_switch_LEDs, LOW);
        param->updateAndReport(val);
    }
}


void write_callback_BombinhaLadrao(Device *device, Param *param, const param_val_t val,
                         void *priv_data, write_ctx_t *ctx)
{
    // Handle changes in the switch_BombinhaLadrao state here
    // Update GPIO state and report parameter
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();

    if (strcmp(param_name, "Power") == 0) {
        Serial.printf("Received value = %s for %s - %s\n",
                      val.val.b ? "true" : "false", device_name, param_name);
        switch_state_bombinhaLadrao = val.val.b;
        (switch_state_bombinhaLadrao == false) ? digitalWrite(gpio_switch_BombinhaLadrao, HIGH)
        : digitalWrite(gpio_switch_BombinhaLadrao, LOW);
        param->updateAndReport(val);
    }
}

void write_callback_ION(Device *device, Param *param, const param_val_t val,
                         void *priv_data, write_ctx_t *ctx)
{
    // Handle changes in the switch_ION state here
    // Update GPIO state and report parameter
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();

    if (strcmp(param_name, "Power") == 0) {
        Serial.printf("Received value = %s for %s - %s\n",
                      val.val.b ? "true" : "false", device_name, param_name);
        switch_state_ION = val.val.b;
        (switch_state_ION == false) ? digitalWrite(gpio_switch_ION, HIGH)
        : digitalWrite(gpio_switch_ION, LOW);
        param->updateAndReport(val);
    }
}


//esse callback é para aquecimento como thermostat
void write_callback_Aquecimento(Device *device, Param *param, const param_val_t val,
                                void *priv_data, write_ctx_t *ctx)
{
    // Handle changes in the Aquecimento thermostat state here
    // You can update GPIO state or perform other actions as needed
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();
    if (strcmp(param_name, "Power") == 0) {
        Serial.printf("Call Back - param_name = Power = %s for %s - %s\n",
                      val.val.b ? "true" : "false", device_name, param_name);
        // Handle the thermostat state change here, if needed
        thermostat_state_Aquecimento = val.val.b;
        (thermostat_state_Aquecimento == false) ? digitalWrite(gpio_switch_Aquecimento, HIGH)
          : digitalWrite(gpio_switch_Aquecimento, LOW);
          param->updateAndReport(val);
    }

    else if (strcmp(param_name, "Temperatura programada") == 0) {
        // Handle changes in the "AC Mode" parameter switch
        Serial.printf("Call Back - param_name = Temperatura Programada -> Received value = %d",
                      val.val.i );
        // You can perform actions based on the AC mode change here, if needed
        // For example, you can turn on/off the AC based on the AC mode
        slidebar_thermostat_state_Aquecimento = val.val.i;
        param->updateAndReport(val);
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(gpio_0, INPUT);
    pinMode(gpio_switch_Filtragem, OUTPUT);
    digitalWrite(gpio_switch_Filtragem, switch_state_Filtragem);
    pinMode(gpio_switch_LEDs, OUTPUT);
    digitalWrite(gpio_switch_LEDs, switch_state_LEDs);
    pinMode(gpio_switch_Aquecimento, OUTPUT);
    digitalWrite(gpio_switch_Aquecimento, thermostat_state_Aquecimento);
    pinMode(gpio_Nivel, INPUT);
    attachInterrupt(gpio_Nivel, trataInterrupcaoNivel, CHANGE); // Configura a interrupção para acionar em qualquer mudança de nível
    pinMode(gpio_switch_BombinhaLadrao, OUTPUT);
    digitalWrite(gpio_switch_BombinhaLadrao, switch_state_bombinhaLadrao);
    pinMode(gpio_switch_ION, OUTPUT);
    digitalWrite(gpio_switch_ION, switch_state_ION);


    if (digitalRead(gpio_Nivel) == LOW){ //there is water. Start esp32 with BombinhaLadrao on
      nivel_Interruption_Flag = true;
    }


    Node my_node;
    my_node = RMaker.initNode("Casa de Maquinas");


    // Initialize ADC
    adc1_config_width(ADC_WIDTH_BIT_12); // 12-bit resolution
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); // Configure ADC1 Channel 0 with attenuation - Piscina
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11); // Configure ADC1 Channel 0 with attenuation - Plcas

    // Initialize switch devices
    switch_Filtragem      = new Switch("Filtragem", &gpio_switch_Filtragem,       false);
    switch_LEDs           = new Switch("LEDs",      &gpio_switch_LEDs,            false);
    switch_BombinhaLadrao = new Switch("Bombinha",  &gpio_switch_BombinhaLadrao,  false);
    switch_ION            = new Switch("Ionizador", &gpio_switch_ION,             false);
    //switch_Aquecimento = new Switch("switch_Aquecimento", &gpio_switch_Aquecimento);
    if (!switch_Filtragem || !switch_LEDs || !switch_BombinhaLadrao || !switch_ION){ //|| !switch_Aquecimento) {
        return;
    }
    
    // Standard switch devices
    switch_Filtragem->addCb(write_callback_Filtragem);
    switch_LEDs->addCb(write_callback_LEDs);
    switch_BombinhaLadrao->addCb(write_callback_BombinhaLadrao);
    switch_ION->addCb(write_callback_ION);

    // Add switch device to the node
    my_node.addDevice(*switch_Filtragem);
    my_node.addDevice(*switch_LEDs);
    my_node.addDevice(*switch_BombinhaLadrao);
    my_node.addDevice(*switch_ION);
    //my_node.addDevice(*switch_Aquecimento);
    my_node.addDevice(TemperaturaPiscina);
    my_node.addDevice(TemperaturaPlacas);
        
    Param parametro_set_temperatura("Temperatura programada", 
                                    ESP_RMAKER_PARAM_RANGE, 
                                    value(DEFAULT_TEMPERATURE_PISCINA), 
                                    PROP_FLAG_READ | PROP_FLAG_WRITE);
    parametro_set_temperatura.addUIType(ESP_RMAKER_UI_SLIDER);
    parametro_set_temperatura.addBounds(value(DEFAULT_MIN_TEMPERATURE_PISCINA), 
                                        value(DEFAULT_MAX_TEMPERATURE_PISCINA), 
                                        value(DEFAULT_SLIDEBAR_INCREMENT));
    // Param parametro_temperatura("Temperatura da Piscina", 
    //                               ESP_RMAKER_PARAM_TEMPERATURE, 
    //                               value(0), 
    //                               PROP_FLAG_READ);// | PROP_FLAG_WRITE);

    // Create the "Power" parameter for the thermostat
    Param parametro_power("Power",
                     ESP_RMAKER_PARAM_POWER,
                     value(true), // Initial value (true for on, false for off)
                     PROP_FLAG_READ | PROP_FLAG_WRITE);
    parametro_power.addUIType(ESP_RMAKER_UI_TOGGLE);
                            
    //thermostat_Aquecimento.addParam(parametro_ac_mode);
    thermostat_Aquecimento.addParam(parametro_set_temperatura);
    //thermostat_Aquecimento.addParam(parametro_temperatura);
    thermostat_Aquecimento.addParam(parametro_power);
    thermostat_Aquecimento.addCb(write_callback_Aquecimento);
    // Add the Thermostat device to the node
    my_node.addDevice(thermostat_Aquecimento);

    // This is optional
    RMaker.enableOTA(OTA_USING_TOPICS);
    // If you want to enable scheduling, set time zone for your region using
    // setTimeZone(). The list of available values are provided here
    // https://rainmaker.espressif.com/docs/time-service.html
    RMaker.setTimeZone("America/Sao_Paulo");
    //  Alternatively, enable the Timezone service and let the phone apps set the
    //  appropriate timezone
    RMaker.enableTZService();

    RMaker.enableSchedule();

    RMaker.enableScenes();
    // Enable ESP Insights. Insteads of using the default http transport, this function will
    // reuse the existing MQTT connection of Rainmaker, thereby saving memory space.
    initAppInsights();

    RMaker.enableSystemService(SYSTEM_SERV_FLAGS_ALL, 2, 2, 2);

    RMaker.start();

    WiFi.onEvent(sysProvEvent);
#if CONFIG_IDF_TARGET_ESP32S2
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE,
                            WIFI_PROV_SECURITY_1, pop, service_name);
#else
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM,
                            WIFI_PROV_SECURITY_1, pop, service_name);
#endif
}

void loop()
{
  currentTime1 = millis();
  if (currentTime1 - timer1StartTime >= timer1Duration) {
    // Timer 1 duration reached
    timer1StartTime = currentTime1; // Reset timer 1
    sendTemperaturas();    
  }
  currentTime2 = millis();
  if ( (currentTime2 - timer2StartTime >= timer2Duration) && (FLAG_TRIGGER_BOMBINHA_OFF) ) {
    // Timer 2 duration reached
    turnOffBombinhaLadrao();
    }  

  if (digitalRead(gpio_0) == LOW) {  // Push button pressed
      // Key debounce handling
      delay(100);
      int startTime = millis();
      while (digitalRead(gpio_0) == LOW) {
          delay(50);
      }
      int endTime = millis();

      if ((endTime - startTime) > 10000) {
          // If key pressed for more than 10secs, reset all
          Serial.printf("Reset to factory.\n");
          RMakerFactoryReset(2);
      } else if ((endTime - startTime) > 3000) {
          Serial.printf("Reset Wi-Fi.\n");
          // If key pressed for more than 3secs, but less than 10, reset Wi-Fi
          RMakerWiFiReset(2);
      } else {
          // Toggle device state
          switch_state_Filtragem = !switch_state_Filtragem;
          Serial.printf("Toggle State to %s.\n", switch_state_Filtragem ? "true" : "false");
          if (switch_Filtragem) {
              switch_Filtragem->updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME,
                                              switch_state_Filtragem);
          }
          (switch_state_Filtragem == false) ? digitalWrite(gpio_switch_Filtragem, HIGH)
          : digitalWrite(gpio_switch_Filtragem, LOW);
      }
  }


    
  //To activate bombinhaLadrao to remove water from machine-house.
  if (nivel_Interruption_Flag){
    nivel_Interruption_Flag = false;
    if (digitalRead(gpio_Nivel) == LOW){ //tem agua. Ligar bombinhaLadrao
      // Toggle device state
      switch_state_bombinhaLadrao = true; //modulo rele ativo em 0
      FLAG_TRIGGER_BOMBINHA_OFF = false;
      Serial.printf("Tem agua. Aciona BombinhaLadrao to %s.\n", switch_state_bombinhaLadrao ? "true" : "false");
      if (switch_BombinhaLadrao) {
                switch_BombinhaLadrao->updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME,
                                                switch_state_bombinhaLadrao);
            }
            digitalWrite(gpio_switch_BombinhaLadrao, LOW);     //activates bombinha ladrao
    }
    else{ //reed switch off again. However, there is still water in the dead volume.
          //we will keep bombinhaLadrao on for more NUMBER_OF_MINUTES_KEEP_BOMBLADRAO minutes.  
      currentTime2 = millis();
      timer2StartTime = currentTime2; // Reset timer 2   
      FLAG_TRIGGER_BOMBINHA_OFF = true;   
    }
  }


  if ( FIRST_CONNECTION && (WiFi.status() != WL_CONNECTED)) {
    if (currentMillis - previousMillis >= RECONNECT_INTERVAL) {        // First time connection is lost
      Serial.println("Wi-Fi connection lost. Timeout over. Reconnecting...");
      WiFi.disconnect();
      WiFi.reconnect();
      // reconnecting = true;
      // Subsequent reconnection attempts
      previousMillis = currentMillis;
    }
  }


  delay(100);
}


void sendTemperaturas()
{
  // Read ADC value (replace with your actual ADC reading code)
  int adc_raw_value_piscina = ntc_Piscina.read();//analogRead(A6);
  int adc_raw_value_placas  = ntc_Placas.read();//analogRead(A3);

  // Convert the raw ADC value to a float based on your sensor calibration
  float adc_value_piscina = static_cast<float>(adc_raw_value_piscina)/10; // Convert to float
  float adc_value_placas  = static_cast<float>(adc_raw_value_placas)/10; // Convert to float
  // Update the parameter with the correct data type (float)
  TemperaturaPiscina.updateAndReportParam("Temperature", adc_value_piscina);
  TemperaturaPlacas.updateAndReportParam("Temperature", adc_value_placas);
  //thermostat_Aquecimento.updateAndReportParam("Temperature da Piscina", adc_value_piscina);

 // Compare temperatures and set the Aquecimento Thermostat state
  if (adc_value_piscina < adc_value_placas && adc_value_piscina < slidebar_thermostat_state_Aquecimento) {
    thermostat_Aquecimento.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, true); // Turn on Aquecimento
    thermostat_state_Aquecimento = true;
    digitalWrite(gpio_switch_Aquecimento, LOW);
    Serial.println("Aquecimento turned on. Deveria ter atualizado os toggle Power no app.");
  } else {
    thermostat_Aquecimento.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, false); // Turn off Aquecimento
    thermostat_state_Aquecimento = false;
    digitalWrite(gpio_switch_Aquecimento, HIGH);
    Serial.println("Aquecimento turned off.  Deveria ter atualizado os toggle Power no app.");
  }

  // Print the temperature value
  Serial.print("Temperatura da Piscina - ");
  Serial.println(adc_value_piscina);
  Serial.print("Temperatura das Placas - ");
  Serial.println(adc_value_placas);
  Serial.print("Goal Temperatura Piscina -");
  Serial.println(slidebar_thermostat_state_Aquecimento);
}


void trataInterrupcaoNivel(){
  nivel_Interruption_Flag = true;
}

void turnOffBombinhaLadrao(){
  nivel_Interruption_Flag = false;
  FLAG_TRIGGER_BOMBINHA_OFF = false;
  Serial.printf(" \n \n \n \n turn off Bombinha ")  ;
  // Toggle device state
        switch_state_bombinhaLadrao = false; //modulo rele desativado em 1
        Serial.printf("Nao tem agua. Desliga BombinhaLadrao to %s.\n", switch_state_bombinhaLadrao ? "true" : "false");
        if (switch_BombinhaLadrao) {
                  switch_BombinhaLadrao->updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME,
                                                  switch_state_bombinhaLadrao);
              }
              digitalWrite(gpio_switch_BombinhaLadrao, HIGH);     //activates bombinha ladrao
}