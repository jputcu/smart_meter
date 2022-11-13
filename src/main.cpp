//#define MY_DEBUG
#define MY_RADIO_RF24
#define MY_NODE_ID 3
#define MY_RF24_CS_PIN 13
#include <MySensors.h>
#include <MovingAveragePlus.h>

#include "dsmr.h"

using MyData = ParsedData<
    energy_delivered_tariff1,
    energy_delivered_tariff2,
    voltage_l1,
    power_delivered_l1,
    gas_delivered_be
    >;

struct Printer
{
  template <typename Item>
  void apply(Item &i)
  {
    if (i.present())
    {
      Serial.print(Item::name);
      Serial.print(F(": "));
      Serial.print(i.val());
      Serial.print(Item::unit());
      Serial.println();
    }
  }
};

#define REQUEST_PIN 2
#define GREEN_LED 7
#define YELLOW_LED 9

P1Reader reader(&Serial1, REQUEST_PIN);

uint32_t last_watt_average = 0;
MovingAveragePlus<decltype(last_watt_average)> watt_average(5);
float last_total_elec = 0;
constexpr uint8_t ELEC_CHILD_ID = 1;
MyMessage wattMsg(ELEC_CHILD_ID, V_WATT);
MyMessage kWhMsg(ELEC_CHILD_ID, V_KWH);

float last_gas = 0;
constexpr uint8_t GAS_CHILD_ID = 2;
MyMessage gas_vol(GAS_CHILD_ID, V_VOLUME);

void setup() {
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  pinMode(YELLOW_LED, OUTPUT);
  digitalWrite(YELLOW_LED, LOW);

  Serial.begin(115200);
  Serial1.begin(115200);

  reader.enable(false);
}

void presentation() {
  sendSketchInfo(F("Smart Meter"), F("0.1"));
  present(ELEC_CHILD_ID, S_POWER);
  present(GAS_CHILD_ID, S_GAS);
}

void loop() {
  reader.loop();

  if (reader.available())
  {
    MyData data;
    String err;
    if (reader.parse(&data, &err))
    {
      //data.applyEach(Printer());

      auto current_watt = data.power_delivered_l1.int_val();
      watt_average.push(current_watt);
      auto new_average = watt_average.get();
      if( new_average != last_watt_average ) {
        send(wattMsg.set(new_average));
        last_watt_average = new_average;
      }
      float elec_tariff1 = data.energy_delivered_tariff1;
      float elec_tariff2 = data.energy_delivered_tariff2;
      float total_elec = elec_tariff1 + elec_tariff2;
      //Serial.println(total_elec);
      if( total_elec != last_total_elec ) {
        send(kWhMsg.set(total_elec, 3));
        last_total_elec = total_elec;
      }

      float current_gas = data.gas_delivered_be;
      if( last_gas != current_gas ) {
        send(gas_vol.set(current_gas, 3));
        last_gas = current_gas;
      }
    }
    else {
      Serial.println(err);
    }
  }
}
