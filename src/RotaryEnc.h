/*
 * RotaryEnc.h
 *
 *  Created on: 21.05.2018
 *      Author: Maxi
 */

#ifndef SRC_RotaryEnc_H_
#define SRC_RotaryEnc_H_

/* Describes new objects based on the Rotary  library */
#include <Rotary.h>

/* function pointer definition */
typedef void (*rotaryActionFunc)(bool clockwise, int id);

/* We describe an object in which we instantiate a rotary encoder
 * over an I2C expander.
 * It holds the information:
 *  * to which MCP object it's connected
 *  * which pin it is connected to
 *  * what function to call when there is a change
 * */
class RotaryEnc {
public:
    RotaryEnc(byte pinA, byte pinB, rotaryActionFunc actionFunc = nullptr, int id = 0)
    : rot(),
      pinA(pinA), pinB(pinB),
      actionFunc(actionFunc), id(id) {
    }

    /* Initialize object in the MCP */
    void init() {
        {
            pinMode(pinA, INPUT_PULLUP);
            pinMode(pinB, INPUT_PULLUP);
        }
    }

    /* On an interrupt, can be called with the value of the GPIOAB register (or INTCAP) */
    void update() {
        uint8_t pinValA = digitalRead(pinA);
        uint8_t pinValB = digitalRead(pinB);
        uint8_t event = rot.process(pinValA, pinValB);
        if(event == DIR_CW || event == DIR_CCW) {
            //clock wise or counter-clock wise
            bool clockwise = event == DIR_CW;
            //Call into action function if registered
            if(actionFunc) {
                actionFunc(clockwise, id);
            }
        }
    }



private:
    Rotary rot;                         /* the rotary object which will be created*/
    uint8_t pinA = 0;
    uint8_t pinB = 0;           /* the pin numbers for output A and output B */
    rotaryActionFunc actionFunc = nullptr;  /* function pointer, will be called when there is an action happening */
    int id = 0;                             /* optional ID for identification */
};


#endif /* SRC_RotaryEnc_H_ */
