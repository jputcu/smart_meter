#define MY_RADIO_RF24

#include <Arduino.h>

#define MY_NODE_ID 3

#include <MySensors.h>
#include <MovingAveragePlus.h>
#include "dsmr.h"

namespace {
    constexpr uint8_t LED = A0; // on board LED is used by SPI

    // This is a full list of all the types we have
    using MyData = ParsedData<
            //p1_version_be
            //,active_energy_import_current_average_demand
            //,active_energy_import_maximum_demand_running_month
            //,active_energy_import_maximum_demand_last_13_months
            //,current_fuse_l1
            //,gas_equipment_id_be
            gas_delivered_be
            //,identification
            //,timestamp
            //,equipment_id
            , energy_delivered_tariff1, energy_delivered_tariff2
            //,energy_returned_tariff1
            //,energy_returned_tariff2
            //,electricity_tariff
            , power_delivered
            //,power_returned
            //,electricity_threshold
            //,electricity_switch_position
            //,message_long
            //,voltage_l1
            //,current_l1
            //,power_delivered_l1
            //,power_returned_l1
            //,gas_device_type
            //,gas_valve_position
    >;
    constexpr uint8_t REQUEST_PIN = 2;
    P1Reader reader(Serial, REQUEST_PIN);
    constexpr unsigned long POLL_INTERVAL_MS = 5000;
    constexpr uint16_t FORCE_SEND_S = 3000; // So domoticz knows the sensor is still alive
    uint16_t FORCE_SECONDS_PASSED = 0;

    uint32_t last_watt_average = 0;
    MovingAveragePlus<decltype(last_watt_average)> watt_average(5);
    float last_total_elec = 0;
    constexpr uint8_t ELEC_CHILD_ID = 1;
    MyMessage wattMsg(ELEC_CHILD_ID, V_WATT);
    MyMessage kWhMsg(ELEC_CHILD_ID, V_KWH);

    float last_gas = 0;
    constexpr uint8_t GAS_CHILD_ID = 2;
    MyMessage gas_vol(GAS_CHILD_ID, V_VOLUME);

    enum class ErrorType : uint8_t {
        SendError = 1,
        ParseError
    };

    void on_error(ErrorType errno) {
        for (uint8_t i = 0; i < static_cast<uint8_t>(errno); ++i) {
            digitalWrite(LED, HIGH);
            delay(300);
            digitalWrite(LED, LOW);
            delay(300);
        }
        delay(500);
    }
}

void setup() {
    pinMode(LED, OUTPUT);
    // Feedback that we started
    digitalWrite(LED, HIGH);
    delay(1000);
    digitalWrite(LED, LOW);

    Serial.begin(115200);
    reader.enable(true);
}

void presentation() {
    sendSketchInfo(F("Smart Meter"), F("0.2"));
    present(ELEC_CHILD_ID, S_POWER);
    present(GAS_CHILD_ID, S_GAS);
}

void loop() {
    if( reader.loop() ) {
        MyData data;
        if (reader.parse(data, nullptr)) {
            const bool force_send = FORCE_SECONDS_PASSED >= FORCE_SEND_S;
            if( force_send ) {
                FORCE_SECONDS_PASSED = 0;
            }

            {
                auto current_watt = data.power_delivered.int_val();
                watt_average.push(current_watt);
                auto new_average = watt_average.get();
                if ((new_average != last_watt_average) || force_send) {
                    if (send(wattMsg.set(new_average)))
                        last_watt_average = new_average;
                    else
                        on_error(ErrorType::SendError);
                }
            }

            {
                const float elec_tariff1 = data.energy_delivered_tariff1;
                const float elec_tariff2 = data.energy_delivered_tariff2;
                const float total_elec = elec_tariff1 + elec_tariff2;
                if ((total_elec != last_total_elec) || force_send) {
                    if (send(kWhMsg.set(total_elec, 3)))
                        last_total_elec = total_elec;
                    else
                        on_error(ErrorType::SendError);
                }
            }

            {
                const float current_gas = data.gas_delivered_be;
                if ((last_gas != current_gas) || force_send) {
                    if (send(gas_vol.set(current_gas, 3)))
                        last_gas = current_gas;
                    else
                        on_error(ErrorType::SendError);
                }
            }
        } else {
            on_error(ErrorType::ParseError);
            //Serial.println(res.fullError(str, end));
        }

        delay(POLL_INTERVAL_MS);
        FORCE_SECONDS_PASSED += POLL_INTERVAL_MS/1000;

        reader.enable(true);
    }
}
