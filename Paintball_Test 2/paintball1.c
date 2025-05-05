/*
 * This template file
 */

#include <ti/devices/msp/msp.h>
#include <elec327.h>

#define POWER_STARTUP_DELAY (16)

/* This results in approximately 0.3s of delay assuming 32.768 kHz CPU_CLK */
#define SHOW_LED_DELAY (10000)

#define LATCH_DELAY (10000)

// Pins are numbered from 0, so PB22 corresponds to the 23rd bit

// GPIOA
#define LATCH_ENABLE (0x1u)
#define P1_SELECT (0x1u << 5)
#define P1_SWAP (0x1u << 6)
#define P1_AIM_IND (0x1u << 7)
#define P1_AIM_3 (0x1u << 8)
#define P1_AIM_2 (0x1u << 10)
#define P1_AIM_1 (0x1u << 11)
#define P2_AIM_IND (0x1u << 14)
#define P2_HIDE_IND (0x1u << 15)
#define P2_AIM_3 (0x1u << 16)
#define P2_AIM_2 (0x1u << 17)
#define P2_AIM_1 (0x1u << 18)
#define P2_LIVES_1 (0x1u << 21)
#define P2_HIDE_1 (0x1u << 23)
#define P1_LIVES_3 (0x1u << 25)
#define P1_LIVES_2 (0x1u << 26)
#define P1_LIVES_1 (0x1u << 27)

//GPIOB
#define P1_HIDE_IND (0x1u << 3)
#define P1_HIDE_3 (0x1u << 7)
#define P1_HIDE_2 (0x1u << 8)
#define P1_HIDE_1 (0x1u << 14)
#define P2_SWAP (0x1u << 15)
#define P2_SELECT (0x1u << 16)
#define P2_LIVES_3 (0x1u << 18)
#define P2_LIVES_2 (0x1u << 19)
#define P2_HIDE_3 (0x1u << 20)
#define P2_HIDE_2 (0x1u << 24)

//Output LED patterns
uint8_t p1hide_loc = 0b100;
uint8_t p2hide_loc = 0b100;
uint8_t p1aim_loc = 0b100;
uint8_t p2aim_loc = 0b100;
uint8_t p1lives = 0b111;
uint8_t p2lives = 0b111;


//LED arrays
const uint32_t P1_HIDE_LEDS[3] = {P1_HIDE_1, P1_HIDE_2, P1_HIDE_3};
const uint32_t P1_AIM_LEDS[3] = {P1_AIM_1, P1_AIM_2, P1_AIM_3};
const uint32_t P1_LIFE_LEDS[3] = {P1_LIVES_1, P1_LIVES_2, P1_LIVES_3};

const uint32_t P2_HIDE_LEDS[3] = {P2_HIDE_1, P2_HIDE_2, P2_HIDE_3};
const uint32_t P2_AIM_LEDS[3] = {P2_AIM_1, P2_AIM_2, P2_AIM_3};
const uint32_t P2_LIFE_LEDS[3] = {P2_LIVES_1, P2_LIVES_2, P2_LIVES_3};

//Flags
int p1_hid = 0;
int p1_aimed = 0;
int p2_aimed = 0;
int p2_hid = 0;
int p1_button_pressed = 0;
int p2_button_pressed = 0;

//Function prototypes
void display_outputs();
void reset_outputs();

//Gameplay states
enum current_state_enum {
    INITIALIZE = 0,
    WAIT_FOR_INPUT = 1,
    CHECK_INPUT = 2,
    TIEBREAK = 3,
    GAMEOVER1 = 4,
    GAMEOVER2 = 5
};


void InitializeCLKOut(void) {
    // This function initializes the GPIO so that the system clock is outputed to pin PA31
    // !!!LOOK-OUT!!! GPIOA POWER CYCLING!!! (change if using GPIOA!)
    GPIOA->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | GPIO_RSTCTL_RESETSTKYCLR_CLR | GPIO_RSTCTL_RESETASSERT_ASSERT);
    GPIOA->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | GPIO_PWREN_ENABLE_ENABLE);
    delay_cycles(POWER_STARTUP_DELAY); // delay to enable GPIO to turn on and reset
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM6)] = (IOMUX_PINCM_PC_CONNECTED | IOMUX_PINCM6_PF_SYSCTL_CLK_OUT);
    GPIOA->DOESET31_0 = 0x1u<<31; // PA31 is our output pin for the clock
}

void InitializeProcessor(void) {
    SYSCTL->SOCLOCK.BORTHRESHOLD = SYSCTL_SYSSTATUS_BORCURTHRESHOLD_BORMIN; // Brownout generates a reset.

    update_reg(&SYSCTL->SOCLOCK.MCLKCFG, (uint32_t) SYSCTL_MCLKCFG_UDIV_NODIVIDE, SYSCTL_MCLKCFG_UDIV_MASK); // Set UPCLK divider

    // Set MCLK to LFCLK
    SYSCTL->SOCLOCK.SYSOSCCFG |= SYSCTL_SYSOSCCFG_DISABLE_ENABLE; // Disable SYSOSC. We don't need it!
    SYSCTL->SOCLOCK.MCLKCFG |= SYSCTL_MCLKCFG_USELFCLK_ENABLE;

    // Verify LFCLK -> MCLK
    while ((SYSCTL->SOCLOCK.CLKSTATUS & SYSCTL_CLKSTATUS_CURMCLKSEL_MASK) != SYSCTL_CLKSTATUS_CURMCLKSEL_LFCLK) {
        ;
    }

    // Disable MCLK Divider
    update_reg(&SYSCTL->SOCLOCK.MCLKCFG, (uint32_t) 0x0, SYSCTL_MCLKCFG_MDIV_MASK);

    // Enable external clock out
    update_reg(&SYSCTL->SOCLOCK.GENCLKCFG,
        (uint32_t) SYSCTL_GENCLKCFG_EXCLKDIVEN_PASSTHRU | (uint32_t) SYSCTL_GENCLKCFG_EXCLKSRC_LFCLK,
        SYSCTL_GENCLKCFG_EXCLKDIVEN_MASK | SYSCTL_GENCLKCFG_EXCLKDIVVAL_MASK |
            SYSCTL_GENCLKCFG_EXCLKSRC_MASK);
    SYSCTL->SOCLOCK.GENCLKEN |= SYSCTL_GENCLKEN_EXCLKEN_ENABLE;

    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk;
    SysTick->LOAD = 0x00FFFFFF;
    SysTick->VAL  = 0;
    SysTick->CTRL |= (SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk);
}

int main(void)
{
    InitializeProcessor();
    InitializeCLKOut();

    /* Code to initialize GPIO PORT */


    // 1. Reset GPIO port (the one(s) for pins that you will use)
    GPIOB->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | GPIO_RSTCTL_RESETSTKYCLR_CLR | GPIO_RSTCTL_RESETASSERT_ASSERT);
    GPIOA->GPRCM.RSTCTL = (GPIO_RSTCTL_KEY_UNLOCK_W | GPIO_RSTCTL_RESETSTKYCLR_CLR | GPIO_RSTCTL_RESETASSERT_ASSERT);

    // 2. Enable power on LED GPIO port
    GPIOB->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | GPIO_PWREN_ENABLE_ENABLE);
    GPIOA->GPRCM.PWREN = (GPIO_PWREN_KEY_UNLOCK_W | GPIO_PWREN_ENABLE_ENABLE);

    delay_cycles(POWER_STARTUP_DELAY); // delay to enable GPIO to turn on and reset

    /* Code to initialize specific GPIO PINS */

    // 3a. Initialize the appropriate pin(s) as digital outputs (Configure the Pinmux!)
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM1)]  = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM14)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM19)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM21)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM22)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM36)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM37)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM38)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM39)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM40)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM46)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM53)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM55)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM59)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM60)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM16)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM24)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM25)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM31)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM44)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM45)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM48)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM52)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001));

    //3b. Configure Pins as Digital Inputs
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM10)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001 | IOMUX_PINCM_INENA_ENABLE));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM11)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001 | IOMUX_PINCM_INENA_ENABLE));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM32)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001 | IOMUX_PINCM_INENA_ENABLE));
    IOMUX->SECCFG.PINCM[(IOMUX_PINCM33)] = (IOMUX_PINCM_PC_CONNECTED | ((uint32_t) 0x00000001 | IOMUX_PINCM_INENA_ENABLE));

    // 4. Clear pins (set values to 0)
    GPIOA->DOUTCLR31_0 =  (LATCH_ENABLE | P1_SELECT | P1_SWAP | P1_AIM_IND | P1_AIM_3 | P1_AIM_2 | P1_AIM_1 | P2_AIM_IND | P2_HIDE_IND);
    GPIOA->DOUTCLR31_0 |= (P2_AIM_3 | P2_AIM_2 | P2_AIM_1 | P2_LIVES_1 | P2_HIDE_1 | P1_LIVES_3 | P1_LIVES_2 | P1_LIVES_1);

    GPIOB->DOUTCLR31_0 = (P1_HIDE_IND | P1_HIDE_3 | P1_HIDE_2 | P1_HIDE_1 | P2_SELECT | P2_SWAP | P2_LIVES_3 | P2_LIVES_2 | P2_HIDE_3 | P2_HIDE_2);


    // 5. Enable GPIO output
    GPIOA->DOESET31_0 =  (LATCH_ENABLE | P1_AIM_IND | P1_AIM_3 | P1_AIM_2 | P1_AIM_1 | P2_AIM_IND | P2_HIDE_IND);
    GPIOA->DOESET31_0 |= (P2_AIM_3 | P2_AIM_2 | P2_AIM_1 | P2_LIVES_1 | P2_HIDE_1 | P1_LIVES_3 | P1_LIVES_2 | P1_LIVES_1);

    GPIOB->DOESET31_0 = (P1_HIDE_IND | P1_HIDE_3 | P1_HIDE_2 | P1_HIDE_1 | P2_LIVES_3 | P2_LIVES_2 | P2_HIDE_3 | P2_HIDE_2);

    // 6. Enable GPIO input on the buttons

    //p1/2 swap, select

    // Initialize state machine variable
    enum current_state_enum next_state;
    next_state = INITIALIZE;

    uint32_t input_signal = 1;

    #pragma nounroll
    while(1){

        //turn on and off leds as tests

//        GPIOB->DOUTSET31_0 = P2_HIDE_2;
//        GPIOA->DOUTSET31_0 = P2_AIM_2;
//        GPIOB->DOUTSET31_0 = P1_HIDE_2;
//        GPIOA->DOUTSET31_0 = P1_AIM_2;
//        GPIOA->DOUTSET31_0 = LATCH_ENABLE;
//        delay_cycles(SHOW_LED_DELAY);
//
//        GPIOB->DOUTCLR31_0 = P2_HIDE_2;
//        GPIOA->DOUTCLR31_0 = P2_AIM_2;
//        GPIOB->DOUTCLR31_0 = P1_HIDE_2;
//        GPIOA->DOUTCLR31_0 = P1_AIM_2;
//        delay_cycles(SHOW_LED_DELAY);

//        GPIOB->DOUTSET31_0 = P2_HIDE_2;
//        GPIOA->DOUTSET31_0 = P2_AIM_2;
//        delay_cycles(SHOW_LED_DELAY);
//
//        GPIOB->DOUTCLR31_0 = P2_HIDE_2;
//        GPIOA->DOUTCLR31_0 = P2_AIM_2;
//        delay_cycles(SHOW_LED_DELAY);
////
////        GPIOA->DOUTSET31_0 = P1_LIVES_2;
////        delay_cycles(SHOW_LED_DELAY);
////
////        GPIOA->DOUTCLR31_0 = P1_LIVES_2;
////        delay_cycles(SHOW_LED_DELAY);
////
////        GPIOB->DOUTSET31_0 = P2_LIVES_2;
////        delay_cycles(SHOW_LED_DELAY);
////
////        GPIOB->DOUTCLR31_0 = P2_LIVES_2;
////        delay_cycles(SHOW_LED_DELAY);
////
//        GPIOB->DOUTSET31_0 = P1_HIDE_2;
//        GPIOA->DOUTSET31_0 = P1_AIM_2;
//        GPIOA->DOUTSET31_0 = LATCH_ENABLE;
//        delay_cycles(SHOW_LED_DELAY);
//
//        GPIOA->DOUTCLR31_0 = LATCH_ENABLE;
//        delay_cycles(SHOW_LED_DELAY);
//
//        GPIOB->DOUTCLR31_0 = P1_HIDE_2;
//        GPIOA->DOUTCLR31_0 = P1_AIM_2;
//        delay_cycles(SHOW_LED_DELAY);
//
//        GPIOA->DOUTSET31_0 = LATCH_ENABLE;
//        delay_cycles(SHOW_LED_DELAY);
//
//        GPIOA->DOUTCLR31_0 = LATCH_ENABLE;
//        delay_cycles(SHOW_LED_DELAY);
////
        //try a read
//        input_signal = ((GPIOA->DIN31_0) & P1_SELECT) == 0;
//        if(input_signal){
//            GPIOA->DOUTSET31_0 = P2_HIDE_1;
//            delay_cycles(SHOW_LED_DELAY);
//            GPIOA->DOUTCLR31_0 = P2_HIDE_1;
//            delay_cycles(SHOW_LED_DELAY);
//        }
//        else{
//            GPIOB->DOUTSET31_0 = P2_HIDE_2;
//            delay_cycles(SHOW_LED_DELAY);
//            GPIOB->DOUTCLR31_0 = P2_HIDE_2;
//            delay_cycles(SHOW_LED_DELAY);
//        }


        switch (next_state) {
        case WAIT_FOR_INPUT:
            if ((p1_hid == 0) & (p1_aimed == 0)){
                GPIOB -> DOUTSET31_0 = P1_HIDE_IND;
                GPIOA -> DOUTCLR31_0 = P1_AIM_IND;
                if (((GPIOA->DIN31_0) & P1_SELECT) == 0){
                    p1_hid = 1;
                    p1_button_pressed = 1;
                }
                else if (((GPIOA->DIN31_0) & P1_SWAP) == 0){
                    if (p1_button_pressed == 0){
                        if (p1hide_loc == 0b100){
                            p1hide_loc = 0b010;
                        }
                        else if (p1hide_loc == 0b010){
                            p1hide_loc = 0b001;
                        }
                        else{
                            p1hide_loc = 0b100;
                        }
                    }
                    p1_button_pressed = 1;
                }
                else{
                    p1_button_pressed = 0;
                }
            }
            else if ((p1_hid == 1) & (p1_aimed ==0)){
                GPIOB -> DOUTCLR31_0 = P1_HIDE_IND;
                GPIOA -> DOUTSET31_0 = P1_AIM_IND;
                if (((GPIOA->DIN31_0) & P1_SELECT) == 0){
                    if (p1_button_pressed == 0){
                        p1_aimed = 1;
                    }
                    p1_button_pressed = 1;
                    GPIOA -> DOUTCLR31_0 = P1_AIM_IND;
                }
                else if (((GPIOA->DIN31_0) & P1_SWAP) == 0){
                    if (p1_button_pressed == 0){
                      if (p1aim_loc == 0b100){
                          p1aim_loc = 0b010;
                      }
                      else if (p1aim_loc == 0b010){
                          p1aim_loc = 0b001;
                      }
                      else{
                          p1aim_loc = 0b100;
                      }
                    }
                    p1_button_pressed = 1;
                }
                else{
                    p1_button_pressed = 0;
                }
            }
            if ((p2_hid == 0) & (p2_aimed == 0)){
                GPIOA -> DOUTSET31_0 = P2_HIDE_IND;
                GPIOA -> DOUTCLR31_0 = P2_AIM_IND;
                if (((GPIOB->DIN31_0) & P2_SELECT) == 0){
                    p2_hid = 1;
                    p2_button_pressed = 1;
                }
                else if (((GPIOB->DIN31_0) & P2_SWAP) == 0){
                    if (p2_button_pressed == 0){
                        if (p2hide_loc == 0b100){
                            p2hide_loc = 0b010;
                        }
                        else if (p2hide_loc == 0b010){
                            p2hide_loc = 0b001;
                        }
                        else{
                            p2hide_loc = 0b100;
                        }
                    }
                    p2_button_pressed = 1;
                }
                else{
                    p2_button_pressed = 0;
                }
            }
            else if ((p2_hid == 1) & (p2_aimed ==0)){
                GPIOA -> DOUTCLR31_0 = P2_HIDE_IND;
                GPIOA -> DOUTSET31_0 = P2_AIM_IND;
                if (((GPIOB->DIN31_0) & P2_SELECT) == 0){
                    if (p2_button_pressed == 0){
                        p2_aimed = 1;
                    }
                    p2_button_pressed = 1;
                    GPIOA -> DOUTCLR31_0 = P2_AIM_IND;
                }
                else if (((GPIOB->DIN31_0) & P2_SWAP) == 0){
                    if (p2_button_pressed == 0){
                      if (p2aim_loc == 0b100){
                          p2aim_loc = 0b010;
                      }
                      else if (p2aim_loc == 0b010){
                          p2aim_loc = 0b001;
                      }
                      else{
                          p2aim_loc = 0b100;
                      }
                    }
                    p2_button_pressed = 1;
                }
                else{
                    p2_button_pressed = 0;
                }
            }
            if((p1_aimed & p2_aimed) == 1){
                next_state = CHECK_INPUT;
            }
            else{
                next_state = WAIT_FOR_INPUT;
            }
            break;
        case CHECK_INPUT:
            p1_hid = 0;
            p1_aimed = 0;
            p2_aimed = 0;
            p2_hid = 0;
            p1_button_pressed = 0;
            p2_button_pressed = 0;
//            GPIOA -> DOUTCLR31_0 = P1_AIM_IND;
//            GPIOA -> DOUTCLR31_0 = P2_AIM_IND;
            GPIOA -> DOUTSET31_0 = LATCH_ENABLE;
            delay_cycles(LATCH_DELAY);
            GPIOA -> DOUTCLR31_0 = LATCH_ENABLE;
            if (p1hide_loc == p2aim_loc){
                if(p1lives == 0b111){
                    p1lives = 0b110;
                }
                else if (p1lives == 0b110){
                    p1lives = 0b100;
                }
                else{
                    p1lives = 0b000;
                }
            }
            if(p2hide_loc == p1aim_loc){
                if (p2lives == 0b111){
                    p2lives = 0b110;
                }
                else if (p2lives == 0b110){
                    p2lives = 0b100;
                }
                else{
                    p2lives = 0b000;
                }
            }
            if((p1lives == 0b000) & (p2lives == 0b000)){
                next_state = TIEBREAK;
            }
            else if ((p1lives == 0b000) | (p2lives == 0b000)){
                p1hide_loc = 0b101;
                p2hide_loc = 0b101;
                p1aim_loc = 0b010;
                p2aim_loc = 0b010;
                next_state = GAMEOVER1;
            }
            else{
                p1hide_loc = 0b100;
                p2hide_loc = 0b100;
                p1aim_loc = 0b100;
                p2aim_loc = 0b100;
                next_state = WAIT_FOR_INPUT;
            }

            break;
        case TIEBREAK:
            p1lives = 0b100;
            p2lives = 0b100;
            p1hide_loc = 0b100;
            p2hide_loc = 0b100;
            p1aim_loc = 0b100;
            p2aim_loc = 0b100;
            next_state = WAIT_FOR_INPUT;
            break;
        case GAMEOVER1:
            p1hide_loc = 0b101;
            p2hide_loc = 0b101;
            p1aim_loc = 0b010;
            p2aim_loc = 0b010;
            display_outputs();
            delay_cycles(SHOW_LED_DELAY);
            next_state = GAMEOVER2;
            break;
        case GAMEOVER2:
            p1hide_loc = 0b010;
            p2hide_loc = 0b010;
            p1aim_loc = 0b101;
            p2aim_loc = 0b101;
            display_outputs();
            delay_cycles(SHOW_LED_DELAY);
            next_state = GAMEOVER1;
            break;
        case INITIALIZE:
        default:
            reset_outputs();
            GPIOA -> DOUTSET31_0 = LATCH_ENABLE;
            delay_cycles(LATCH_DELAY);
            GPIOA -> DOUTCLR31_0 = LATCH_ENABLE;
            next_state = WAIT_FOR_INPUT;
            break;
        }
//        if(((GPIOA->DIN31_0) & RESTART) == 0){
//            next_state = INITIALIZE;
//        }
        display_outputs();

    }

}

void display_outputs(){

    //Clear all LEDs
    GPIOB -> DOUTCLR31_0 = P1_HIDE_LEDS[0] | P1_HIDE_LEDS[1] | P1_HIDE_LEDS[2];
    GPIOA -> DOUTCLR31_0 = P1_AIM_LEDS[0] | P1_AIM_LEDS[1] | P1_AIM_LEDS[2];
    GPIOA -> DOUTCLR31_0 = P1_LIFE_LEDS[0] | P1_LIFE_LEDS[1] | P1_LIFE_LEDS[2];

    GPIOB -> DOUTCLR31_0 = P2_HIDE_LEDS[2] | P2_HIDE_LEDS[1];
    GPIOA -> DOUTCLR31_0 = P2_HIDE_LEDS[0];
    GPIOA -> DOUTCLR31_0 = P2_AIM_LEDS[0] | P2_AIM_LEDS[1] | P2_AIM_LEDS[2];
    GPIOB -> DOUTCLR31_0 = P2_LIFE_LEDS[2] | P2_LIFE_LEDS[1];
    GPIOA -> DOUTCLR31_0 = P2_LIFE_LEDS[0];

    //Set the LEDs
    for (int i = 0; i<3; i++){
        if (p1hide_loc & (1 << (2-i))){
            GPIOB -> DOUTSET31_0 = P1_HIDE_LEDS[i];
        }
        if (p1aim_loc & (1 << (2-i))){
            GPIOA -> DOUTSET31_0 = P1_AIM_LEDS[i];
        }
        if (p1lives & (1 << (2-i))){
            GPIOA -> DOUTSET31_0 = P1_LIFE_LEDS[i];
        }
        if (p2aim_loc & (1 << (2-i))){
            GPIOA -> DOUTSET31_0 = P2_AIM_LEDS[i];
        }
    }

    if(p2hide_loc & 0x1u){
        GPIOB -> DOUTSET31_0 = P2_HIDE_LEDS[2];
    }
    if(p2hide_loc & (0x1u << 1)){
        GPIOB -> DOUTSET31_0 = P2_HIDE_LEDS[1];
    }
    if(p2hide_loc & (0x1u << 2)){
        GPIOA -> DOUTSET31_0 = P2_HIDE_LEDS[0];
    }

    if(p2lives & 0x1u){
        GPIOB -> DOUTSET31_0 = P2_LIFE_LEDS[2];
    }
    if(p2lives & (0x1u << 1)){
        GPIOB -> DOUTSET31_0 = P2_LIFE_LEDS[1];
    }
    if(p2lives & (0x1u << 2)){
        GPIOA -> DOUTSET31_0 = P2_LIFE_LEDS[0];
    }
}

void reset_outputs(){
    GPIOA->DOUTCLR31_0 =  (LATCH_ENABLE | P1_SELECT | P1_SWAP | P1_AIM_IND | P1_AIM_3 | P1_AIM_2 | P1_AIM_1 | P2_AIM_IND | P2_HIDE_IND);
    GPIOA->DOUTCLR31_0 |= (P2_AIM_3 | P2_AIM_2 | P2_AIM_1 | P2_LIVES_1 | P2_HIDE_1 | P1_LIVES_3 | P1_LIVES_2 | P1_LIVES_1);
    GPIOB->DOUTCLR31_0 = (P1_HIDE_IND | P1_HIDE_3 | P1_HIDE_2 | P1_HIDE_1 | P2_SELECT | P2_SWAP | P2_LIVES_3 | P2_LIVES_2 | P2_HIDE_3 | P2_HIDE_2);
    p1hide_loc = 0b100;
    p2hide_loc = 0b100;
    p1aim_loc = 0b100;
    p2aim_loc = 0b100;
    p1lives = 0b111;
    p2lives = 0b111;
    p1_hid = 0;
    p1_aimed = 0;
    p2_aimed = 0;
    p2_hid = 0;
    p1_button_pressed = 0;
    p2_button_pressed = 0;
}

/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
