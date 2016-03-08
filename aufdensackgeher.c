#include <stdbool.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/fuse.h>

#ifdef __AVR_ATmega644__
#define F_CPU 8000000UL
FUSES = {
	.extended = 0xFF, // (FUSE_BODLEVEL1) // 2.7 V
	.high = (FUSE_JTAGEN & FUSE_SPIEN & FUSE_EESAVE & FUSE_BOOTSZ0 & FUSE_BOOTSZ1),
	.low = (FUSE_SUT0 & FUSE_CKSEL3 & FUSE_CKSEL2 & FUSE_CKSEL0),
};
#else
#error "Unknown chip type. Fuses not configured."
#endif

// uses F_CPU, should be set appropriately
#include <util/delay.h>

//////////////////////////////////////////////////////////////////////////
// declarations
//

static inline void debounce_sample(void);
static inline void media_tick(void);

//////////////////////////////////////////////////////////////////////////
// inputs and outputs
//

void io_setup(void)
{
	// input
	PORTA |= _BV(PA0); // pullup
	DDRA &= ~_BV(DDA0);

	// outputs
	DDRC |= _BV(DDC1) | _BV(DDC0);
}

static inline bool read_button(void)
{
	return (PINA & _BV(PINA0)) == 0;
}

static inline void bell_pulse(void)
{
	PORTC |= _BV(PORTC1);
	//_delay_us(1300);
	_delay_us(3000);
	PORTC &= ~_BV(PORTC1);
}

static inline void light_off(void)
{
	PORTC &= ~_BV(PC0);
}

static inline void light_on(void)
{
	PORTC |= _BV(PC0);
}

static inline void light_toggle(void)
{
	PORTC ^= _BV(PC0);
}

//////////////////////////////////////////////////////////////////////////
// timer
//
#define CLOCK_PERIOD 0.010
uint16_t sysclock = 0;

void timer_setup(void)
{
	// WGM: Fast PWM 0b111
	TCCR0A = _BV(WGM01) | _BV(WGM00);
	TCCR0B = _BV(WGM02);

	// overflow interrupt
	TIMSK0 = _BV(TOIE0);

	OCR0A = CLOCK_PERIOD * F_CPU / 1024;
	TCCR0B |= 0b101 << CS00; // prescaler 1024
}

ISR(TIMER0_OVF_vect)
{
	sysclock += 1;
	debounce_sample();
	media_tick();
}

//////////////////////////////////////////////////////////////////////////
// input sampling, debounce and detect noise
//
// sample
// if same, count up
//   if stable, trigger
// if not not, reset
//
//

#define DEBOUNCE_PERIOD 0.1 // should cover more than one period of 50 Hz
#define DEBOUNCE_TOP (DEBOUNCE_PERIOD / CLOCK_PERIOD)

//#define DEBOUNCE_NOISE_THRESHOLD 0.1
//#define DEBOUNCE_NOISE_COUNT 5

// "RLE" of input
uint8_t debounce_value = 0;
uint16_t debounce_counter = 0;

bool debounce_state_stable = false;
uint16_t debounce_state_since = 0;

bool debounce_event_handled = true;

static inline void debounce_sample(void)
{
	bool const button_value = read_button();

	if (button_value != debounce_value)
	{
		debounce_value = button_value;
		debounce_counter = 0;

		if (debounce_state_stable)
		{
			debounce_state_since = sysclock;
			debounce_state_stable = false;
		}
	}

	if (debounce_counter < DEBOUNCE_TOP)
	{
		debounce_counter += 1;
	}
	else if (!debounce_state_stable)
	{
		debounce_state_since = sysclock;
		debounce_state_stable = true;
		debounce_event_handled = false;
	}

	//if (!debounce_state_stable && () > DEBOUNCE_NOISE_THRESHOLD)

}

//////////////////////////////////////////////////////////////////////////
// "media": blinkenlights and bell
//

#define MEDIA_DURATION 5.0
#define MEDIA_BUCKET_SIZE (MEDIA_DURATION / CLOCK_PERIOD)
//#define MEDIA_BUCKET_LEAK 1.0 // secs per sec
//#define MEDIA_BUCKET_FILL 1.0

#define MEDIA_MIN_PLAYBACK 1.0
#define MEDIA_LAMBDA_LIGHT 0.2
#define MEDIA_LAMBDA_SOUND 0.05

uint16_t media_bucket = 0; // fills...
uint16_t media_countdown = 0;
uint16_t media_countup = 0;

bool media_playback_stop = false;

static inline void media_tick(void)
{
	if (debounce_state_stable)
	{
		if (!debounce_event_handled)
		{
			if (debounce_value)
			{
				media_countdown = media_bucket;
				media_countup = 0;
			}
			else
			{
				media_playback_stop = true;
			}
			debounce_event_handled = true;
		}

		// don't annoy
		if (debounce_value)
		{
			if (media_bucket > 0) media_bucket -= 1;
		}
		else
		{
			if (media_bucket < MEDIA_BUCKET_SIZE) media_bucket += 1;
		}
	}

	if (media_playback_stop && media_countup >= ((unsigned)(MEDIA_MIN_PLAYBACK / CLOCK_PERIOD)))
	{
		media_countdown = 1; // one last iteration
		media_playback_stop = false;
	}

	if (media_countdown > 0)
	{
		if (media_countdown % ((unsigned)(MEDIA_LAMBDA_LIGHT / CLOCK_PERIOD)) == 0)
			light_toggle();

		if (media_countdown % ((unsigned)(MEDIA_LAMBDA_SOUND / CLOCK_PERIOD)) == 0)
			bell_pulse();

		media_countdown -= 1;
		media_countup += 1;

		if (media_countdown == 0)
		{
			light_off();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// setup and stuff
//

void setup(void)
{
	io_setup();
	timer_setup();
	sei();
}

//////////////////////////////////////////////////////////////////////////
// main loop
//

int main(void)
{
	setup();

	while (true)
	{
		set_sleep_mode(SLEEP_MODE_IDLE);
		sleep_mode();
	}
}
