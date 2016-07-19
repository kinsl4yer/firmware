/*
 * Copyright 2015 BrewPi/Elco Jacobs.
 *
 * This file is part of BrewPi.
 *
 * BrewPi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BrewPi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BrewPi.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#include "DS2408.h"
#include "ActuatorInterfaces.h"
#include "ControllerMixins.h"

/**
 * ValveController controls a single valve on a DS2408.
 *
 * Each DS2408 can control 2 valves, 1 with the upper 4 bits (valve A) and one with the lower 4 bits (valve B). \n
 * Bit 7-6 and 3-2 drive an H-bridge to control the valve motor. Bit 5-4 and 1-0 are used to read out the status of
 * the fully open and fully closed feedback switches. \n
 * bit 7-6: Valve A action: 01 = open, 10 = close, 11 = off, 00 = off but LEDS on \n
 * bit 5-4: Valve A status: 01 = opened, 10 = closed, 11 = in between \n
 * bit 3-2: Valve B action: 01 = open, 10 = close, 11 = off, 00 = off but LEDS on \n
 * bit 1-0: Valve B status: 01 = opened, 10 = closed, 11 = in between \n
 *
 */
class ValveController final : public ActuatorDigital, public ValveControllerMixin {
public:
    /**
     * Constructor that creates a new valve controller, using an already existing DS2408
     * @param device_ reference to the existing DS2408
     * @param output_ the valve is connected to either the upper or the lower bits. 1 for upper (B), 0 for lower (A)
     */
    ValveController(DS2408 & device_,
                    uint8_t  output_) :
                    device(device_),
                    output(output_)
					{
    }

    /**
     * Destructor does nothing. User is responsible for destructing the DS2408 when it is not used anymore.
     */
    ~ValveController() = default;

    /**
     * The valve itself can be in 3 states: fully closed, fully open or somewhere in between.
     */
    static const uint8_t VALVE_OPENED = 0b01; //  Feedback switch for fully open is connected to GND.
    static const uint8_t VALVE_CLOSED = 0b10; // = 0b10  Feedback switch for fully closed is connected to GND
    static const uint8_t VALVE_HALFWAY= 0b11; // = 0b11  Neither switches are closed, so valve is neither open or closed

    /**
     * The motor can be driven in clockwise, anti-clockwise or idle
     */
    static const uint8_t VALVE_OPENING = 0b01; // H-bridge is driven in direction to open the valve
    static const uint8_t VALVE_CLOSING = 0b10; // H-bridge is driven in direction to close the valve
    static const uint8_t VALVE_IDLE = 0b11;    // H-bridge has both legs at same level, so motor is idle

    /**
     * Gets the state of the single valve (chosen by output nr). \n
     * State is based on reading the I/O pins connected to the feedback switches. \n
     * @returns state of valve (0b01 = opened, 0b10 = closed, 0b11 = in between \n
     */
    uint8_t getState() const {
        uint8_t states = device.readPios(true);
        if(output == 1){
            states = states >> 4;
        }
        return states & 0b11;
    }

    /**
     * Gets the action currently performed by the motor of the valve, read from the latches. \n
     * @returns action performed by valve (0b01 = opening, 0b10 = closing, 0b11 = idle, 0b00= idle \n
     */
    uint8_t getAction() const {
        uint8_t latches = device.readLatches(true);
        if(output == 1){
            latches = latches >> 4;
        }
        return (latches >> 2) & 0b11;
    }

    /**
     * update reads the status from the valve. It does not start opening or closing.
     * When the valve is opening or closing, it reverts back to idle when it detects that the action is completed.
     */
    void update() override final;

    /**
     * fastUpdate is not needed for valves, because they are very slow. This function is a nop placeholder for compatibility
     * with the actuator interface.
     */
    void fastUpdate() override final {} // valves are slow. Fast update is nop to limit OneWire traffic

    /**
     * setActive will open or close the valve, for compatibility with the actuator interface.
     * @param active true opens the valve, false closes it.
     */
    void setActive(bool active) override final {
        if(active){
            open();
        }
        else{
            close();
        }
    }

    /**
     * Check if valve is open.
     * @return true if open, false if closed.or halfway
     */
    bool isActive() const override final {
        // return active when not closed, so a half open valve also returns active
        return getAction() != VALVE_CLOSING;
    }

    /**
     * Returns the state of the valve (action and current state) as a single 4 bit value
     * @param doUpdate when true, read new values from the hardware device
     * @return 4-bit value, with upper 2 bits motor state and lower bits the valve state.
     */
    uint8_t read(bool doUpdate = true){
        if(doUpdate){
            update();
        }
        return (getAction() << 2 | getState() );
    }

    /**
     * Apply a new motor state to the valve.
     * @param action the new motor state (VALVE_OPENING, VALVE_CLOSING or VALVE_IDLE)
     */
    void write(uint8_t action);

    /**
     * Open the valve
     */
    inline void open(){
        write(VALVE_OPENING);
    }

    /**
     * Close the valve
     */
    inline void close(){
        write(VALVE_CLOSING);
    }

    /**
     * Stop opening or closing the valve. The valves themselves automatically stop driving the motor with an internal switch.
     * This function stops the H-bridge from driving the motor. It could be used to stop the valve halfway.
     */
    inline void idle(){
        write(VALVE_IDLE);
    }

    /**
     * This function can be used to get a reference to the DS2408, so it can be shared with another valve controller.
     * The caller can also use it to destroy the DS2408 object when it is not used by any valve controller anymore.
     * @return reference to the DS2408.
     */
    DS2408 & getHardwareDevice(){
    	return device;
    }

protected:
    DS2408 & device;
    uint8_t output; // 0=A or 1=B


    friend class ValveControllerMixin;
};

