//McCallister Security Solutions: Museum Security Sensor

#include <stdio.h>
#include <string.h>
#include "mcc_generated_files/system/system.h"

// prototypes
void initLCD(void);
void clearLCD(void);
void homeLCD(void);
void putcharLCD(void);
void setaddrLCD(unsigned char);
void setline0LCD(void);
void setline1LCD(void);

#define sample_lower 23    // Lower bound of CCP values
#define sample_upper 83    // Upper bound of CCP values

unsigned int hightime, echo_delay, sec_timer;
unsigned int sample = sample_lower;
unsigned short ms_timer, ave_inches;
unsigned long sum, current_inches;


//================CONFIGURATION============CONFIGURATION============CONFIGURATION=================================

/*
 
 CONFIGURATION SECTION MCC:
 * Resources: ADC and I2C_Host
 * RC4 for BUTTON
 * RB4 and RB6 for SDA and SCL respectively
 * RB7 for BEEPN
 * RC3 for BEEPP
 * RC0 for POT (configure ADC to use AN4 and RIGHT ALIGN)
 * THIS PROJECT REQUIRES i2c-LCD-2024.c, MODIFIED VERSION IS PREFERRED
 * THIS PROECT ALSO REQUIRES A MODIFIER I2C HOST .C FILE IN ORDER TO NOT STACK OVERFLOW
 */

int button_length;

// main menu
#define NUM_ENTRIES_MAIN 4

char* main_entries[NUM_ENTRIES_MAIN] = {"Config Mode", "Time", "Angle", "Distance"};
int main_selection = 0;

// ------------------------- CONFIGURATION SECTION ------------------------- //

// Number of entries at Depth 0 for config
#define NUM_ENTRIES_D0 8

// Number of menu options available in each D1 menu item
const int NUM_ENTRIES_D1[NUM_ENTRIES_D0] = {
    0,                          // exit config
    5,                          // show events
    0,                          // motor period
    0,                          // ultrasonic read speed
    0,                          // num averages
    3,                          // field of view range
    3,                          // distance range
    0,                          // self destruct (so sad)
};

// Menu option specifications in D0
// D0
char* conf_entries[NUM_ENTRIES_D0] = {
    "Exit Config",
    "Show Events",
    "Motor Period",
    "Ultrasonic Read Speed",
    "Num Averages Ultrasonic",
    "Field of View Range",
    "Distance Range",
    "Self Destruct Sequence"
};

// Option in security event menu at D1
char* security_entries_options[] = {"Exit Event", "Clear Event", "Show Time", "Show Angle", "Show Min Distance"};
char* FOV_entries[] = {"Exit", "Max FOV", "Min FOV"};
char* dist_entries[] = {"Exit", "Max Dist", "Min Dist"};


char** conf_entries_D1[NUM_ENTRIES_D0] = { // Table of pointers to tables of entries in D1
    0,                          // exit config
    security_entries_options,    // show events
    0,                          // motor period
    0,                          // ultrasonic read speed
    0,                          // num averages
    FOV_entries,               // field of view range
    dist_entries,              // distance range
    0,                          // self destruct
};

// menu variables
#define MENU_DEPTH 2
#define SILLY_SCROLLER_PERIOD 40 // 10s of ms
int config_menu_active; // is the config mode activated or not?
int num_entries = NUM_ENTRIES_MAIN; // default start state
int conf_selection[MENU_DEPTH]; // which item is selected at each depth
int conf_depth; 
int silly_scroller_counter; // counter for the silly scroller
unsigned int silly_scroller_max;
unsigned int silly_scroller_ind;
int silly_scroller_direction;
int silly_scroller_delay_mult;
char* current_text; // keeps track of the text of the entry currently selected



// CONFIGURATION VARIABLES
unsigned int security_event_time; // s
unsigned int security_event_angle; // deg
unsigned int security_event_distance; // in
int motor_period; // ms
unsigned int ultrasonic_read_speed; // ms 
int num_ultrasonic_averages; 
int FOV_range_min; // degrees  
int FOV_range_max; // degrees  
int dist_min; // inches  
int dist_max; // inches  
//#define FOV_range_min 0
//#define FOV_range_max 180
//#define dist_min 12
//#define dist_max 120


// CONFIGURATION CALCULATIONS
int steps_per_period;
float step_size_deg;
int step_size_CCP;

/*
 * main limiting factors:
 * max angular speed:
 * 4000 ms over 360 degrees (180 degrees both ways)
 * 90 degrees per second OR 0.09 degrees per ms
 * 
 * angular_speed = (FOV_range_max - FOV_range_min)/motor_period
 * 
 * max degrees per step:
 * 30 degrees per step
 * 
 * num_steps = (motor_period/2)/(ultrasonic_read_speed*num_ultrasonic_averages)
 * degrees_per_step = (FOV_range_max - FOV_range_min)/num_steps
 * 
 * minimum ultrasonic read speed:
 * 60 ms
 * 
 * dist: 1 to 156 inches
 * servo range: 0 to 180 degrees
 * 
 * 180 > FOV_range_max > 0
 * 180 > FOV_range_min > 0
 * 
 * FOV_range_max > FOV_range_min
 * 
 
 
 */

// MISC FUNCTIONS
int button_check_config() {
    if (BUTTON_PORT == 0) { // if button is pressed
        button_length++;
        if (button_length > 100) { // long press
            
            button_length = 0; // acknowledged
            return 0; // code for long press
            
        }
    } else if (button_length > 0) { // short press    
        
        button_length = 0; // acknowledged
        return 1; // code for short press
        
    }
    return -1; // otherwise
}

int button_check_main() {
    if (BUTTON_PORT == 0) { // if button is pressed
        button_length++;
        if (button_length > 10) {
            button_length = 0;
            return 0; // long press
        }
    } else if (button_length > 0) { // short press    
        
        button_length = 0; // acknowledged
        return 1; // code for short press
        
    }
    return -1; // otherwise
}

// INITIALIZATION/RESET FUNCTIONS -------------------------------------------------------------------------------- //
void reset_menu_vars() {
    conf_selection[0] = 0;
    conf_selection[1] = 0;
    conf_depth = 0;
    num_entries = config_menu_active ? NUM_ENTRIES_D0 : NUM_ENTRIES_MAIN;
    silly_scroller_counter = 0;
    silly_scroller_max = 0;
    silly_scroller_ind = 0;
    silly_scroller_direction = 1;
    silly_scroller_delay_mult = 1;
    current_text = ""; // keeps track of the text of the entry currently selected
    
    main_selection = 0;
}

void init_config() {
    reset_menu_vars();
    config_menu_active = 0;
    motor_period = 4000;
    ultrasonic_read_speed = 200;
    num_ultrasonic_averages = 2;
    
    steps_per_period = 8;
    step_size_deg = 22.5;
    step_size_CCP = 7;
    
    security_event_time = 0;
    security_event_angle = 0;
    security_event_distance = 200;
    
    FOV_range_min = 0;
    FOV_range_max = 180;
    dist_min = 12;
    dist_max = 120;
}

void reset_silly_scroller() {
    silly_scroller_max = strlen(current_text) - 14; // max scroll silly scroller must achieve
    if (silly_scroller_max > 100) { // overflow case
        silly_scroller_max = 0;
    }
    silly_scroller_ind = 0; // reset silly scroller to normal
    silly_scroller_direction = 1;
    silly_scroller_delay_mult = 1;
    silly_scroller_counter = 0;
}

// COMPUTATION/GET UTILITY FUNCTIONS -------------------------------------------------------------------------------- //
void compute_steps_per_period() {
    steps_per_period = motor_period/(ultrasonic_read_speed*num_ultrasonic_averages);
}

char* get_entry(int add) { // get name of entry that is selected, add "add" to the index to get next value if needed
    if (config_menu_active) {
        switch (conf_depth) {
            case 0: // D0, normal behavior
                return conf_entries[conf_selection[0] + add];
            case 1: // D1, use special table
                return conf_entries_D1[conf_selection[0]][conf_selection[1] + add];
        }
    } else { // main menu mode
        return main_entries[main_selection + add];
    }
}

// DISPLAY FUNCTIONS -------------------------------------------------------------------------------- //
void fancy_display(char* data, int length, char start_char, char end_char, unsigned int offset) { // display with .. for too long and start and end chars and offset
    int extra_chars = 0;
    
    if (start_char != '\0') { // account for extra characters due to start and end chars
        extra_chars++;
        printf("%c", start_char);
    }
    if (end_char != '\0') {
        extra_chars++;
    }
    
    char c;
    int i;
    i = 0;
    
    do {
        if (i - offset >= length - extra_chars - 2) { // this accounts for the possibility of having 2 periods and however many extra chars specified from start and end chars
            if (strlen(data) - offset > length - extra_chars) { // if we know we'll run out of space we need to start adding dots
                c = '.';
            } else {
                c = data[i + offset];
            }
        } else {
            c = data[i + offset];
        }

        if (c == '\0') { break; } // if string ends, stop drop and roll

        printf("%c", c);

        i++;
    } while (i < length - extra_chars);
    
    if (end_char != '\0') {
        printf("%c", end_char);
    }
}

void update_display() { // display proper entries based on selection
    clearLCD();
    setline0LCD();
    char* entry1 = get_entry(0); // get current entry and display it
    
    fancy_display(entry1, 16, '[', ']', 0);
    
    if ((config_menu_active ? conf_selection[conf_depth] : main_selection) + 1 < num_entries) { // get next entry and display it if it exists
        
        char* entry2 = get_entry(1);
        
        setline1LCD(); 
        fancy_display(entry2, 16, '\0', '\0', 0);
        
    }
    printf("                "); // get rid of annoying cursor
}

// MENU UTILITY FUNCTIONS -------------------------------------------------------------------------------- //
void update_num_entries() { // get number of entries at this depth level and selection
    if (config_menu_active) {
        switch (conf_depth) {
            case 0:
                num_entries = NUM_ENTRIES_D0;
                break;
            case 1:
                num_entries = NUM_ENTRIES_D1[conf_selection[conf_depth - 1]]; // needs to be - 1 because its the prev menu selection that matters
                break;
        }
    } else { // main menu
        num_entries = NUM_ENTRIES_MAIN;
    }
}

void dive() { // dive deeper into menu one level, update a bunch of random stuff
    conf_depth++;
    update_num_entries();
    
    current_text = get_entry(0);
    
    reset_silly_scroller();
    update_display();
}

void surface() { // surface out from menu one level, update a bunch of random stuff
    
    conf_depth--;
    update_num_entries();
    
    current_text = get_entry(0);
    
    reset_silly_scroller();
    update_display();
}

// MENU BEHAVIORAL FUNCTIONS ------------------------------------------------------------------------ //

int adc_get_value(unsigned int min, unsigned int max, char* units) { // this is the entire mode for capturing an ADC value, has max and min for mapping
    unsigned int mapped_val;
    adc_result_t read_val;
    int random_counter = 25; // setting it to 25 removes unwanted latency after button press to display of ADC mode
    
    while (1) {
        
        if (random_counter > 25) { //every 250 ms
            random_counter = 0;
            read_val = ADC_GetConversion(POT);
            mapped_val = ((unsigned long) (max - min)*read_val)/1024 + min; // turn ADC value into a mapped value from min to max

            clearLCD();
            setline0LCD();
            printf("Value:");
            setline1LCD();
            printf("%d %s", mapped_val, units); // display value with units provided
        }
        
        if (button_check_config() == 0) { // if long press
            
            clearLCD();
            setline0LCD();
            printf("Setting Value       ");
            
            __delay_ms(250);
            return mapped_val;
        }
        __delay_ms(10);
        random_counter++;
        
        NOP();
    }
}

void select_entry() { // this is the main behavior code for the user interface, it defines the function that each entry will have
    if (config_menu_active) {
        switch (conf_selection[0]) { // 0 depth config menu selection

            case 0: // exit config mode D0

                clearLCD();
                setline0LCD();
                printf("Exiting...");

                __delay_ms(1000);
                clearLCD();

                config_menu_active = 0;
                update_num_entries();

                break;
            case 1: // Show Event D1

                switch (conf_depth) {
                    case 0: // long click to enter into security event list

                        dive();

                        break;
                    case 1: // long click to exit or select event

                        switch (conf_selection[1]) {
                            case 0: // exit to config menu

                                clearLCD();
                                setline0LCD();
                                printf("Exiting...");

                                __delay_ms(500);

                                surface();

                                break;
                            case 1: // clear event

                                clearLCD();
                                setline0LCD();
                                printf("Clearing data...");

                                security_event_time = 0;
                                security_event_angle = 0;
                                security_event_distance = 200; // max distance

                                __delay_ms(700);

                                update_display();

                                break;
                            case 2: // time

                                clearLCD(); // display value while button is held down
                                setline0LCD();
                                printf("Time of event:");
                                setline1LCD();
                                printf("%u sec", security_event_time);

                                while (BUTTON_PORT == 0) {}

                                update_display();

                                break;
                            case 3: // angle

                                clearLCD();
                                setline0LCD();
                                printf("Angle of Event:");
                                setline1LCD();
                                printf("%u deg", security_event_angle);

                                while (BUTTON_PORT == 0) {}

                                update_display();

                                break;

                            case 4: // distance

                                clearLCD();
                                setline0LCD();
                                printf("Dist of Event:");
                                setline1LCD();
                                printf("%u in min", security_event_distance);

                                while (BUTTON_PORT == 0) {}

                                update_display();

                                break;
                        }

                        break;
                }

                break;
            case 2: { // Motor Period ADC

                // MINIMUM   motor_period > 11*(FOV_range_max - FOV_range_min); derived from angular speed
                // MINIMUM   motor_period > ultrasonic_read_speed*num_ultrasonic_averages*(FOV_range_max - FOV_range_min)/15; derived from num of steps
                // NO MAX HOWEVER LETS JUST SAY 16 s
                NOP();
                int min1 = 11*(FOV_range_max - FOV_range_min);
                int min2 = ((unsigned long) ultrasonic_read_speed*num_ultrasonic_averages*(FOV_range_max - FOV_range_min))/15;
                int min = min1 > min2 ? min1 : min2;
                int max = 16000;

                motor_period = adc_get_value(min, max, "ms");
                update_display();

                break;
            } case 3: { // Ultrasonic Read Speed ADC

                // MINIMUM  ultrasonic_read_speed > 60
                // MAXIMUM  ultrasonic_read_speed < motor_period*15/(FOV_range_max - FOV_range_min)/num_ultrasonic_averages
                NOP();
                unsigned int min = 60;
                compute_steps_per_period();
                unsigned int max = ((unsigned long) motor_period*15)/((FOV_range_max - FOV_range_min)*num_ultrasonic_averages);

                ultrasonic_read_speed = adc_get_value(min, max, "ms");
                update_display();

                break;

            } case 4: { // Num Averages Ultrasonic ADC

                // MAXIMUM  num_ultrasonic_averages < motor_period*15/(FOV_range_max - FOV_range_min)/ultrasonic_read_speed
                // MINIMUM  1
                NOP();
                unsigned int min = 1;
                unsigned int max = ((unsigned long) motor_period*15)/((unsigned int) (FOV_range_max - FOV_range_min)*ultrasonic_read_speed);

                num_ultrasonic_averages = adc_get_value(min, max, "samples");
                update_display();

                break;

            } case 5: { // Field of View Range D1 ADC

                // MAX RANGE   FOV_range_max - FOV_range_min < motor_period*15/num_ultrasonic_averages/ultrasonic_read_speed
                // MAX RANGE   FOV_range_max - FOV_range_min < motor_period/11


                // FOV_range_max
                // MAXIMUM   FOV_range_max < motor_period*15/num_ultrasonic_averages/ultrasonic_read_speed + FOV_range_min
                // MAXIMUM   FOV_range_max < motor_period/11 + FOV_range_min
                // MINIMUM   FOV_range_max > FOV_range_min

                // FOV_range_min
                // MINIMUM   FOV_range_min > FOV_range_max - motor_period*15/num_ultrasonic_averages/ultrasonic_read_speed
                // MINIMUM   FOV_range_min > FOV_range_max - motor_period/11
                // MAXIMUM   FOV_range_min < FOV_range_max
                switch (conf_depth) {
                    case 0: // enter into menu

                        dive();

                        break;
                    case 1: // select exit, FOV min or FOV max

                        switch (conf_selection[1]) { // different options in menu
                            case 0: // exit

                                clearLCD();
                                setline0LCD();
                                printf("Exiting...");

                                __delay_ms(500);

                                surface();

                                break;
                            case 1: { // FOV_range_max
                                NOP();
                                unsigned int min = FOV_range_min;
                                compute_steps_per_period();
                                unsigned int max1 = 15*steps_per_period + FOV_range_min;
                                unsigned int max2 = motor_period/11 + FOV_range_min;
                                unsigned int max3 = 180;
                                unsigned int max = max1 < max2 ? max1 : max2;
                                max = max < max3 ? max : max3;

                                FOV_range_max = adc_get_value(min, max, "deg");
                                update_display();

                                break;

                            } case 2: { // FOV_range_min
                                NOP();
                                compute_steps_per_period();
                                int min1 = FOV_range_max - 15*steps_per_period;
                                int min2 = FOV_range_max - motor_period/11;
                                int min3 = 0;
                                int min = min1 > min2 ? min1 : min2;
                                min = min > min3 ? min : min3;
                                int max = FOV_range_max;

                                FOV_range_min = adc_get_value(min, max, "deg");
                                update_display();

                                break;
                            }
                        }

                        break;
                }


                break; 

            } case 6: { // Distance Range D1 ADC

                // dist_max
                // MINIMUM  dist_max > 0
                // MINIMUM  dist_max > dist_min
                // MAXIMUM  dist_max < 156

                 // dist_min
                // MINIMUM  dist_min > 0
                // MAXIMUM  dist_min < dist_max
                // MAXIMUM  dist_min < 156

                switch (conf_depth) {
                    case 0: // enter into menu

                        dive();

                        break;
                    case 1: // select exit, FOV min or FOV max

                        switch (conf_selection[1]) { // different options in menu
                            case 0: // exit

                                clearLCD();
                                setline0LCD();
                                printf("Exiting...");

                                __delay_ms(500);

                                surface();

                                break;
                            case 1: { // dist_max
                                NOP();
                                int min = dist_min;
                                int max = 156;

                                dist_max = adc_get_value(min, max, "in");
                                update_display();

                                break;

                            } case 2: { // dist_min
                                NOP();
                                int max = dist_max;
                                int min = 0;

                                dist_min = adc_get_value(min, max, "in");
                                update_display();

                                break;
                            }
                        }

                        break;
                }

                break;

            } case 7: // Self Destruct Sequence D0

                clearLCD();
                setline0LCD();
                printf("Initiated...          ");
                __delay_ms(500);
                int ctr = 0;
                for (int i = 2048; i > 16; i = i/2) {
                    while (ctr < i) {
                        __delay_ms(2);
                        ctr++;
                    }
                    ctr = 0;
                    BEEPP_LAT = 1;
                    __delay_ms(100);
                    BEEPP_LAT = 0;
                }
                for (int i = 0; i < 15; i++) {
                    __delay_ms(32);
                    BEEPP_LAT = 1;
                    __delay_ms(50);
                    BEEPP_LAT = 0;
                }

                clearLCD();
                config_menu_active = 0;

                init_config();

                break;
        }
    } else {
        switch (main_selection) {
            
            case 0: // enter conf mode
                config_menu_active = 1;
                update_num_entries();
                
                break;
            case 1: // time
                clearLCD(); // display value while button is held down
                setline0LCD();
                printf("Current Time:    ");
                setline1LCD();
                printf("%u sec", sec_timer);

                while (BUTTON_PORT == 0) {}

                update_display();

                break;
            case 2: // angle
                clearLCD(); // display value while button is held down
                setline0LCD();
                printf("Current Angle:   ");
                setline1LCD();
                printf("%u deg", (sample - sample_lower)*3);

                while (BUTTON_PORT == 0) {}

                update_display();
                
                break;
            case 3: // distance
                clearLCD(); // display value while button is held down
                setline0LCD();
                printf("Current Dist:   ");
                setline1LCD();
                printf("%u in", ave_inches);

                while (BUTTON_PORT == 0) {}

                update_display();
                
                break;
        }
    }
}

//=============================================================================================================



// speed of sound is 343 m/s = 13504 inches/sec = .0135 inches/microsec
// remember round trip travel
// so each microsec is .013504/2 inches to target =.006752 inches
// the scalefactor is .006752, scale up by 1000000 to avoid floating math
// this approximates to 1/148

void delay_ms(unsigned int count) { //variable delay time of 1ms intervals
    while (count > 0) {
        __delay_ms(1);              //delays for 1ms
        count--;                    //just counts down
    }
}

void Ranging_function(){
    T1GGO_nDONE=0;                  // stop TMR1
    TMR1H=0;                        // set TMR1 to 0
    TMR1L=0;
    T1GGO_nDONE=1;                  // enable TMR1 to count when the gate is high

    Trigger_SetHigh();              // send a trigger pulse to the SR04
    __delay_us(10);                 // at least 10 usec
    Trigger_SetLow();    

    __delay_us(500);                // allow time for the burst and allow the gate to open
    //while (T1GVAL==1) {};           // wait while counter is still counting
    __delay_ms(23);
    
    echo_delay=TMR1_CounterGet();   // get the measurement
    //echo_delay = 0;
    //unsigned int echo_delay_ms = echo_delay/1000;
    //if (echo_delay_ms < ultrasonic_read_speed) {
    //    delay_ms(ultrasonic_read_speed - echo_delay_ms);// calls the single ms delay to be repeated for the count of ultrasonic_read_speed
    //}*/
    
    delay_ms(ultrasonic_read_speed - 23);
    
    NOP();
}

int8_t rampDirection = 1;  //initial ramp direction, 1 = counting up, -1 = counting down

void updateRampSample() {
    sample = sample+(rampDirection*step_size_CCP);//sets a new value for sample, ramping from 
    if (sample >= sample_upper) {
        sample=sample_upper;            //ensures the servo does not exceed the upper bound
        rampDirection = -1;             //switches the count iteration to down
    }
    if (sample <= sample_lower) {
        sample = sample_lower;          //ensures the servo does not exceed the lower bound
        rampDirection = 1;              //switches the count iteration to up
    }
}

int map(int value, int in_min, int in_max, int out_min, int out_max) {
    return (value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int mappedDelay;
int beeping;
int alarming;
int countBeep;
void updateBeepTiming() {
    if (ave_inches <= dist_min && sec_timer > 5) {
        // Critical alarm mode
        alarming = 1;
        beeping = 0;
        
    } else if(ave_inches  <= dist_max) {
	// Normal beeping mode
        beeping = 1;

        
        mappedDelay = map(ave_inches, dist_max, dist_min, 10, 1); // scaled OFF time
    } else if (beeping) {
        beeping = 0;
    }
}
void beepCycle() {
    NOP();
    if (alarming) { // Currently OFF
        if (countBeep == 0) {
            BEEPP_LAT = 1;
            LED_LAT = 1;        // LED ON with BEEPER
        }
        else if(countBeep >= 5 && BEEPP_LAT){
	     BEEPP_LAT = 0;
	     LED_LAT =0;	
        }
        else if(countBeep >= 10){
	     countBeep =0;
         return;
        }
    } 
    else if(beeping){
           if(countBeep == 0){
	    BEEPP_LAT = 1;
            LED_LAT = 1;        
	   }
           else if( countBeep >= 1 && BEEPP_LAT){
	    BEEPP_LAT = 0;
	     LED_LAT =0;
	   }
           else if( countBeep >= mappedDelay) {
		countBeep =0;
        return;
           }
    } else if (BEEPP_LAT) {
        BEEPP_LAT = 0;
        LED_LAT = 0;
    }
	countBeep++;
}
void resetAlarm() {
    alarming = 0;
    // Reset the security event log if needed
}

unsigned int num_samples = 0;

#if 1
int main(void)
{
    SYSTEM_Initialize();
    
    INTERRUPT_GlobalInterruptEnable(); 
    INTERRUPT_PeripheralInterruptEnable(); 

    init_config();
    
    // initializing random variables
    button_length = 0;
    
    BEEPN_LAT = 0;
    BEEPP_LAT = 0;
    
    int ave_count=0;    //sets the counts needed to run the average if statement
    
    initLCD();          // top and bottom LCD initialization below
    setline0LCD();
    printf("LCD initialized");
    __delay_ms(1000);
    clearLCD();
    setline1LCD();
    printf("LCD initialized");
    __delay_ms(1000);
    clearLCD();
    setline0LCD();   
    
    update_display();
    
    while(1){   
        // TODO control_logic()
        
        switch (button_check_main()) {
            case 0: // long press
                
                select_entry();
                
                break;
            case 1: // short press
                
                if (main_selection < num_entries - 1) { // basic cycling code for menu options
                    main_selection++;
                } else {
                    main_selection = 0;
                }

                // for purposes of silly scrolling
                current_text = get_entry(0);
                reset_silly_scroller();

                update_display();

                __delay_ms(250); // simple cooldown 
                
                break;
            default:
                
                
                
                break;
        }
        
        if (config_menu_active) { // ------------- CONFIG MODE ------------- //
                
            resetAlarm();
            BEEPP_LAT = 0;
            
            reset_menu_vars();
            // boring display stuff
            clearLCD();
            setline0LCD();
            printf("Loading Config...         ");
            __delay_ms(1000);
            /*clearLCD();
            setline0LCD();
            printf("Next Entry:");
            setline1LCD();
            printf("Short Press              ");
            __delay_ms(2000);
            clearLCD();
            setline0LCD();
            printf("Select Entry:");
            setline1LCD();
            printf("Long Press               ");
            __delay_ms(2000);
            clearLCD();*/

            // this function will simply update the display to properly reflect the current state of the menu
            update_display(); 
            while (config_menu_active) { // MAIN CONFIG MENU LOOP

                // ---------------------------- Silly Scroller TM ---------------------------- //
                silly_scroller_counter++; // just keeps track of time
                if (silly_scroller_counter >= silly_scroller_delay_mult*SILLY_SCROLLER_PERIOD) { // when its time to shift characters
                    if (silly_scroller_max > 0) { // seeing if text is big enough to need scrolling
                        silly_scroller_delay_mult = 1;
                        silly_scroller_counter = 0;

                        homeLCD();
                        silly_scroller_ind += silly_scroller_direction; // shift text
                        fancy_display(current_text, 16, '[', ']', silly_scroller_ind); // display shifted text
                        if (silly_scroller_ind >= silly_scroller_max || silly_scroller_ind == 0) { // if it has reached the end of scrolling in a direction
                            silly_scroller_direction *= -1; 
                            silly_scroller_delay_mult = 4; // delay a bit extra on the edges to make it look nicer
                        }
                    }
                } // ----------------------------------------------------------------------------------

                switch (button_check_config()) {
                    case (0): // long press

                        select_entry();

                        __delay_ms(250); // simple cooldown 

                        break;
                    case (1): // short press

                        if (conf_selection[conf_depth] < num_entries - 1) { // basic cycling code for menu options
                            conf_selection[conf_depth]++;
                        } else {
                            conf_selection[conf_depth] = 0;
                        }

                        // for purposes of silly scrolling
                        current_text = get_entry(0);
                        reset_silly_scroller();

                        update_display();

                        __delay_ms(250); // simple cooldown 

                        break;

                    default:
                        break;
                }

                __delay_ms(10);
            }
            // CONFIG MODE EXIT
            clearLCD();
            setline0LCD();
            printf("Calculating...");

            // Do calculations based on what has been configured
            compute_steps_per_period();
            step_size_deg = (FOV_range_max - FOV_range_min)/steps_per_period; // degrees per step
            step_size_CCP = step_size_deg/3; // Determined experimentally

            update_display();

        } // CONFIG MODE END =============================================================================

        
        updateBeepTiming();
        beepCycle();
        
        
        Ranging_function();                     //calls ranging function
        current_inches = echo_delay/148;        //converts the echo delay to a total distance
        if (current_inches != 155) {
            sum=sum+current_inches;                 //adds to a running sum
            ave_count++;
        }
        num_samples++;
        if(num_samples>=num_ultrasonic_averages){ //Average range function
            if (ave_count == 0) { ave_count = 1; sum = 155; }
            num_samples=0;                        //resets the ave_count
            ave_inches=sum/ave_count; //takes average of recent measurements
            ave_count = 0;
            sum = 0;
            
            if (ave_inches > 200) { ave_inches = 200; }
            
                //Security protocols
            if (security_event_distance>ave_inches){    //only updates the security variables if there is an important measurement
                security_event_distance=ave_inches;     //sets closest inches into a variable to be recalled
                security_event_time=sec_timer;          //sets current time into a variable to be recalled
                security_event_angle=(sample - sample_lower)*3;     //sets current angle into a variable to be recalled
            }        

            CCP1_LoadDutyValue(sample);//Sets the CCP duty value to the sample 

            updateRampSample();     //updates the value used in sample
            
            ms_timer += num_ultrasonic_averages*ultrasonic_read_speed+ultrasonic_read_speed/2;
            //approximated time that has passed in ms
            if(ms_timer>=1000){     //increases the single second count for the security recall
                sec_timer++;        
                ms_timer-=1000;     //removes a seconds worth of time from the ms_timer
            //TODO decide on if we need 2000ms conversion and beep time
            }
        }        
    }
}    

#else
void main() {
    SYSTEM_Initialize();
    
    INTERRUPT_GlobalInterruptEnable(); 
    INTERRUPT_PeripheralInterruptEnable(); 
    
    initLCD();          // top and bottom LCD initialization below
    setline0LCD();
    printf("LCD initialized");
    __delay_ms(1000);
    clearLCD();
    setline1LCD();
    printf("LCD initialized");
    __delay_ms(1000);
    clearLCD();
    setline0LCD();   

    init_config();
    
    // initializing random variables
    button_length = 0;
    
    BEEPN_LAT = 0;
    BEEPP_LAT = 0;
    
    int ave_count=0;    //sets the counts needed to run the average if statement
   
    while (1) {
    Ranging_function();
    clearLCD();
    setline0LCD();
    printf("%d", echo_delay/148);
    __delay_ms(1000);
    }
    
}
#endif
