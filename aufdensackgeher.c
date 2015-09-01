/*
 * clubklingel.c
 *
 * Created: 01.09.2015 00:50:09
 *  Author: Chris
 */

#include <stdbool.h>
#define F_CPU 8000000UL

#include <util/delay.h>

#include <avr/io.h>

static inline void pulse_bell(void)
{
	PORTC |= _BV(PORTC1);

#ifdef DEBUG
	_delay_us(1500);
#else
	_delay_ms(3);
#endif

	PORTC &= ~_BV(PORTC1);
}

static inline void toggle_light(void)
{
	PORTC ^= _BV(PORTC0);
}

static inline void lights_off(void)
{
	PORTC &= ~_BV(PORTC0);
}

static inline void bell_off(void)
{
	PORTC &= ~_BV(PORTC1);
}

static void setup(void)
{
	DDRC |= _BV(DDC1) | _BV(DDC0);
	DDRA &= ~_BV(DDA0);
	PORTA |= _BV(PORTA0);
}

void bell_sequence_door(void)
{
	for (uint16_t foo = 3; foo > 0; foo -= 1)
	{
		_delay_ms(50);
		pulse_bell();
	}
}

void ring_door(void)
{
	toggle_light();
	bell_sequence_door();
	toggle_light();
	bell_sequence_door();
}

void bell_sequence_irc(void)
{
	for (uint16_t foo = 3; foo > 0; foo -= 1)
	{
		_delay_ms(500);
		pulse_bell();
	}
}

void ring_irc(void)
{
	bell_sequence_irc();
}

bool door_active(void)
{
	return (PINA & _BV(PINA0)) == 0;
}

bool irc_active(void)
{
	return false;
}

void loop(void)
{
	static uint8_t bucket = 0;
	uint8_t const bucketsize = 10;

	if (door_active() || irc_active())
	{
		if (bucket < bucketsize)
		{
			if (door_active())
				ring_door();

			if (irc_active())
				ring_irc();

			bucket += 1;
		}
		else
		{
			_delay_ms(300);
		}
	}
	else
	{
		if (bucket > 0)
			bucket -= 1;
		_delay_ms(300);
	}

	lights_off();
	bell_off();
}

int main(void)
{
	setup();

	while(true)
	{
		loop();
	}
}