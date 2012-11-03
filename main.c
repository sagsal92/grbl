/*
  main.c - An embedded CNC Controller with rs274/ngc (g-code) support
  Part of Grbl

  Copyright (c) 2009-2011 Simen Svale Skogsrud
  Copyright (c) 2011-2012 Sungeun K. Jeon

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

/* A big thanks to Alden Hart of Synthetos, supplier of grblshield and TinyG, who has
   been integral throughout the development of the higher level details of Grbl, as well
   as being a consistent sounding board for the future of accessible and free CNC. */

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "config.h"
#include "planner.h"
#include "nuts_bolts.h"
#include "stepper.h"
#include "spindle_control.h"
#include "coolant_control.h"
#include "motion_control.h"
#include "gcode.h"
#include "protocol.h"
#include "limits.h"
#include "report.h"
#include "settings.h"
#include "serial.h"

// Declare system global variable structure
system_t sys; 

int main(void)
{
  // Initialize system
  serial_init(BAUD_RATE); // Setup serial baud rate and interrupts
  st_init(); // Setup stepper pins and interrupt timers
  sei(); // Enable interrupts
  
  memset(&sys, 0, sizeof(sys));  // Clear all system variables
  sys.abort = true;   // Set abort to complete initialization
  sys.state = STATE_LOST;  // Set state to indicate unknown initial position
  
  for(;;) {
  
    // Execute system reset upon a system abort, where the main program will return to this loop.
    // Once here, it is safe to re-initialize the system. At startup, the system will automatically
    // reset to finish the initialization process.
    if (sys.abort) {
      // If a critical event has occurred, set the position lost system state. For example, a 
      // hard limit event can cause the stepper to lose steps and position due to an immediate
      // stop, not with a controlled deceleration. Or, if an abort was issued while a cycle
      // was active, the immediate stop can also cause lost steps.
      if (sys.state == STATE_ALARM) { sys.state = STATE_LOST; }
      
      // Reset system.
      serial_reset_read_buffer(); // Clear serial read buffer
      settings_init(); // Load grbl settings from EEPROM
      plan_init(); // Clear block buffer and planner variables
      gc_init(); // Set g-code parser to default state
      protocol_init(); // Clear incoming line data and execute startup lines
      spindle_init();
      coolant_init();
      limits_init();
      st_reset(); // Clear stepper subsystem variables.

      // Set cleared gcode and planner positions to current system position, which is only
      // cleared upon startup, not a reset/abort. If Grbl does not know or ensure its position,
      // a feedback message will be sent back to the user to let them know.
      gc_set_current_position(sys.position[X_AXIS],sys.position[Y_AXIS],sys.position[Z_AXIS]);
      plan_set_current_position(sys.position[X_AXIS],sys.position[Y_AXIS],sys.position[Z_AXIS]);
      
      // Reset system variables
      sys.abort = false;
      sys.execute = 0;
      if (sys.state == STATE_LOST && bit_istrue(settings.flags,BITFLAG_HOMING_ENABLE)) { 
        report_feedback_message(MESSAGE_POSITION_LOST); 
      } else {
        sys.state = STATE_IDLE;
      }
      if (bit_istrue(settings.flags,BITFLAG_AUTO_START)) { sys.auto_start = true; }
      
      // Execute user startup script
      protocol_execute_startup();
    }
    
    protocol_execute_runtime();
    protocol_process(); // ... process the serial protocol
    
  }
  return 0;   /* never reached */
}
