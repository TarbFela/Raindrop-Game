#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

uint8_t random[100] = {9,4,82,34,53,13,124,64,67,49,79,69,2,126,119,28,13,111,82,81,16,55,23,33,79,66,63,14,96,38,6,15,102,17,114,108,102,53,53,7,83,85,67,30,30,87,77,12,65,127,113,22,9,125,84,77,127,96,114,126,73,71,117,68,35,127,54,56,33,61,46,109,103,36,54,66,117,104,40,109,11,105,1,73,57,57,6,18,79,31,61,60,114,104,7,88,97,54,80,29};

#include "ssd1306.h"
#include "image.h"
#include "BMSPA_font.h"

const uint8_t num_chars_per_disp[]={7,7,7,5};
const uint8_t *fonts[1]= {BMSPA_font};
ssd1306_t disp;


uint32_t analog_in;

#define SLEEPTIME           10
#define RESET_PIN           0
#define SCREEN_POWER_PIN    4
#define ADC_INPUT_PIN       26

#define SSD1306_HEIGHT      64
#define SSD1306_WIDTH       128
#define SSD1306_CENTER_X    64
#define SSD1306_CENTER_Y    32

#define CONFINE_X 0x7F
#define CONFINE_Y 0x3F

#define DISPLAY_LEFT_MARGIN 5
#define ROW_HEIGHT 8


#define MAX_NUMBER_OF_BULLETS   40
#define WAIT_RANDOMNESS_CONST   6
#define BULLET_SPAWN_RARITY     180
#define BULLET_DESPAWN_Y        4
//#define SPEED_INCREASE_RARITY   1000
#define PLAYER_HEIGHT           8
#define PLAYER_WIDTH            8
#define PLAYER_Y_STARTING       8
#define BULLET_SIZE             4

int bullet_wait_speed = 1;

            //game variables
uint8_t player_position[2]; //x, y
uint8_t bullets_data[MAX_NUMBER_OF_BULLETS][3]; //x, y
int number_of_bullets = 0;
int player_hp = 0; //not health points, but HAT points (ha ha) because each raindrop on your head makes you taller
int game_restart = true;

void disp_init() {
    disp.external_vcc=false;
    ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, 0x3C, i2c1);
    ssd1306_poweron(&disp);
    ssd1306_clear(&disp);
}
void setup_gpios(void);
void bootup_animation(void);
void display_ADC(void);
void display_text(int row, const char *s) {
    int x = DISPLAY_LEFT_MARGIN;
    int y = row*ROW_HEIGHT;
    ssd1306_draw_string(&disp, x, y, 1, s);
}
void display_adc_value(int row, int channel) {
    adc_select_input(channel);
    uint32_t value = adc_read();
    char buf[20];
    sprintf(buf, "%d, 0x%X", value, value);
    display_text(row, buf);
}
void display_number(int row, int number) {
    char buf[20];
    sprintf(buf, "%d", number);
    display_text(row, buf);
}
int reset_pin_check() {
    return (sio_hw->gpio_in & (1<<RESET_PIN) != 0);
}

void bootloader_animation();


void game_engine();
void game_graphics();
void new_bullet_init();

void um() {sleep_ms(50);}





void second_core_code() {
    while(game_restart) game_graphics();
    return;
    
}

int main() {
    //INITIALIZE SECOND CORE
    

    stdio_init_all();
    um();

    //printf("configuring pins...\n");
    
    setup_gpios();
    um();
    
    adc_init();
    adc_gpio_init(ADC_INPUT_PIN);
    adc_select_input(0);
    
    

    //printf("jumping to animation...\n");
    um();
    
    gpio_init(RESET_PIN);
    gpio_pull_down(RESET_PIN);
    
    
    
    multicore_launch_core1(second_core_code);
    
    while(game_restart) game_engine();
    
    int reset_counter = 0;
    while(1) {
        //reset check
        if(sio_hw->gpio_in & (1<<RESET_PIN)){
            ++reset_counter;
            sleep_ms(10);
        }
        if(reset_counter == 100) {
            bootloader_animation();
            reset_usb_boot(0,0);
            return 0;
        }
        
    }

    return 0;
}


void setup_gpios(void) {
    i2c_init(i2c1, 400000);
    gpio_init(1);
    gpio_set_dir(1, GPIO_OUT);
    sio_hw->gpio_out |= 0x1<<1;
    gpio_set_function(2, GPIO_FUNC_I2C);
    gpio_set_function(3, GPIO_FUNC_I2C);
    gpio_pull_up(2);
    gpio_pull_up(3);
    
    gpio_init(SCREEN_POWER_PIN);
    gpio_set_dir(SCREEN_POWER_PIN, GPIO_OUT);
    
    sleep_ms(1000);
    sio_hw->gpio_out |= (1<<SCREEN_POWER_PIN);
}


void bootup_animation(void) {
    const char *words[]= {"DISPLAY", "INIT", "SUCCESS!"};

    
    disp_init();

    //printf("ANIMATION!\n");

    char buf[8];

    //for(;;) {
    for(int y=0; y<31; ++y) {
        ssd1306_draw_line(&disp, 0, y, 127, y);
        ssd1306_show(&disp);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(&disp);
    }

    for(int y=0, i=1; y>=0; y+=i) {
        ssd1306_draw_line(&disp, 0, 31-y, 127, 31+y);
        ssd1306_draw_line(&disp, 0, 31+y, 127, 31-y);
        ssd1306_show(&disp);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(&disp);
        if(y==32) i=-1;
    }


    for(int y=31; y<63; ++y) {
        ssd1306_draw_line(&disp, 0, y, 127, y);
        ssd1306_show(&disp);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(&disp);
    }
    for(int i=0; i<3; i++) {
        ssd1306_draw_string(&disp, 8, 24, 2, words[i]);
        ssd1306_show(&disp);
        sleep_ms(1500);
        ssd1306_clear(&disp);
    }
    
    sleep_ms(1000);
    ssd1306_poweroff(&disp);
    sleep_ms(100);
    ssd1306_deinit(&disp);
    
    //}
}

void display_ADC(void) {
    disp.external_vcc=false;
    ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, 0x3C, i2c1);
    ssd1306_poweron(&disp);
    ssd1306_clear(&disp);
    /*
    int y = SSD1306_CENTER_Y;
    int i = 0;
    while(1) {
        y += (adc_read() - 2048)>>7;
    
        ssd1306_draw_line(&disp, 0, y&0x3F, 127, y&0x3F);
        ssd1306_show(&disp);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(&disp);
    }
     */
    while(1) {
        display_adc_value(3,0);
        ssd1306_show(&disp);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(&disp);
    }
    
    sleep_ms(1000);
    ssd1306_poweroff(&disp);
    sleep_ms(100);
    ssd1306_deinit(&disp);
    
    //}
}

void bootloader_animation() {
    // RESET SEQUENCE...
    multicore_reset_core1();
    um();
    
    ssd1306_deinit(&disp);
    um();
    
    disp.external_vcc=false;
    ssd1306_init(&disp, SSD1306_WIDTH, SSD1306_HEIGHT, 0x3C, i2c1);
    ssd1306_poweron(&disp);
    ssd1306_clear(&disp);
    sleep_ms(1000);
    
        //display reset message
    const char *reset_message[]= {"REBOOT", "TO", "USB"};
    
    for(int i=0;i<3;i++) {
        display_text(i+2, reset_message[i]);
        ssd1306_show(&disp);
    }
    sleep_ms(1000);
    ssd1306_clear(&disp);
    ssd1306_invert(&disp,1);
    //closing animation for screen
    for(int y=0; y<SSD1306_CENTER_Y; y++) {
        ssd1306_draw_line(&disp, 0, y, SSD1306_WIDTH, y);
        ssd1306_draw_line(&disp, 0, SSD1306_HEIGHT-y, SSD1306_WIDTH, SSD1306_HEIGHT-y);
        ssd1306_show(&disp);
    }
    ssd1306_clear(&disp);
        //turn screen off
    ssd1306_poweroff(&disp);
    ssd1306_deinit(&disp);
}


void game_engine(){
    game_restart = false;
    //default values...
    bullet_wait_speed = 1;
    number_of_bullets = 0;
    player_hp = 0;
    adc_select_input(0);
    
    
    
    // wait a second
    sleep_ms(1000);
    
    //          VARIABLE INITS
    
    int x = SSD1306_CENTER_X;
    player_position[1] = PLAYER_Y_STARTING;
    
    uint8_t *bx, *by, *bt;
    
    /*/      INITIALIZE BULLETS /*/

    new_bullet_init();
    //              GAME LOOP
    
    int ge_frame_counter = 0;
    uint8_t xtra_randumb = 0;
    int game_lost = false;
    
    while(reset_pin_check() == false && game_restart == false) {
        xtra_randumb += adc_read()&0xF + x&0xF + ge_frame_counter%0xF;
        //read pot input to move player
        x += (adc_read() * 8 / 4096) - 4;
        x&=0x7F; //confine to screen
        player_position[0] = x; //update global value
        
        //          BULLET BEHAVIOR
                /// t increments when the sum of adc input and t is not divisible by 3 t += (( t + adc-read() )%3 != 0)
                /// *that's pseudo-random*
                /// bullets start up top ( y = 60 ) , and then drop once they are mature. t ≤ 50
                /// bullets drop until y ≤ 4
                /// at y ≤ 4,
                ///     x = t & 0x7F
                ///     t = 0
                ///     y = 60
        for(int i=0; i<number_of_bullets; i++) {
            bx = &bullets_data[i][0];
            by = &bullets_data[i][1];
            bt = &bullets_data[i][2];
            
                //      BULLET MOVEMENT
            *bt += bullet_wait_speed * ( ( x + ge_frame_counter + xtra_randumb) % WAIT_RANDOMNESS_CONST != 0 ); //"random" time increment
            *by += -1*( *bt > 60 ); //if time is big, move down
            if(*by < player_position[1] + PLAYER_HEIGHT + player_hp) { //if y is low, check for collisions and respawn
                
                //  COLLISION CHECK
                if(  (*bx-x<PLAYER_WIDTH) && (x-*bx<BULLET_SIZE)  ) {
                    player_hp++; //could be optimized by doing bitwise AND, but it probably isn't necessary
                    *by-=3;
                    // GAME LOSE CONDITION
                    if( (player_hp + PLAYER_HEIGHT + PLAYER_Y_STARTING) > (SSD1306_HEIGHT - 5) ) game_restart = true;
                }
                //RESET
                if(*by< BULLET_DESPAWN_Y) {
                    *bx = ( random[xtra_randumb%100] )&CONFINE_X; //pseudo-random
                    *bt = 0;
                    *by = 60;
                }
            }
            
        }
        
        //  MAKE THE GAME GET HARDER
        if(ge_frame_counter % BULLET_SPAWN_RARITY == 0) new_bullet_init(); //new bullets
        //if(ge_frame_counter % SPEED_INCREASE_RARITY == 0) bullet_wait_speed++;
        
        ge_frame_counter ++;
        //render
        multicore_fifo_push_blocking(ge_frame_counter);
        sleep_ms(SLEEPTIME);
    }
}

void new_bullet_init() {
    if(number_of_bullets>=MAX_NUMBER_OF_BULLETS) return; //more bullets than allowed? don't make a new one.
    uint8_t *bx, *by, *bt;
    bx = &bullets_data[number_of_bullets][0];
    by = &bullets_data[number_of_bullets][1];
    bt = &bullets_data[number_of_bullets][2];
    
    *bx = (25*(number_of_bullets+1))&CONFINE_X; //x on an even spread (confined)
    *by = 60; //y
    *bt = 0;
    number_of_bullets++; //global variable update
}

void game_graphics() {
    disp_init();
    ssd1306_poweron(&disp);
    /*
    int y = SSD1306_CENTER_Y;
    int i = 0;
    while(1) {
        y += (adc_read() - 2048)>>7;
    
        ssd1306_draw_line(&disp, 0, y&0x3F, 127, y&0x3F);
        ssd1306_show(&disp);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(&disp);
    }
     */
    
    int x = 0;
    int y = PLAYER_Y_STARTING;
    int frame_count = 0;
    
    while(game_restart == false) {
        x = player_position[0];
        y = player_position[1];
        
        ssd1306_draw_square(&disp, x, y, PLAYER_WIDTH, PLAYER_HEIGHT + player_hp); //draw player
        
        for(int i=0; i<number_of_bullets; i++) {
            ssd1306_draw_square(&disp, bullets_data[i][0], bullets_data[i][1], BULLET_SIZE, BULLET_SIZE); //draw bullets
        }
        display_number(3, frame_count);
        
        ssd1306_show(&disp);
        sleep_ms(SLEEPTIME);
        ssd1306_clear(&disp);
        
        frame_count = multicore_fifo_pop_blocking(); //wait...
    }
    
}
